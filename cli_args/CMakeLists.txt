# CLOpts: コマンドライン引数の解析ライブラリ。静的ライブラリ clopts と別名 CLOpts::clopts を作る。
# 移動の段階では Monde の共通部品（monde::foundation）にリンクし、Monde のコンパイラオプションを使う
add_library(clopts STATIC
  src/clopts_decl.c
  src/clopts_result.c
  src/clopts_parse.c
  src/clopts_help.c
  src/clopts_argv.c
)
add_library(CLOpts::clopts ALIAS clopts)
target_include_directories(clopts
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(clopts PUBLIC monde::foundation)
# Windows では CommandLineToArgvW のために shell32 をリンクする
if(WIN32)
  target_link_libraries(clopts PUBLIC shell32)
endif()
monde_apply_compile_options(clopts)

# テストは Monde のテストが有効なときだけ登録する
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
