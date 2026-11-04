monde_add_module_tests(po
  SOURCES
    main.c
    entry_test.c
    header_test.c
    escape_test.c
    read_test.c
    read_error_test.c
    linebreak_test.c
    write_test.c
    roundtrip_test.c
    init_test.c
  DEFINITIONS
    "MND_PO_TEST_DATA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/data\""
)

# 互換性テスト（gettext のある環境でだけ登録する）
set(MONDE_PO_GETTEXT_DIR "${CMAKE_CURRENT_BINARY_DIR}/gettext")
file(MAKE_DIRECTORY "${MONDE_PO_GETTEXT_DIR}")

# 正解ファイルが msgcat の出力そのものであることを確かめる（7.3 の前提）
find_program(MONDE_MSGCAT msgcat)
set(msgcat_version_ok FALSE)
if(MONDE_MSGCAT)
  # 正解ファイルは msgcat 0.21 の出力で、ほかのバージョンでは一致しないことがある（理由は data/README.md）
  execute_process(COMMAND "${MONDE_MSGCAT}" --version
    OUTPUT_VARIABLE msgcat_version_output ERROR_QUIET)
  string(REGEX MATCH "^[^\n]*" msgcat_version_line "${msgcat_version_output}")
  if(msgcat_version_line MATCHES "\\(GNU gettext-tools\\) 0\\.21([^0-9.]|\\.[0-9]|$)")
    set(msgcat_version_ok TRUE)
  else()
    message(STATUS "po: msgcat \"${msgcat_version_line}\" is not 0.21 (the golden files are msgcat 0.21 output); the po_msgcat_golden and po_msgcat_normalize tests are not registered")
  endif()
endif()
if(msgcat_version_ok)
  foreach(golden IN ITEMS basic wrap_latin wrap_cjk refs nowrap_flag nowrap_option fuzzy_empty)
    set(msgcat_args "")
    if(golden STREQUAL "nowrap_option")
      set(msgcat_args "--no-wrap")
    endif()
    add_test(NAME po_msgcat_golden.${golden}
      COMMAND ${CMAKE_COMMAND}
        "-DMSGCAT=${MONDE_MSGCAT}"
        "-DMSGCAT_ARGS=${msgcat_args}"
        "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/data/${golden}.po"
        "-DOUTPUT=${MONDE_PO_GETTEXT_DIR}/msgcat_${golden}.po"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/msgcat_compare.cmake")
    set_tests_properties(po_msgcat_golden.${golden} PROPERTIES LABELS po)
  endforeach()

  # 手で書いた入力に msgcat をかけた出力が正解ファイルと一致することを確かめる（ライターの 5.7 の規則が gettext と同じ）
  add_test(NAME po_msgcat_normalize.fuzzy_empty
    COMMAND ${CMAKE_COMMAND}
      "-DMSGCAT=${MONDE_MSGCAT}"
      "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/data/fuzzy_empty_in.po"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/data/fuzzy_empty.po"
      "-DOUTPUT=${MONDE_PO_GETTEXT_DIR}/msgcat_normalize_fuzzy_empty.po"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/msgcat_compare.cmake")
  set_tests_properties(po_msgcat_normalize.fuzzy_empty PROPERTIES LABELS po)
elseif(NOT MONDE_MSGCAT)
  message(STATUS "po: msgcat not found; the po_msgcat_golden and po_msgcat_normalize tests are not registered")
endif()

# ライターと初期化の出力が msgfmt --check を通ることを確かめる（5.6、9.6）
find_program(MONDE_MSGFMT msgfmt)
if(MONDE_MSGFMT)
  set(msgfmt_out_dir "${MONDE_PO_GETTEXT_DIR}/msgfmt")
  file(MAKE_DIRECTORY "${msgfmt_out_dir}")

  add_executable(po_msgfmt_gen msgfmt_gen.c)
  target_link_libraries(po_msgfmt_gen PRIVATE monde::po)
  target_compile_definitions(po_msgfmt_gen PRIVATE
    "MND_PO_TEST_DATA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/data\""
    "MND_PO_MSGFMT_OUT_DIR=\"${msgfmt_out_dir}\"")
  monde_apply_compile_options(po_msgfmt_gen)

  add_test(NAME po_msgfmt_gen COMMAND po_msgfmt_gen)
  set_tests_properties(po_msgfmt_gen PROPERTIES LABELS po FIXTURES_SETUP po_msgfmt)

  # msgfmt は警告では 0 を返し、エラーのときだけ 0 以外を返す（既定値のヘッダ項目の警告は許容する）
  foreach(generated IN ITEMS template.pot init_ja.po basic.po)
    add_test(NAME po_msgfmt_check.${generated}
      COMMAND "${MONDE_MSGFMT}" --check -o "${msgfmt_out_dir}/${generated}.mo"
        "${msgfmt_out_dir}/${generated}")
    set_tests_properties(po_msgfmt_check.${generated} PROPERTIES
      LABELS po FIXTURES_REQUIRED po_msgfmt)
  endforeach()
else()
  message(STATUS "po: msgfmt not found; the po_msgfmt tests are not registered")
endif()
