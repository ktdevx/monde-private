# Brief: monde-cli

## Problem
利用者は、新規翻訳と原文更新の 2 つのフロー（RFP §3）を、1 つのコマンド `monde` で実行したい。個々の部品ができていても、サブコマンドとして統合し、受け入れ基準で検証しなければ、ツールとして使えない。

## Current State
cli-args、po-merge、translation-units などの部品が提供される予定である。実行ファイルはまだない。

## Desired Outcome
- 次の 4 つのサブコマンドが動作する。

  | コマンド | 入力 → 出力 |
  | --- | --- |
  | `monde extract` | `.md` → `.pot` |
  | `monde init` | `.pot` → `.po` |
  | `monde update` | `.pot` + `.po` → `.po` |
  | `monde translate` | `.md` + `.po` → `.md` |

- CommonMark spec の全テストケースで、「extract → translate（翻訳なし）」の結果が原文と一致する。
- 生成した `.pot` / `.po` が `msgfmt --check` を通ることを CI で確認している。

## Approach
- cli-args でサブコマンドとオプションを宣言し、各サブコマンドのハンドラから部品を呼び出す薄い統合層にする。
- 受け入れテストでは、spec のテストケースを入力にしたデータ駆動テストを Naos と CTest で実行する。

## Scope
- **In**:
  - 4 サブコマンドの実装（1 ファイル単位）
  - 入出力ファイルの指定（標準入出力に対応するかは要件で決める）
  - `--front-matter-keys`
  - fuzzy の翻訳を採用するオプション
  - 言語の指定（`init`）
  - 翻訳率などの統計の表示
  - 終了コードの体系、英語のエラーメッセージ
  - CommonMark spec 0.31.2 によるラウンドトリップの受け入れテスト
  - CI への `msgfmt --check` ジョブの追加（少なくとも Ubuntu）
- **Out**: 部品のロジック、日本語のドキュメント、リリースの配布（docs-and-release が担う）

## Boundary Candidates
- サブコマンドの定義とディスパッチ
- 各サブコマンドのハンドラ（部品の呼び出しとファイル入出力）
- 受け入れテストと互換性検証のハーネス

## Out of Boundary
- 複数ファイルやディレクトリの一括処理、設定ファイル
- 翻訳率による出力の制御

## Upstream / Downstream
- **Upstream**: cli-args、po-format、po-merge、translation-units（間接的に markdown-parser、yaml-front-matter）
- **Downstream**: docs-and-release

## Existing Spec Touchpoints
- **Extends**: なし
- **Adjacent**: cli-args（宣言の仕組み）、translation-units（抽出と再構成の API）

## Constraints
- コマンドライン引数のみで指定する。メッセージは英語。
- spec.txt はライセンス上の理由から、無改変のまま独立したディレクトリに置くか、テスト時に取得する。リリース成果物には含めない。
- 改行コードは入力に合わせる。BOM 付きの入力も受け付ける。
