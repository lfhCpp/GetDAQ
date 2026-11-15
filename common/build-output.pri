# Qt Creator defaults remain familiar; the suite builder selects an isolated bin.
isEmpty(GETDAQ_DESTDIR) {
    DESTDIR = $$PWD/../bin
} else {
    DESTDIR = $$GETDAQ_DESTDIR
}
