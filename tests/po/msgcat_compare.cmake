# 正解ファイルに msgcat をかけた出力が、正解ファイルとバイト単位で一致することを確かめる（cmake -P で実行する）。
#
#   cmake -DMSGCAT=<msgcat> -DGOLDEN=<正解ファイル> -DOUTPUT=<出力先> [-DMSGCAT_ARGS=<引数>...] -P msgcat_compare.cmake
foreach(var IN ITEMS MSGCAT GOLDEN OUTPUT)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "msgcat_compare: ${var} is required")
  endif()
endforeach()

file(REMOVE "${OUTPUT}")
execute_process(
  COMMAND "${MSGCAT}" ${MSGCAT_ARGS} -o "${OUTPUT}" "${GOLDEN}"
  RESULT_VARIABLE msgcat_result
)
if(NOT msgcat_result EQUAL 0)
  message(FATAL_ERROR "msgcat_compare: msgcat failed (${msgcat_result}): ${GOLDEN}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E compare_files "${OUTPUT}" "${GOLDEN}"
  RESULT_VARIABLE compare_result
)
if(NOT compare_result EQUAL 0)
  message(FATAL_ERROR "msgcat_compare: the msgcat output differs from the golden file: ${OUTPUT} vs ${GOLDEN}")
endif()
message(STATUS "msgcat_compare: identical: ${GOLDEN}")
