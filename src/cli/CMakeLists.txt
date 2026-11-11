monde_add_module(cli
  SOURCES
    cli_run.c
    cli_commands.c
    cli_io.c
    cli_report.c
    cli_time.c
    cli_language.c
    cmd_extract.c
    cmd_init.c
    cmd_update.c
    cmd_translate.c
  DEPENDS
    po_merge
    tu
    po
    foundation
)
# 引数パーサ CLOpts は monde_add_module の DEPENDS（monde::<module> 専用）ではなく、ここでリンクする
target_link_libraries(monde_cli PUBLIC CLOpts::clopts)
