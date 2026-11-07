# ドキュメントの検査（4.3、9.1）。src/ に対応しないテストディレクトリ（tests/toolchain と同じ例外）。
# docs_check.cmake を cmake -P で起動し、ドキュメントの記述をビルドした monde と照合する。
set(MONDE_DOCS_CHECK_WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/work")

# ヘルプの引用: docs/commands.md のマーカー help の直後のコードブロックを --help の出力と比べる
add_test(NAME docs_check.help
  COMMAND ${CMAKE_COMMAND}
    "-DMODE=help"
    "-DMONDE=$<TARGET_FILE:monde>"
    "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DFILES=${PROJECT_SOURCE_DIR}/docs/commands.md"
    "-DWORK=${MONDE_DOCS_CHECK_WORK_DIR}/help"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/docs_check.cmake")
set_tests_properties(docs_check.help PROPERTIES LABELS docs)

# クイックスタート: README のマーカー quickstart の直後のコマンド列を docs/examples の例で実行する
add_test(NAME docs_check.quickstart
  COMMAND ${CMAKE_COMMAND}
    "-DMODE=quickstart"
    "-DMONDE=$<TARGET_FILE:monde>"
    "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DFILES=${PROJECT_SOURCE_DIR}/README.md"
    "-DEXAMPLES=${PROJECT_SOURCE_DIR}/docs/examples"
    "-DWORK=${MONDE_DOCS_CHECK_WORK_DIR}/quickstart"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/docs_check.cmake")
set_tests_properties(docs_check.quickstart PROPERTIES LABELS docs)
