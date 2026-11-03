# ビルド構成の自己検証。プローブは通常のビルドではコンパイルせず、CTest から CMake のビルドコマンドで個別にビルドする。
foreach(probe IN ITEMS clean warning vla)
  add_library(probe_${probe} OBJECT EXCLUDE_FROM_ALL probe_${probe}.c)
  monde_apply_compile_options(probe_${probe})
endforeach()

add_test(NAME toolchain.probe_clean_builds
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target probe_clean)
add_test(NAME toolchain.probe_warning_fails
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target probe_warning)
add_test(NAME toolchain.probe_vla_fails
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target probe_vla)

set_tests_properties(
  toolchain.probe_clean_builds
  toolchain.probe_warning_fails
  toolchain.probe_vla_fails
  PROPERTIES RUN_SERIAL TRUE LABELS toolchain)
set_tests_properties(
  toolchain.probe_warning_fails
  toolchain.probe_vla_fails
  PROPERTIES WILL_FAIL TRUE)
