# Markdown から抽出した .pot が msgfmt --check を通ることを確かめる（3.9。cmake -P で実行する）。
# EXTRACT_POT（tests/tu/extract_pot_main.c の実行ファイル）で .pot を作り、msgfmt --check にかける。
# msgfmt は警告では 0 を返し、エラーのときだけ 0 以外を返す（テンプレート用ヘッダの既定値の警告は許容する）。
#
#   cmake -DEXTRACT_POT=<実行ファイル> -DMSGFMT=<msgfmt> -DINPUT=<入力の Markdown> -DREF_FILE=<#: のファイル名>
#         -DOUTPUT=<出力の .pot> [-DKEYS=<キーパス>,...] -P msgfmt_check.cmake
foreach(var IN ITEMS EXTRACT_POT MSGFMT INPUT REF_FILE OUTPUT)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "msgfmt_check: ${var} is required")
  endif()
endforeach()

# キーパスはカンマ区切りで受け取る（add_test の引数で ; を渡すとエスケープが残るため）
set(keys "")
if(DEFINED KEYS AND NOT KEYS STREQUAL "")
  string(REPLACE "," ";" keys "${KEYS}")
endif()

# 前回の出力が残っていると、抽出に失敗しても古い .pot を検査してしまうので先に消す
file(REMOVE "${OUTPUT}" "${OUTPUT}.mo")
execute_process(
  COMMAND "${EXTRACT_POT}" "${INPUT}" "${OUTPUT}" "${REF_FILE}" ${keys}
  RESULT_VARIABLE extract_result
)
if(NOT extract_result EQUAL 0)
  message(FATAL_ERROR "msgfmt_check: extraction failed (${extract_result}): ${INPUT}")
endif()
if(NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR "msgfmt_check: the .pot was not written: ${OUTPUT}")
endif()

# /dev/null は移植性がないので、po の互換性テストと同じく .mo をビルドディレクトリに書く
execute_process(
  COMMAND "${MSGFMT}" --check -o "${OUTPUT}.mo" "${OUTPUT}"
  RESULT_VARIABLE msgfmt_result
)
if(NOT msgfmt_result EQUAL 0)
  message(FATAL_ERROR "msgfmt_check: msgfmt --check failed (${msgfmt_result}): ${OUTPUT}")
endif()
message(STATUS "msgfmt_check: msgfmt --check passed: ${OUTPUT}")
