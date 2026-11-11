# CLOpts ディレクトリの単独性の検査。cmake -DROOT=<cli_args> -P isolation_check.cmake で起動する。
#
# ROOT の下のソース（.c、.h、.cpp）、ビルド定義（.cmake、.in、CMakeLists.txt）、README.md を走査し、
# ディレクトリの外への依存を示す記述があれば、ファイル名と行を報告して失敗する。
#   - Monde の名前の接頭辞（下の _isolation_names）
#   - Monde の共通部品のパス（foundation/）
#   - ソースでは ../ を含む #include、ビルド定義と README では ../ のすべて
# コメントの中の語も検出する。このスクリプト自身は検査する語を含むので、走査の対象から除く。
# ROOT の下に置かれたビルドディレクトリ（CMakeCache.txt のあるディレクトリ）も、生成物なので除く。
cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED ROOT OR ROOT STREQUAL "")
  message(FATAL_ERROR "isolation_check: ROOT is not set (use -DROOT=<cli_args directory>)")
endif()
get_filename_component(_isolation_root "${ROOT}" REALPATH)
if(NOT IS_DIRECTORY "${_isolation_root}")
  message(FATAL_ERROR "isolation_check: ROOT is not a directory: ${ROOT}")
endif()
get_filename_component(_isolation_self "${CMAKE_CURRENT_LIST_FILE}" REALPATH)

# 検出する語。部分一致で探す（大文字と小文字は区別する）
set(_isolation_names "mnd_" "MND_" "MONDE_" "monde_" "monde::" "foundation/")

file(GLOB_RECURSE _isolation_files LIST_DIRECTORIES false
  "${_isolation_root}/*.c"
  "${_isolation_root}/*.h"
  "${_isolation_root}/*.cpp"
  "${_isolation_root}/*.cmake"
  "${_isolation_root}/*.in"
  "${_isolation_root}/CMakeLists.txt"
  "${_isolation_root}/README.md"
)
list(REMOVE_DUPLICATES _isolation_files)
list(SORT _isolation_files)

# ROOT の下のビルドディレクトリを集める
file(GLOB_RECURSE _isolation_caches LIST_DIRECTORIES false "${_isolation_root}/CMakeCache.txt")
set(_isolation_build_dirs "")
foreach(_cache IN LISTS _isolation_caches)
  get_filename_component(_dir "${_cache}" DIRECTORY)
  list(APPEND _isolation_build_dirs "${_dir}/")
endforeach()

# CONTENT の位置 POS を含む行の行番号（1 始まり）と行の文字列を返す
function(_isolation_line_at CONTENT POS OUT_LINE OUT_TEXT)
  string(SUBSTRING "${CONTENT}" 0 ${POS} _before)
  string(REGEX MATCHALL "\n" _newlines "${_before}")
  list(LENGTH _newlines _count)
  math(EXPR _line "${_count} + 1")
  # 行の始まり: 直前の改行の次
  string(FIND "${_before}" "\n" _last_nl REVERSE)
  math(EXPR _start "${_last_nl} + 1")
  string(SUBSTRING "${CONTENT}" ${_start} -1 _rest)
  string(FIND "${_rest}" "\n" _end)
  string(SUBSTRING "${_rest}" 0 ${_end} _text)
  string(REGEX REPLACE "\r$" "" _text "${_text}")
  string(STRIP "${_text}" _text)
  set(${OUT_LINE} ${_line} PARENT_SCOPE)
  set(${OUT_TEXT} "${_text}" PARENT_SCOPE)
endfunction()

# CONTENT の中の NEEDLE の出現をすべて探し、行ごとに報告を _isolation_report に加え、
# _isolation_count を増やす（行の文字列は ; を含みうるので、リストではなく改行区切りの文字列にする）。
# INCLUDE_ONLY が真なら、#include の行での出現だけを数える
function(_isolation_scan CONTENT NEEDLE REL INCLUDE_ONLY)
  set(_report "${_isolation_report}")
  set(_count ${_isolation_count})
  set(_offset 0)
  set(_rest "${CONTENT}")
  set(_last_line 0)
  while(TRUE)
    string(FIND "${_rest}" "${NEEDLE}" _pos)
    if(_pos EQUAL -1)
      break()
    endif()
    math(EXPR _abs "${_offset} + ${_pos}")
    _isolation_line_at("${CONTENT}" ${_abs} _line _text)
    set(_hit TRUE)
    if(INCLUDE_ONLY AND NOT _text MATCHES "^#[ \t]*include")
      set(_hit FALSE)
    endif()
    # 同じ行に同じ語が複数あっても 1 回だけ報告する
    if(_hit AND NOT _line EQUAL _last_line)
      string(APPEND _report "${REL}:${_line}: '${NEEDLE}': ${_text}\n")
      math(EXPR _count "${_count} + 1")
      set(_last_line ${_line})
    endif()
    string(LENGTH "${NEEDLE}" _len)
    math(EXPR _next "${_pos} + ${_len}")
    string(SUBSTRING "${_rest}" ${_next} -1 _rest)
    math(EXPR _offset "${_offset} + ${_next}")
  endwhile()
  set(_isolation_report "${_report}" PARENT_SCOPE)
  set(_isolation_count ${_count} PARENT_SCOPE)
endfunction()

set(_isolation_report "")
set(_isolation_count 0)
set(_isolation_scanned 0)
foreach(_file IN LISTS _isolation_files)
  get_filename_component(_real "${_file}" REALPATH)
  if(_real STREQUAL _isolation_self)
    continue()
  endif()
  set(_in_build FALSE)
  foreach(_build_dir IN LISTS _isolation_build_dirs)
    string(FIND "${_file}" "${_build_dir}" _at)
    if(_at EQUAL 0)
      set(_in_build TRUE)
      break()
    endif()
  endforeach()
  if(_in_build)
    continue()
  endif()

  file(RELATIVE_PATH _rel "${_isolation_root}" "${_file}")
  file(READ "${_file}" _content)
  math(EXPR _isolation_scanned "${_isolation_scanned} + 1")

  foreach(_name IN LISTS _isolation_names)
    _isolation_scan("${_content}" "${_name}" "${_rel}" FALSE)
  endforeach()

  # ソースでは #include の中の ../ だけを、ビルド定義と README ではすべての ../ を検出する
  if(_file MATCHES "\\.(c|h|cpp)$")
    _isolation_scan("${_content}" "../" "${_rel}" TRUE)
  else()
    _isolation_scan("${_content}" "../" "${_rel}" FALSE)
  endif()
endforeach()

if(_isolation_scanned EQUAL 0)
  message(FATAL_ERROR "isolation_check: no files to scan under ${_isolation_root}")
endif()

if(_isolation_count GREATER 0)
  message("${_isolation_report}")
  message(FATAL_ERROR
    "isolation_check: ${_isolation_count} reference(s) outside CLOpts found under ${_isolation_root}")
endif()

message(STATUS "isolation_check: ${_isolation_scanned} file(s) scanned, no references outside CLOpts")
