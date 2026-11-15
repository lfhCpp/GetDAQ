#ifndef GETDAQ_SOAP_PROTOCOL_TESTS_P_H
#define GETDAQ_SOAP_PROTOCOL_TESTS_P_H

namespace GetDAQTestInternal {
void RunSoapStringParametersAndTopology();
#ifdef GETDAQ_GSOAP_INTEROP
void RunLegacyGsoapProcessInterop();
#endif
void RunProtocolValidation();
void RunSoapStringAndInlineXmlParameterForms();
void RunSoapNamespacedResponseEnvelopes();
}

#endif
