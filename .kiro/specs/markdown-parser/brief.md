# Brief: markdown-parser

## Problem
Monde は、Markdown 文書から翻訳すべきブロックを正確に特定し、その原文上の位置（バイト範囲）を知る必要がある。構造を正しく認識できないと、コードブロックを翻訳対象にしてしまったり、訳文の差し戻しで書式を壊したりする。既存の Markdown ライブラリは使えないため、CommonMark パーサを自前で実装する。

## Current State
パーサはまだない。

## Desired Outcome
- CommonMark 0.31.2 のブロック構造を正しく解析し、各ブロックとその内容のソース範囲を返す再利用部品がある。
- 翻訳に必要な範囲でインラインも解析する。
- 次の版で GFM を追加できるように、方言を差し替えるインターフェースを持つ。

## Approach
- CommonMark 仕様の 2 段階方式（ブロック構造の解析 → インライン解析）に沿って実装する。
- 各ノードには、原文バッファ上のバイト範囲と行番号を持たせる。
- 複数行にわたるブロックについては、内容がどの行のどの位置から始まるかを保持する。たとえば、リスト項目や引用の継続行にあるプレフィックス（`>` やインデント）を除いた内容の範囲である。
- 方言ごとの拡張点（ブロックとインラインの規則の追加）をインターフェースとして定義する。

## Scope
- **In**:
  - CommonMark の全ブロック要素: 見出し（ATX / Setext）、段落、リスト、引用、フェンスコードブロックとインデントコードブロック、HTML ブロック、テーマ区切り、リンク参照定義
  - Monde が必要とするインライン情報: リンクと画像の URL・タイトル・alt の範囲、コードスパン、生の HTML など
  - ノードのソース範囲と、行ごとの内容範囲
  - 方言インターフェース（既定は CommonMark）
  - CommonMark spec のテストケースによる構造の検証
- **Out**:
  - HTML へのレンダリング
  - GFM の拡張（次の版）
  - 翻訳単位を決める処理（translation-units）
  - フロントマター（yaml-front-matter が検出する）

## Boundary Candidates
- ブロック構造の解析
- インライン解析（必要な範囲）
- 構文木とソース範囲のデータモデル
- 方言の拡張インターフェース

## Out of Boundary
- どのノードを翻訳単位とするかの判断
- 訳文の差し戻し

## Upstream / Downstream
- **Upstream**: project-foundation（バッファ、ソース範囲の型）
- **Downstream**: translation-units、次の版の GFM 対応

## Existing Spec Touchpoints
- **Extends**: なし
- **Adjacent**: translation-units（構文木とソース範囲を利用する）

## Constraints
- 受け入れ基準（spec.txt のラウンドトリップ）に直結するため、CommonMark 0.31.2 に厳密に従う。
- spec.txt は CC-BY-SA 4.0 であり、取り扱いは roadmap の制約に従う。
- インライン要素を msgid に含めつつ、URL を翻訳対象外とし、画像の alt を翻訳対象とする扱い（Q2-1、Q2-2）は、translation-units と要件を揃える。
