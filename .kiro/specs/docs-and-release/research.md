# Research & Design Decisions

## Summary
- **Feature**: `docs-and-release`
- **Discovery Scope**: Extension（実装済みの monde-cli と project-foundation の CI に、ドキュメントとリリースの自動化を加える）
- **Key Findings**:
  - monde-cli は実装済みで、4 つのサブコマンドのヘルプ（`monde <cmd> --help`）、統計の書式、診断の書式、終了コードが確定している。ドキュメントはこの出力をそのまま出典にでき、`tests/cli/` の `cli_case.cmake` と同じ「ビルドした `monde` を `cmake -P` で起動して比べる」方式でドキュメントの整合を機械的に検査できる。
  - アーカイブは CPack（`ZIP` / `TGZ` ジェネレータ）と `install()` の 2 行で作れ、実行権限と最上位ディレクトリ（`monde-X.Y.Z-<os>-<arch>/`）を保つ。チェックサムは `cmake -E sha256sum` または `sha256sum` で作れる。追加の依存はいらない。
  - Linux は `-DCMAKE_EXE_LINKER_FLAGS=-static` だけで完全静的の実行ファイルになり（手元で確認: `statically linked`、`localtime` のタイムゾーン解決も動く）、古い glibc の環境でも動く配布物にできる。ソースの変更はいらない。
  - macOS x64 は Intel ランナーに頼らず、arm64 ランナー上で `CMAKE_OSX_ARCHITECTURES=x86_64` によるクロスビルドで作り、Rosetta 2（`arch -x86_64`）で `--version` を確かめる。

## Research Log

### 既存の CLI の確定した出力
- **Context**: 利用マニュアルのコマンドリファレンス（4.1〜4.10）と README のクイックスタート（1.4）は、monde-cli の確定した仕様と一致させる必要がある。
- **Sources Consulted**: `.kiro/specs/monde-cli/requirements.md`（Req 1〜9）、ビルドした `monde` の `--help` / `<cmd> --help` の出力、`src/cli/cli_report.c`（統計の書式）、`tests/cli/CMakeLists.txt`（起動テストの方式）。
- **Findings**:
  - `monde --version` → `monde 0.1.0`。`project(monde VERSION 0.1.0)` から `MONDE_VERSION` で渡される（1 か所）。
  - 統計の書式: `extract: N messages`、`init: N messages, language L`、`init: using language 'L' from VAR`、`update: A matched, B fuzzy, C added, D obsoleted`、`translate: A translated, B fuzzy, C untranslated, D missing, E total, P.Q% translated`。
  - 診断の書式: `<path>:<line>: error|warning: <message>`、`<path>: error: ...`、`monde: error: ...`。
  - 起動テストは `tests/cli/cli_case.cmake`（`cmake -P`）で、標準出力をファイルとバイト比較し、標準エラー出力は部分文字列で確かめる。`tests/cli/data/help.txt` に `--help` の期待値がある。
- **Implications**: ドキュメントのヘルプの引用は `--help` の出力をそのまま載せ、テストで同一性を検査する（9.1）。クイックスタートは例のファイルを添えて実行可能にし、同じ方式で検査する（9.2）。

### 既存のビルド構成と CI
- **Context**: リリースワークフローは project-foundation の 3 行の手順と CI の定義を再利用し、CI を変更しない（7.2、7.12）。
- **Sources Consulted**: `CMakeLists.txt`、`cmake/MondeTargets.cmake`、`.github/workflows/ci.yml`、`src/monde/CMakeLists.txt`。
- **Findings**:
  - MSVC は静的 CRT（`MultiThreaded`）、MinGW は `-static` がすでに指定されている。Linux と macOS に静的リンクの指定はない。
  - `install()` と CPack の設定はない。`spec.txt` は `MND_MD_SPEC_FILE` でテストだけが読み、製品ターゲットには含まれない。
  - CI は `push`（main）と `pull_request` で起動する。タグの push では起動しない。
  - `BUILD_TESTING=OFF` で Naos を取得せずに製品だけをビルドできる（手元で確認）。
- **Implications**: リリースは `BUILD_TESTING=OFF` の製品ビルドを使い、テストは main の CI に任せる。リリース手順に「main の CI が緑であることを確かめてからタグを付ける」を含める。

### アーカイブの作り方（CPack vs. 手書きのスクリプト）
- **Context**: 8.1〜8.5（ファイル名、形式、同梱物、最上位ディレクトリ、実行権限）を各 OS で同じ方法で満たしたい。
- **Sources Consulted**: 手元での試作（`install(TARGETS monde RUNTIME DESTINATION .)`、`install(FILES LICENSE README.md DESTINATION .)`、`CPACK_GENERATOR TGZ|ZIP`、`CPACK_PACKAGE_FILE_NAME`、`cpack -C Release`）。
- **Findings**:
  - TGZ は `monde-0.1.0-linux-x64/monde`（`-rwxr-xr-x`）、`README.md`、`LICENSE` の 3 ファイルで、最上位ディレクトリ名はパッケージ名と同じになる（`CPACK_INCLUDE_TOPLEVEL_DIRECTORY` の既定）。ZIP も同じ構成。
  - `cpack -G ZIP` のように生成器をコマンドラインから選べるため、CMake 側は OS ごとの既定だけ持てばよい。
  - `cmake -E sha256sum` は `sha256sum` と同じ書式（ハッシュ、空白 2 つ、ファイル名）で出力する。
- **Implications**: アーカイブは CPack に任せ、OS・アーキテクチャの名前は構成時のキャッシュ変数で受け取る。チェックサムは公開ジョブ（Linux）で `sha256sum` を使い、1 つの `SHA256SUMS` にまとめる。

### ランナーと対象の組み合わせ
- **Context**: 7.1 の 4 対象（Windows x64、macOS arm64、macOS x64、Linux x64）をどのランナーで作るか。
- **Sources Consulted**: GitHub Actions のランナーイメージに関する既知の情報（`macos-latest` は arm64、Intel の `macos-13` は退役済み。`windows-latest`・`ubuntu-latest` は x64。`gh` CLI は全ランナーに導入済み）、CI の既存のマトリクス。
- **Findings**:
  - macOS x64 は arm64 ランナーでのクロスビルド（`-DCMAKE_OSX_ARCHITECTURES=x86_64`）で作れる。Apple Clang はこれを標準で扱う。検証は Rosetta 2 経由の `arch -x86_64 ./monde --version`。
  - Linux は `ubuntu-latest` の glibc に依存しないよう `-static` でリンクする（試作で動作確認）。
  - Windows は CI と同じ MSVC（`windows-latest`）。CRT は静的なので再頒布可能パッケージはいらない。
  - Release の作成と添付は、ランナー同梱の `gh` CLI（`gh release view` / `create` / `upload`）で行える。サードパーティの Action はいらない。
- **Implications**: マトリクスは 4 行、macOS は同じランナーで 2 行。公開は 1 つの後続ジョブに集約し、4 つのビルドがすべて成功したときだけ動く（`needs`）。

### サードパーティ表記
- **Context**: 1.7 で spec.txt と Naos の名称・用途・ライセンスを README に示す。
- **Sources Consulted**: `tests/data/commonmark/README.md`（spec.txt は CC-BY-SA 4.0、SHA-256 付き）、Naos の `LICENSE`（`v0.4.0`、MIT License、The Naos Authors）。
- **Findings**: どちらも製品のソースではなく、テストでのみ使う。Naos は `BUILD_TESTING=OFF` で取得されない。
- **Implications**: README のサードパーティの節に 2 つを表で載せ、リリース成果物に含まれないことを明記する。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| CPack + `gh` CLI（採用） | `install()` と CPack でアーカイブ、ランナーの `gh` で Release | 追加依存なし、手元で `cpack` を再現できる、権限とディレクトリ構成を CPack が保証 | CPack の生成器名とファイル名の規約を知る必要がある | steering の「依存を増やさない」に合う |
| 手書きの `cmake -P` スクリプトで `cmake -E tar` | スクリプトでコピーと圧縮 | 細かく制御できる | `install()` を書かないため、同梱物の定義が 2 か所になる | 不採用 |
| サードパーティの Release Action | `softprops/action-gh-release` など | 記述が短い | 外部 Action への依存と更新追従が増える。`gh` で同じことができる | 不採用 |
| ドキュメントの検査: 手書きの期待値 | ヘルプのテキストを `tests/` に別途持つ | 単純 | ドキュメントと期待値の 2 か所がずれる | 不採用。ドキュメント自体から抜き出して比べる |

## Design Decisions

### Decision: ドキュメントの引用をマーカー付きのコードブロックから抜き出して検査する
- **Context**: 9.1、9.2、9.4 は「手順または自動の検査」を求める。手順だけでは更新時に見落とす。
- **Alternatives Considered**:
  1. チェックリストの手順のみ — 機械的な保証がない
  2. ヘルプの期待値を `tests/` に別途置く — ドキュメントと二重管理
  3. ドキュメントの HTML コメントのマーカー（`<!-- monde-check: help extract -->` など）の直後のコードブロックを `cmake -P` で抜き出し、ビルドした `monde` の出力やコマンドの実行結果と比べる
- **Selected Approach**: 3。`tests/docs/` に `docs_check.cmake` を置き、CTest（ラベル `docs`）から実行する。
- **Rationale**: 既存の `cli_case.cmake` と同じ方式で、シェルに依存せず各 OS の CI で動く。ドキュメントが唯一の出典になる。
- **Trade-offs**: マーカーの約束事をマニュアルの保守者が守る必要がある（`docs/dev/release.md` に書く）。
- **Follow-up**: Windows での改行（CRLF）の扱い。`--help` の出力は LF、ドキュメントも LF（`.gitattributes` で固定しない場合、git の `autocrlf` で CRLF になり得る）。比較前に CRLF を LF に正規化する。

### Decision: Linux の配布物は完全静的リンクにする
- **Context**: `ubuntu-latest` でビルドすると新しい glibc に依存し、古い配布版で動かない。
- **Alternatives Considered**:
  1. 古いランナー（`ubuntu-22.04`）でビルド — ランナーの退役に追従し続ける必要がある
  2. `-static` で glibc ごと静的にリンク — NSS や `dlopen` を使わない本製品では問題がない
- **Selected Approach**: 2。ワークフローから `-DCMAKE_EXE_LINKER_FLAGS=-static` を渡す（ソースの変更なし）。
- **Rationale**: 製品は C 標準ライブラリしか使わず、ネットワークやユーザー名の解決をしない。試作で `--version` と `extract`（`localtime` を含む）が動作した。
- **Trade-offs**: 実行ファイルが約 1.3 MB になる。許容範囲。
- **Follow-up**: ワークフローの検証ステップで `ldd` が `not a dynamic executable` を返すことを確かめる。

### Decision: Release の本文は書かず、作成と添付だけを自動化する
- **Context**: 7.5、7.6 と Boundary Context（リリースノートの自動生成は範囲外）。
- **Selected Approach**: `gh release view <tag>` で存在を確かめ、なければ `gh release create <tag> --title <tag> --notes ""` で作り、`gh release upload <tag> <files> --clobber` で添付する。既存（下書きを含む）なら upload だけを行う。
- **Rationale**: 人が先に下書きを書いてからタグを付ける運用と、タグを先に付けて後から本文を書く運用の両方に対応する。
- **Trade-offs**: 作成直後の Release は本文が空。手順書で本文の記入を促す。

### Decision: リリースの検証は `--version`、静的リンク、アーカイブの内容の 3 点に絞る
- **Context**: 7.3、8.6、8.3〜8.5 を各対象で確かめたいが、テスト一式（Naos、gettext）はリリースの範囲外。
- **Selected Approach**: 各ビルドジョブで (1) `monde --version` が `monde X.Y.Z` と一致、(2) Linux は `ldd` で静的、macOS は `otool -L` が `/usr/lib/libSystem.B.dylib` 以外を含まない、Windows は確認を省略（静的 CRT を構成で保証）、(3) アーカイブの一覧が期待する 3 つのエントリと一致、を `bash` のステップで確かめる。
- **Rationale**: 最小の検証で 7.3 と 8.x の主要な失敗（バージョンの不一致、動的依存、同梱漏れ）を捕まえる。
- **Follow-up**: 手元で同じ検証ができるよう `tools/release/verify_package.cmake` に切り出す（`cmake -P`、ワークフローからも呼ぶ）。

### Decision: 設計の合成（Generalization / Build vs Adopt / Simplification）
- **Generalization**: 9.1（ヘルプの引用）、9.2（コマンド列）、9.4（リンク）の 3 つの検査は「ドキュメントから抜き出して確かめる」1 つの仕組み（`docs_check.cmake` のマーカーの種類）にまとめる。
- **Build vs Adopt**: アーカイブは CPack、Release の操作は `gh` CLI、チェックサムは `sha256sum` を採用。自前のスクリプトは「抜き出して比べる」検査と、パッケージの検証の 2 つに限る。
- **Simplification**: CMake に新しいオプションやモジュールを増やさず、リリース固有の指定（静的リンク、アーキテクチャ、対象名）はワークフローからキャッシュ変数で渡す。マニュアルは 4 章 + 目次 + 例のファイルに留める。

## Risks & Mitigations
- Rosetta 2 がランナーにない、または `arch -x86_64` が失敗する — macOS x64 の `--version` の検証をスキップせず失敗にする。発生したら Intel ランナーの有無を調べて設計を見直す（Revalidation Trigger）。
- ドキュメントの CRLF 化で検査が落ちる — 比較前に LF に正規化し、`.gitattributes` で `*.md text eol=lf` を指定するかは実装時に判断する（既存の `.md` の差分に注意）。
- `gh release upload --clobber` で既存の添付を置き換えてしまう — 再実行（7.11）と手動起動（7.10）で意図した挙動。本文は変更しない。
- `project(VERSION)` の更新忘れ — `check` ジョブでタグと不一致なら失敗（7.8）。手順書にも明記。
- `workflow_dispatch` で存在しないタグを指定 — `check` ジョブで `git rev-parse` によりタグの存在を確かめて失敗にする。

## References
- `.kiro/specs/monde-cli/requirements.md` — CLI の確定仕様（本 spec のドキュメントの出典）
- `.kiro/specs/yaml-front-matter/design.md` の「改訂」— 既知の制限の出典（5.3〜5.5）
- `tests/data/commonmark/README.md` — spec.txt の出典とライセンス
- https://github.com/vabelorg/naos — Naos（MIT License、テスト専用）
- CMake: `install()`、CPack（`ZIP`、`TGZ`）、`cmake -E sha256sum`、`cmake -P`
- GitHub CLI: `gh release view` / `create` / `upload`
