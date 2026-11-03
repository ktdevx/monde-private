set(MONDE_FOUNDATION_SOURCES
  buf.c
  diag.c
  utf8.c
  eol.c
  source.c
  file_io.c
)
# wide は Windows 専用。他の OS で加えると空の翻訳単位になり -Wpedantic で警告になるため、Windows でだけ加える
if(WIN32)
  list(APPEND MONDE_FOUNDATION_SOURCES wide.c)
endif()

monde_add_module(foundation
  SOURCES
    ${MONDE_FOUNDATION_SOURCES}
)
