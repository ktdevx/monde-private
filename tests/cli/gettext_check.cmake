# monde の extract → init → update の出力が msgfmt --check を通ることを確かめる（11.1。cmake -P で実行する）。
#   monde extract <DATA>/sample.md --front-matter-keys title,description -o <WORK>/sample.pot
#   monde init <WORK>/sample.pot -l ja -o <WORK>/ja.po
#   monde update <WORK>/sample.pot <DATA>/sample_old.po -o <WORK>/merged.po
# を実行し、3 つの出力それぞれに msgfmt --check -o <出力>.mo をかける。
# msgfmt は警告では 0 を返し、エラーのときだけ 0 以外を返す（テンプレート用ヘッダの既定値の警告は許容する）。
#
#   cmake -DMONDE=<実行ファイル> -DMSGFMT=<msgfmt> -DDATA=<tests/cli/data> -DWORK=<作業先> -P gettext_check.cmake
foreach(var IN ITEMS MONDE MSGFMT DATA WORK)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "gettext_check: ${var} is required")
  endif()
endforeach()

# 前回の出力が残っていると、monde が失敗しても古い出力を検査してしまうので作業先を作り直す
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# monde を起動し、終了コードが 0 で出力ファイルが書かれたことを確かめる
function(gettext_check_run output)
  execute_process(
    COMMAND "${MONDE}" ${ARGN} -o "${output}"
    RESULT_VARIABLE result
    ERROR_VARIABLE error_text
  )
  list(JOIN ARGN " " command_line)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "gettext_check: monde ${command_line} failed (${result}):\n${error_text}")
  endif()
  if(NOT EXISTS "${output}")
    message(FATAL_ERROR "gettext_check: monde ${command_line} did not write ${output}")
  endif()
endfunction()

# /dev/null は移植性がないので、po と tu の互換性テストと同じく .mo を作業先に書く
function(gettext_check_msgfmt file)
  execute_process(
    COMMAND "${MSGFMT}" --check -o "${file}.mo" "${file}"
    RESULT_VARIABLE result
    ERROR_VARIABLE error_text
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "gettext_check: msgfmt --check failed (${result}): ${file}\n${error_text}")
  endif()
  message(STATUS "gettext_check: msgfmt --check passed: ${file}")
endfunction()

set(pot "${WORK}/sample.pot")
set(ja_po "${WORK}/ja.po")
set(merged_po "${WORK}/merged.po")

gettext_check_run("${pot}" extract "${DATA}/sample.md" --front-matter-keys title,description)
gettext_check_run("${ja_po}" init "${pot}" -l ja)
gettext_check_run("${merged_po}" update "${pot}" "${DATA}/sample_old.po")

foreach(file IN ITEMS "${pot}" "${ja_po}" "${merged_po}")
  gettext_check_msgfmt("${file}")
endforeach()
