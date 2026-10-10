find_package(GTest CONFIG REQUIRED)
add_executable(api-tests NavalCommand/Gateway/Api/CodecTest.cpp)
target_link_libraries(api-tests PRIVATE Gateway.Api GTest::gtest_main)
NavalCommandConfigureTarget(api-tests)
add_test(NAME codec COMMAND api-tests)
find_program(DOTNET_EXECUTABLE dotnet REQUIRED)
add_test(NAME managed-conformance
    COMMAND "${CMAKE_COMMAND}" "-DDOTNET=${DOTNET_EXECUTABLE}"
        "-DPROJECT=${PROJECT_SOURCE_DIR}/test/managed/Conformance.csproj"
        "-DCONFIGURATION=$<CONFIG>" "-DROUNDTRIP=$<TARGET_FILE:schema-roundtrip>"
        "-DOUTPUT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/conformance/$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ManagedConformance.cmake")
