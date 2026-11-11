# CLOpts のテスト実行ファイル clopts_tests。各テストを CTest に clopts_tests.<suite>.<test> の名前と
# ラベル clopts で登録する。内部ヘッダ（clopts_internal.h）を使うため src/ を include の基点に加える
naos_add_executable(clopts_tests
  SOURCES
    main.c
    test_decl.c
    decl_test.c
    dispatch_test.c
    long_test.c
    short_test.c
    values_test.c
    positional_test.c
    errors_test.c
    help_test.c
    single_test.c
    nested_test.c
    inherit_test.c
    argv_test.c
    errors_api_test.c
  LIBRARIES CLOpts::clopts
  PREFIX "clopts_tests."
  LABELS clopts
)
target_include_directories(clopts_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
monde_apply_compile_options(clopts_tests)
