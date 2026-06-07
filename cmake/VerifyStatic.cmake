if(NOT DEFINED WOL_BINARY)
    message(FATAL_ERROR "WOL_BINARY is required")
endif()

execute_process(
    COMMAND ldd "${WOL_BINARY}"
    RESULT_VARIABLE ldd_result
    OUTPUT_VARIABLE ldd_output
    ERROR_VARIABLE ldd_error
)

set(ldd_combined "${ldd_output}${ldd_error}")
if(NOT ldd_combined MATCHES "not a dynamic executable")
    message(FATAL_ERROR "Expected a static executable from ldd, got: ${ldd_combined}")
endif()

execute_process(
    COMMAND readelf -d "${WOL_BINARY}"
    RESULT_VARIABLE readelf_result
    OUTPUT_VARIABLE readelf_output
    ERROR_VARIABLE readelf_error
)

if(readelf_output MATCHES "NEEDED")
    message(FATAL_ERROR "Static verification failed: readelf reported NEEDED entries")
endif()

execute_process(
    COMMAND file "${WOL_BINARY}"
    RESULT_VARIABLE file_result
    OUTPUT_VARIABLE file_output
    ERROR_VARIABLE file_error
)

if(NOT file_output MATCHES "statically linked")
    message(FATAL_ERROR "Static verification failed: file output was: ${file_output}${file_error}")
endif()

message(STATUS "Static verification passed for ${WOL_BINARY}")
