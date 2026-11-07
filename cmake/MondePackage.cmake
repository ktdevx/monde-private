include_guard(GLOBAL)

# 配布物の定義（install の規則と CPack の設定）。配布物の構成の変更はこのファイルだけで行う。
# アーカイブには実行ファイル monde、LICENSE、README.md の 3 つだけを入れる。
# テスト、spec.txt、ヘッダ、静的ライブラリは install の対象にしない。
install(TARGETS monde RUNTIME DESTINATION .)
install(FILES
  "${PROJECT_SOURCE_DIR}/LICENSE"
  "${PROJECT_SOURCE_DIR}/README.md"
  DESTINATION .
)

# 対象名 <os>-<arch> の既定を CMAKE_SYSTEM_NAME と CMAKE_SYSTEM_PROCESSOR から作る。
# CMAKE_OSX_ARCHITECTURES が 1 つだけ指定されていれば、そのアーキテクチャを優先する。
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(_monde_os macos)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(_monde_os windows)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(_monde_os linux)
else()
  string(TOLOWER "${CMAKE_SYSTEM_NAME}" _monde_os)
endif()

set(_monde_arch "${CMAKE_SYSTEM_PROCESSOR}")
list(LENGTH CMAKE_OSX_ARCHITECTURES _monde_osx_arch_count)
if(_monde_osx_arch_count EQUAL 1)
  set(_monde_arch "${CMAKE_OSX_ARCHITECTURES}")
endif()
string(TOLOWER "${_monde_arch}" _monde_arch)
if(_monde_arch STREQUAL "amd64" OR _monde_arch STREQUAL "x86_64")
  set(_monde_arch x64)
elseif(_monde_arch STREQUAL "arm64" OR _monde_arch STREQUAL "aarch64")
  set(_monde_arch arm64)
endif()

set(MONDE_RELEASE_TARGET "${_monde_os}-${_monde_arch}" CACHE STRING
  "配布物の対象名 <os>-<arch>（例: windows-x64、macos-arm64、macos-x64、linux-x64）")
unset(_monde_os)
unset(_monde_arch)
unset(_monde_osx_arch_count)

# アーカイブの名前は monde-<version>-<os>-<arch>。展開すると同名のディレクトリの下に 3 つのファイルが置かれる
set(CPACK_PACKAGE_NAME "monde")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "monde-${PROJECT_VERSION}-${MONDE_RELEASE_TARGET}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
# 生成器の既定は Windows で ZIP、それ以外で TGZ。cpack -G で上書きできる
if(WIN32)
  set(CPACK_GENERATOR "ZIP")
else()
  set(CPACK_GENERATOR "TGZ")
endif()
# ソースパッケージは作らない。CPACK_SOURCE_GENERATOR を空にするだけでは CPack が既定の生成器を補うため、
# 補われる生成器の選択（CPACK_SOURCE_<生成器>）もすべて OFF にする
set(CPACK_SOURCE_GENERATOR "")
foreach(_monde_source_generator IN ITEMS 7Z CYGWIN RPM TBZ2 TGZ TXZ TZ ZIP)
  set(CPACK_SOURCE_${_monde_source_generator} OFF)
endforeach()
unset(_monde_source_generator)

include(CPack)
