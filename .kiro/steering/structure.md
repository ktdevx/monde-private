# Project Structure

> 2026-11-03 時点で project-foundation の基盤（ビルド構成、テスト基盤、CI、`src/foundation/`）が実装済み。以下の規約は後続のすべての spec に共通する。

## Organization Philosophy

**部品（モジュール）単位の構成**。再利用できる部品ごとに 1 つのディレクトリと 1 つの CMake ライブラリターゲットを持ち、ソースとテストを同じモジュール名で対にする。依存は下位の部品へ向かう一方向とし、汎用部品から Monde 固有の部品（`translation-units`、CLI）へ依存しない。

## Directory Patterns

### モジュールのソース
**Location**: `src/<module>/`  
**Purpose**: 1 つの部品の実装とヘッダ。1 モジュール = 1 静的ライブラリターゲット（`monde_add_module` で作る）。  
**Example**: `src/foundation/`（共通ユーティリティ）、`src/po/`（PO の読み書き）、`src/markdown/`（CommonMark パーサ）

### 製品の実行ファイル
**Location**: `src/monde/`  
**Purpose**: 実行ファイル `monde` の `main`。CLI の処理はライブラリ `monde::cli`（`src/cli/`）に置き、ここでは環境の組み立てと入口の呼び出しだけを行う。`monde_add_executable` で作り、バージョンは `project` の VERSION からコンパイル定義 `MONDE_VERSION` で渡す。

### モジュールのテスト
**Location**: `tests/<module>/`  
**Purpose**: 対応するモジュールの Naos テスト。`monde_add_module_tests` で CTest に登録する。  
**Example**: `tests/foundation/` は `src/foundation/` をテストする  
**Exception**: `src/` に対応するモジュールを持たないテストディレクトリは次の 2 つだけ。
- `tests/toolchain/`: ビルド構成（警告・VLA のエラー化、MSVC のオプション）そのものを検証する。CTest のラベルは `toolchain`。
- `tests/docs/`: README と `docs/` 直下の章を、ビルドした `monde` と照合する（`--help` の引用、クイックスタートのコマンド列、相対リンク）。あわせて `THIRD_PARTY_NOTICES`、`licenses/`、`src/` の ID の行の対応を確かめる。Naos を使わず、`docs_check.cmake` を `cmake -P` で起動する CTest のテスト（`docs_check.<help|quickstart|links|notices>`）。ラベルは `docs`。

### ビルド構成
**Location**: `CMakeLists.txt`、`cmake/`  
**Purpose**: コンパイラオプション（`monde_apply_compile_options`）、ターゲット作成関数（`monde_add_module` / `monde_add_module_tests` / `monde_add_executable`）、Naos の取得。オプションやターゲットの作り方はここに集約し、各モジュールの `CMakeLists.txt` では個別に設定しない。  
**Package**: `cmake/MondePackage.cmake` は配布物の唯一の定義（install の規則と CPack の設定）。アーカイブ `monde-X.Y.Z-<os>-<arch>` に入れるのは実行ファイル、`LICENSE`、`README.md`、`THIRD_PARTY_NOTICES` と `licenses/`（ディレクトリごと）だけで、対象名は `MONDE_RELEASE_TARGET` で上書きできる。バージョン番号を書く場所はルートの `CMakeLists.txt` の `project(monde VERSION ...)` だけ。

### 取り込んだ素材の表示
**Location**: `THIRD_PARTY_NOTICES`、`licenses/`（リポジトリの直下）  
**Purpose**: 実行ファイルに取り込んだ第三者の素材（生成したデータの表、取り込んだソース、静的にリンクするライブラリ）の表示。`THIRD_PARTY_NOTICES` は索引で、素材ごとに空行で区切った `ID`、`Name`、`Version`、`Source`、`License`（SPDX）、`License-File`、`Used-In` の塊を持つ。`licenses/<ID>.txt` は素材ごとに 1 つの、出典、版、著作権表示とライセンスの全文（ライセンスごとではなく素材ごとに分ける）。ソースと配布物で同じファイルを使い、`licenses/` には索引に載せたファイルだけを置く。取り込んだ先のファイル（`Used-In`）は先頭のコメントに ` * THIRD_PARTY_NOTICES: <ID>` の行を持ち、`tests/docs/` の `docs_check.notices` が対応を検査する。テスト専用の素材（`spec.txt`、Naos）は載せない。素材の追加・更新・削除の手順は `docs/dev/release.md`。

### 受け入れテストのデータ
**Location**: `tests/data/commonmark/`（markdown-parser が作り、monde-cli も使う）  
**Purpose**: CommonMark spec.txt などの外部テストデータ。ライセンス表記を添えて無改変で置くか、テスト時に取得する。リリース成果物には含めない。

### 開発用スクリプト
**Location**: `tools/`  
**Purpose**: Unicode のデータからの表の生成など、開発者が手で実行するスクリプト。ビルドとテストからは呼ばず、生成物をリポジトリに含める。  
**Example**: `tools/gen_po_ucd.py`（UCD から `src/po/po_ucd_tables.c` を生成する）。第三者の素材から生成する `tools/gen_*.py` は、生成したファイルの先頭のコメントに ID の行 ` * THIRD_PARTY_NOTICES: <ID>` を出力する（素材の ID はスクリプトの `NOTICES_ID`）  
**Exception**: `tools/release/` はリリースのワークフローと保守者が手元で `cmake -P` で実行する検査スクリプト（`check_version.cmake`: タグとビルド構成のバージョンの一致、`verify_package.cmake`: 実行ファイルと配布アーカイブの検証）。生成物は作らず、ビルドとテスト（CTest）からは呼ばない。

### ドキュメント
**Location**: `docs/`、`README.md`  
**Purpose**: 日本語の利用マニュアルと README。README は概要、インストール、クイックスタート、ビルド手順、ライセンス、サードパーティ表記を持ち、詳細はマニュアルに委ねる。  
**User chapters**: `docs/` 直下の `.md` が利用者向けの章。`docs/README.md`（目次）、`install.md`（インストール）、`workflow.md`（翻訳フロー）、`commands.md`（コマンドリファレンス）、`limitations.md`（既知の制限）。章の間は相対リンクで結び、`docs/dev/` と `docs/inputs/` へはリンクしない。`commands.md` のヘルプの引用と README のクイックスタートには `<!-- monde-check: ... -->` のマーカーを付け、`tests/docs/` が照合する。  
**Examples**: `docs/examples/` はクイックスタートの材料（`hello.md` と訳を入れた `hello.ja.po`）。CLI の抽出結果が変わったら `hello.ja.po` を合わせる。  
**Maintainer docs**: `docs/dev/` は保守者向け。`docs/dev/release.md` はバージョン番号の規則、リリースの手順、失敗時の対処、CLI の仕様が変わったときに同じ変更で直すドキュメントの一覧、取り込む素材を追加・更新・削除するときの手順を持つ。  
**Note**: `docs/inputs/` は要件定義用の入力資料（RFP、Q&A）。要件定義以外のフェーズでは参照しない。確定した内容は steering と各 spec の requirements.md に反映されている。

### CI とリリースのワークフロー
**Location**: `.github/workflows/`  
**Purpose**: `ci.yml` は main への push と pull request で動くビルドとテスト。`release.yml` はそれとは別のリリースのワークフローで、`vX.Y.Z` のタグの push または手動起動（入力 `tag`）で動き、`check`（タグとバージョンの一致）→ `build`（4 対象のビルド、`--version` とアーカイブの検証）→ `publish`（`SHA256SUMS` を作り GitHub Releases に添付）の 3 ジョブからなる。リリースの変更で `ci.yml` を変えない。

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
  - 短縮名は各モジュールの設計で決め、既存のものと重複させない。foundation が予約済み: `buf`、`diag`、`severity`、`utf8`、`eol`、`span`、`slice`、`source`、`file`、`wide`（ほかにマクロ `MND_PRINTF_LIKE` も foundation が使用している）。cli_args が予約済み: `args`、po-format が予約済み: `po`、po-merge が予約済み: `merge`、markdown が予約済み: `md`、yaml-front-matter が予約済み: `yaml`、translation-units が予約済み: `tu`、monde-cli が予約済み: `cli`
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
3. 製品の実行ファイルは `src/<実行ファイル名>/` に置き、`monde_add_executable` で作る（Naos へのリンクは構成時エラーになる）。

コンパイラオプションはこれらの関数が自動で適用する。

## Code Organization Principles

- **依存は一方向**: foundation ← 汎用パーサ（cli-args、po-format、markdown-parser、yaml-front-matter）← po-merge / translation-units ← monde-cli。逆向きの依存や、パーサ同士の横の依存を作らない。
- **共有する型は下位に置く**: ソース範囲や PO エントリのように複数の部品が使う型は、それを定義する下位の部品（foundation、po-format）に置き、上位で再定義しない。
- **I/O は境界に寄せる**: パーサや再構成器はメモリ上のバッファを入力・出力とし、ファイルの読み書きは foundation の共通部品と CLI 層が担う。これにより部品を単体でテストしやすくする。
- **エラーは診断として返す**: 部品は直接 `stderr` に書いたり `exit` したりせず、foundation の診断の仕組みで呼び出し側に返す。メッセージの表示と終了コードの決定は CLI 層が行う。

---
_Document patterns, not file trees. New files following patterns shouldn't require updates_
