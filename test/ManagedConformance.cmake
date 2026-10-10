function(RunConformanceCommand)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Conformance command failed (${result}): ${ARGV}")
    endif()
endfunction()

file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}")
RunConformanceCommand("${DOTNET}" build "${PROJECT}" --configuration "${CONFIGURATION}" --nologo)
RunConformanceCommand("${DOTNET}" run --no-build --project "${PROJECT}" --configuration "${CONFIGURATION}"
    -- write "${OUTPUT_DIRECTORY}/csharp.bin")
RunConformanceCommand("${ROUNDTRIP}" "${OUTPUT_DIRECTORY}/csharp.bin" "${OUTPUT_DIRECTORY}/cpp.bin")
RunConformanceCommand("${DOTNET}" run --no-build --project "${PROJECT}" --configuration "${CONFIGURATION}"
    -- read "${OUTPUT_DIRECTORY}/cpp.bin")
