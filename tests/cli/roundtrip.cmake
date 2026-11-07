# ビルドした monde で spec.txt の全例をラウンドトリップする（10.1、10.3、10.4。cmake -P で実行する）。
# cli_spec_split が DIR に展開した例ごとに
#   monde extract <例> -o <WORK>/<番号>.pot
#   monde translate <例> <WORK>/<番号>.pot -o <WORK>/<番号>.out
# を実行し、出力を原文とバイト単位で比べる（cmake -E compare_files）。
# 失敗した例の番号を集め、最後に一覧で FATAL_ERROR にする。
#
#   cmake -DMONDE=<実行ファイル> -DDIR=<展開先> -DVARIANT=md|crlf.md -DWORK=<作業先> -P roundtrip.cmake
foreach(var IN ITEMS MONDE DIR VARIANT WORK)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "roundtrip: ${var} is required")
  endif()
endforeach()
if(NOT VARIANT STREQUAL "md" AND NOT VARIANT STREQUAL "crlf.md")
  message(FATAL_ERROR "roundtrip: VARIANT must be md or crlf.md: ${VARIANT}")
endif()

# 例のファイルは <番号>.md と <番号>.crlf.md（*.md は *.crlf.md にも合うので番号だけの名前に絞る）
string(REPLACE "." "\\." variant_regex "${VARIANT}")
file(GLOB examples RELATIVE "${DIR}" "${DIR}/*.${VARIANT}")
list(FILTER examples INCLUDE REGEX "^[0-9]+\\.${variant_regex}$")
list(SORT examples)
list(LENGTH examples example_count)
if(example_count EQUAL 0)
  message(FATAL_ERROR "roundtrip: no examples (*.${VARIANT}) in ${DIR}")
endif()

# 前回の出力が残っていると、失敗しても古い出力を比べてしまうので作業先を作り直す
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(failed "")
set(first_detail "")
foreach(example IN LISTS examples)
  string(REGEX MATCH "^[0-9]+" number "${example}")
  set(input "${DIR}/${example}")
  set(pot "${WORK}/${number}.pot")
  set(out "${WORK}/${number}.out")

  set(step "")
  execute_process(
    COMMAND "${MONDE}" extract "${input}" -o "${pot}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )
  if(NOT result EQUAL 0)
    set(step "extract exited with ${result}: ${stderr}")
  else()
    execute_process(
      COMMAND "${MONDE}" translate "${input}" "${pot}" -o "${out}"
      RESULT_VARIABLE result
      OUTPUT_VARIABLE stdout
      ERROR_VARIABLE stderr
    )
    if(NOT result EQUAL 0)
      set(step "translate exited with ${result}: ${stderr}")
    else()
      execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files "${input}" "${out}"
        RESULT_VARIABLE result
        OUTPUT_QUIET
        ERROR_QUIET
      )
      if(NOT result EQUAL 0)
        set(step "the output differs from the input: ${out}")
      endif()
    endif()
  endif()

  if(NOT step STREQUAL "")
    list(APPEND failed "${number}")
    if(first_detail STREQUAL "")
      set(first_detail "${example}: ${step}")
    endif()
  endif()
endforeach()

list(LENGTH failed failed_count)
if(failed_count GREATER 0)
  string(REPLACE ";" " " failed_text "${failed}")
  message(FATAL_ERROR
    "roundtrip: ${failed_count} of ${example_count} examples (*.${VARIANT}) failed: ${failed_text}\n"
    "roundtrip: first failure: ${first_detail}")
endif()
message(STATUS "roundtrip: all ${example_count} examples (*.${VARIANT}) round-tripped")
