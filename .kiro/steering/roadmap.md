# Roadmap

## Overview

Monde は、Markdown 文書を gettext の `.pot` / `.po` を介して翻訳するフローを支援する C99 製の CLI ツール（`monde`）である。原文 Markdown から翻訳テンプレートを抽出し（`extract`）、言語ごとの翻訳ファイルを作成し（`init`）、原文更新時に既存翻訳をマージし（`update`）、原文と翻訳ファイルから翻訳済み Markdown を生成する（`translate`）。翻訳作業そのものは外部の PO 対応ツールに任せる。

初版（MVP）は CommonMark + YAML フロントマター + 4 サブコマンドとする。中核となる技術方式は「ソース範囲（バイトオフセット）を保持するパーサで翻訳単位を特定し、翻訳時は原文バッファの該当範囲だけを置換する」ことで、翻訳対象以外の書式をバイト単位で保持する。

入力資料: `docs/inputs/inputs.md`（RFP）、`docs/inputs/qa.md`（要件すり合わせ Q&A）

## Approach Decision

- **Chosen**: 部品別分割（再利用可能な部品ごとに spec を分け、最後に CLI で統合する）。技術方式はソース範囲保持パーサ + 原文バッファへの範囲置換。
- **Why**: RFP §6 の「部品ごとに再利用できる単位で設計・実装する」方針と Q8-2（内部モジュールとして再利用しやすい設計）に合致する。パーサごとに境界とテストが明確になり、`/kiro-spec-batch` で並行して spec を作成できる。
- **Rejected alternatives**:
  - 構文木から Markdown を再生成する方式: 記号や空行が正規化され、Q4-1（翻訳対象以外はバイト単位で保持）を満たせない。
  - 行ベースの簡易処理: CommonMark spec.txt のラウンドトリップ（Q9-1）を満たせない。
  - 縦スライス分割（段落・見出しで 4 コマンドの骨格を先に作る）: 早期に動作確認できるが、パーサの責務が各 spec に分散し、部品として再利用しにくい。
  - 粗い 3 分割（基盤／パーサ群／アプリ）: パーサ群の spec が肥大化し、20 タスクを超える。

## Scope

- **In**:
  - CommonMark 準拠 Markdown の解析（方言を差し替えられる設計）
  - YAML フロントマターのうち、オプションで指定したキーの翻訳
  - `.pot` / `.po` の読み書き、msgmerge 相当のマージ
  - `monde extract` / `init` / `update` / `translate` の 4 サブコマンド（1 ファイル単位、コマンドライン引数のみ、英語のメッセージ）
  - 翻訳率の表示、fuzzy の翻訳を採用するオプション
  - CMake によるビルド、Naos によるテスト、GitHub Actions による CI、GitHub Releases でのバイナリ配布
  - 日本語のドキュメント（README、利用マニュアル）
- **Out**:
  - 翻訳作業そのもの（`.po` の編集）
  - GFM 対応（表、タスクリスト、取り消し線、自動リンク）: 次の版で対応する。脚注はさらに将来対応とする
  - 複数ファイル・ディレクトリの一括処理、設定ファイル
  - 翻訳率による出力の制御
  - 複数形エントリの本格対応（読み込み時にエラーにしない程度のみ対応する）
  - 同一文を分けて翻訳するオプション（将来対応の可能性あり）
  - ライブラリとしての公開 API、パッケージマネージャーへの登録

## Constraints

- C99。MSVC で使えない機能（VLA、`_Complex`）は使わない。GCC / Clang では `-Werror=vla` を指定する。MSVC では `/std:c11`、`/utf-8`、`/Zc:preprocessor`、`_CRT_SECURE_NO_WARNINGS` を指定する。
- CMake 3.21 以上（Naos の要件）。
- 対応環境: Windows（MSVC、MinGW-w64）、macOS（Apple Clang）、Linux（GCC、Clang）。GitHub Actions で各環境のビルドとテストを行う。
- 外部の実行時ライブラリには依存しない。Markdown、YAML、PO、コマンドライン引数のパーサはすべて自前で実装する。
- テストフレームワークは Naos。pre-1.0 のため、タグまたはコミットを固定して取り込む。テストは CTest 経由で実行する。
- 入出力は UTF-8 のみ（BOM 付きも読み込める）。改行コードは入力に合わせる。ファイルは常にバイナリモードで開く。Windows で非 ASCII のパスを扱えるように、UTF-8 から UTF-16 に変換して `_wfopen` などを使う。
- 受け入れ基準: CommonMark spec（0.31.2 に固定）の全テストケースで、「extract → translate（翻訳なし）」の結果が原文と一致すること。spec.txt は CC-BY-SA 4.0 のため、無改変のまま独立したディレクトリに置いてライセンス表記を添えるか、テスト時に取得する。リリース成果物には含めない。
- 生成した `.pot` / `.po` は GNU gettext の `msgfmt --check` を通ること（CI では少なくとも Ubuntu で `gettext` を導入して確認する）。
- ドキュメントは日本語、CLI のメッセージは英語で書く。

## Boundary Strategy

- **Why this split**:
  - 汎用部品（引数パーサ、PO、YAML、Markdown）は Monde 固有の知識を持たず、それぞれ単体でテストできる。
  - Monde 固有のロジックは 2 か所に集める。翻訳単位の抽出と再構成は `translation-units` が、CLI の統合は `monde-cli` が担う。
  - foundation が整えば、パーサ系の 4 spec（cli-args、po-format、markdown-parser、yaml-front-matter）は互いに独立して並行に進められる。
- **Shared seams to watch**:
  - **ソース範囲の表現**: markdown-parser と yaml-front-matter が返すバイト範囲の形式を、translation-units が置換に使う。共通の型を foundation に置くかどうかを、設計時に揃える。
  - **複数行ブロックのプレフィックス**: リスト項目や引用の継続行にある `>` やインデントを、markdown-parser がどこまで範囲として表現するか。translation-units は訳文を差し戻すときに、そのプレフィックスを復元する必要がある。
  - **インライン要素の扱い**: インライン記法は msgid に含める（Q2-1）。一方で、リンク先 URL は翻訳対象外、画像の alt は翻訳対象とされている（Q2-2）。この 2 つをどう両立するか（URL を msgid に含めたままにするのか、プレースホルダにするのか）を、markdown-parser と translation-units の両方の要件で揃える。
  - **PO のデータモデル**: po-format が定義するエントリ型（コメント、フラグ、廃止エントリ、複数形）を、po-merge、translation-units、monde-cli が共有する。
  - **方言の差し替え**: markdown-parser の方言インターフェースは、次の版で GFM（表のセル単位の翻訳単位を含む）を追加できる形にしておく。

## Specs (dependency order)

- [x] project-foundation -- CMake ビルド、Naos の導入、GitHub Actions による CI、共通部品（UTF-8 / BOM の入出力、改行コードの検出、文字列バッファ、エラー報告、ソース範囲の型）。Dependencies: none
- [x] cli-args -- 再利用できるコマンドライン引数パーサ（サブコマンド、オプション、英語のヘルプとエラー）。Dependencies: project-foundation
- [x] po-format -- `.po` / `.pot` の読み書き（ヘッダ、各種コメント、fuzzy、廃止エントリ、エスケープ、複数形の読み込み許容）。Dependencies: project-foundation
- [x] po-merge -- msgmerge 相当のマージ（完全一致、あいまい一致による fuzzy の付与、廃止エントリ化）。Dependencies: po-format
- [x] markdown-parser -- ソース範囲を保持する CommonMark パーサと、方言を差し替えるインターフェース。Dependencies: project-foundation
- [x] yaml-front-matter -- フロントマターの検出と YAML サブセットの解析（スカラー値のソース範囲を保持）。Dependencies: project-foundation
- [x] translation-units -- Markdown とフロントマターからの翻訳単位の抽出（msgid、`#:`、`#.`、重複の統合）と、翻訳済み Markdown の範囲置換による再構成。Dependencies: po-format, markdown-parser, yaml-front-matter
- [x] monde-cli -- `extract` / `init` / `update` / `translate` の統合、各オプション、翻訳率の表示、CommonMark spec のラウンドトリップによる受け入れテスト。Dependencies: cli-args, po-merge, translation-units
- [x] docs-and-release -- 日本語の README と利用マニュアル、GitHub Releases での各 OS 向けバイナリ配布ワークフロー。Dependencies: monde-cli

## Phase 2: CLOpts の独立化

### Overview

コマンドライン引数パーサ（cli-args）を、将来別リポジトリで管理する独立ライブラリ「CLOpts」（CMake では `CLOpts::clopts`）として扱えるようにする。ルートの `cli_args/` に必要なファイルをすべて集め、そのディレクトリだけで構成・ビルド・テストができる状態にする。Monde 本体は `add_subdirectory(cli_args)` で取り込む。

### Approach Decision

- **Chosen**: 自己完結化 + 改名 + パッケージ化。foundation への依存をなくし（自前の最小限のエラー報告型と内部ユーティリティ）、公開 API を `clopts_*` に改名し、`install` / `find_package(CLOpts)` にも対応する。
- **Why**: 切り離し時に破壊的変更を残さず、「`cli_args/` を移して Monde 側の取り込みを差し替えるだけ」にできる。
- **Rejected alternatives**:
  - foundation の一部を同梱する: Monde と同時にリンクするとシンボルが衝突し、コードが二重管理になる。
  - 診断をコールバックで受け渡す: API の形が大きく変わり、テストの書き換えが増える。
  - 改名を切り離し時まで先送りする: ライブラリ名と API 名が食い違う状態が残る。
  - `add_subdirectory` / `FetchContent` だけに対応する: インストールして使う形態を今回確認できない。

### Scope

- **In**: `cli_args/` への移設、foundation 依存の除去、`clopts_*` への改名、独立した CMake プロジェクト、install / find_package 対応、Monde 本体の追従、単独ビルドの CI、README とライセンス、steering の更新
- **Out**: 別リポジトリへの実際の切り離し、引数パーサの機能追加と挙動の変更、共有ライブラリ対応、パッケージマネージャーへの登録、CLOpts 単体のリリース運用
- Phase 1 の Scope で Out とした「ライブラリとしての公開 API」は、CLOpts に限って In とする。Monde のほかの部品は引き続き内部モジュールのままとする。

### Constraints

- `cli_args/` の中から、その外にあるファイルを参照しない。
- サブプロジェクトとして取り込まれたときに、親プロジェクトへ警告オプションを伝播させず、Naos を二重に取得しない。
- `monde` の外から見える動作（ヘルプ、エラーメッセージ、終了コード）を変えない。
- C99、CMake 3.21 以上、対応環境、警告水準は Phase 1 と同じ。

### Boundary Strategy

- **Why this split**: 配置、名前、依存、ビルド、配布の変更は互いに密接で、途中の状態ではビルドが通らないため、1 つの spec にまとめる。挙動の要件は cli-args が持ち続ける。
- **Shared seams to watch**:
  - **エラーの橋渡し**: CLOpts のエラー報告型から `mnd_diag_list_t` への変換を `src/cli/` に置く。メッセージの文言と順序を変えない。
  - **ヘルプ整形の出力先**: 公開 API が `mnd_buf_t` を受け取っている箇所を、CLOpts 独自の形に置き換える。
  - **Naos の共有**: Monde と一緒に構成したときと単独で構成したときの両方で、同じ固定版を 1 回だけ取得する。
  - **リリース構成**: Monde の配布物（CPack）に CLOpts の install 規則が混ざらないようにする。

### Specs (dependency order)

- [ ] clopts-standalone -- cli-args をルートの `cli_args/` に移し、foundation に依存しない独立ライブラリ CLOpts（`CLOpts::clopts`、`clopts_*` API、install / find_package 対応）として単独でビルド・テストできるようにする。Monde 本体は `add_subdirectory` で取り込む。Dependencies: cli-args, monde-cli, docs-and-release
