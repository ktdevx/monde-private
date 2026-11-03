# Requirements Document

## Project Description (Input)
**誰が困っているか**: Monde の開発者（とくに po-merge、translation-units、monde-cli を実装する開発者）。Monde は翻訳テンプレート（`.pot`）と翻訳ファイル（`.po`）を読み書きする必要があり、生成するファイルは GNU gettext のツールや既存の翻訳プラットフォームと互換でなければならない。外部ライブラリ（libgettextpo など）は使えない。

**現状**: PO を扱う部品はまだない。

**何を変えるか**:
- `.po` / `.pot` を読み込んでエントリの列に変換し、そのエントリから書き出せる再利用部品を用意する。書き出したファイルは `msgfmt --check` を通り、一般的な PO ファイルを読んで書き直しても意味が変わらない。
- PO のエントリ（msgctxt、msgid、msgstr、各種コメント、フラグ、廃止エントリ）をデータモデルとして定義し、字句解析と構文解析を行うリーダーと、gettext の慣例に沿って整形するライターを実装する。
- 対象範囲:
  - ヘッダエントリ（Content-Type、Language など）の解析と生成
  - コメントの読み書き: 翻訳者コメント `#`、抽出コメント `#.`、参照 `#:`、フラグ `#,`、前回の msgid `#|`
  - fuzzy フラグ、廃止エントリ `#~`、msgctxt
  - 文字列のエスケープとアンエスケープ、複数行の文字列の連結と折り返し
  - 複数形エントリ（`msgid_plural` / `msgstr[n]`）を読み込んでもエラーにせず、保持して書き戻す
  - 解析エラーの位置（行番号）の報告
  - `.pot` から `.po` を新規作成する初期化処理（ヘッダの Language の設定など、`msginit` 相当の最小限）
- 範囲外: マージ処理（あいまい一致を含む。po-merge が担う）、Markdown から msgid を作る処理（translation-units が担う）、`.mo` のコンパイル。

**制約**: UTF-8 のみ。互換性は GNU gettext の `msgfmt --check` で確認する。出力は gettext ツールの出力形式（行の折り返しなど）に近づけ、差分を読みやすくする。上流は project-foundation、データモデルは po-merge と共有する。

## Requirements
<!-- Will be generated in /kiro-spec-requirements phase -->
