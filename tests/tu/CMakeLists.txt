# spec.txt のラウンドトリップは tests/markdown/spec_reader.c で例を取り出し、MND_MD_SPEC_FILE のパスから読む
monde_add_module_tests(tu
  SOURCES
    main.c
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
