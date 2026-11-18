# Receiver waiting output

The original TCP receiver emits `Waiting for client connection` with a rotating
dot suffix and carriage return while accepting a CoBo data connection. This is
a terminal spinner, not repeated errors. The managed log viewer now collapses
consecutive complete spinner records into one waiting message. Raw log files,
other messages, errors and repeated waiting episodes after a disconnect remain.
The original receiver and data path are unchanged. Product tests: 29 passed.
