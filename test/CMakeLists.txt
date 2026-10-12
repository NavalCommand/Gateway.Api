find_package(GTest CONFIG REQUIRED)
include(GoogleTest)
add_executable(NavalCommand.Gateway.Api.Test NavalCommand/Gateway/Api/CodecTest.cpp)
target_link_libraries(NavalCommand.Gateway.Api.Test PRIVATE NavalCommand.Gateway.Api GTest::gtest_main)
NavalCommandConfigureTarget(NavalCommand.Gateway.Api.Test)
gtest_discover_tests(NavalCommand.Gateway.Api.Test DISCOVERY_MODE PRE_TEST)

add_executable(NavalCommand.Gateway.Api.Conformance NavalCommand/Gateway/Api/Conformance.cpp)
target_link_libraries(NavalCommand.Gateway.Api.Conformance PRIVATE NavalCommand.Gateway.Api)
NavalCommandConfigureTarget(NavalCommand.Gateway.Api.Conformance)
find_program(DOTNET_EXECUTABLE dotnet REQUIRED)
add_test(NAME NavalCommand.Gateway.Api.ManagedConformance
    COMMAND "${CMAKE_COMMAND}"
    "-DDOTNET=${DOTNET_EXECUTABLE}"
    "-DPROJECT=${PROJECT_SOURCE_DIR}/test/managed/Conformance.csproj"
    "-DCONFIGURATION=$<CONFIG>"
    "-DROUNDTRIP=$<TARGET_FILE:NavalCommand.Gateway.Api.Conformance>"
    "-DOUTPUT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/conformance"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/ManagedConformance.cmake")
