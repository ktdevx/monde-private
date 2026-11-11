# 実行ファイル monde。--version のバージョンは project(monde VERSION ...) から 1 か所で決めて渡す
monde_add_executable(monde
  SOURCES
    main.c
  LIBRARIES
    monde::cli
    CLOpts::clopts
)
target_compile_definitions(monde PRIVATE "MONDE_VERSION=\"${PROJECT_VERSION}\"")
