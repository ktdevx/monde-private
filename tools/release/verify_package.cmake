# ビルドした実行ファイルと配布アーカイブが配布物の要件を満たすことを確かめる（6.3、7.3、8.5〜8.7。cmake -P で実行する）。
#   cmake -DMONDE=<実行ファイル> -DVERSION=X.Y.Z [-DARCHIVE=<アーカイブ> -DNOTICES=<THIRD_PARTY_NOTICES>]
#         -DTARGET=<os>-<arch> [-DRUNNER=<前置きコマンドのリスト>] -P tools/release/verify_package.cmake
# ARCHIVE を指定するときは NOTICES（リポジトリの THIRD_PARTY_NOTICES のパス）も必須。
# RUNNER は実行ファイルを起動するときの前置きコマンド（macOS x64 では arch;-x86_64）。
# 確かめる項目:
#   1. [RUNNER...] MONDE --version の終了コードが 0 で、標準出力が monde X.Y.Z の 1 行と一致する（CRLF は LF とみなす）。
#   2. 動的依存。linux-* は ldd の出力に not a dynamic executable（glibc によっては statically linked）を含む。
#      macos-* は otool -L の依存が /usr/lib/libSystem.B.dylib だけ。windows-* は確認しない（静的 CRT は構成で保証する）。
#   3. ARCHIVE を指定したとき、ファイル名が monde-X.Y.Z-<TARGET>.zip または .tar.gz で、
#      一覧（cmake -E tar tf）が monde-X.Y.Z-<TARGET>/ の下の実行ファイル（windows-* は monde.exe、ほかは monde）、
#      LICENSE、README.md、THIRD_PARTY_NOTICES と、NOTICES の各素材の License-File（licenses/<ID>.txt）
#      だけである（ディレクトリのエントリは最上位ディレクトリ自身と licenses/ だけを許す）。
#      期待する一覧は NOTICES のコメントでない License-File: <パス> の行から組み立てる（1 つもなければ失敗）。
#   4. .tar.gz では cmake -E tar tvf の権限欄で実行ファイルに所有者の実行権限がある。
#   5. アーカイブの THIRD_PARTY_NOTICES が NOTICES とバイト単位で一致する（一時ディレクトリに展開して比べる）。
# ARCHIVE を省略すると 1 と 2 だけを確かめる（ワークフローは cpack の前にこの形で実行する）。
# 失敗はすべて集めて一覧にし、終了コード 1 で終わる（メッセージは標準エラー）。
# すべて通れば標準出力に package <ARCHIVE> verified（ARCHIVE 省略時は executable verified）の 1 行を出す。
cmake_minimum_required(VERSION 3.21)

set(usage "cmake -DMONDE=<exe> -DVERSION=X.Y.Z [-DARCHIVE=<path> -DNOTICES=<THIRD_PARTY_NOTICES>] -DTARGET=<os>-<arch> [-DRUNNER=<prefix>] -P verify_package.cmake")
foreach(required IN ITEMS MONDE VERSION TARGET)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "${required} is required: ${usage}")
  endif()
endforeach()
if(NOT "${VERSION}" MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR "VERSION ${VERSION} is not of the form X.Y.Z (for example 0.1.0)")
endif()
if("${TARGET}" MATCHES "^(windows|macos|linux)-[A-Za-z0-9_]+$")
  set(target_os "${CMAKE_MATCH_1}")
else()
  message(FATAL_ERROR "TARGET ${TARGET} is not of the form <os>-<arch> with os windows, macos or linux")
endif()
if(DEFINED ARCHIVE AND NOT ARCHIVE STREQUAL "" AND (NOT DEFINED NOTICES OR NOTICES STREQUAL ""))
  message(FATAL_ERROR "NOTICES is required when ARCHIVE is given (path to THIRD_PARTY_NOTICES): ${usage}")
endif()

set(failures "")
# 失敗の文面に ; があるとリストの要素が分かれるため、, に置き換えてから加える
macro(add_failure text)
  string(REPLACE ";" "," _add_failure_text "${text}")
  list(APPEND failures "${_add_failure_text}")
endmacro()

if(target_os STREQUAL "windows")
  set(exe_name "monde.exe")
else()
  set(exe_name "monde")
endif()

# 1. --version の出力
get_filename_component(monde_path "${MONDE}" ABSOLUTE)
if(NOT EXISTS "${monde_path}" OR IS_DIRECTORY "${monde_path}")
  add_failure("executable ${MONDE} not found")
else()
  set(expected_version_line "monde ${VERSION}\n")
  execute_process(
    COMMAND ${RUNNER} "${monde_path}" --version
    RESULT_VARIABLE version_rc
    OUTPUT_VARIABLE version_out
    ERROR_VARIABLE version_err
  )
  string(REPLACE "\r\n" "\n" version_out "${version_out}")
  if(NOT version_rc STREQUAL "0")
    add_failure("${MONDE} --version exited with ${version_rc} (stderr: ${version_err})")
  endif()
  if(NOT version_out STREQUAL expected_version_line)
    add_failure("${MONDE} --version printed [${version_out}], expected [monde ${VERSION}] followed by one newline")
  endif()

  # 2. 動的依存
  if(target_os STREQUAL "linux")
    find_program(ldd_program ldd)
    if(NOT ldd_program)
      add_failure("ldd not found; cannot check dynamic dependencies of ${MONDE}")
    else()
      # 静的な実行ファイルに対して ldd は 0 以外で終わり、標準エラーに出すことがあるため、両方の出力を見る
      execute_process(
        COMMAND "${ldd_program}" "${monde_path}"
        RESULT_VARIABLE ldd_rc
        OUTPUT_VARIABLE ldd_out
        ERROR_VARIABLE ldd_err
      )
      set(ldd_all "${ldd_out}${ldd_err}")
      if(NOT ldd_all MATCHES "not a dynamic executable" AND NOT ldd_all MATCHES "statically linked")
        string(STRIP "${ldd_all}" ldd_all)
        add_failure("${MONDE} is dynamically linked: ldd did not report 'not a dynamic executable' but printed:\n${ldd_all}")
      endif()
    endif()
  elseif(target_os STREQUAL "macos")
    find_program(otool_program otool)
    if(NOT otool_program)
      add_failure("otool not found; cannot check dynamic dependencies of ${MONDE}")
    else()
      execute_process(
        COMMAND "${otool_program}" -L "${monde_path}"
        RESULT_VARIABLE otool_rc
        OUTPUT_VARIABLE otool_out
        ERROR_VARIABLE otool_err
      )
      if(NOT otool_rc STREQUAL "0")
        add_failure("otool -L ${MONDE} exited with ${otool_rc} (stderr: ${otool_err})")
      else()
        # 字下げのない行はファイル名（ユニバーサルバイナリではアーキテクチャごと）の見出し。字下げした行が依存
        string(REPLACE ";" "\\;" otool_out "${otool_out}")
        string(REPLACE "\n" ";" otool_lines "${otool_out}")
        set(otool_dependency_count 0)
        foreach(line IN LISTS otool_lines)
          if(NOT line MATCHES "^[ \t]+([^ \t].*)$")
            continue()
          endif()
          set(dependency "${CMAKE_MATCH_1}")
          string(REGEX REPLACE " \\(compatibility version.*$" "" dependency "${dependency}")
          string(STRIP "${dependency}" dependency)
          math(EXPR otool_dependency_count "${otool_dependency_count} + 1")
          if(NOT dependency STREQUAL "/usr/lib/libSystem.B.dylib")
            add_failure("${MONDE} depends on ${dependency}; only /usr/lib/libSystem.B.dylib is allowed")
          endif()
        endforeach()
        if(otool_dependency_count EQUAL 0)
          add_failure("otool -L ${MONDE} listed no dependencies; expected /usr/lib/libSystem.B.dylib")
        endif()
      endif()
    endif()
  endif()
endif()

# 3〜5. アーカイブ
if(DEFINED ARCHIVE AND NOT ARCHIVE STREQUAL "")
  set(top "monde-${VERSION}-${TARGET}")

  # 索引から License-File のパスを集める（# で始まる行は無視し、CRLF は LF とみなす）
  get_filename_component(notices_path "${NOTICES}" ABSOLUTE)
  set(license_files "")
  set(notices_ok FALSE)
  if(NOT EXISTS "${notices_path}" OR IS_DIRECTORY "${notices_path}")
    add_failure("notices index ${NOTICES} not found")
  else()
    file(READ "${notices_path}" notices_text)
    string(REPLACE "\r\n" "\n" notices_text "${notices_text}")
    string(REPLACE ";" "\\;" notices_text "${notices_text}")
    string(REPLACE "\n" ";" notices_lines "${notices_text}")
    foreach(line IN LISTS notices_lines)
      if(line MATCHES "^#")
        continue()
      endif()
      if(line MATCHES "^License-File:[ \t]*([^ \t].*)$")
        string(STRIP "${CMAKE_MATCH_1}" license_file)
        if(license_file MATCHES "^/|^[A-Za-z]:|(^|/)\\.\\.(/|$)|\\\\")
          add_failure("${NOTICES}: License-File ${license_file} is not a relative path inside the archive")
        else()
          list(APPEND license_files "${license_file}")
        endif()
      endif()
    endforeach()
    list(REMOVE_DUPLICATES license_files)
    if(license_files STREQUAL "")
      add_failure("${NOTICES} lists no License-File (expected at least one licenses/<ID>.txt)")
    else()
      set(notices_ok TRUE)
    endif()
  endif()

  get_filename_component(archive_path "${ARCHIVE}" ABSOLUTE)
  get_filename_component(archive_name "${ARCHIVE}" NAME)
  if(archive_name MATCHES "\\.tar\\.gz$")
    set(archive_kind tgz)
  elseif(archive_name MATCHES "\\.zip$")
    set(archive_kind zip)
  else()
    set(archive_kind unknown)
  endif()
  if(NOT archive_name STREQUAL "${top}.zip" AND NOT archive_name STREQUAL "${top}.tar.gz")
    add_failure("archive name ${archive_name} is neither ${top}.zip nor ${top}.tar.gz")
  endif()

  if(NOT EXISTS "${archive_path}" OR IS_DIRECTORY "${archive_path}")
    add_failure("archive ${ARCHIVE} not found")
  elseif(archive_kind STREQUAL "unknown")
    add_failure("archive ${ARCHIVE} is neither .zip nor .tar.gz; cannot list its contents")
  else()
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E tar tf "${archive_path}"
      RESULT_VARIABLE list_rc
      OUTPUT_VARIABLE list_out
      ERROR_VARIABLE list_err
    )
    if(NOT list_rc STREQUAL "0")
      add_failure("cmake -E tar tf ${ARCHIVE} exited with ${list_rc} (stderr: ${list_err})")
    else()
      set(required_entries "${top}/${exe_name}" "${top}/LICENSE" "${top}/README.md" "${top}/THIRD_PARTY_NOTICES")
      # License-File の親ディレクトリ（licenses/ など）はディレクトリのエントリとして許す
      set(allowed_dirs "${top}")
      foreach(license_file IN LISTS license_files)
        list(APPEND required_entries "${top}/${license_file}")
        get_filename_component(license_dir "${license_file}" DIRECTORY)
        while(NOT license_dir STREQUAL "")
          list(APPEND allowed_dirs "${top}/${license_dir}")
          get_filename_component(license_dir "${license_dir}" DIRECTORY)
        endwhile()
      endforeach()
      set(found_entries "")
      string(REPLACE "\r\n" "\n" list_out "${list_out}")
      string(REPLACE ";" "\\;" list_out "${list_out}")
      string(REPLACE "\n" ";" list_lines "${list_out}")
      foreach(entry IN LISTS list_lines)
        if(entry STREQUAL "")
          continue()
        endif()
        string(REGEX REPLACE "^\\./" "" normalized "${entry}")
        string(REGEX REPLACE "/+$" "" normalized "${normalized}")
        list(FIND allowed_dirs "${normalized}" dir_index)
        if(NOT dir_index EQUAL -1)
          continue()
        endif()
        list(FIND required_entries "${normalized}" required_index)
        if(required_index EQUAL -1)
          add_failure("archive ${ARCHIVE} contains unexpected entry ${entry}")
        else()
          list(APPEND found_entries "${normalized}")
        endif()
      endforeach()
      foreach(required_entry IN LISTS required_entries)
        list(FIND found_entries "${required_entry}" found_index)
        if(found_index EQUAL -1)
          add_failure("archive ${ARCHIVE} does not contain ${required_entry}")
        endif()
      endforeach()

      # tar.gz では実行ファイルの実行権限が保たれていること（ZIP は Windows 向けのため確認しない）
      if(archive_kind STREQUAL "tgz")
        list(FIND found_entries "${top}/${exe_name}" exe_index)
        if(NOT exe_index EQUAL -1)
          execute_process(
            COMMAND "${CMAKE_COMMAND}" -E tar tvf "${archive_path}"
            RESULT_VARIABLE verbose_rc
            OUTPUT_VARIABLE verbose_out
            ERROR_VARIABLE verbose_err
          )
          if(NOT verbose_rc STREQUAL "0")
            add_failure("cmake -E tar tvf ${ARCHIVE} exited with ${verbose_rc} (stderr: ${verbose_err})")
          else()
            string(REPLACE "\r\n" "\n" verbose_out "${verbose_out}")
            string(REPLACE ";" "\\;" verbose_out "${verbose_out}")
            string(REPLACE "\n" ";" verbose_lines "${verbose_out}")
            string(REGEX REPLACE "([][.*+?^$()|\\\\])" "\\\\\\1" exe_entry_re "${top}/${exe_name}")
            set(exe_mode "")
            foreach(line IN LISTS verbose_lines)
              if(line MATCHES "^([-a-zA-Z]+)[ \t].*[ \t](\\./)?${exe_entry_re}$")
                set(exe_mode "${CMAKE_MATCH_1}")
                break()
              endif()
            endforeach()
            if(exe_mode STREQUAL "")
              add_failure("cannot find the permissions of ${top}/${exe_name} in cmake -E tar tvf ${ARCHIVE}")
            elseif(NOT exe_mode MATCHES "^-..[xs]")
              add_failure("${top}/${exe_name} in ${ARCHIVE} is not executable (mode ${exe_mode})")
            endif()
          endif()
        endif()
      endif()

      # 5. 同梱した THIRD_PARTY_NOTICES が NOTICES と一致すること（一時ディレクトリに展開して比べ、後で消す）
      list(FIND found_entries "${top}/THIRD_PARTY_NOTICES" notices_entry_index)
      if(notices_ok AND NOT notices_entry_index EQUAL -1)
        string(RANDOM LENGTH 12 extract_suffix)
        set(extract_dir "${CMAKE_CURRENT_BINARY_DIR}/verify_package_${extract_suffix}")
        file(REMOVE_RECURSE "${extract_dir}")
        file(MAKE_DIRECTORY "${extract_dir}")
        execute_process(
          COMMAND "${CMAKE_COMMAND}" -E tar xf "${archive_path}" -- "${top}/THIRD_PARTY_NOTICES"
          WORKING_DIRECTORY "${extract_dir}"
          RESULT_VARIABLE extract_rc
          OUTPUT_VARIABLE extract_out
          ERROR_VARIABLE extract_err
        )
        set(extracted_notices "${extract_dir}/${top}/THIRD_PARTY_NOTICES")
        if(NOT extract_rc STREQUAL "0" OR NOT EXISTS "${extracted_notices}")
          add_failure("cannot extract ${top}/THIRD_PARTY_NOTICES from ${ARCHIVE} (exit ${extract_rc}, stderr: ${extract_err})")
        else()
          file(SHA256 "${extracted_notices}" archived_hash)
          file(SHA256 "${notices_path}" expected_hash)
          if(NOT archived_hash STREQUAL expected_hash)
            add_failure("${top}/THIRD_PARTY_NOTICES in ${ARCHIVE} differs from ${NOTICES}")
          endif()
        endif()
        file(REMOVE_RECURSE "${extract_dir}")
      endif()
    endif()
  endif()
  set(success_line "package ${ARCHIVE} verified")
else()
  set(success_line "executable verified")
endif()

if(failures)
  list(LENGTH failures failure_count)
  set(report "verification failed (${failure_count}):")
  foreach(failure IN LISTS failures)
    string(APPEND report "\n  - ${failure}")
  endforeach()
  message(FATAL_ERROR "${report}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "${success_line}" RESULT_VARIABLE echo_rc)
if(NOT echo_rc EQUAL 0)
  message(FATAL_ERROR "failed to write the result to standard output")
endif()
