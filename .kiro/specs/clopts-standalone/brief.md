# Brief: clopts-standalone

## Problem

コマンドライン引数パーサ（cli-args）は Monde 固有の知識を持たない汎用部品だが、Monde のビルドと共通部品に組み込まれており、単独では扱えない。将来、別リポジトリに切り離して独立したライブラリ「CLOpts」として管理したいが、現状のままでは切り離しのたびに依存の解消、改名、ビルド定義の書き直しが必要になる。

## Current State

- ソースは `src/cli_args/`（6 ファイル、約 2,700 行）、テストは `tests/cli_args/`（14 ファイル、約 6,200 行）にある。
- ビルドは Monde 専用の CMake 関数（`monde_add_module`、`monde_add_module_tests`、`monde_apply_compile_options`）に依存し、ターゲットは `monde_cli_args`（別名 `monde::cli_args`）。単独では構成できない。
- foundation に依存している。
  - `foundation/diag.h`: 公開 API（`mnd_args_parse`、`mnd_args_argv_get`、`mnd_args_argv_from_wide`）が `mnd_diag_list_t *` を引数に取る。実装内で `mnd_diag_add` を 34 か所で使う。
  - `foundation/buf.h`: `mnd_buf_t` を内部の文字列組み立てと、ヘルプ整形の公開 API（`args_help.h`）で使う。
  - `foundation/wide.h`: Windows の UTF-16 → UTF-8 変換（`mnd_wide_to_utf8`）を 1 か所で使う。
- 公開名は `mnd_args_*` / `MND_ARGS_*`、ヘッダは `cli_args/args.h`、`cli_args/args_argv.h`、`cli_args/args_help.h`。
- 利用側は `src/cli/`（複数ファイル）と `src/monde/main.c`。約 20 種類のシンボルを使う。
- テストは Naos を使い、診断の検証に `mnd_diag_list_*`、ヘルプの検証に `mnd_buf_*` を使う。

## Desired Outcome

- リポジトリのルートに `cli_args/` があり、ライブラリのソース、公開ヘッダ、テスト、CMake 定義、ライセンス、README など、CLOpts に必要なファイルがすべてその中にある。`src/cli_args/` と `tests/cli_args/` は残らない。
- `cli_args/` だけを取り出しても、`cmake -S cli_args -B build` で構成、ビルド、テスト（CTest）ができる。`cli_args/` の外のファイル（`cmake/`、`src/foundation/` など）を一切参照しない。
- ライブラリ名は CLOpts。CMake のプロジェクト名は `CLOpts`、ターゲットは `clopts`、別名は `CLOpts::clopts`。
- 公開 API は `clopts_*` / `CLOPTS_*` に改名され、公開ヘッダは `<clopts/...>` の形で取り込める。
- foundation への依存がない。エラー報告、文字列バッファ、UTF-16 変換は CLOpts の中で完結する。
- `install` と `find_package(CLOpts)` に対応し、インストール先からも `CLOpts::clopts` でリンクできる。
- Monde 本体はルートの `add_subdirectory(cli_args)` で取り込み、`CLOpts::clopts` にリンクする。`monde` の動作（ヘルプ、エラーメッセージ、終了コード）は変わらず、既存の全テストが通る。
- 将来の切り離しは「`cli_args/` を別リポジトリに移し、Monde 側の取り込みを `FetchContent` か `find_package` に差し替える」だけで済む。

## Approach

**自己完結化 + 改名 + パッケージ化**（discovery で選択）。

- **依存の解消**: CLOpts が自前の最小限のエラー報告の型（メッセージの列を保持する型）を公開 API に持ち、文字列バッファと UTF-16 変換は内部実装として持つ。Monde 側は `src/cli/` で、CLOpts のエラーを `mnd_diag_list_t` に橋渡しする。ヘルプ整形の公開 API が `mnd_buf_t` を受け取っている点は、CLOpts 独自の出力先（自前のバッファ型、または確保した文字列を返す形）に置き換える。具体的な形は設計で決める。
- **改名**: `mnd_args_*` → `clopts_*`、`MND_ARGS_*` → `CLOPTS_*`、インクルードガードとヘッダの配置も CLOpts 用にする。Monde 側の利用箇所とテストを同時に追従させる。
- **ビルド**: `cli_args/CMakeLists.txt` を独立したプロジェクトとして書く。トップレベルで構成されたとき（`PROJECT_IS_TOP_LEVEL`）だけテストとインストールを既定で有効にし、サブプロジェクトとして取り込まれたときは親に影響を与えない。コンパイラオプションと Naos の取得は CLOpts 自身が持つ。
- **パッケージ**: `install(TARGETS ... EXPORT)` と `CLOptsConfig.cmake` / `CLOptsConfigVersion.cmake` を用意する。

**選ばなかった案**:

- foundation の一部（diag / buf / wide）を `cli_args/` に同梱する: Monde と同時にリンクするとシンボルが衝突するため結局は改名が必要で、コードが二重管理になる。
- 診断をコールバックで受け渡す: 依存は消えるが、API の形が大きく変わり、テストの書き換えが増える。
- 改名を切り離し時まで先送りする: ライブラリ名と API 名が食い違う状態が残り、切り離し時に破壊的変更が発生する。
- `add_subdirectory` / `FetchContent` だけに対応する: 範囲は小さいが、切り離し後の利用形態（インストールして `find_package`）を今回確認できない。

## Scope

- **In**:
  - `src/cli_args/` と `tests/cli_args/` をルートの `cli_args/` に移す（履歴を保つため `git mv` を使う）
  - foundation への依存の除去（エラー報告の型、内部の文字列バッファ、UTF-16 変換）
  - 公開 API、マクロ、ヘッダの CLOpts 用の改名
  - 独立した CMake プロジェクト（ターゲット `clopts`、別名 `CLOpts::clopts`、コンパイラオプション、Windows の `shell32` のリンク、Naos によるテスト）
  - `install` と `find_package(CLOpts)` の対応、およびインストール先から利用できることの検証
  - Monde 本体の追従（ルートと `src/`・`tests/` の CMake、`src/cli/`、`src/monde/main.c`、エラーの橋渡し）
  - CI に「`cli_args/` 単独での構成・ビルド・テスト」を追加する
  - `cli_args/` に置く README とライセンス表記
  - steering（`structure.md`、必要なら `tech.md`）と既存ドキュメントのパスの記述の更新
- **Out**:
  - 別リポジトリの作成と、そこへの実際の切り離し（履歴の抽出、Monde 側の `FetchContent` 化）
  - 引数パーサの機能追加と挙動の変更（解析規則、ヘルプの書式、エラーメッセージの文言は変えない）
  - 共有ライブラリ（DLL / .so）としてのビルド対応、シンボルの可視性の制御
  - パッケージマネージャー（vcpkg、Conan など）への登録
  - CLOpts 単体のリリースワークフローとバージョン管理の運用
  - Monde のリリース成果物への CLOpts のヘッダやライブラリの同梱

## Boundary Candidates

- **CLOpts のライブラリ本体**: 解析、宣言の検証、結果の参照、ヘルプ整形、argv の取得。foundation に頼らないエラー報告と内部ユーティリティを含む。
- **CLOpts のビルドとパッケージ**: 独立した CMake プロジェクト、トップレベルとサブプロジェクトの切り替え、install / export / Config ファイル。
- **CLOpts のテスト**: Naos の取得と、移設・改名後のテスト一式。インストール先からの利用の検証。
- **Monde 側の取り込み**: `add_subdirectory(cli_args)`、`CLOpts::clopts` へのリンク、CLOpts のエラーから `mnd_diag` への橋渡し、利用箇所の改名。
- **CI とドキュメント**: 単独ビルドのジョブ、README、steering の更新。

## Out of Boundary

- foundation の共通部品そのものの変更（CLOpts が使わなくなるだけで、foundation の API は変えない）
- `monde` のサブコマンドとオプションの宣言内容（monde-cli の責務）
- Naos のバージョンの変更（Monde 本体と同じ固定版を使う。更新は別の作業とする）
- cli-args spec が定めた要件の変更（挙動の基準はそのまま引き継ぐ）

## Upstream / Downstream

- **Upstream**:
  - cli-args（実装済み）: 移設と改名の対象。要件と設計が挙動の基準になる。
  - project-foundation: 現在の依存先。CMake の共通関数とコンパイラオプションの方針を参考にするが、参照はしない。
- **Downstream**:
  - monde-cli: `CLOpts::clopts` の利用側。改名と橋渡しの追従が必要。
  - docs-and-release: CI ワークフローとリリース構成。Monde の配布物に影響を与えないことを確認する。
  - 将来の作業: 別リポジトリへの切り離しと、Monde 側の `FetchContent` / `find_package` 化。

## Existing Spec Touchpoints

- **Extends**: なし（新しい spec）。ただし cli-args の `design.md` に書かれたパス、ターゲット名、API 名は本 spec の完了後に古くなるため、本 spec の設計で対応表を示す。
- **Adjacent**:
  - cli-args: 挙動の要件は cli-args が持ち続ける。本 spec は配置、名前、依存、ビルド、配布だけを扱う。
  - project-foundation: `cmake/MondeTargets.cmake` の `monde_add_module` 系は、CLOpts では使わない。
  - monde-cli: `src/cli/` の変更は、改名とエラーの橋渡しに限る。

## Constraints

- C99。MSVC で使えない機能は使わない。Monde 本体と同じ警告水準（GCC / Clang: `-Wall -Wextra -Wpedantic -Werror -Werror=vla`、MSVC: `/W4 /WX /utf-8 /Zc:preprocessor`）でビルドできること。ただし、サブプロジェクトとして取り込まれたときに、親プロジェクトへ警告オプションを伝播させない。
- CMake 3.21 以上（`PROJECT_IS_TOP_LEVEL` が使える）。
- 対応環境は Monde と同じ（Windows の MSVC と MinGW-w64、macOS、Linux）。Windows では `shell32` へのリンクを利用側に伝える（インストール後の `find_package` でも同様）。
- 外部の実行時ライブラリに依存しない。テストだけが Naos（固定版）に依存する。
- Monde と一緒に構成したときに Naos を二重に取得・定義しない。`-DFETCHCONTENT_SOURCE_DIR_NAOS=...` によるオフライン構成を、単独ビルドでも使えるようにする。
- `cli_args/` の中から、その外にあるファイルを参照しない（切り離しの前提）。
- `monde` の外から見える動作（ヘルプ、エラーメッセージ、終了コード）を変えない。既存の全テストと CommonMark のラウンドトリップ受け入れテストが通ること。
- ライセンスは Monde と同じ（`LICENSE` を `cli_args/` にも置く）。
- ドキュメントは日本語、ライブラリが出すメッセージは英語。
