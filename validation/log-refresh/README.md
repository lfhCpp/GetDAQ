# Managed backend log refresh

The original Logging.h defaults UTL_LOG_LEVEL to 10 when logging is not enabled,
which compiles out INFO, warnings and errors. Build the unchanged original
backends with UTL_LOG_LEVEL=1 (INFO and above; no DEBUG). Normal TCP connection
established messages and original ECC startup/configuration messages now reach
the existing log files and authenticated log reader. No synthetic polling log
messages or hardware actions were added. Existing processes require a safe
backend restart to enable the new build setting; no live acquisition was stopped.

The existing log header buttons have sufficient width and font-height/padding,
and a project changed after opening the log panel automatically refreshes its
instance choices without switching tabs. Other main window layout is unchanged.

30 product tests pass; 10 full project integration checks pass, now explicitly
requiring original SOAP startup and receiver connection INFO in streamed logs.
Real original receiver/synthetic frames only; no physical board acceptance.
