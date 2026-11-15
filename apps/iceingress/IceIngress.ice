// New optional stream-end extension. The original CEA DataProcessor interface
// remains unchanged in source/ under its original CeCILL licence.
#include <mdaq/daq/DataProcessor.ice>
module GetDAQ
{
interface DataIngress extends mdaq::daq::DataProcessor
{
    // Source-side discovery only; this identifier is not an authentication token.
    idempotent string streamSession();
    // Sent by the data source on the same ordered connection after its last byte.
    // This confirms application stream end, not TCP EOF or hardware event counts.
    // The byte count detects earlier one-way loss or reconnect before first data.
    void finishStream(string sessionId, long expectedBytes);
};
};
