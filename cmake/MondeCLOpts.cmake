include_guard(GLOBAL)

# CLOpts（コマンドライン引数の解析ライブラリ）の入手方法の唯一の定義。
# ほかのビルド定義は CLOpts::clopts という名前だけに依存する。
# 切り離しの後は、ここの add_subdirectory を FetchContent または find_package(CLOpts) に差し替える。
add_subdirectory("${PROJECT_SOURCE_DIR}/cli_args" "${PROJECT_BINARY_DIR}/cli_args")
