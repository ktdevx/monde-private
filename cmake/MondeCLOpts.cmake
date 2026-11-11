include_guard(GLOBAL)

# CLOpts（コマンドライン引数の解析ライブラリ）の入手方法の唯一の定義。
# ほかのビルド定義は CLOpts::clopts という名前だけに依存する。
# 切り離しの後は、ここの add_subdirectory を FetchContent または find_package(CLOpts) に差し替える。
# CLOpts のテストは Monde のテストの有効・無効に合わせる（Naos は MondeNaos で取得済みのものを使う）。
# CLOpts のインストールの規則は Monde の配布物に加えない。警告は Monde と同じくエラーにする。
set(CLOPTS_BUILD_TESTS ${BUILD_TESTING})
set(CLOPTS_INSTALL OFF)
set(CLOPTS_WARNINGS_AS_ERRORS ON)
add_subdirectory("${PROJECT_SOURCE_DIR}/cli_args" "${PROJECT_BINARY_DIR}/cli_args")
