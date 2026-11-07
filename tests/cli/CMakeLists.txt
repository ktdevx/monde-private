# プロセス内テスト（Naos）が使う作業ディレクトリ（ビルドディレクトリ配下）を構成時に作る
set(MONDE_CLI_TEST_WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/work")
file(MAKE_DIRECTORY "${MONDE_CLI_TEST_WORK_DIR}")

# spec.txt のラウンドトリップは tests/markdown/spec_reader.c で例を取り出し、MND_MD_SPEC_FILE のパスから読む
monde_add_module_tests(cli
  SOURCES
    main.c
    test_util.c
    ../markdown/spec_reader.c
    run_test.c
    io_test.c
    report_test.c
    time_test.c
    language_test.c
    extract_test.c
    init_test.c
    update_test.c
    translate_test.c
    roundtrip_test.c
  DEFINITIONS
    "MONDE_CLI_TEST_WORK_DIR=\"${MONDE_CLI_TEST_WORK_DIR}\""
    "MND_MD_SPEC_FILE=\"${PROJECT_SOURCE_DIR}/tests/data/commonmark/spec.txt\""
)

# 実行ファイルの起動テスト: spec.txt の全例をビルドした monde でラウンドトリップする（10.1、10.3〜10.5）。
# spec.txt は MND_MD_SPEC_FILE と同じパスから読み、コピーも改変もしない。monde のターゲットには含めない
set(MONDE_CLI_SPEC_FILE "${PROJECT_SOURCE_DIR}/tests/data/commonmark/spec.txt")
set(MONDE_CLI_SPEC_DIR "${CMAKE_CURRENT_BINARY_DIR}/spec")
set(MONDE_CLI_ROUNDTRIP_WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/roundtrip")
file(MAKE_DIRECTORY "${MONDE_CLI_SPEC_DIR}")

# Naos を使わない実行ファイル: spec.txt の例を <番号>.md と <番号>.crlf.md に展開する
add_executable(cli_spec_split spec_split_main.c ../markdown/spec_reader.c)
target_link_libraries(cli_spec_split PRIVATE monde::foundation)
monde_apply_compile_options(cli_spec_split)

# 展開は 1 回だけ行い、2 つのラウンドトリップのテストで共有する
add_test(NAME cli_spec_split
  COMMAND cli_spec_split "${MONDE_CLI_SPEC_FILE}" "${MONDE_CLI_SPEC_DIR}")
set_tests_properties(cli_spec_split PROPERTIES LABELS cli FIXTURES_SETUP cli_spec)

foreach(variant IN ITEMS original crlf)
  if(variant STREQUAL "crlf")
    set(suffix "crlf.md")
  else()
    set(suffix "md")
  endif()
  add_test(NAME cli_roundtrip.${variant}
    COMMAND ${CMAKE_COMMAND}
      "-DMONDE=$<TARGET_FILE:monde>"
      "-DDIR=${MONDE_CLI_SPEC_DIR}"
      "-DVARIANT=${suffix}"
      "-DWORK=${MONDE_CLI_ROUNDTRIP_WORK_DIR}/${variant}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/roundtrip.cmake")
  set_tests_properties(cli_roundtrip.${variant} PROPERTIES
    LABELS cli FIXTURES_REQUIRED cli_spec)
endforeach()
