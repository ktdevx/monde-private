# ドキュメントの検査（4.3、9.1、9.4）。src/ に対応しないテストディレクトリ（tests/toolchain と同じ例外）。
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

# リポジトリ内リンク: README.md と docs/*.md の相対リンクの先が存在することを確かめる。
# docs/*.md は再帰しない GLOB なので docs/dev/ と docs/inputs/ は対象にならない。
# CONFIGURE_DEPENDS により、docs/ に章を足すと次のビルドで再構成されて対象に加わる。
file(GLOB MONDE_DOCS_LINK_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/docs/*.md")
list(SORT MONDE_DOCS_LINK_FILES)
list(PREPEND MONDE_DOCS_LINK_FILES "${PROJECT_SOURCE_DIR}/README.md")
add_test(NAME docs_check.links
  COMMAND ${CMAKE_COMMAND}
    "-DMODE=links"
    "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DFILES=${MONDE_DOCS_LINK_FILES}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/docs_check.cmake")
set_tests_properties(docs_check.links PROPERTIES LABELS docs)
