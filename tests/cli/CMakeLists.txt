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

# 実行ファイルの起動テスト（10.6）: cli_case.cmake で 1 ケースずつ monde を起動し、終了コード・標準出力・
# 標準エラー出力を確かめる。入力と期待値は tests/cli/data/ に置く（改行コードと BOM を保つため
# .gitattributes で -text にしている）。実際の時計を使う extract と init は標準出力を比べない
set(MONDE_CLI_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")
set(MONDE_CLI_CASE_WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/case")

# --version の期待値はバージョンを 1 か所（project の VERSION）から作る
set(MONDE_CLI_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/version.txt")
file(WRITE "${MONDE_CLI_VERSION_FILE}" "monde ${PROJECT_VERSION}\n")

# monde_cli_case(<name> EXIT <n> [ARGS <arg>...] [STDIN <file>] [STDOUT <file>]
#                [STDERR_CONTAINS <text>] [ENVIRONMENT <VAR=value>...])
# cli_case.<name> を登録する。ARGS のリストは ; を \; にして 1 つの -DARGS に渡す（cli_case.cmake で
# リストに戻る）。add_test は ; で引数を分けるため、STDERR_CONTAINS の ; も \; にする
function(monde_cli_case name)
  cmake_parse_arguments(PARSE_ARGV 1 case "" "EXIT;STDIN;STDOUT;STDERR_CONTAINS" "ARGS;ENVIRONMENT")
  if(case_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "monde_cli_case(${name}): unknown arguments: ${case_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT DEFINED case_EXIT)
    message(FATAL_ERROR "monde_cli_case(${name}): EXIT is required")
  endif()
  list(JOIN case_ARGS "\\;" joined_args)
  set(command
    "${CMAKE_COMMAND}"
    "-DMONDE=$<TARGET_FILE:monde>"
    "-DARGS=${joined_args}"
    "-DEXPECT_EXIT=${case_EXIT}"
    "-DWORK=${MONDE_CLI_CASE_WORK_DIR}/${name}")
  if(DEFINED case_STDIN)
    list(APPEND command "-DSTDIN=${case_STDIN}")
  endif()
  if(DEFINED case_STDOUT)
    list(APPEND command "-DEXPECT_STDOUT=${case_STDOUT}")
  endif()
  if(DEFINED case_STDERR_CONTAINS)
    string(REPLACE ";" "\\;" stderr_contains "${case_STDERR_CONTAINS}")
    list(APPEND command "-DEXPECT_STDERR_CONTAINS=${stderr_contains}")
  endif()
  list(APPEND command -P "${CMAKE_CURRENT_SOURCE_DIR}/cli_case.cmake")
  add_test(NAME cli_case.${name} COMMAND ${command})
  set_tests_properties(cli_case.${name} PROPERTIES LABELS cli)
  if(case_ENVIRONMENT)
    set_tests_properties(cli_case.${name} PROPERTIES ENVIRONMENT "${case_ENVIRONMENT}")
  endif()
endfunction()

set(data "${MONDE_CLI_DATA_DIR}")

# ヘルプ・バージョン・使い方のエラー（1.2〜1.6、9.5、9.7）。エラーのときは標準出力が空であること（9.8）
monde_cli_case(help ARGS --help EXIT 0 STDOUT "${data}/help.txt")
monde_cli_case(version ARGS --version EXIT 0 STDOUT "${MONDE_CLI_VERSION_FILE}")
monde_cli_case(no_args EXIT 2 STDOUT "${data}/empty.txt"
  STDERR_CONTAINS "Usage: monde <command>")
monde_cli_case(unknown_option ARGS extract --bogus "${data}/sample.md" EXIT 2
  STDOUT "${data}/empty.txt"
  STDERR_CONTAINS "Run 'monde extract --help' for usage.")

# extract（ファイルと標準入力の -、2.4）。値に空白を含む引数がそのまま 1 つの引数として渡ることも確かめる
monde_cli_case(extract_file
  ARGS extract "${data}/sample.md" --front-matter-keys title,description
       --project-id-version "Sample 1.0"
  EXIT 0 STDERR_CONTAINS "extract: 9 messages")
monde_cli_case(extract_stdin ARGS extract - STDIN "${data}/sample.md" EXIT 0
  STDERR_CONTAINS "extract: 8 messages")

# init（-l の指定、環境変数なしで使い方のエラー 2、5.5）
monde_cli_case(init_language ARGS init "${data}/sample.pot" -l ja EXIT 0
  STDERR_CONTAINS "init: 9 messages, language ja")
monde_cli_case(init_no_env ARGS init "${data}/sample.pot" EXIT 2
  STDOUT "${data}/empty.txt" STDERR_CONTAINS "cannot determine the language; specify --language"
  ENVIRONMENT "LC_ALL=" "LC_MESSAGES=" "LANG=")

# update は時計を使わないので標準出力もバイト比較する
monde_cli_case(update ARGS update "${data}/sample.pot" "${data}/sample_old.po" EXIT 0
  STDOUT "${data}/update.expected.po"
  STDERR_CONTAINS "update: 3 matched, 2 fuzzy, 4 added, 1 obsoleted")

# translate（BOM を保つ、CRLF を保つ）
monde_cli_case(translate_bom ARGS translate "${data}/translate_bom.md" "${data}/translate.po"
  EXIT 0 STDOUT "${data}/translate_bom.expected.md"
  STDERR_CONTAINS "translate: 3 translated, 1 fuzzy")
monde_cli_case(translate_crlf ARGS translate "${data}/translate_crlf.md" "${data}/translate.po"
  EXIT 0 STDOUT "${data}/translate_crlf.expected.md"
  STDERR_CONTAINS "translate: 3 translated, 1 fuzzy")

# 存在しない入力は 1（2.9、9.6）
monde_cli_case(missing_input ARGS extract "${data}/no_such_file.md" EXIT 1
  STDOUT "${data}/empty.txt" STDERR_CONTAINS "no_such_file.md: error: cannot open file for reading")
