function(run_and_expect name expected_result expected_output)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL expected_result)
        message(FATAL_ERROR
            "${name}: expected exit ${expected_result}, got ${result}\nstdout:\n${output}\nstderr:\n${error}")
    endif()
    set(combined "${output}${error}")
    if(NOT combined MATCHES "${expected_output}")
        message(FATAL_ERROR
            "${name}: expected output to match '${expected_output}'\nstdout:\n${output}\nstderr:\n${error}")
    endif()
endfunction()

run_and_expect("dry-run json" 0 "\"action\":\"dry-run\""
    "${WOL_BINARY}" --config "${SAMPLE_CONFIG}" --dry-run --json desktop)

run_and_expect("list json" 0 "\"targets\""
    "${WOL_BINARY}" --config "${SAMPLE_CONFIG}" --list --json)

run_and_expect("check config text" 0 "Config OK"
    "${WOL_BINARY}" --config "${SAMPLE_CONFIG}" --check-config)

run_and_expect("config path" 0 "wol.toml"
    "${WOL_BINARY}" --print-config-path)

run_and_expect("missing config json" 1 "\"kind\":\"runtime\""
    "${WOL_BINARY}" --config "${MISSING_CONFIG}" --check-config --json)
