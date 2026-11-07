# spec.txt のラウンドトリップは tests/markdown/spec_reader.c で例を取り出し、MND_MD_SPEC_FILE のパスから読む
monde_add_module_tests(tu
  SOURCES
    main.c
    test_util.c
    ../markdown/spec_reader.c
    markdown_test.c
    front_matter_test.c
    extract_test.c
    splice_test.c
    check_test.c
    translate_test.c
    roundtrip_test.c
  DEFINITIONS
    "MND_MD_SPEC_FILE=\"${PROJECT_SOURCE_DIR}/tests/data/commonmark/spec.txt\""
)

# 互換性テスト: 抽出した .pot が msgfmt --check を通ることを確かめる（3.9。msgfmt のある環境でだけ登録する）。
# MONDE_MSGFMT は po の互換性テストと同じキャッシュ変数（find_program は見つけた結果をキャッシュする）
find_program(MONDE_MSGFMT msgfmt)
if(MONDE_MSGFMT)
  set(MONDE_TU_GETTEXT_DIR "${CMAKE_CURRENT_BINARY_DIR}/gettext")
  file(MAKE_DIRECTORY "${MONDE_TU_GETTEXT_DIR}")

  # Naos を使わない実行ファイル: 走査 → ヘッダ付きで抽出 → .pot を書き出す
  add_executable(tu_extract_pot extract_pot_main.c)
  target_link_libraries(tu_extract_pot PRIVATE monde::tu monde::po monde::foundation)
  monde_apply_compile_options(tu_extract_pot)

  add_test(NAME tu_msgfmt_check.compat
    COMMAND ${CMAKE_COMMAND}
      "-DEXTRACT_POT=$<TARGET_FILE:tu_extract_pot>"
      "-DMSGFMT=${MONDE_MSGFMT}"
      "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/data/compat.md"
      "-DREF_FILE=compat.md"
      "-DOUTPUT=${MONDE_TU_GETTEXT_DIR}/compat.pot"
      "-DKEYS=title,description,tags"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/msgfmt_check.cmake")
  set_tests_properties(tu_msgfmt_check.compat PROPERTIES LABELS tu)
else()
  message(STATUS "tu: msgfmt not found; the tu_msgfmt_check tests are not registered")
endif()
