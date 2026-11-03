# Project Structure

> 2026-11-03 時点で project-foundation の基盤（ビルド構成、テスト基盤、CI、`src/foundation/`）が実装済み。以下の規約は後続のすべての spec に共通する。

## Organization Philosophy

**部品（モジュール）単位の構成**。再利用できる部品ごとに 1 つのディレクトリと 1 つの CMake ライブラリターゲットを持ち、ソースとテストを同じモジュール名で対にする。依存は下位の部品へ向かう一方向とし、汎用部品から Monde 固有の部品（`translation-units`、CLI）へ依存しない。

## Directory Patterns

### モジュールのソース
**Location**: `src/<module>/`  
**Purpose**: 1 つの部品の実装とヘッダ。1 モジュール = 1 静的ライブラリターゲット（`monde_add_module` で作る）。  
**Example**: `src/foundation/`（共通ユーティリティ）、`src/po/`（PO の読み書き）、`src/markdown/`（CommonMark パーサ）

### モジュールのテスト
**Location**: `tests/<module>/`  
**Purpose**: 対応するモジュールの Naos テスト。`monde_add_module_tests` で CTest に登録する。  
**Example**: `tests/foundation/` は `src/foundation/` をテストする  
**Exception**: `tests/toolchain/` は `src/` に対応するモジュールを持たない唯一の例外で、ビルド構成（警告・VLA のエラー化、MSVC のオプション）そのものを検証する。CTest のラベルは `toolchain`。

### ビルド構成
**Location**: `CMakeLists.txt`、`cmake/`  
**Purpose**: コンパイラオプション（`monde_apply_compile_options`）、ターゲット作成関数（`monde_add_module` / `monde_add_module_tests` / `monde_add_executable`）、Naos の取得。オプションやターゲットの作り方はここに集約し、各モジュールの `CMakeLists.txt` では個別に設定しない。

### 受け入れテストのデータ
**Location**: テスト用の独立したディレクトリ（名前は monde-cli の設計で決める）  
**Purpose**: CommonMark spec.txt などの外部テストデータ。ライセンス表記を添えて無改変で置くか、テスト時に取得する。リリース成果物には含めない。

### ドキュメント
**Location**: `docs/`、`README.md`  
**Purpose**: 日本語の利用マニュアルと README。  
**Note**: `docs/inputs/` は要件定義用の入力資料（RFP、Q&A）。要件定義以外のフェーズでは参照しない。確定した内容は steering と各 spec の requirements.md に反映されている。

### 仕様とプロジェクトメモリ
**Location**: `.kiro/specs/<feature>/`、`.kiro/steering/`  
**Purpose**: spec 駆動開発の仕様（brief → requirements → design → tasks）と、プロジェクト全体の方針。spec 名は kebab-case（例: `markdown-parser`、`yaml-front-matter`）。

## Naming Conventions

- **モジュール名**: 小文字の snake_case で、spec 名の短い形とする（例: spec `project-foundation` → モジュール `foundation`）。ディレクトリ `src/<module>/`、`tests/<module>/` に使う。
- **CMake ターゲット**: ライブラリは `monde_<module>`、別名 `monde::<module>`（リンクには別名を使う）。テスト実行ファイルは `<module>_tests`、CTest のテスト名は `<module>_tests.<suite>.<test>`、ラベルは `<module>`。
- **ファイル**: 小文字の snake_case（`.c` / `.h`）。
- **関数・型・変数**: snake_case。C には名前空間がないため、外部から見えるシンボルには接頭辞 `mnd_` と短縮名を付ける。
  - 関数: `mnd_<短縮名>_<動作>`（例: `mnd_buf_append`、`mnd_diag_list_at`）。型名の `_t` は関数名に含めない（`mnd_buf_t_append` としない）。
  - 型: `typedef` で定義し、型名の末尾に `_t` を付ける。構造体・列挙のタグ名は `_t` を除いた同じ名前にする（例: `typedef struct mnd_buf { ... } mnd_buf_t;`、`typedef enum mnd_severity { ... } mnd_severity_t;`）。
  - マクロ・列挙値: `MND_<短縮名>_`（例: `MND_BUF_INIT`、`MND_SEVERITY_ERROR`）。`_T` は含めない。
  - 短縮名は各モジュールの設計で決め、既存のものと重複させない。foundation が予約済み: `buf`、`diag`、`severity`、`utf8`、`eol`、`span`、`slice`、`source`、`file`、`wide`（ほかにマクロ `MND_PRINTF_LIKE` も foundation が使用している）。cli_args が予約済み: `args`
- **ヘッダガード**: `MONDE_<MODULE>_<FILE>_H`（例: `MONDE_FOUNDATION_BUF_H`）。
- **モジュール内部の関数**: `static` にしてファイル外に公開しない。

## Include Organization

```c
#include "foundation/buf.h"  /* プロジェクトのヘッダは同じモジュール内も含め src/ からのパスで指定 */
#include <stdio.h>           /* 標準ライブラリ */
```

- include の基点は `src/`。`monde_add_module` がこれを PUBLIC で公開するため、リンクしたモジュールのヘッダはそのまま include できる。
- 各モジュールの公開ヘッダは、依存する他のモジュールからのみ include する。依存関係は `monde_add_module(<module> ... DEPENDS <module>...)` で明示する。

## 新しいモジュールの追加手順

1. `src/CMakeLists.txt` に `add_subdirectory(<module>)` を 1 行追加し、`src/<module>/CMakeLists.txt` で `monde_add_module(<module> SOURCES ... [DEPENDS ...])` を呼ぶ。
2. `tests/CMakeLists.txt` に `add_subdirectory(<module>)` を 1 行追加し、`tests/<module>/CMakeLists.txt` で `monde_add_module_tests(<module> SOURCES ...)` を呼ぶ。
3. 製品の実行ファイルは `monde_add_executable` で作る（Naos へのリンクは構成時エラーになる）。

コンパイラオプションはこれらの関数が自動で適用する。

## Code Organization Principles

- **依存は一方向**: foundation ← 汎用パーサ（cli-args、po-format、markdown-parser、yaml-front-matter）← po-merge / translation-units ← monde-cli。逆向きの依存や、パーサ同士の横の依存を作らない。
- **共有する型は下位に置く**: ソース範囲や PO エントリのように複数の部品が使う型は、それを定義する下位の部品（foundation、po-format）に置き、上位で再定義しない。
- **I/O は境界に寄せる**: パーサや再構成器はメモリ上のバッファを入力・出力とし、ファイルの読み書きは foundation の共通部品と CLI 層が担う。これにより部品を単体でテストしやすくする。
- **エラーは診断として返す**: 部品は直接 `stderr` に書いたり `exit` したりせず、foundation の診断の仕組みで呼び出し側に返す。メッセージの表示と終了コードの決定は CLI 層が行う。

---
_Document patterns, not file trees. New files following patterns shouldn't require updates_
