# Brief: docs-and-release

## Problem
利用者は、`monde` を入手し、翻訳フローでの使い方を日本語で理解する必要がある。RFP は、日本語のドキュメントと、GitHub での配布を成果物として求めている。

## Current State
README.md は最小限の内容しかない。利用マニュアルとリリースのワークフローはない。

## Desired Outcome
- 日本語の README と利用マニュアルで、インストール、ソースからのビルド、各サブコマンドとオプション、2 つの翻訳フローの手順がわかる。
- タグを付けると、GitHub Releases に各 OS 向けのビルド済みバイナリが公開される。

## Approach
- 利用マニュアルは、monde-cli で確定したサブコマンドとオプションの仕様から作る。
- リリースは、GitHub Actions のタグをトリガーにしたワークフローで、各 OS 向けにビルドしてアーカイブを作り、Release にアップロードする。

## Scope
- **In**:
  - README.md の更新（概要、インストール、クイックスタート、ビルド手順、ライセンス、spec.txt などのサードパーティ表記）
  - 利用マニュアル（`docs/` 配下）
  - リリースワークフロー（Windows、macOS、Linux 向けのバイナリとアーカイブ）
  - バージョン番号の付け方
- **Out**: パッケージマネージャーへの登録、英語版のドキュメント

## Boundary Candidates
- 利用者向けのドキュメント
- リリースの自動化

## Out of Boundary
- 機能の追加や変更
- 開発者向けの内部設計ドキュメント（各 spec の design.md が担う）

## Upstream / Downstream
- **Upstream**: monde-cli（確定した CLI 仕様）、project-foundation（ビルドと CI）
- **Downstream**: なし（次の版の GFM 対応でドキュメントを更新する）

## Existing Spec Touchpoints
- **Extends**: なし
- **Adjacent**: project-foundation（CI ワークフロー）

## Constraints
- ドキュメントは日本語で書く。
- CC-BY-SA 4.0 のテストデータ（spec.txt）はリリース成果物に含めない。
- 著作権者は The Monde Authors、ライセンスは MIT とする。
