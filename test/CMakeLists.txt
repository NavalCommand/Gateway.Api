add_executable(schema-roundtrip NavalCommand/Gateway/Api/SchemaRoundtrip.cpp)
target_link_libraries(schema-roundtrip PRIVATE Gateway.Api)
NavalCommandConfigureTarget(schema-roundtrip)

find_package(GTest CONFIG REQUIRED)
add_executable(api-tests NavalCommand/Gateway/Api/CodecTest.cpp)
target_link_libraries(api-tests PRIVATE Gateway.Api GTest::gtest_main)
NavalCommandConfigureTarget(api-tests)
add_test(NAME codec COMMAND api-tests)
add_executable(domain-consumer DomainConsumer.cpp)
target_link_libraries(domain-consumer PRIVATE Gateway.Api)
NavalCommandConfigureTarget(domain-consumer)
add_test(NAME domain-transitive-includes COMMAND domain-consumer)
find_program(DOTNET_EXECUTABLE dotnet REQUIRED)
add_test(NAME managed-conformance
    COMMAND "${CMAKE_COMMAND}" "-DDOTNET=${DOTNET_EXECUTABLE}"
        "-DPROJECT=${PROJECT_SOURCE_DIR}/test/managed/Conformance.csproj"
        "-DCONFIGURATION=$<CONFIG>" "-DROUNDTRIP=$<TARGET_FILE:schema-roundtrip>"
        "-DOUTPUT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/conformance/$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ManagedConformance.cmake")

find_program(POWERSHELL_EXECUTABLE NAMES pwsh powershell)
if (POWERSHELL_EXECUTABLE)
    add_test(NAME canonical-schema
        COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -File "${CMAKE_CURRENT_SOURCE_DIR}/Check-Schema.ps1"
            -Root "${PROJECT_SOURCE_DIR}" -Protoc "$<TARGET_FILE:protobuf::protoc>")
else ()
    message(STATUS "PowerShell unavailable: canonical schema ownership check is not registered")
endif ()
