# 調査と設計判断の記録: markdown-parser

## Summary
- **Feature**: `markdown-parser`
- **Discovery Scope**: New Feature（新しいモジュール `src/markdown/`。既存のモジュールは変更しない）
- **Key Findings**:
  - CommonMark spec 0.31.2 の spec.txt（GitHub のタグ `0.31.2`）は 205,025 バイト、例は 652 個ある。先頭の YAML メタデータに CC-BY-SA 4.0 のライセンスが書かれている。spec.commonmark.org で配布される spec.txt にはこのメタデータがないため、GitHub のタグのものを無改変で置く。
  - 公式のテストランナー（`test/spec_tests.py`）は、例の中の `→` をタブに置き換えてから、`normalize.py` で両方の HTML を正規化して比べる。この正規化は Python の `html.parser` に依存するため、C に移植するのは近似にとどまる。そこで、まず完全一致で比べ、一致しない場合だけ正規化して比べる方式にする。
  - 0.31 では、Unicode の句読点の定義が「一般カテゴリ P または S」に広がった。そのため、強調の左右の隣接の判定には、P と S の表が必要になる。リンクラベルの照合には Unicode の大文字小文字の畳み込み（CaseFolding の C と F）が、実体参照の認識には HTML5 の実体名の一覧が必要になる。これらは、po-format と同じ方式（`tools/` のスクリプトで生成し、生成物をリポジトリに含める）で用意する。

## Research Log

### CommonMark spec 0.31.2 とテストデータ
- **Context**: 要件 9（全例で期待 HTML と一致すること）の入力と、ライセンス上の扱いを確認した。
- **Sources Consulted**: https://raw.githubusercontent.com/commonmark/commonmark-spec/0.31.2/spec.txt 、https://spec.commonmark.org/0.31.2/spec.txt 、`test/spec_tests.py`、`test/normalize.py`（同じタグ）
- **Findings**:
  - タグ `0.31.2` の spec.txt: 205,025 バイト、SHA-256 `257c41ad946f7a1414a499aca402a1aa8fdac3678532266611348c1cf54f4b80`、例は 652 個。先頭のメタデータ: `title: CommonMark Spec`、`version: '0.31.2'`、`date: '2024-01-28'`、`license: '[CC-BY-SA 4.0](...)'`。
  - 例の書式: 32 個のバッククォートと ` example` の行で始まる。`.` だけの行で Markdown と HTML を区切り、32 個のバッククォートだけの行で終わる。直前の `#` の見出しが節の名前になる。`→` はタブを表す。
  - spec_tests.py は、正規化のあと、`actual == expected` で比べる（正規化しない `--no-normalize` もある）。
  - normalize.py の要点: `pre` の外では連続する空白を 1 つにまとめる。ブロックレベルのタグの前後の空白を除く。属性を名前順に並べ、値を `html.escape(quote=True)` で書き直す（`href` の特別扱いは、キーではなく値と比べる誤りがあるため、実際には働かない）。実体参照は HTML4 の名前（`name2codepoint`）だけを文字に戻し、`< > & "` は再びエスケープする。`br` の直後のデータの先頭の改行を除く。
- **Implications**: spec.txt は `tests/data/commonmark/spec.txt` に無改変で置き、`README.md` に出典・SHA-256・ライセンスを書く。チェックアウトで改行が変わらないように `.gitattributes` で `-text` を指定する。比べ方は「完全一致 → 一致しなければ正規化して比べる」とし、正規化は normalize.py の規則を C に移植する（テスト専用）。

### 文字の分類と Unicode のデータ
- **Context**: 強調の判定、ラベルの照合、実体参照に必要なデータを確認した。
- **Sources Consulted**: spec.txt の「Characters and lines」「Entity and numeric character references」「Link reference definitions」、UCD 15.0.0（`extracted/DerivedGeneralCategory.txt`、`CaseFolding.txt`）、WHATWG の `entities.json`
- **Findings**:
  - Unicode の空白: 一般カテゴリ Zs と、タブ・LF・FF・CR。Unicode の句読点: 一般カテゴリ P または S。
  - ラベルの照合: Unicode の大文字小文字の畳み込みを行い、連続する空白を 1 つにまとめ、前後の空白を除く。参照実装（cmark、commonmark.js）は完全な畳み込み（C と F）を使い、`ẞ` と `SS` は一致する（例 540）。
  - 実体参照: HTML5 の実体名のうち、`;` で終わるもの（約 2,125 個）だけを認識する。1 つの名前が 2 つのコードポイントを表すことがあり、UTF-8 では最大で 6 バイトになる。
  - po-format は UCD 15.0.0 を使っている。モジュール間で横の依存を作らない規約があるため、po の表は使えない。
- **Implications**: `tools/gen_md_ucd.py`（UCD 15.0.0 → `md_unicode_tables.c`）と `tools/gen_md_entities.py`（`entities.json` → `md_entity_table.c`）を作り、生成物をリポジトリに含める。Unicode のバージョンは po と揃えて 15.0.0 にする。

### 参照実装の構成
- **Context**: 2 段階の解析と、方言の拡張点の形を決めるために、既存の実装を確認した（コードは取り込まない）。
- **Sources Consulted**: spec.txt の「Appendix: A parsing strategy」、cmark と commonmark.js の構成、cmark-gfm の構文拡張の仕組み（`cmark_syntax_extension`）
- **Findings**:
  - ブロックの段階: 1 行ずつ、開いているコンテナの継続を確かめ、新しいブロックの開始を試し、遅延継続行を判定し、行をリーフに追加する。閉じるときに、段落からリンク参照定義を取り出す。
  - インラインの段階: 区切り記号のスタックと角括弧のスタックを使う。リンクの解決は、閉じ角括弧の位置で行う。強調の処理（process emphasis）は、`openers_bottom` で探索の範囲を限り、二次的な時間がかからないようにする。
  - cmark-gfm の拡張は、ブロックの開始（`try_opening_block`。開いている段落を受け取って表に変えられる）、継続（`match_block`）、閉じるときの後処理、インラインの開始文字と認識（`match_inline`）、区切り記号の対応づけ（`insert_inline_from_delim`）、木全体の後処理（`postprocess`。自動リンクの拡張で使う）という拡張点を持つ。GFM の 4 つの拡張は、これらで実現されている。
- **Implications**: 方言の拡張点を cmark-gfm と同じ 5 種類（ブロックの開始、継続、閉じる処理、インラインの規則（開始文字形式と区切り記号形式）、後処理）にする。次の版の GFM が必要とする拡張点を、すでに実績のある形で揃えられる。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| 2 段階の解析（spec の付録の方式） | 行単位のブロック解析のあと、リーフの内容をインライン解析する | CommonMark の定義と同じ順序で判定できる。参照実装と比べやすい | ソース範囲を保つため、行の対応表を持つ必要がある | **採用** |
| 1 段階の再帰下降 | ブロックとインラインを 1 回の走査で解析する | 実装が短く見える | 遅延継続行と、後ろで定義されるリンク参照を正しく扱えない | 不採用 |
| 既存の C 実装（cmark、MD4C）を取り込む | ライブラリをソースごと取り込み、範囲の情報を足す | 正しさの実績がある | 外部依存ゼロの方針（steering）に反する。行ごとのプレフィックスの範囲を返せず、大きく改造することになる | 不採用（自前で実装する） |

## Design Decisions

### Decision: 構文木のメモリをアリーナにまとめる
- **Context**: 8.2（メモリ不足のときに、確保したものをすべて解放する）と 8.5（構文木の解放）を、確実かつ簡単に満たしたい。入れ子が深い入力（8.3）では、再帰による解放は使えない。
- **Alternatives Considered**:
  1. ノードごとに `malloc` し、木をたどって解放する
  2. 文書ごとのアリーナに確保し、アリーナごと解放する
- **Selected Approach**: 2。作業用の配列（開いているブロックの列、区切り記号のスタックなど）は確保関数から直接確保し、解析の終わりに解放する。
- **Rationale**: 解放が木の形に依存せず、失敗した途中からでも同じ手順で片付けられる。
- **Trade-offs**: 個々のノードを解放できない（不要）。行の配列を伸ばすと古い領域が無駄になるが、行の配列は作業用の配列で組み立て、閉じるときにアリーナへ 1 回だけ複製する。
- **Follow-up**: メモリ不足を注入するテストで、すべての確保の失敗について、解放漏れがないことを確かめる。

### Decision: 確保関数をオプションで差し替えられるようにする
- **Context**: 8.2 をテストで確かめるには、n 回目の確保を失敗させる必要がある。
- **Alternatives Considered**:
  1. テスト用のグローバル変数でフックする
  2. 解析のオプションで確保関数を渡す（省略すると `malloc` と `free` を使う）
- **Selected Approach**: 2。
- **Rationale**: グローバルな状態を持たずに済み、解析を再入可能に保てる。
- **Trade-offs**: 公開の型が 1 つ増える。

### Decision: 行ごとの内容範囲を、プレフィックスと内容の連続した 2 つの範囲で表す
- **Context**: 要件 4。translation-units は、msgid を作るときにプレフィックスを除き、差し戻すときに復元する。
- **Selected Approach**: 各行を `prefix`（物理的な行の先頭から内容の開始まで）と `content`（内容の開始から行末記号の前まで）で表す。`prefix.end == content.start` とし、行末記号の長さ、補う空白の数（タブの一部だけが取り除かれた場合）、遅延継続行かどうかを添える。リスト項目の最初の行では、リスト記号もプレフィックスに入る。
- **Rationale**: 範囲置換に必要なのは内容の範囲だけで、プレフィックスは原文のまま残る。2 行目以降の書き方は、リスト項目の `content_indent` と引用の有無から、translation-units が組み立てられる。
- **Trade-offs**: 「どのコンテナがどのバイトを使ったか」の内訳は返さない。必要になったら、行に内訳を加える（Revalidation Trigger）。

### Decision: インラインのノードを原文のバイトオフセットで表し、行をまたぐ内容は行の表で解釈する
- **Context**: 5.10。コードスパンやリンクのタイトルは、行をまたげる。その間には、次の行のプレフィックス（`>` など）がある。
- **Selected Approach**: インラインの解析は、リーフの行の内容を順に読むカーソルの上で行い、すべての位置を原文のオフセットで記録する。行の境目は、論理的には 1 つの改行として扱う。行をまたぐ範囲の論理的な文字列は、`mnd_md_logical_text`（プレフィックスを飛ばし、行末記号を `\n` にする）で得る。
- **Rationale**: 範囲置換と行番号の表示の両方に、原文の位置をそのまま使える。テスト用の HTML 出力も同じ関数を使うため、範囲の誤りが HTML の不一致として現れる。

### Decision: 方言の拡張点は cmark-gfm と同じ形にする
- **Context**: 要件 7 と、次の版の GFM（表、タスクリスト、取り消し線、自動リンクの拡張）。
- **Alternatives Considered**:
  1. CommonMark の規則そのものを表にして差し替える
  2. 追加の規則だけを登録する拡張点（cmark-gfm の方式）
- **Selected Approach**: 2。ブロックの規則（開始、継続、閉じる処理）、インラインの規則（開始文字形式、区切り記号形式）、木全体の後処理。
- **Rationale**: GFM の 4 つの拡張は、すべてこの拡張点で実現されている。CommonMark の規則は差し替えないため、何も追加しない方言は、CommonMark と同じ木になる（7.5）。
- **Trade-offs**: CommonMark の規則を無効にする方言（例: インデントコードブロックを持たない方言）は作れない。現在の要件にはない。

### Decision: 一般化・採用・単純化の見直し
- **Generalization**: 「行の上の範囲」はすべてのリーフで同じ形（`mnd_md_line_t`）にし、段落・見出し・コード・HTML・リンク参照定義・方言のリーフで共有する。リンクと画像は同じデータの形にし、`text` がリンクテキストまたは alt を指す。
- **Build vs. Adopt**: パーサは自前で実装する（steering の外部依存ゼロ）。spec のデータ（spec.txt、normalize.py の規則）、UCD、HTML5 の実体名は、外部の標準をそのまま採用する。拡張点の形は cmark-gfm を参考にする。
- **Simplification**: HTML 出力は製品に入れず、テストにだけ置く。方言の登録の仕組み（名前による検索など）は作らず、呼び出し側が方言の構造体を直接渡す。ノードの削除は提供しない（後処理での置き換えは、分割と挿入で足りる）。

## Risks & Mitigations
- normalize.py の C への移植が、Python の `html.parser` と完全には同じにならない — まず完全一致で比べる。正規化は両方の HTML に同じように行うため、ずれがあっても判定が甘くなることはない。完全一致しない例の数を、テストのメモに出す。
- 病的な入力（多数の `[`、`*`、深い入れ子）で時間がかかりすぎる — 参照実装と同じ対策をとる。リンクラベルを 999 文字までに限り、角括弧が使えなくなったことを記録し、強調の探索の下限を設ける。入れ子は再帰を使わずに扱う。病的な入力のテストを置く。
- タブの扱い（タブの一部だけを取り除く場合）を誤る — spec の「Tabs」の節の全例と、行ごとの範囲の個別のテストで確かめる。
- 単独の CR を含む文書で、行番号と CommonMark の行がずれる — 要件どおりの挙動として、テストで固定する。

## References
- [CommonMark Spec 0.31.2](https://spec.commonmark.org/0.31.2/) — 仕様本体
- [commonmark-spec タグ 0.31.2](https://github.com/commonmark/commonmark-spec/tree/0.31.2) — spec.txt、`test/spec_tests.py`、`test/normalize.py`
- [Unicode Character Database 15.0.0](https://www.unicode.org/Public/15.0.0/ucd/) — `DerivedGeneralCategory.txt`、`CaseFolding.txt`
- [WHATWG HTML の実体名の一覧](https://html.spec.whatwg.org/entities.json) — 実体参照の表の生成元
- [cmark-gfm](https://github.com/github/cmark-gfm) — 構文拡張の拡張点の参考

## タスク生成時の補足（設計の小さな明確化）
- 並行して実装できるように、内部の宣言を部品ごとの内部ヘッダ（`md_lines.h`、`md_scan.h` など）に分けた。`md_internal.h` には、アリーナ、ノードの作成と連結、解析の作業状態の型だけを置く。テストファイルも部品ごとに分けた。
- 方言の後処理のあとの範囲の検査は、製品側（`md_dialect.c`）で行う（9.6 により、テストの tree_check は使えない）。メッセージは `dialect "%s": postprocess produced an invalid tree` とした。
- normalize.py が HTML4 の実体名だけを文字に戻す処理は、本モジュールの HTML5 の表で代用する。正規化は両方の HTML に同じように行うため、判定が甘くなることはない。
