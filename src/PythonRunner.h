#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

// Runs Python scripts inside the embedded CPython interpreter.
// stdout/stderr are captured and forwarded as Qt signals.
//
// GIL threading model:
//   - init()     : called on main thread; initialises Python and then
//                  releases the GIL via PyEval_SaveThread so other threads
//                  can acquire it.
//   - run()      : called on worker QThread; acquires the GIL before any
//                  Python API call and releases it when done.
//   - shutdown() : called on main thread; re-acquires the GIL then finalises.
class PythonRunner : public QObject
{
    Q_OBJECT
public:
    explicit PythonRunner(QObject *parent = nullptr);
    ~PythonRunner();

    static bool init();
    static void shutdown();

public slots:
    void run(const QString &scriptPath, const QStringList &args);

signals:
    void output(const QString &text);
    void error(const QString &text);
    void finished(bool success);

private:
    static bool s_initialized;
};
