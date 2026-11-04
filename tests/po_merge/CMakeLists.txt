monde_add_module_tests(po_merge
  SOURCES
    main.c
    similarity_test.c
    index_test.c
    entry_test.c
    fuzzy_test.c
    header_test.c
    merge_test.c
    golden_test.c
    test_util.c
  DEFINITIONS
    "MND_MERGE_TEST_DATA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/data\""
)

# 互換性テスト: 正解ファイルが msgmerge の出力そのものであることを確かめる（9.1、9.3。msgmerge のある環境でだけ登録する）
set(MONDE_PO_MERGE_GETTEXT_DIR "${CMAKE_CURRENT_BINARY_DIR}/gettext")
file(MAKE_DIRECTORY "${MONDE_PO_MERGE_GETTEXT_DIR}")

find_program(MONDE_MSGMERGE msgmerge)
set(msgmerge_version_ok FALSE)
if(MONDE_MSGMERGE)
  # 正解ファイルは msgmerge 0.21 の出力で、ほかのバージョンでは一致しないことがある（理由は data/README.md）
  execute_process(COMMAND "${MONDE_MSGMERGE}" --version
    OUTPUT_VARIABLE msgmerge_version_output ERROR_QUIET)
  string(REGEX MATCH "^[^\n]*" msgmerge_version_line "${msgmerge_version_output}")
  if(msgmerge_version_line MATCHES "\\(GNU gettext-tools\\) 0\\.21([^0-9.]|\\.[0-9]|$)")
    set(msgmerge_version_ok TRUE)
  else()
    message(STATUS "po_merge: msgmerge \"${msgmerge_version_line}\" is not 0.21 (the golden files are msgmerge 0.21 output); the po_merge_msgmerge_golden tests are not registered")
  endif()
endif()
if(msgmerge_version_ok)
  foreach(golden IN ITEMS exact fuzzy obsolete header no_header plural nofuzzy)
    set(msgmerge_args "")
    if(golden STREQUAL "nofuzzy")
      set(msgmerge_args "--no-fuzzy-matching")
    endif()
    add_test(NAME po_merge_msgmerge_golden.${golden}
      COMMAND ${CMAKE_COMMAND}
        "-DMSGMERGE=${MONDE_MSGMERGE}"
        "-DMSGMERGE_ARGS=${msgmerge_args}"
        "-DCASE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/data/${golden}"
        "-DOUTPUT=${MONDE_PO_MERGE_GETTEXT_DIR}/msgmerge_${golden}.po"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/msgmerge_compare.cmake")
    set_tests_properties(po_merge_msgmerge_golden.${golden} PROPERTIES LABELS po_merge)
  endforeach()
elseif(NOT MONDE_MSGMERGE)
  message(STATUS "po_merge: msgmerge not found; the po_merge_msgmerge_golden tests are not registered")
endif()
