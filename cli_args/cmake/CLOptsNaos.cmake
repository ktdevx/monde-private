include_guard(GLOBAL)

# CLOpts のテストのためにテストフレームワーク Naos を用意する（CLOPTS_BUILD_TESTS が ON のときだけ読み込む）。
# 取り込んだ側がすでに Naos を用意していれば（naos_add_executable が定義済みなら）、取得も定義もせずにそれを使う。
# v0.4.0 = コミット 09fde7172c4b63d416c32e98499cb0c0306d3dea
# オフライン環境では -DFETCHCONTENT_SOURCE_DIR_NAOS=<Naos のソースのパス> でローカルのソースを使える。
if(COMMAND naos_add_executable)
  message(STATUS "CLOpts: using the Naos already provided by the including project")
  return()
endif()

include(FetchContent)

set(CLOPTS_NAOS_TAG v0.4.0)

FetchContent_Declare(Naos
  GIT_REPOSITORY https://github.com/vabelorg/naos.git
  GIT_TAG ${CLOPTS_NAOS_TAG}
  GIT_SHALLOW TRUE
)
message(STATUS "CLOpts: fetching Naos ${CLOPTS_NAOS_TAG}")
FetchContent_MakeAvailable(Naos)
message(STATUS "CLOpts: Naos ${CLOPTS_NAOS_TAG} source: ${naos_SOURCE_DIR}")
