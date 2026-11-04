# Requirements Document

## Project Description (Input)
**誰が困っているか**: Monde の開発者（とくに translation-units と、次の版で GFM 対応を実装する開発者）。Monde は Markdown 文書から翻訳すべきブロックを正確に特定し、その原文上の位置（バイト範囲）を知る必要がある。構造を正しく認識できないと、コードブロックを翻訳対象にしてしまったり、訳文の差し戻しで書式を壊したりする。既存の Markdown ライブラリは使えない。

**現状**: Markdown パーサはまだない。

**何を変えるか**:
- CommonMark 0.31.2 のブロック構造を正しく解析し、各ブロックとその内容のソース範囲を返す再利用部品を自前で実装する。CommonMark 仕様の 2 段階方式（ブロック構造の解析 → インライン解析）に沿う。
- 各ノードに、原文バッファ上のバイト範囲と行番号を持たせる。複数行にわたるブロックについては、リスト項目や引用の継続行にあるプレフィックス（`>` やインデント）を除いた、行ごとの内容範囲を保持する。
- 翻訳に必要な範囲でインラインも解析する。
- 次の版で GFM を追加できるように、方言ごとの拡張点（ブロックとインラインの規則の追加）をインターフェースとして定義する（既定は CommonMark）。
- 対象範囲:
  - CommonMark の全ブロック要素: 見出し（ATX / Setext）、段落、リスト、引用、フェンスコードブロックとインデントコードブロック、HTML ブロック、テーマ区切り、リンク参照定義
  - Monde が必要とするインライン情報: リンクと画像の URL・タイトル・alt の範囲、コードスパン、生の HTML など
  - ノードのソース範囲と、行ごとの内容範囲
  - 方言インターフェース
  - CommonMark spec のテストケースによる構造の検証
- 対象外: HTML へのレンダリング、GFM の拡張（次の版）、翻訳単位を決める処理（translation-units）、訳文の差し戻し、フロントマター（yaml-front-matter が検出する）
- 依存関係: 上流は project-foundation（バッファ、ソース範囲の型）。下流は translation-units（構文木とソース範囲を利用する）と次の版の GFM 対応。
- 制約:
  - 受け入れ基準（spec.txt のラウンドトリップ）に直結するため、CommonMark 0.31.2 に厳密に従う。
  - spec.txt は CC-BY-SA 4.0 であり、取り扱いは roadmap の制約に従う。
  - インライン要素を msgid に含めつつ、URL を翻訳対象外とし、画像の alt を翻訳対象とする扱い（Q2-1、Q2-2）は、translation-units と要件を揃える。

## Requirements
<!-- Will be generated in /kiro-spec-requirements phase -->
