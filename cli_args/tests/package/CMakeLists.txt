# インストールした CLOpts を使う利用側プロジェクト。tests/package_test.cmake が構成・ビルド・実行する。
# find_package(CLOpts) で見つけた CLOpts::clopts だけにリンクし、include のパスや OS のライブラリは
# 自分では指定しない。CLOPTS_REQUIRED_VERSION を指定すると、そのバージョンを要求する
cmake_minimum_required(VERSION 3.21)
project(CLOptsPackageConsumer LANGUAGES C)

set(CLOPTS_REQUIRED_VERSION "" CACHE STRING "The CLOpts version to request from find_package (empty for any)")

find_package(CLOpts ${CLOPTS_REQUIRED_VERSION} CONFIG REQUIRED)

# 公開ヘッダを、警告を有効にしてもそのままコンパイルできることを確かめる
function(consumer_warnings TARGET)
  if(MSVC)
    target_compile_options(${TARGET} PRIVATE /W4 /WX)
  else()
    target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Werror)
  endif()
endfunction()

# 引数を解析してヘルプを生成するプログラム
add_executable(consumer consumer.c)
target_link_libraries(consumer PRIVATE CLOpts::clopts)
consumer_warnings(consumer)
set(_consumer_programs "$<TARGET_FILE:consumer>")

# C++ のコンパイラがあれば、公開ヘッダを C++ から取り込むプログラムもビルドする
include(CheckLanguage)
check_language(CXX)
if(CMAKE_CXX_COMPILER)
  enable_language(CXX)
  add_executable(consumer_cxx consumer_cxx.cpp)
  target_link_libraries(consumer_cxx PRIVATE CLOpts::clopts)
  consumer_warnings(consumer_cxx)
  string(APPEND _consumer_programs "\n$<TARGET_FILE:consumer_cxx>")
else()
  message(STATUS "No C++ compiler found; consumer_cxx is not built")
endif()

# 検証のスクリプトが実行するプログラムのパスを、構成ごとに書き出す
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/consumer_programs_$<CONFIG>.txt" CONTENT "${_consumer_programs}\n")
