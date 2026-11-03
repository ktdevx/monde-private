# Project Structure

> 現時点（2026-11-03）ではソースコードはまだなく、以下は roadmap と project-foundation の方針に基づく予定の構成である。実装が始まったら、実際のパターンに合わせて更新する。

## Organization Philosophy

**部品（モジュール）単位の構成**。再利用できる部品ごとに 1 つのディレクトリと 1 つの CMake ライブラリターゲットを持ち、ソースとテストを同じモジュール名で対にする。依存は下位の部品へ向かう一方向とし、汎用部品から Monde 固有の部品（`translation-units`、CLI）へ依存しない。

## Directory Patterns

### モジュールのソース
**Location**: `src/<module>/`  
**Purpose**: 1 つの部品の実装とヘッダ。1 モジュール = 1 ライブラリターゲット。  
**Example**: `src/po/`（PO の読み書き）、`src/markdown/`（CommonMark パーサ）、`src/foundation/`（共通ユーティリティ）

### モジュールのテスト
**Location**: `tests/<module>/`  
**Purpose**: 対応するモジュールの Naos テスト。CTest に登録する。  
**Example**: `tests/po/` は `src/po/` をテストする

### 受け入れテストのデータ
**Location**: テスト用の独立したディレクトリ（名前は monde-cli の設計で確定）  
**Purpose**: CommonMark spec.txt などの外部テストデータ。ライセンス表記を添えて無改変で置くか、テスト時に取得する。リリース成果物には含めない。

### ドキュメント
**Location**: `docs/`、`README.md`  
**Purpose**: 日本語の利用マニュアルと README。  
**Note**: `docs/inputs/` は要件定義用の入力資料（RFP、Q&A）。要件定義以外のフェーズでは参照しない。確定した内容は steering と各 spec の requirements.md に反映されている。

### 仕様とプロジェクトメモリ
**Location**: `.kiro/specs/<feature>/`、`.kiro/steering/`  
**Purpose**: spec 駆動開発の仕様（brief → requirements → design → tasks）と、プロジェクト全体の方針。spec 名は kebab-case（例: `markdown-parser`、`yaml-front-matter`）。

## Naming Conventions

- **ディレクトリ・モジュール名**: 小文字。spec 名（kebab-case）とソースのモジュール名の対応は project-foundation の設計で確定する。
- **ファイル**: 小文字の snake_case（`.c` / `.h`）。
- **関数・型・変数**: snake_case。C には名前空間がないため、外部から見えるシンボルにはモジュール名の接頭辞を付ける（具体的な接頭辞の規則は project-foundation の設計で確定する）。
- **モジュール内部の関数**: `static` にしてファイル外に公開しない。

## Include Organization

```c
#include "po/po_entry.h"   /* 他モジュールのヘッダは src/ からのパスで指定 */
#include "local_helper.h"  /* 同じモジュール内のヘッダ */
#include <stdio.h>         /* 標準ライブラリ */
```

- 各モジュールの公開ヘッダは、依存する他のモジュールからのみ include する。依存関係は CMake の `target_link_libraries` で明示する。
- （include パスの基点は project-foundation の設計で確定する）

## Code Organization Principles

- **依存は一方向**: foundation ← 汎用パーサ（cli-args、po-format、markdown-parser、yaml-front-matter）← po-merge / translation-units ← monde-cli。逆向きの依存や、パーサ同士の横の依存を作らない。
- **共有する型は下位に置く**: ソース範囲や PO エントリのように複数の部品が使う型は、それを定義する下位の部品（foundation、po-format）に置き、上位で再定義しない。
- **I/O は境界に寄せる**: パーサや再構成器はメモリ上のバッファを入力・出力とし、ファイルの読み書きは foundation の共通部品と CLI 層が担う。これにより部品を単体でテストしやすくする。
- **エラーは診断として返す**: 部品は直接 `stderr` に書いたり `exit` したりせず、foundation の診断の仕組みで呼び出し側に返す。メッセージの表示と終了コードの決定は CLI 層が行う。

---
_Document patterns, not file trees. New files following patterns shouldn't require updates_
