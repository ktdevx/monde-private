# file_io のテストが使う作業ディレクトリ（ビルドディレクトリ配下、ASCII 名）を構成時に作る
set(MONDE_FOUNDATION_TEST_WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/work")
file(MAKE_DIRECTORY "${MONDE_FOUNDATION_TEST_WORK_DIR}")
# 非 ASCII パスのテスト用のサブディレクトリ。名前は file_io_test.c の NON_ASCII_DIR_NAME と一致させる
file(MAKE_DIRECTORY "${MONDE_FOUNDATION_TEST_WORK_DIR}/日本語ディレクトリ")

monde_add_module_tests(foundation
  SOURCES
    main.c
    buf_test.c
    diag_test.c
    utf8_test.c
    eol_test.c
    source_test.c
    file_io_test.c
  DEFINITIONS
    "MONDE_TEST_WORK_DIR=\"${MONDE_FOUNDATION_TEST_WORK_DIR}\""
)
