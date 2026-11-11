# インストール後の利用の検証。CTest のテスト clopts_package.find_package から cmake -P で起動する。
#
# 入力（-D で渡す）:
#   BUILD_DIR           CLOpts のビルドディレクトリ（インストールの元。変更しない）
#   CONFIG              構成名（$<CONFIG>）。インストールと利用側のビルドの両方に使う。空なら指定しない
#   MULTI_CONFIG        外側のジェネレータが複数構成なら真
#   SOURCE_DIR          利用側プロジェクト（tests/package）
#   WORK_DIR            作業用のディレクトリ。毎回作り直す
#   GENERATOR           外側と同じジェネレータ（GENERATOR_PLATFORM、GENERATOR_TOOLSET、MAKE_PROGRAM も同様）
#   TOOLCHAIN_FILE      外側と同じツールチェーンファイル（空なら指定しない）
#   C_COMPILER          外側と同じ C のコンパイラ。CXX_COMPILER は外側で指定されていれば渡す
#   VERSION             CLOpts のバージョン番号
#   INCLUDEDIR、LIBDIR、DATADIR  インストール先の相対ディレクトリ（GNUInstallDirs の値）
#   LIBRARY_FILE        静的ライブラリのファイル名
#
# 手順:
#   1. WORK_DIR を空にする
#   2. CLOpts を WORK_DIR/prefix にインストールする
#   3. インストールしたファイルが、設計の配置のファイルとちょうど一致することを確かめる
#   4. WORK_DIR/prefix を WORK_DIR/moved に移す
#   5. 利用側プロジェクトを CMAKE_PREFIX_PATH=WORK_DIR/moved で構成・ビルドし、consumer（と、C++ の
#      コンパイラがあれば consumer_cxx）を実行する
#   6. 合わないバージョン（99.0）を要求した構成が失敗することを確かめる
# 失敗したら、失敗した手順とコマンドの出力を FATAL_ERROR で報告する。
cmake_minimum_required(VERSION 3.21)

foreach(_var IN ITEMS BUILD_DIR SOURCE_DIR WORK_DIR GENERATOR C_COMPILER VERSION INCLUDEDIR LIBDIR DATADIR
                      LIBRARY_FILE)
  if(NOT DEFINED ${_var} OR "${${_var}}" STREQUAL "")
    message(FATAL_ERROR "package_test: ${_var} is not set")
  endif()
endforeach()
if(NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "package_test: consumer project not found: ${SOURCE_DIR}/CMakeLists.txt")
endif()

set(_prefix "${WORK_DIR}/prefix")
set(_moved "${WORK_DIR}/moved")
set(_consumer_build "${WORK_DIR}/consumer-build")
set(_mismatch_build "${WORK_DIR}/consumer-mismatch")

# STEP のコマンドを実行し、終了コードと出力を OUT_RESULT と OUT_OUTPUT に返す
function(_package_run OUT_RESULT OUT_OUTPUT)
  execute_process(COMMAND ${ARGN}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _output
    ERROR_VARIABLE _output
  )
  set(${OUT_RESULT} "${_result}" PARENT_SCOPE)
  set(${OUT_OUTPUT} "${_output}" PARENT_SCOPE)
endfunction()

# コマンドを実行し、失敗したら手順の名前とコマンドの出力を報告して止める
function(_package_step STEP)
  _package_run(_result _output ${ARGN})
  if(NOT _result EQUAL 0)
    string(REPLACE ";" " " _command "${ARGN}")
    message(FATAL_ERROR
      "package_test: ${STEP} failed (${_result})\n  command: ${_command}\n${_output}")
  endif()
  message(STATUS "package_test: ${STEP}: ok")
endfunction()

# 利用側プロジェクトを構成するコマンドの共通部分（外側と同じジェネレータとコンパイラを使う）
set(_configure_args -G "${GENERATOR}")
if(NOT "${GENERATOR_PLATFORM}" STREQUAL "")
  list(APPEND _configure_args -A "${GENERATOR_PLATFORM}")
endif()
if(NOT "${GENERATOR_TOOLSET}" STREQUAL "")
  list(APPEND _configure_args -T "${GENERATOR_TOOLSET}")
endif()
if(NOT "${MAKE_PROGRAM}" STREQUAL "")
  list(APPEND _configure_args "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
endif()
if(NOT "${TOOLCHAIN_FILE}" STREQUAL "")
  list(APPEND _configure_args "-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}")
endif()
list(APPEND _configure_args "-DCMAKE_C_COMPILER=${C_COMPILER}")
if(NOT "${CXX_COMPILER}" STREQUAL "")
  list(APPEND _configure_args "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}")
endif()
if(NOT "${CONFIG}" STREQUAL "")
  if(MULTI_CONFIG)
    list(APPEND _configure_args "-DCMAKE_CONFIGURATION_TYPES=${CONFIG}")
  else()
    list(APPEND _configure_args "-DCMAKE_BUILD_TYPE=${CONFIG}")
  endif()
endif()
list(APPEND _configure_args "-DCMAKE_PREFIX_PATH=${_moved}")

set(_config_args "")
if(NOT "${CONFIG}" STREQUAL "")
  set(_config_args --config "${CONFIG}")
endif()

# 1. 作業用のディレクトリを作り直す
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

# 2. インストール
_package_step("install" "${CMAKE_COMMAND}" --install "${BUILD_DIR}" ${_config_args} --prefix "${_prefix}")

# 3. インストールしたファイルの検査。設計の配置のファイルがすべてあり、それ以外のファイルがないこと
#    （内部ヘッダ、Naos、テストの実行ファイルなどを置かない）
if("${CONFIG}" STREQUAL "")
  set(_targets_config "noconfig")
else()
  string(TOLOWER "${CONFIG}" _targets_config)
endif()
set(_expected
  "${INCLUDEDIR}/clopts/argv.h"
  "${INCLUDEDIR}/clopts/clopts.h"
  "${INCLUDEDIR}/clopts/errors.h"
  "${INCLUDEDIR}/clopts/help.h"
  "${LIBDIR}/${LIBRARY_FILE}"
  "${LIBDIR}/cmake/CLOpts/CLOptsConfig.cmake"
  "${LIBDIR}/cmake/CLOpts/CLOptsConfigVersion.cmake"
  "${LIBDIR}/cmake/CLOpts/CLOptsTargets.cmake"
  "${LIBDIR}/cmake/CLOpts/CLOptsTargets-${_targets_config}.cmake"
  "${DATADIR}/licenses/CLOpts/LICENSE"
)
list(SORT _expected)
file(GLOB_RECURSE _installed LIST_DIRECTORIES false RELATIVE "${_prefix}" "${_prefix}/*")
list(SORT _installed)

set(_missing "${_expected}")
list(REMOVE_ITEM _missing ${_installed})
set(_unexpected "${_installed}")
list(REMOVE_ITEM _unexpected ${_expected})
if(_missing OR _unexpected)
  string(REPLACE ";" "\n    " _missing_text "${_missing}")
  string(REPLACE ";" "\n    " _unexpected_text "${_unexpected}")
  string(REPLACE ";" "\n    " _installed_text "${_installed}")
  message(FATAL_ERROR "package_test: installed files do not match the expected layout\n"
    "  missing:\n    ${_missing_text}\n"
    "  unexpected:\n    ${_unexpected_text}\n"
    "  installed:\n    ${_installed_text}")
endif()
# 上の一致の検査で十分だが、要件の対象（内部ヘッダ、Naos、テスト）を名前でも確かめる
foreach(_file IN LISTS _installed)
  if(_file MATCHES "clopts_internal\\.h|clopts_buf\\.h|[Nn]aos|clopts_tests")
    message(FATAL_ERROR "package_test: a file that must not be installed was installed: ${_file}")
  endif()
endforeach()
list(LENGTH _installed _installed_count)
message(STATUS "package_test: installed files: ok (${_installed_count} files)")

# 4. インストール先を別の場所に移す（元の場所が残っていないことも確かめる）
file(RENAME "${_prefix}" "${_moved}")
if(EXISTS "${_prefix}")
  message(FATAL_ERROR "package_test: ${_prefix} still exists after moving it")
endif()

# 5. 移した先を探索の対象にして、利用側プロジェクトを構成・ビルド・実行する。
#    一致するバージョンを要求して、バージョンの照合が通ることも確かめる
_package_step("configure consumer"
  "${CMAKE_COMMAND}" -S "${SOURCE_DIR}" -B "${_consumer_build}" ${_configure_args}
  "-DCLOPTS_REQUIRED_VERSION=${VERSION}")
_package_step("build consumer" "${CMAKE_COMMAND}" --build "${_consumer_build}" ${_config_args})

# 利用側プロジェクトは、ビルドした実行ファイルのパスを consumer_programs_<構成名>.txt に書き出す
set(_programs_file "${_consumer_build}/consumer_programs_${CONFIG}.txt")
if(NOT EXISTS "${_programs_file}")
  message(FATAL_ERROR "package_test: ${_programs_file} was not generated")
endif()
file(STRINGS "${_programs_file}" _programs)
set(_ran_c FALSE)
foreach(_program IN LISTS _programs)
  get_filename_component(_name "${_program}" NAME_WE)
  if(_name STREQUAL "consumer")
    set(_ran_c TRUE)
  endif()
  _package_run(_result _output "${_program}" --name world --verbose input.txt)
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR "package_test: run ${_name} failed (${_result})\n  program: ${_program}\n${_output}")
  endif()
  message(STATUS "package_test: run ${_name}: ok")
endforeach()
if(NOT _ran_c)
  message(FATAL_ERROR "package_test: the consumer program was not built:\n${_programs}")
endif()

# 6. 合わないバージョンを要求した構成は失敗する
_package_run(_result _output
  "${CMAKE_COMMAND}" -S "${SOURCE_DIR}" -B "${_mismatch_build}" ${_configure_args}
  "-DCLOPTS_REQUIRED_VERSION=99.0")
if(_result EQUAL 0)
  message(FATAL_ERROR "package_test: configuring with CLOPTS_REQUIRED_VERSION=99.0 succeeded"
    " although CLOpts ${VERSION} is installed\n${_output}")
endif()
if(NOT _output MATCHES "\"CLOpts\".*compatible.*\"99\\.0\"")
  message(FATAL_ERROR "package_test: configuring with CLOPTS_REQUIRED_VERSION=99.0 failed"
    " for an unexpected reason\n${_output}")
endif()
message(STATUS "package_test: version mismatch is rejected: ok")
