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
target_include_directories(clopts_tests PRIVATE "${CLOpts_SOURCE_DIR}/src")
clopts_apply_compile_options(clopts_tests)

# 単独性の検査: cli_args/ の中に Monde の名前、Monde の共通部品のパス、ディレクトリの外を指す
# 相対パスがないことを確かめる
add_test(NAME clopts_check.isolation
  COMMAND "${CMAKE_COMMAND}" "-DROOT=${CLOpts_SOURCE_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/isolation_check.cmake"
)
set_tests_properties(clopts_check.isolation PROPERTIES LABELS clopts)

# インストール後の利用の検証: CLOpts をインストールし、置かれたファイルを検査し、インストール先を移してから、
# find_package(CLOpts) で見つけた CLOpts::clopts だけにリンクする利用側プロジェクト（package/）を構成・ビルド・
# 実行する。インストールの規則がある（CLOPTS_INSTALL が ON の）ときだけ登録する。複数構成のジェネレータでも
# インストールと利用側のビルドを同じ構成名で行うため、構成名は $<CONFIG> で渡す。利用側のビルドには、
# このビルドと同じジェネレータ、コンパイラ、ツールチェーンを使う
if(CLOPTS_INSTALL)
  get_property(_clopts_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  add_test(NAME clopts_package.find_package
    COMMAND "${CMAKE_COMMAND}"
      "-DBUILD_DIR=${CLOpts_BINARY_DIR}"
      "-DCONFIG=$<CONFIG>"
      "-DMULTI_CONFIG=${_clopts_multi_config}"
      "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/package"
      "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/package_test"
      "-DGENERATOR=${CMAKE_GENERATOR}"
      "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
      "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
      "-DTOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
      "-DC_COMPILER=${CMAKE_C_COMPILER}"
      "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
      "-DVERSION=${PROJECT_VERSION}"
      "-DINCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
      "-DLIBDIR=${CMAKE_INSTALL_LIBDIR}"
      "-DDATADIR=${CMAKE_INSTALL_DATADIR}"
      "-DLIBRARY_FILE=$<TARGET_FILE_NAME:clopts>"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/package_test.cmake"
  )
  set_tests_properties(clopts_package.find_package PROPERTIES LABELS clopts)
endif()
