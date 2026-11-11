# Technology Stack

## Architecture

再利用可能な部品（内部ライブラリ）を積み上げ、最上位の薄い CLI 層で統合する構成。

```
monde-cli（サブコマンドのディスパッチ、ファイル入出力、統計表示）
  ├─ CLOpts（cli_args/）  汎用の引数パーサ（独立ライブラリ。foundation にも依存しない）
  ├─ po-merge ── po-format   PO の読み書きとマージ
  └─ translation-units   翻訳単位の抽出と再構成（Monde 固有）
        ├─ markdown-parser     ソース範囲を保持する CommonMark パーサ
        ├─ yaml-front-matter   フロントマター検出と YAML サブセット
        └─ po-format
foundation（UTF-8 入出力、改行検出、バッファ、ソース範囲、診断）← CLOpts 以外の全部品が依存
```

- **汎用部品は Monde を知らない**: 引数・PO・YAML・Markdown のパーサは単体でテストでき、Monde 固有の知識は `translation-units` と `monde-cli` の 2 か所にだけ置く。
- **CLOpts はリポジトリ内の独立ライブラリ**: 引数パーサ CLOpts（`cli_args/`、CMake のターゲット `CLOpts::clopts`、公開名 `clopts_*` / `CLOPTS_*`）は、将来の別リポジトリへの切り離しに備えて、Monde の部品（foundation を含む）に依存しない独立した CMake プロジェクトにしている。単独で構成・ビルド・テストでき、`install` と `find_package(CLOpts)` にも対応する。エラーは `clopts_errors_t` で返し、`src/cli/cli_clopts.[ch]` が Monde の診断に変換する。Monde への取り込みは `cmake/MondeCLOpts.cmake` の 1 か所で定義する。配置と命名の規約は `structure.md`。
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
- **実行時の依存と、バイナリに取り込む素材を区別する**。「実行時の外部依存はゼロ」は実行時に必要な共有ライブラリなどの話で、変えない。実行ファイルに取り込む第三者の素材（生成したデータの表、取り込んだソース、静的にリンクするライブラリ）は、許容型のライセンス（MIT、BSD、`Unicode-3.0` など）のものに限り、同じ変更でリポジトリの直下の `THIRD_PARTY_NOTICES`（索引）と `licenses/<ID>.txt`（素材ごとの著作権表示とライセンスの全文）に登録し、取り込んだ先のファイルの先頭のコメントに ID の行 ` * THIRD_PARTY_NOTICES: <ID>` を置く（`ctest -L docs` の `docs_check.notices` が対応を検査する。手順は `docs/dev/release.md`）。現在の素材は Unicode Character Database 15.0.0（`Unicode-3.0`）と WHATWG の `entities.json`（`BSD-3-Clause`）。コピーレフト（GPL、CC-BY-SA など。`spec.txt` は CC-BY-SA 4.0）の素材はテスト専用にとどめ、実行ファイルに取り込まない。
- **Naos はテスト専用**。pre-1.0 のため、FetchContent でタグ `v0.4.0` に固定して取り込む。Monde は `cmake/MondeNaos.cmake` で管理する。CLOpts は単独で構成できるように自分の `cli_args/cmake/CLOptsNaos.cmake` を持ち、同じ版に固定する（版を変えるときは 2 つのファイルを同時に変える）。Monde の構成では先に `MondeNaos` が取得し、CLOpts はそれを使うので、Naos の取得は 1 回だけになる。`BUILD_TESTING=OFF`（CLOpts の単独構成では `CLOPTS_BUILD_TESTS=OFF`）では取得しない。

## Development Standards

### 言語機能と移植性
- VLA と `_Complex` は使わない。GCC / Clang では `-Werror=vla` を指定する。
- MSVC では `/std:c11`、`/utf-8`、`/Zc:preprocessor`、`_CRT_SECURE_NO_WARNINGS` を指定する。
- コンパイラの警告とフラグは CMake で一元管理し、各ターゲットに同じ設定を適用する。

### コーディングスタイル
- **波括弧は独立した行に置く（Allman スタイル）**: 関数定義、`if` / `else` / `for` / `while`、`struct` / `enum` の定義では、`{` を次の行の先頭に置く。`else` と `else if` は `}` の次の行から書く。初期化子（`{ NULL, 0, 0 }` など）は対象外。
- **本体が 1 文でも波括弧を省略しない**: `if`、`else`、`for`、`while` の本体は常に `{ }` で囲む。
- **複合条件では比較を個別に括弧で囲む**: `&&` / `||` の各項が比較なら、それぞれを括弧で囲む（例: `if ((i > 0) && (data[i - 1] == '\r'))`）。比較の片側が演算式なら、その式も括弧で囲む（例: `if (len > (SIZE_MAX / 2))`）。条件を折り返すときは、演算子を次の行の先頭に置く。
- **複数行のコメントは次の形にする**:
  ```c
  /*
   * 1 行目
   * 2 行目
   */
  ```
  1 行で収まるコメントは `/* ... */` とし、`//` は使わない。

### 文字コード・ファイル入出力
- ファイルは常にバイナリモードで開く（改行の自動変換を避け、バイト単位の保持を守るため）。
- 入出力は UTF-8 のみ。読み込み時に BOM を検出して除去できる。改行コードは入力から検出し、出力に引き継ぐ。
- Windows では非 ASCII のパスを扱うため、UTF-8 から UTF-16 に変換して `_wfopen` などを使う。ファイル入出力は foundation の共通部品に集約し、各部品から直接 `fopen` を呼ばない。

### 互換性
- 生成する `.pot` / `.po` は GNU gettext の `msgfmt --check` を通ること（CI では少なくとも Ubuntu で確認する）。
- マージの挙動は `msgmerge` を基準にする。

### Testing
- 各部品（`tests/<module>/`）ごとに Naos の単体テストを持ち、CTest から実行する。CLOpts のテストは `cli_args/tests/` にあり、Monde の構成でも登録される（ラベル `clopts`）。
- **受け入れ基準**: CommonMark spec 0.31.2 の全テストケースで「extract → translate（翻訳なし）」の結果が原文とバイト単位で一致すること（データ駆動テスト）。
- spec.txt は CC-BY-SA 4.0 のため、無改変のまま独立したディレクトリにライセンス表記を添えて置くか、テスト時に取得する。リリース成果物には含めない。
- 性能目標は特に設けない。
- Naos を使っていて「こんな機能があったらいいのに」と感じたこと（足りない機能、警告を誘発するマクロ、回避策が必要だった挙動など）は、`docs/dev/naos-feedback.md` に記録する。Issue への登録は開発者が行い、AI エージェントは記録だけを行う（Naos のリポジトリに Issue やコメントを作成しない）。

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
- 1 部品のテストだけ実行する: `ctest ... -L <module>`（ビルド構成の自己検証は `-L toolchain`、CLOpts のテストは `-L clopts`、ドキュメントの照合は `-L docs`）
- CLOpts だけを単独で構成・ビルド・テストする: `cmake -S cli_args -B build-clopts` の後、同じ 2 行（`cmake --build`、`ctest`）。インストール後の利用の検証（`clopts_package.find_package`）は単独構成でだけ登録される
- オフラインで構成する: `-DFETCHCONTENT_SOURCE_DIR_NAOS=<ローカルの Naos>`

## Key Technical Decisions

- **範囲置換方式を採用**: 構文木からの再生成は記号や空行を正規化してしまい、バイト単位の保持を満たせない。行ベースの簡易処理では CommonMark のラウンドトリップを満たせない。
- **部品別分割**: 部品ごとに境界とテストを明確にし、パーサ群を並行して開発できるようにする。初版では部品を内部モジュールとして扱い、公開 API の互換性は保証しない。例外は CLOpts で、ライブラリとしての公開 API を持ち、バージョンは `cli_args/CMakeLists.txt` の `project(CLOpts VERSION ...)` で Monde とは別に管理する。
- **言語の使い分け**: CLI のメッセージ・コード内識別子は英語、ドキュメントと spec は日本語。

---
_Document standards and patterns, not every dependency_
