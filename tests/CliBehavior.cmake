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

find_program(PYTHON3_EXE NAMES python3 python REQUIRED)
file(MAKE_DIRECTORY "${JSON_TEMP_DIR}")

function(run_json_and_expect name expected_output output_file)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "${name}: expected exit 0, got ${result}\nstdout:\n${output}\nstderr:\n${error}")
    endif()
    file(WRITE "${output_file}" "${output}")
    execute_process(
        COMMAND "${PYTHON3_EXE}" -m json.tool "${output_file}"
        RESULT_VARIABLE json_result
        OUTPUT_VARIABLE json_output
        ERROR_VARIABLE json_error
    )
    if(NOT json_result EQUAL 0)
        message(FATAL_ERROR
            "${name}: output was not valid JSON\nstdout:\n${output}\njson stdout:\n${json_output}\njson stderr:\n${json_error}")
    endif()
    if(NOT output MATCHES "${expected_output}")
        message(FATAL_ERROR
            "${name}: expected JSON output to match '${expected_output}'\nstdout:\n${output}")
    endif()
endfunction()

run_and_expect("dry-run json" 0 "\"action\":\"dry-run\""
    "${WOL_BINARY}" --config "${SAMPLE_CONFIG}" --dry-run --json desktop)

run_and_expect("dry-run json single send" 0 "\"send_count\":1"
    "${WOL_BINARY}" --config "${SAMPLE_CONFIG}" --dry-run --json desktop)

run_and_expect("list json" 0 "\"targets\""
    "${WOL_BINARY}" --config "${SAMPLE_CONFIG}" --list --json)

run_json_and_expect("dry-run json parses" "\"action\":\"dry-run\"" "${JSON_TEMP_DIR}/dry-run.json"
    "${WOL_BINARY}" --config "${SAMPLE_CONFIG}" --dry-run --json desktop)

run_json_and_expect("list json parses" "\"targets\"" "${JSON_TEMP_DIR}/list.json"
    "${WOL_BINARY}" --config "${SAMPLE_CONFIG}" --list --json)

run_json_and_expect("check config json parses" "\"target_count\"" "${JSON_TEMP_DIR}/check-config.json"
    "${WOL_BINARY}" --config "${SAMPLE_CONFIG}" --check-config --json)

run_and_expect("check config text" 0 "Config OK"
    "${WOL_BINARY}" --config "${SAMPLE_CONFIG}" --check-config)

run_and_expect("config path" 0 "wol.toml"
    "${WOL_BINARY}" --print-config-path)

run_and_expect("missing config json" 1 "\"kind\":\"runtime\""
    "${WOL_BINARY}" --config "${MISSING_CONFIG}" --check-config --json)

run_and_expect("missing config value" 2 "usage error: --config requires a value"
    "${WOL_BINARY}" --config)

run_and_expect("unknown option" 2 "usage error: unknown option: --bogus"
    "${WOL_BINARY}" --bogus)

run_and_expect("learn missing args" 2 "usage error: usage: wol learn <name> <ipv4>"
    "${WOL_BINARY}" learn desktop)

run_and_expect("control command with target" 2 "target name cannot be combined"
    "${WOL_BINARY}" --list desktop)
