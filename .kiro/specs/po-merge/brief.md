# Brief: po-merge

## Problem
原文 Markdown が更新されると、翻訳テンプレートの msgid も変わる。既存の翻訳ファイルを捨てずに、変更のない箇所の翻訳は引き継ぎ、似た箇所は fuzzy として再利用し、不要になった訳は廃止エントリとして残す必要がある（`monde update`）。

## Current State
マージの部品はまだない。gettext の `msgmerge` は外部依存になるため使えない。

## Desired Outcome
新しい `.pot` と既存の `.po` を入力として、`msgmerge` と同等の結果となる更新後の `.po` を生成できる再利用部品がある。

## Approach
- po-format のデータモデルの上で、次の 3 段階でマージする。
  1. 完全一致（msgctxt + msgid）
  2. あいまい一致（文字列の類似度にしきい値を設ける）
  3. 廃止エントリ化
- あいまい一致の候補には fuzzy フラグと前回の msgid（`#|`）を付ける。
- 類似度のアルゴリズムと、件数が多いときの計算量の扱いは設計時に決める。

## Scope
- **In**:
  - 完全一致による翻訳の引き継ぎ
  - あいまい一致による fuzzy での引き継ぎ（`#|` の付与を含む）
  - テンプレートにないエントリの廃止エントリ化（`#~`）と、廃止エントリが再び出現したときの復活
  - 翻訳者コメントの保持、参照コメントと抽出コメントのテンプレートからの更新
  - ヘッダの更新（POT-Creation-Date などをテンプレートから更新する）
  - マージ結果の統計（一致、fuzzy、新規、廃止の件数）
- **Out**: PO ファイルの入出力（po-format）、CLI のオプション（monde-cli）

## Boundary Candidates
- 完全一致のマッチング
- 類似度の計算とあいまい一致
- 廃止エントリの管理

## Out of Boundary
- 翻訳メモリや機械翻訳との連携
- 複数形エントリの本格的なマージ（データの保持にとどめる）

## Upstream / Downstream
- **Upstream**: po-format
- **Downstream**: monde-cli（`update` サブコマンド）

## Existing Spec Touchpoints
- **Extends**: なし
- **Adjacent**: po-format（データモデル）

## Constraints
- 結果は `msgmerge` と同等の振る舞いを目標とする。テストでは `msgmerge` の出力と比較できるケースを用意するとよい。
- 性能の目標は特に設けないが、一般的な文書の規模で体感上すぐ終わること。
