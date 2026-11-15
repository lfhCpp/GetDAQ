include(ecc_system_client_probe.pro)
TARGET = input-fault-client-probe
SOURCES -= ecc_system_client_probe.cpp
SOURCES += input_fault_client_probe.cpp ../apps/envoy/eccclientstore.cpp ../apps/envoy/eccclientstore_state.cpp ../apps/envoy/eccclientstore_queries.cpp ../apps/envoy/eccclientstore_retirement.cpp ../apps/envoy/eccclientstore_mutations.cpp ../apps/envoy/eccclientstore_observationpolicy.cpp ../apps/envoy/eccclientstore_recoverypolicy.cpp ../apps/envoy/eccclientstore_storage.cpp
HEADERS += ../apps/envoy/eccclientstore.h ../apps/envoy/eccclientstore_impl_p.h ../apps/envoy/eccclientstore_observationpolicy.h ../apps/envoy/eccclientstore_recoverypolicy.h ../apps/envoy/eccclientstore_storage_p.h
