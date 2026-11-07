# ビルドした monde を 1 回起動し、1 ケースの結果を確かめる（10.6。cmake -P で実行する）。
#   cmake -DMONDE=<実行ファイル> -DARGS=<;区切りの引数> [-DSTDIN=<file>] -DEXPECT_EXIT=<n>
#         [-DEXPECT_STDOUT=<file>] [-DEXPECT_STDERR_CONTAINS=<text>] -DWORK=<作業先> -P cli_case.cmake
# 標準出力と標準エラー出力は WORK のファイルに受け、標準出力は EXPECT_STDOUT と
# cmake -E compare_files でバイト単位で比べる（改行コードや BOM を変換しない）。
# 標準エラー出力は EXPECT_STDERR_CONTAINS を部分文字列として含むことを確かめる。
# ARGS を省略するか空にすると引数なしで起動する。引数そのものに ; は使えない。
foreach(var IN ITEMS MONDE EXPECT_EXIT WORK)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "cli_case: ${var} is required")
  endif()
endforeach()
if(NOT EXPECT_EXIT MATCHES "^[0-9]+$")
  message(FATAL_ERROR "cli_case: EXPECT_EXIT must be a non-negative integer: ${EXPECT_EXIT}")
endif()
# monde は WORK を作業ディレクトリにして起動するため、相対パスは呼び出し元の作業ディレクトリから解決しておく
foreach(var IN ITEMS MONDE WORK STDIN EXPECT_STDOUT)
  if(DEFINED ${var})
    get_filename_component(${var} "${${var}}" ABSOLUTE)
  endif()
endforeach()
foreach(var IN ITEMS STDIN EXPECT_STDOUT)
  if(DEFINED ${var} AND NOT EXISTS "${${var}}")
    message(FATAL_ERROR "cli_case: ${var} does not exist: ${${var}}")
  endif()
endforeach()

# 前回の出力が残っていると、起動に失敗しても古い出力を比べてしまうので作業先を作り直す
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(stdout_file "${WORK}/stdout")
set(stderr_file "${WORK}/stderr")

# 標準入力の指定がなければ空のファイルをつなぐ（端末やテストの実行環境の入力を読まないように）
if(DEFINED STDIN)
  set(stdin_file "${STDIN}")
else()
  set(stdin_file "${WORK}/stdin")
  file(WRITE "${stdin_file}" "")
endif()

if(NOT DEFINED ARGS)
  set(ARGS "")
endif()
execute_process(
  COMMAND "${MONDE}" ${ARGS}
  WORKING_DIRECTORY "${WORK}"
  INPUT_FILE "${stdin_file}"
  OUTPUT_FILE "${stdout_file}"
  ERROR_FILE "${stderr_file}"
  RESULT_VARIABLE result
)
file(READ "${stderr_file}" stderr_text)

set(failures "")
if(NOT "${result}" STREQUAL "${EXPECT_EXIT}")
  list(APPEND failures "exit code: expected ${EXPECT_EXIT}, got ${result}")
endif()
if(DEFINED EXPECT_STDOUT)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${EXPECT_STDOUT}" "${stdout_file}"
    RESULT_VARIABLE compare_result
    OUTPUT_QUIET
    ERROR_QUIET
  )
  if(NOT compare_result EQUAL 0)
    file(READ "${EXPECT_STDOUT}" expected_hex LIMIT 64 HEX)
    file(READ "${stdout_file}" actual_hex LIMIT 64 HEX)
    list(APPEND failures "stdout differs from ${EXPECT_STDOUT} (actual: ${stdout_file})")
    list(APPEND failures "expected (hex, first 64 bytes): ${expected_hex}")
    list(APPEND failures "actual (hex, first 64 bytes): ${actual_hex}")
  endif()
endif()
if(DEFINED EXPECT_STDERR_CONTAINS)
  string(FIND "${stderr_text}" "${EXPECT_STDERR_CONTAINS}" found)
  if(found EQUAL -1)
    list(APPEND failures "stderr does not contain: ${EXPECT_STDERR_CONTAINS}")
  endif()
endif()

if(failures)
  list(JOIN failures "\n  " failure_lines)
  list(JOIN ARGS " " args_text)
  message(FATAL_ERROR
    "cli_case: ${MONDE} ${args_text}\n  ${failure_lines}\n--- stderr ---\n${stderr_text}")
endif()
