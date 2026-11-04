# Requirements Document

## Project Description (Input)
**対象者**: monde で Markdown 文書を翻訳・保守する翻訳者と、`monde update` を実装する monde-cli の開発者。

**現状**: 原文 Markdown が更新されると翻訳テンプレート（`.pot`）の msgid も変わるが、既存の翻訳ファイル（`.po`）を新しいテンプレートへマージする部品がまだない。gettext の `msgmerge` は外部依存になるため使えない。

**変えたいこと**: po-format のデータモデルの上で、新しい `.pot` と既存の `.po` を入力として `msgmerge` と同等の結果となる更新後の `.po` を生成する再利用部品を用意する。マージは次の 3 段階で行う。
1. 完全一致（msgctxt + msgid）による翻訳の引き継ぎ
2. あいまい一致（文字列の類似度にしきい値を設ける）による fuzzy での引き継ぎ。前回の msgid（`#|`）を付与する
3. テンプレートにないエントリの廃止エントリ化（`#~`）と、廃止エントリが再び出現したときの復活

あわせて、翻訳者コメントの保持、参照コメントと抽出コメントのテンプレートからの更新、ヘッダの更新（POT-Creation-Date など）、マージ結果の統計（一致・fuzzy・新規・廃止の件数）を扱う。

**範囲外**: PO ファイルの入出力（po-format）、CLI のオプション（monde-cli）、翻訳メモリや機械翻訳との連携、複数形エントリの本格的なマージ（データの保持にとどめる）。

**制約**: `msgmerge` と同等の振る舞いを目標とし、テストでは `msgmerge` の出力と比較できるケースを用意する。類似度のアルゴリズムと件数が多いときの計算量の扱いは設計時に決める。一般的な文書の規模で体感上すぐ終わること。

**関連**: Upstream は po-format、Downstream は monde-cli（`update` サブコマンド）。

## Requirements
<!-- Will be generated in /kiro-spec-requirements phase -->
