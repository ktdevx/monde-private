# Technology Stack

## Architecture

再利用可能な部品（内部ライブラリ）を積み上げ、最上位の薄い CLI 層で統合する構成。

```
monde-cli（サブコマンドのディスパッチ、ファイル入出力、統計表示）
  ├─ cli-args            汎用の引数パーサ
  ├─ po-merge ── po-format   PO の読み書きとマージ
  └─ translation-units   翻訳単位の抽出と再構成（Monde 固有）
        ├─ markdown-parser     ソース範囲を保持する CommonMark パーサ
        ├─ yaml-front-matter   フロントマター検出と YAML サブセット
        └─ po-format
foundation（UTF-8 入出力、改行検出、バッファ、ソース範囲、診断）← 全部品が依存
```

- **汎用部品は Monde を知らない**: 引数・PO・YAML・Markdown のパーサは単体でテストでき、Monde 固有の知識は `translation-units` と `monde-cli` の 2 か所にだけ置く。
- **範囲置換による再構成（中核の方式）**: パーサは各要素のソース範囲（バイトオフセット・行番号）を保持する。翻訳時は原文バッファをコピーし、翻訳対象の範囲だけを訳文で置き換える。構文木から Markdown を再生成しない。
- **方言の差し替え**: Markdown パーサは方言インターフェースを持ち、次の版で GFM を追加できる形にする。

## Core Technologies

- **Language**: C99（MSVC で使えない機能は使わない）
- **Build**: CMake 3.21 以上
- **Test**: [Naos](https://github.com/vabelorg/naos)（CTest 経由で実行）
- **CI**: GitHub Actions
- **配布**: GitHub Releases（各 OS 向けのビルド済みバイナリ）

## Key Libraries

- **実行時の外部依存はゼロ**。Markdown、YAML、PO、コマンドライン引数のパーサはすべて自前で実装する。C 標準ライブラリと、Windows の場合は Win32 API のみを使う。
- **Naos はテスト専用**。pre-1.0 のため、FetchContent でタグ `v0.4.0` に固定して取り込む（`cmake/MondeNaos.cmake` の 1 か所で管理）。`BUILD_TESTING=OFF` では取得しない。

## Development Standards

### 言語機能と移植性
- VLA と `_Complex` は使わない。GCC / Clang では `-Werror=vla` を指定する。
- MSVC では `/std:c11`、`/utf-8`、`/Zc:preprocessor`、`_CRT_SECURE_NO_WARNINGS` を指定する。
- コンパイラの警告とフラグは CMake で一元管理し、各ターゲットに同じ設定を適用する。

### 文字コード・ファイル入出力
- ファイルは常にバイナリモードで開く（改行の自動変換を避け、バイト単位の保持を守るため）。
- 入出力は UTF-8 のみ。読み込み時に BOM を検出して除去できる。改行コードは入力から検出し、出力に引き継ぐ。
- Windows では非 ASCII のパスを扱うため、UTF-8 から UTF-16 に変換して `_wfopen` などを使う。ファイル入出力は foundation の共通部品に集約し、各部品から直接 `fopen` を呼ばない。

### 互換性
- 生成する `.pot` / `.po` は GNU gettext の `msgfmt --check` を通ること（CI では少なくとも Ubuntu で確認する）。
- マージの挙動は `msgmerge` を基準にする。

### Testing
- 各部品（`tests/<module>/`）ごとに Naos の単体テストを持ち、CTest から実行する。
- **受け入れ基準**: CommonMark spec 0.31.2 の全テストケースで「extract → translate（翻訳なし）」の結果が原文とバイト単位で一致すること（データ駆動テスト）。
- spec.txt は CC-BY-SA 4.0 のため、無改変のまま独立したディレクトリにライセンス表記を添えて置くか、テスト時に取得する。リリース成果物には含めない。
- 性能目標は特に設けない。
- Naos を使っていて「こんな機能があったらいいのに」と感じたこと（足りない機能、警告を誘発するマクロ、回避策が必要だった挙動など）は、`docs/dev/naos-feedback.md` に記録する。定期的に Naos の Issue として登録する。

## Development Environment

### 対応環境（CI マトリクス）

| OS | コンパイラ |
| --- | --- |
| Windows | MSVC、MinGW-w64 |
| macOS | Apple Clang |
| Linux | GCC、Clang |

### Common Commands
単一構成・複数構成（Visual Studio）のどちらのジェネレータでも同じ 3 行で構成・ビルド・テストする。
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
- 製品だけをビルドする（Naos を取得しない）: 構成時に `-DBUILD_TESTING=OFF`
- 1 部品のテストだけ実行する: `ctest ... -L <module>`（ビルド構成の自己検証は `-L toolchain`）
- オフラインで構成する: `-DFETCHCONTENT_SOURCE_DIR_NAOS=<ローカルの Naos>`

## Key Technical Decisions

- **範囲置換方式を採用**: 構文木からの再生成は記号や空行を正規化してしまい、バイト単位の保持を満たせない。行ベースの簡易処理では CommonMark のラウンドトリップを満たせない。
- **部品別分割**: 部品ごとに境界とテストを明確にし、パーサ群を並行して開発できるようにする。初版では部品を内部モジュールとして扱い、公開 API の互換性は保証しない。
- **言語の使い分け**: CLI のメッセージ・コード内識別子は英語、ドキュメントと spec は日本語。

---
_Document standards and patterns, not every dependency_
