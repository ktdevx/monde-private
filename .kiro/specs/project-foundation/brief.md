# Brief: project-foundation

## Problem
Monde の開発者は、C99 の自前実装部品を Windows / macOS / Linux の複数のコンパイラで一貫してビルド・テストする必要がある。ビルド、テスト、CI と、各部品が共通で使う低レベルのユーティリティがないと、以降の spec を並行して進められない。

## Current State
リポジトリには LICENSE、README、入力資料（`docs/inputs/`）しかない。ソースコード、ビルド構成、CI はまだない。

## Desired Outcome
- CMake（3.21 以上）で全対象環境のビルドが通り、Naos のテストが CTest で実行できる。
- GitHub Actions で MSVC、MinGW-w64、Apple Clang、GCC、Clang のビルドとテストが自動で行われる。
- 以降の部品が使う共通ユーティリティがテスト付きで揃っている。

## Approach
- モジュールごとに CMake のライブラリターゲットを分けるディレクトリ構成にする（例: `src/<module>/`、`tests/<module>/`）。
- Naos は FetchContent などで、固定したタグ・コミットを取り込む。
- コンパイラの警告とフラグを統一する（VLA の禁止、MSVC の `/utf-8`、`/Zc:preprocessor` など）。

## Scope
- **In**:
  - CMake のプロジェクト構成とコンパイラフラグ、Naos の取り込みと CTest の連携
  - GitHub Actions の CI マトリクス（ビルドとテスト）
  - 共通部品:
    - UTF-8 ファイルの読み書き（バイナリモード、BOM の検出と除去、Windows での非 ASCII パス対応）
    - 改行コード（LF / CRLF）の検出
    - 可変長の文字列・バイトバッファ
    - ソース範囲（バイトオフセットと行番号）の型
    - エラーと診断の報告
- **Out**: 各パーサの実装、リリース配布のワークフロー（docs-and-release が担う）

## Boundary Candidates
- ビルド・テスト・CI の基盤
- 共通の I/O とバッファのユーティリティ
- ソース範囲と診断の共通型

## Out of Boundary
- Markdown、YAML、PO、引数の解析ロジック
- `msgfmt --check` の CI ジョブ（PO を出力できるようになってから monde-cli で追加する）

## Upstream / Downstream
- **Upstream**: なし
- **Downstream**: cli-args、po-format、markdown-parser、yaml-front-matter をはじめ、すべての spec

## Existing Spec Touchpoints
- **Extends**: なし
- **Adjacent**: なし（最初の spec）

## Constraints
- C99。MSVC 互換の範囲で書く（VLA と `_Complex` は使わない）。
- 外部の実行時ライブラリには依存しない。Naos はテスト専用で、pre-1.0 のためバージョンを固定する。
- 改行コードを入力に合わせるため、ファイルは常にバイナリモードで開く。
