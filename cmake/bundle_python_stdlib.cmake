# bundle_python_stdlib.cmake
# Copies the Python standard library to the deployment directory, stripping
# large subdirectories that are never needed at runtime:
#   test / tests    — test suite (~155 MB in CPython 3.14)
#   __pycache__     — .pyc bytecache, regenerated on first run (~16 MB)
#   idlelib         — IDE (~8 MB)
#   tkinter         — Tk GUI toolkit (~1.5 MB)
#   ensurepip       — bootstrapper (~1.8 MB)
#   site-packages   — third-party installs (~1 MB)
#
# Required variables (pass with -D):
#   PY_STDLIB_SRC   source stdlib dir (e.g. C:/msys64/mingw64/lib/python3.14)
#   PY_STDLIB_DST   destination dir   (e.g. <build>/python/lib/python3.14)

file(COPY "${PY_STDLIB_SRC}/"
    DESTINATION "${PY_STDLIB_DST}"
    PATTERN "test"          EXCLUDE
    PATTERN "tests"         EXCLUDE
    PATTERN "__pycache__"   EXCLUDE
    PATTERN "idlelib"       EXCLUDE
    PATTERN "tkinter"       EXCLUDE
    PATTERN "turtle.py"     EXCLUDE
    PATTERN "turtledemo"    EXCLUDE
    PATTERN "site-packages" EXCLUDE
    PATTERN "ensurepip"     EXCLUDE
)
