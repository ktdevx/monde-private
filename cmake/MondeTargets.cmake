include_guard(GLOBAL)

include(MondeCompilerOptions)

# モジュールの静的ライブラリ monde_<module> と別名 monde::<module> を作る。
# include の基点 ${PROJECT_SOURCE_DIR}/src を PUBLIC で公開し、DEPENDS のモジュールに PUBLIC でリンクする。
#
#   monde_add_module(<module> SOURCES <source>... [DEPENDS <module>...])
function(monde_add_module module)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;DEPENDS")
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "monde_add_module(${module}): unknown arguments: ${arg_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT arg_SOURCES)
    message(FATAL_ERROR "monde_add_module(${module}): SOURCES is required")
  endif()

  set(target monde_${module})
  add_library(${target} STATIC ${arg_SOURCES})
  add_library(monde::${module} ALIAS ${target})
  target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}/src")
  foreach(dep IN LISTS arg_DEPENDS)
    target_link_libraries(${target} PUBLIC monde::${dep})
  endforeach()
  monde_apply_compile_options(${target})
endfunction()

# モジュールのテスト実行ファイル <module>_tests を作る（tests/<module>/ から、BUILD_TESTING が ON のときだけ呼ぶ）。
# naos_add_executable で monde::<module> にリンクし、各テストを CTest に
# <module>_tests.<suite>.<test> の名前とラベル <module> で登録する。
# DEFINITIONS はテスト実行ファイルのコンパイル定義、PROPERTIES は各 CTest テストのプロパティになる。
#
#   monde_add_module_tests(<module> SOURCES <source>...
#       [DEFINITIONS <name=value>...] [PROPERTIES <name> <value>...])
function(monde_add_module_tests module)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;DEFINITIONS;PROPERTIES")
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "monde_add_module_tests(${module}): unknown arguments: ${arg_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT arg_SOURCES)
    message(FATAL_ERROR "monde_add_module_tests(${module}): SOURCES is required")
  endif()
  if(NOT COMMAND naos_add_executable)
    message(FATAL_ERROR "monde_add_module_tests(${module}): Naos is not available (requires BUILD_TESTING=ON)")
  endif()

  set(target ${module}_tests)
  naos_add_executable(${target}
    SOURCES ${arg_SOURCES}
    LIBRARIES monde::${module}
    PREFIX "${target}."
    LABELS ${module}
    PROPERTIES ${arg_PROPERTIES}
  )
  if(arg_DEFINITIONS)
    target_compile_definitions(${target} PRIVATE ${arg_DEFINITIONS})
  endif()
  monde_apply_compile_options(${target})
endfunction()

# 製品の実行ファイルを作る。MSVC では静的 CRT、MinGW-w64 では静的リンクとする。
# テストフレームワーク Naos を LIBRARIES に含めると構成時エラーにする。
#
#   monde_add_executable(<target> SOURCES <source>... [LIBRARIES <library>...])
function(monde_add_executable target)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LIBRARIES")
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "monde_add_executable(${target}): unknown arguments: ${arg_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT arg_SOURCES)
    message(FATAL_ERROR "monde_add_executable(${target}): SOURCES is required")
  endif()
  foreach(lib IN LISTS arg_LIBRARIES)
    set(resolved "${lib}")
    if(TARGET "${lib}")
      get_target_property(aliased "${lib}" ALIASED_TARGET)
      if(aliased)
        set(resolved "${aliased}")
      endif()
    endif()
    if(lib STREQUAL "Naos::naos" OR resolved STREQUAL "naos")
      message(FATAL_ERROR "monde_add_executable(${target}): the test framework Naos (${lib}) must not be linked into a product executable")
    endif()
  endforeach()

  add_executable(${target} ${arg_SOURCES})
  target_link_libraries(${target} PRIVATE ${arg_LIBRARIES})
  set_target_properties(${target} PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  )
  if(MINGW)
    target_link_options(${target} PRIVATE -static)
  endif()
  monde_apply_compile_options(${target})
endfunction()
