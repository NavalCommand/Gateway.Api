add_executable(schema-roundtrip NavalCommand/Gateway/Api/SchemaRoundtrip.cpp)
target_link_libraries(schema-roundtrip PRIVATE Gateway.Api)
NavalCommandConfigureTarget(schema-roundtrip)
