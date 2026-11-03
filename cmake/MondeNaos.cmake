include_guard(GLOBAL)

# テストフレームワーク Naos を固定版で取得する（BUILD_TESTING が ON のときだけ読み込む）。
# バージョンの更新はこのファイルだけを変更して行う。
# v0.4.0 = コミット 09fde7172c4b63d416c32e98499cb0c0306d3dea
# オフライン環境では -DFETCHCONTENT_SOURCE_DIR_NAOS=<Naos のソースのパス> でローカルのソースを使える。
include(FetchContent)

set(MONDE_NAOS_TAG v0.4.0)

FetchContent_Declare(Naos
  GIT_REPOSITORY https://github.com/vabelorg/naos.git
  GIT_TAG ${MONDE_NAOS_TAG}
  GIT_SHALLOW TRUE
)
message(STATUS "Monde: fetching Naos ${MONDE_NAOS_TAG}")
FetchContent_MakeAvailable(Naos)
message(STATUS "Monde: Naos ${MONDE_NAOS_TAG} source: ${naos_SOURCE_DIR}")
