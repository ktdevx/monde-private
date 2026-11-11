# CLOpts: コマンドライン引数の解析ライブラリ。静的ライブラリ clopts と別名 CLOpts::clopts を作る。
# 単独で構成する（最上位のプロジェクト）ことも、利用側のプロジェクトが add_subdirectory などで
# 取り込むこともできる。C の標準ライブラリと OS のライブラリだけにリンクする。
cmake_minimum_required(VERSION 3.21)

# CLOpts のバージョン番号を書く場所はここだけ
project(CLOpts VERSION 0.2.0 LANGUAGES C)

# 既定は「最上位なら有効、取り込まれたなら無効」。利用側が先に変数を設定していれば、その値が優先される
option(CLOPTS_BUILD_TESTS "Build the CLOpts tests and register them with CTest" ${PROJECT_IS_TOP_LEVEL})
option(CLOPTS_INSTALL "Define the CLOpts install and export rules" ${PROJECT_IS_TOP_LEVEL})
option(CLOPTS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors when building CLOpts" ${PROJECT_IS_TOP_LEVEL})

# 自分のモジュールはパスを指定して読み込む（CMAKE_MODULE_PATH などの全体に効く変数は変更しない）
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CLOptsCompilerOptions.cmake")

add_library(clopts STATIC
  src/clopts_errors.c
  src/clopts_buf.c
  src/clopts_decl.c
  src/clopts_result.c
  src/clopts_parse.c
  src/clopts_help.c
  src/clopts_argv.c
)
add_library(CLOpts::clopts ALIAS clopts)
target_include_directories(clopts
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
# Windows では CommandLineToArgvW のために shell32 をリンクする
if(WIN32)
  target_link_libraries(clopts PUBLIC shell32)
endif()
clopts_apply_compile_options(clopts)

if(CLOPTS_INSTALL)
  # インストールの規則とパッケージ構成ファイル。テスト、src/ の内部ヘッダ、Naos はインストールしない
  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)

  # インストール先では、公開ヘッダの場所をインストール先の相対パスで伝える
  target_include_directories(clopts PUBLIC "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

  set(_clopts_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/CLOpts")

  install(TARGETS clopts
    EXPORT CLOptsTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )
  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/clopts/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/clopts"
    FILES_MATCHING PATTERN "*.h"
  )
  install(EXPORT CLOptsTargets
    NAMESPACE CLOpts::
    DESTINATION "${_clopts_package_dir}"
  )

  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CLOptsConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/CLOptsConfig.cmake"
    INSTALL_DESTINATION "${_clopts_package_dir}"
    NO_SET_AND_CHECK_MACRO
  )
  # 1.0 より前のため、マイナーバージョンが違えば互換なしとする
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/CLOptsConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMinorVersion
  )
  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/CLOptsConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/CLOptsConfigVersion.cmake"
    DESTINATION "${_clopts_package_dir}"
  )
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/CLOpts"
  )
endif()

if(CLOPTS_BUILD_TESTS)
  # 取り込み構成では、利用側が CTest を有効にしている前提でテストを登録する
  if(PROJECT_IS_TOP_LEVEL)
    enable_testing()
  endif()
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CLOptsNaos.cmake")
  add_subdirectory(tests)
endif()
