monde_add_module(cli_args
  SOURCES
    args_decl.c
    args_result.c
    args_parse.c
    args_help.c
    args_argv.c
  DEPENDS
    foundation
)
# Windows では CommandLineToArgvW のために shell32 をリンクする
if(WIN32)
  target_link_libraries(monde_cli_args PUBLIC shell32)
endif()
