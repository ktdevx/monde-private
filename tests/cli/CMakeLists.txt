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
