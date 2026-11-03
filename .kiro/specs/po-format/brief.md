# Brief: po-format

## Problem
Monde は翻訳テンプレート（`.pot`）と翻訳ファイル（`.po`）を読み書きする必要がある。また、生成するファイルは GNU gettext のツールや既存の翻訳プラットフォームと互換でなければならない。外部ライブラリ（libgettextpo など）は使えない。

## Current State
PO を扱う部品はまだない。

## Desired Outcome
- `.po` / `.pot` を読み込んでエントリの列に変換し、そのエントリから書き出せる再利用部品がある。
- 書き出したファイルは `msgfmt --check` を通る。
- 一般的な PO ファイルを読んで書き直しても、意味が変わらない。

## Approach
PO のエントリ（msgctxt、msgid、msgstr、各種コメント、フラグ、廃止エントリ）をデータモデルとして定義する。そのうえで、字句解析と構文解析を行うリーダーと、gettext の慣例に沿って整形するライターを実装する。

## Scope
- **In**:
  - ヘッダエントリ（Content-Type、Language など）の解析と生成
  - コメントの読み書き: 翻訳者コメント `#`、抽出コメント `#.`、参照 `#:`、フラグ `#,`、前回の msgid `#|`
  - fuzzy フラグ、廃止エントリ `#~`、msgctxt
  - 文字列のエスケープとアンエスケープ、複数行の文字列の連結と折り返し
  - 複数形エントリ（`msgid_plural` / `msgstr[n]`）を読み込んでもエラーにせず、保持して書き戻す
  - 解析エラーの位置（行番号）の報告
  - `.pot` から `.po` を新規作成する初期化処理（ヘッダの Language の設定など、`msginit` 相当の最小限）
- **Out**: マージ処理（po-merge）、Markdown から msgid を作る処理（translation-units）

## Boundary Candidates
- PO のデータモデル
- リーダー（パーサ）
- ライター（整形）
- `.pot` から `.po` への初期化

## Out of Boundary
- あいまい一致を含むマージ
- `.mo` のコンパイル

## Upstream / Downstream
- **Upstream**: project-foundation
- **Downstream**: po-merge、translation-units、monde-cli

## Existing Spec Touchpoints
- **Extends**: なし
- **Adjacent**: po-merge（データモデルを共有する）

## Constraints
- UTF-8 のみ。互換性は GNU gettext の `msgfmt --check` で確認する。
- 出力は gettext ツールの出力形式（行の折り返しなど）に近づけ、差分を読みやすくする。
