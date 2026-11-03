# Requirements Document

## Project Description (Input)
**誰が困っているか**: Monde の開発者。C99 の自前実装部品を Windows / macOS / Linux の複数のコンパイラ（MSVC、MinGW-w64、Apple Clang、GCC、Clang）で一貫してビルド・テストする必要がある。

**現状**: リポジトリには LICENSE、README、入力資料（`docs/inputs/`）しかなく、ソースコード、ビルド構成、CI はまだない。ビルド・テスト・CI の基盤と、各部品が共通で使う低レベルのユーティリティがないため、以降の spec（cli-args、po-format、markdown-parser、yaml-front-matter など）を並行して進められない。

**何を変えるか**:
- CMake（3.21 以上）でモジュールごとにライブラリターゲットを分けた構成（`src/<module>/`、`tests/<module>/`）を用意し、全対象環境でビルドが通るようにする。コンパイラの警告とフラグを統一する（VLA の禁止、MSVC の `/utf-8`、`/Zc:preprocessor` など）。
- テストフレームワーク Naos を FetchContent などで固定したタグ・コミットで取り込み、CTest からテストを実行できるようにする。
- GitHub Actions の CI マトリクスで、上記 5 種のコンパイラによるビルドとテストを自動で行う。
- 以降の部品が使う共通ユーティリティをテスト付きで揃える:
  - UTF-8 ファイルの読み書き（常にバイナリモード、BOM の検出と除去、Windows での非 ASCII パス対応）
  - 改行コード（LF / CRLF）の検出
  - 可変長の文字列・バイトバッファ
  - ソース範囲（バイトオフセットと行番号）の型
  - エラーと診断の報告

**制約**: C99 を MSVC 互換の範囲で使う（VLA と `_Complex` は使わない）。外部の実行時ライブラリには依存しない（Naos はテスト専用、pre-1.0 のためバージョン固定）。

**対象外**: Markdown、YAML、PO、引数の解析ロジック。リリース配布のワークフロー（docs-and-release が担う）。`msgfmt --check` の CI ジョブ（monde-cli で追加する）。

## Requirements
<!-- Will be generated in /kiro-spec-requirements phase -->
