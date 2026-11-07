# リリースのタグとビルド構成のバージョンの一致を確かめる（6.3、7.8。cmake -P で実行する）。
#   cmake -DTAG=<タグ> -DSOURCE_DIR=<リポジトリのルート> -P tools/release/check_version.cmake
# TAG は vX.Y.Z（X、Y、Z は数字の並び）の形式でなければ失敗する。
# SOURCE_DIR/CMakeLists.txt から project(monde VERSION A.B.C ...) の番号を読む。
# project の呼び出しは大文字小文字を問わず、括弧の内側と引数の間の空白と改行を許す。
# 該当する記述がちょうど 1 つでなければ失敗する。
# TAG から v を除いた番号と A.B.C が異なれば
#   tag vX.Y.Z does not match project VERSION A.B.C
# を出して失敗する（終了コード 1、メッセージは標準エラー）。
# 一致すれば標準出力に version X.Y.Z の 1 行だけを出して終了コード 0 で終わる。
# リリースワークフローがこの行を読んで後続のジョブにバージョンを渡すため、
# 成功時の行は message() ではなく cmake -E echo で標準出力に出す。
cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED TAG OR TAG STREQUAL "")
  message(FATAL_ERROR "TAG is required: cmake -DTAG=vX.Y.Z -DSOURCE_DIR=<repo root> -P check_version.cmake")
endif()
if(NOT DEFINED SOURCE_DIR OR SOURCE_DIR STREQUAL "")
  message(FATAL_ERROR "SOURCE_DIR is required: cmake -DTAG=vX.Y.Z -DSOURCE_DIR=<repo root> -P check_version.cmake")
endif()

if(NOT TAG MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)$")
  message(FATAL_ERROR "tag ${TAG} is not of the form vX.Y.Z (for example v0.1.0)")
endif()
set(tag_version "${CMAKE_MATCH_1}")

set(cmakelists "${SOURCE_DIR}/CMakeLists.txt")
if(NOT EXISTS "${cmakelists}" OR IS_DIRECTORY "${cmakelists}")
  message(FATAL_ERROR "${cmakelists} not found")
endif()
file(READ "${cmakelists}" content)

set(ws "[ \t\r\n]")
set(project_re "[Pp][Rr][Oo][Jj][Ee][Cc][Tt]${ws}*\\(${ws}*monde${ws}+VERSION${ws}+([0-9][0-9.]*)")
string(REGEX MATCHALL "${project_re}" matches "${content}")
list(LENGTH matches match_count)
if(match_count EQUAL 0)
  message(FATAL_ERROR "no project(monde VERSION ...) found in ${cmakelists}")
elseif(NOT match_count EQUAL 1)
  message(FATAL_ERROR "found ${match_count} project(monde VERSION ...) in ${cmakelists}; expected exactly 1")
endif()
string(REGEX MATCH "${project_re}" _ "${matches}")
set(project_version "${CMAKE_MATCH_1}")

if(NOT tag_version STREQUAL project_version)
  message(FATAL_ERROR "tag ${TAG} does not match project VERSION ${project_version}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "version ${project_version}" RESULT_VARIABLE echo_rc)
if(NOT echo_rc EQUAL 0)
  message(FATAL_ERROR "failed to write the version to standard output")
endif()
