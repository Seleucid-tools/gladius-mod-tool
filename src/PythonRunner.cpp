// Include Python.h before Qt headers to avoid the `slots` macro collision.
// Python 3.14 object.h has a struct member named `slots`; Qt defines `slots`
// as an empty macro. By including Python.h first here (before PythonRunner.h
// pulls in QObject), we avoid the conflict entirely.

#define PY_SSIZE_T_CLEAN
#include <Python.h>

// Now Qt headers are safe — `slots` is not yet defined at this point
// because we haven't included any Qt header yet.  Python.h itself doesn't
// define `slots`.  Qt will define it when QObject is included below.
#include "PythonRunner.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#ifdef _WIN32
#include <string>
#endif

// ── GIL state ─────────────────────────────────────────────────────────────────
// After Py_Initialize(), the main thread holds the GIL.  We immediately save
// (release) it so worker threads can acquire it freely.  s_gilState holds the
// saved state needed to re-acquire on shutdown.
static PyThreadState *s_gilState = nullptr;

bool PythonRunner::s_initialized = false;

// ── QtStream type — forwards Python stdout/stderr to Qt signals ───────────────

static PythonRunner *g_currentRunner = nullptr;

struct QtStream {
    PyObject_HEAD
    bool isStderr;
};

static PyObject *qtstream_write(PyObject *self, PyObject *args)
{
    const char *text = nullptr;
    if (!PyArg_ParseTuple(args, "s", &text))
        Py_RETURN_NONE;
    auto *s = reinterpret_cast<QtStream *>(self);
    if (g_currentRunner && text) {
        if (s->isStderr)
            emit g_currentRunner->error(QString::fromUtf8(text));
        else
            emit g_currentRunner->output(QString::fromUtf8(text));
    }
    Py_RETURN_NONE;
}

static PyObject *qtstream_flush(PyObject *, PyObject *)
{
    Py_RETURN_NONE;
}

static PyMethodDef qtstream_methods[] = {
    {"write", qtstream_write, METH_VARARGS, nullptr},
    {"flush", qtstream_flush, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr}
};

static PyType_Slot qtstream_slots[] = {
    {Py_tp_methods, qtstream_methods},
    {0, nullptr}
};

static PyType_Spec qtstream_spec = {
    "gladius.QtStream",
    sizeof(QtStream),
    0,
    Py_TPFLAGS_DEFAULT,
    qtstream_slots
};

static PyObject *g_QtStreamType = nullptr;

// ── PythonRunner ──────────────────────────────────────────────────────────────

PythonRunner::PythonRunner(QObject *parent) : QObject(parent) {}
PythonRunner::~PythonRunner() = default;

bool PythonRunner::init()
{
    if (s_initialized) return true;

#ifdef _WIN32
    // For self-contained Windows deployment: if a "python" directory sits next
    // to the exe (populated with the Windows embeddable Python package), point
    // the interpreter there so the standard library is loaded from our bundled
    // copy rather than requiring a system-wide Python installation.
    // Py_SetPythonHome must be called before Py_Initialize, and the wchar_t*
    // must remain valid for the lifetime of the process, hence the static.
    {
        QString localHome = QCoreApplication::applicationDirPath() + "/python";
        if (QDir(localHome).exists()) {
            static std::wstring s_pyHome = localHome.toStdWString();
            Py_SetPythonHome(s_pyHome.c_str());
        }
    }
#endif

    Py_Initialize();
    if (!Py_IsInitialized()) {
        qWarning() << "PythonRunner: Py_Initialize failed";
        return false;
    }

    // Build our stream type while we still hold the GIL (main thread)
    g_QtStreamType = PyType_FromSpec(&qtstream_spec);
    if (!g_QtStreamType) {
        qWarning() << "PythonRunner: PyType_FromSpec failed";
        PyErr_Print();
        return false;
    }

    // Release the GIL so the worker thread can acquire it when run() is called.
    // This is mandatory — without this, any Python API call from another thread
    // will deadlock or crash because that thread doesn't own the GIL.
    s_gilState = PyEval_SaveThread();

    s_initialized = true;
    return true;
}

void PythonRunner::shutdown()
{
    if (!s_initialized) return;

    // Re-acquire the GIL on the main thread before finalizing
    PyEval_RestoreThread(s_gilState);
    s_gilState = nullptr;

    Py_XDECREF(g_QtStreamType);
    g_QtStreamType = nullptr;

    Py_Finalize();
    s_initialized = false;
}

void PythonRunner::run(const QString &scriptPath, const QStringList &args)
{
    if (!s_initialized) {
        emit error("Python interpreter not initialised.");
        emit finished(false);
        return;
    }

    // Acquire the GIL for this worker thread before touching any Python API.
    // PyGILState_Ensure() handles the case where this thread has never held
    // the GIL before, or where it previously released it.
    PyGILState_STATE gstate = PyGILState_Ensure();

    g_currentRunner = this;

    // ── sys.argv ──────────────────────────────────────────────────────────────
    QStringList allArgs;
    allArgs << scriptPath << args;

    PyObject *pyArgv = PyList_New(allArgs.size());
    for (int i = 0; i < allArgs.size(); ++i) {
        PyList_SetItem(pyArgv, i,
            PyUnicode_DecodeFSDefault(allArgs[i].toUtf8().constData()));
    }
    PyObject *sysModule = PyImport_ImportModule("sys");
    if (sysModule)
        PyObject_SetAttrString(sysModule, "argv", pyArgv);
    Py_DECREF(pyArgv);

    // ── Redirect stdout / stderr ──────────────────────────────────────────────
    auto makeStream = [](bool isStderr) -> PyObject * {
        if (!g_QtStreamType) return nullptr;
        QtStream *s = PyObject_New(QtStream,
                                   reinterpret_cast<PyTypeObject *>(g_QtStreamType));
        if (s) s->isStderr = isStderr;
        return reinterpret_cast<PyObject *>(s);
    };

    PyObject *outStream = makeStream(false);
    PyObject *errStream = makeStream(true);

    if (sysModule && outStream && errStream) {
        PyObject_SetAttrString(sysModule, "stdout", outStream);
        PyObject_SetAttrString(sysModule, "stderr", errStream);
    }
    Py_XDECREF(outStream);
    Py_XDECREF(errStream);
    Py_XDECREF(sysModule);

    // ── sys.path ──────────────────────────────────────────────────────────────
    QString scriptDir = scriptPath.left(scriptPath.lastIndexOf('/'));
    QString sysPathSetup = QString(
        "import sys\n"
        "if '%1' not in sys.path:\n"
        "    sys.path.insert(0, '%1')\n"
    ).arg(scriptDir);
    PyRun_SimpleString(sysPathSetup.toUtf8().constData());

    // ── Run the script ────────────────────────────────────────────────────────
    QByteArray pathBytes = scriptPath.toUtf8();
    FILE *fp = fopen(pathBytes.constData(), "r");
    bool success = false;

    if (!fp) {
        emit error(QString("Cannot open script: %1").arg(scriptPath));
    } else {
        int ret = PyRun_SimpleFile(fp, pathBytes.constData());
        fclose(fp);
        success = (ret == 0);
        if (!success)
            PyErr_Print();
    }

    g_currentRunner = nullptr;

    // Release the GIL so other threads (or the next run() call) can use it
    PyGILState_Release(gstate);

    emit finished(success);
}
