# Optional native Windows services, independent of the Qt GUI.
TEMPLATE = aux
CONFIG -= debug_and_release debug_and_release_target
win32 {
    isEmpty(GETDAQ_PYTHON): GETDAQ_PYTHON = python
    isEmpty(GETDAQ_CMAKE): GETDAQ_CMAKE = cmake
    isEmpty(GETDAQ_BACKEND_JOBS): GETDAQ_BACKEND_JOBS = 2
    include(../../common/build-output.pri)
    windows_backends.target = windows_backends
    windows_backends.commands = $$shell_quote($$GETDAQ_PYTHON) $$shell_quote($$PWD/../../tools/build-windows-backends.py) --build-dir $$shell_quote($$OUT_PWD/native) --bin-dir $$shell_quote($$DESTDIR) --cmake $$shell_quote($$GETDAQ_CMAKE) --jobs $$GETDAQ_BACKEND_JOBS
    windows_backends.CONFIG += phony
    first.depends += windows_backends
    QMAKE_EXTRA_TARGETS += first windows_backends
}
