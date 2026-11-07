cmake_minimum_required(VERSION 3.21)

project(monde VERSION 0.1.0 LANGUAGES C)

# MSVC の CRT を静的ランタイムに揃える（Naos を含む全ターゲットに効くよう、Naos の取得より前に設定する）
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(MondeCompilerOptions)
include(MondeTargets)

# 単一構成ジェネレータで構成の種類が未指定なら Release とする（複数構成ジェネレータでは --config で選ぶ）
get_property(MONDE_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT MONDE_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(CTest)

add_subdirectory(src)

include(MondePackage)

if(BUILD_TESTING)
  include(MondeNaos)
  add_subdirectory(tests)
endif()
