# Requirements Document

## Project Description (Input)
**誰が困っているか**: Monde の開発者と、コマンドライン引数パーサを Monde 以外でも使いたい開発者。引数パーサ（cli-args）は Monde 固有の知識を持たない汎用部品だが、Monde のビルドと共通部品に組み込まれており、単独では扱えない。将来、別リポジトリに切り離して独立ライブラリ「CLOpts」として管理する予定だが、現状のままでは切り離し時に依存の解消、改名、ビルド定義の書き直しが必要になる。

**現状**:
- ソースは `src/cli_args/`、テストは `tests/cli_args/` にある。
- ビルドは Monde 専用の CMake 関数（`monde_add_module` など）に依存し、ターゲットは `monde_cli_args`（別名 `monde::cli_args`）。単独では構成できない。
- foundation（`diag`、`buf`、`wide`）に依存している。公開 API が `mnd_diag_list_t` を引数に取り、ヘルプ整形の公開 API が `mnd_buf_t` を受け取り、Windows の UTF-16 変換に `mnd_wide_to_utf8` を使う。
- 公開名は `mnd_args_*` / `MND_ARGS_*`、ヘッダは `cli_args/args.h` など。利用側は `src/cli/` と `src/monde/main.c`。

**何を変えるか**:
- リポジトリのルートに `cli_args/` を作り、ライブラリのソース、公開ヘッダ、テスト、CMake 定義、README、ライセンスなど、CLOpts に必要なファイルをすべてそこに格納する。`src/cli_args/` と `tests/cli_args/` は残さない。
- `cli_args/` だけで構成・ビルド・テスト（CTest、Naos）ができるようにする。`cli_args/` の中から、その外にあるファイルを参照しない。
- ライブラリ名を CLOpts とする。CMake のターゲットは `clopts`、別名は `CLOpts::clopts`。
- foundation への依存をなくす。CLOpts が自前の最小限のエラー報告型を持ち、文字列バッファと UTF-16 変換は内部に持つ。
- 公開 API を `clopts_*` / `CLOPTS_*` に改名し、公開ヘッダを `<clopts/...>` の形で取り込めるようにする。
- `install` と `find_package(CLOpts)` に対応し、インストール先からも `CLOpts::clopts` でリンクできるようにする。
- Monde 本体はルートの `add_subdirectory(cli_args)` で取り込み、`CLOpts::clopts` にリンクする。CLOpts のエラーは `src/cli/` で `mnd_diag_list_t` に橋渡しする。`monde` の外から見える動作（ヘルプ、エラーメッセージ、終了コード）は変えない。
- CI に `cli_args/` 単独での構成・ビルド・テストを追加し、steering と既存ドキュメントのパスの記述を更新する。

**範囲外**: 別リポジトリへの実際の切り離し、引数パーサの機能追加と挙動の変更、共有ライブラリ対応、パッケージマネージャーへの登録、CLOpts 単体のリリース運用。

詳細（選ばなかった案、境界、制約）は `brief.md` を参照。

## Requirements
<!-- Will be generated in /kiro-spec-requirements phase -->
