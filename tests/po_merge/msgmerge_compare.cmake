# ケースの old.po と template.pot に msgmerge --previous をかけた出力が、正解ファイル（expected.po）とバイト単位で一致することを確かめる（cmake -P で実行する）。
#
#   cmake -DMSGMERGE=<msgmerge> -DCASE_DIR=<ケースのディレクトリ> -DOUTPUT=<出力先> [-DMSGMERGE_ARGS=<引数>...] -P msgmerge_compare.cmake
#
# MSGMERGE_ARGS には、--previous に加える引数（nofuzzy のケースは --no-fuzzy-matching）を渡す。
foreach(var IN ITEMS MSGMERGE CASE_DIR OUTPUT)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "msgmerge_compare: ${var} is required")
  endif()
endforeach()

set(old_po "${CASE_DIR}/old.po")
set(template_pot "${CASE_DIR}/template.pot")
set(golden "${CASE_DIR}/expected.po")

file(REMOVE "${OUTPUT}")
execute_process(
  COMMAND "${MSGMERGE}" --previous ${MSGMERGE_ARGS} -o "${OUTPUT}" "${old_po}" "${template_pot}"
  RESULT_VARIABLE msgmerge_result
)
if(NOT msgmerge_result EQUAL 0)
  message(FATAL_ERROR "msgmerge_compare: msgmerge failed (${msgmerge_result}): ${CASE_DIR}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E compare_files "${OUTPUT}" "${golden}"
  RESULT_VARIABLE compare_result
)
if(NOT compare_result EQUAL 0)
  message(FATAL_ERROR "msgmerge_compare: the msgmerge output differs from the golden file: ${OUTPUT} vs ${golden}")
endif()
message(STATUS "msgmerge_compare: identical: ${golden}")
