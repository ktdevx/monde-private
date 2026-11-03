# 調査と設計判断: po-format

## Summary
- **Feature**: `po-format`
- **Discovery Scope**: New Feature（新しいモジュール `po`。既存の foundation と CMake 関数の上に積む）
- **Key Findings**:
  - GNU gettext（msgcat 0.21、libunistring 1.1）は、文字列の折り返し位置を Unicode の行分割規則（UAX #14）で決め、桁を東アジアの文字幅（UAX #11）で数える。日本語の文は語の境界がなくても文字の間で折り返されるため、バイト単位の一致（7.3）には UAX #14 の表と文字幅の表が必要になる。参照コメントの折り返しだけは、文字幅ではなくバイト数で数える。
  - 廃止エントリでは、`#.`、`#:`、`#,` の行に `#~` を付けない（`#|` だけが `#~|` になる）。requirements.md の 5.3 はこれと食い違っていたため、gettext の実際の出力に合わせて修正した。
  - msgcat は、フラグの `fuzzy` を先頭に並べ替える。CRLF の入力は LF で出力する。文字列中の制御文字（`\001` など）は、エスケープせずにそのままのバイトで書き出す。

## Research Log

### 既存の基盤（foundation と CMake）
- **Context**: 依存してよい部品と、モジュールの追加手順を確認する。
- **Sources Consulted**: `src/foundation/{buf,diag,utf8,source,eol}.h`、`cmake/MondeTargets.cmake`、`src/cli_args/`、`tests/cli_args/`、`.kiro/steering/structure.md`、`.github/workflows/ci.yml`
- **Findings**:
  - `mnd_buf_t`（NUL 終端の可変長バッファ）、`mnd_diag_list_t`（英語のメッセージ、パス、行番号を持つ診断のリスト）、`mnd_utf8_validate`、`mnd_eol_t` が使える。
  - `monde_add_module_tests` は `DEFINITIONS` を受け付けるので、テストデータのディレクトリをコンパイル定義で渡せる。
  - 予約済みの短縮名は `buf`、`diag`、`severity`、`utf8`、`eol`、`span`、`slice`、`source`、`file`、`wide`、`args` である。
  - CI の Ubuntu のジョブには gettext が入っていない。
- **Implications**: モジュール名と短縮名は `po` とする（structure.md の例 `src/po/` と一致する）。gettext を使う互換性テストは、`msgfmt` / `msgcat` が見つかったときだけ登録し、CI の Ubuntu のジョブで gettext を入れる。

### GNU gettext の出力形式（msgcat 0.21 で実測）
- **Context**: 7.3（gettext の出力のバイト単位の再現）と 6.x（折り返し）を設計できるだけの正確な規則が必要。
- **Sources Consulted**: Ubuntu 24.04 の gettext 0.21-14ubuntu2、libunistring 1.1 に、さまざまな入力を `msgcat`、`msgcat --no-wrap`、`msgfmt --check`、`msginit --no-translator` で処理して確認した。GNU gettext マニュアル「The Format of PO Files」。
- **Findings**:
  - **1 行に収まる条件**: 文字列が途中に `\n` を含まず、`<前置き><キーワード> "<エスケープ後の文字列>"` の全体が 79 桁以内なら 1 行で書く。それ以外は、キーワードの行に `""` を書き、次の行から書く。末尾にだけ `\n` がある文字列は、1 行に収まれば 1 行になる（例: 1 項目だけのヘッダ）。
  - **折り返し**: 2 行目以降は、`<前置き>"<部分>"` の全体が 79 桁以内になるように、行分割の機会の位置で貪欲に区切る。行末の空白も桁に数える。`\n` の直後では必ず区切る。分割の機会がない長い部分は、79 桁を超えてもそのまま 1 行にする。
  - **前置き**: 廃止エントリの `#~ `、前回の msgid の `#| `、廃止エントリの前回の msgid の `#~| ` も桁に数える。
  - **行分割の機会**: 空白の後、ハイフンの後（`well-known-` で区切る）、CJK の文字の間（ひらがな、漢字、全角英字、絵文字）。閉じ括弧や句読点（`」`、`。`、`、`）の前では区切らない。エスケープ（`\"` など）の内部では区切らない。
  - **桁の数え方**: 全角文字と絵文字は 2 桁、そのほかは 1 桁。エスケープは表記の文字数（`\"` は 2 桁）。
  - **参照コメント**: `#:` に続けて ` <参照>` を並べ、行が 79 バイトを超える場合は新しい `#:` の行を始める。日本語のファイル名で確かめると、桁ではなくバイト数で数えている。1 つだけで 79 バイトを超える参照は、その 1 つだけを行に置く。空白を含むファイル名は U+2068 と U+2069 で囲まれる。
  - **コメントの順序**: `#`、`#.`、`#:`、`#,`、`#|` の順に書く（入力の順序によらない）。
  - **フラグ**: `#, fuzzy, c-format` のように `, ` で区切って 1 行に書く。`fuzzy` を先頭にする。
  - **廃止エントリ**: キーワードの行に `#~ `、前回の msgid に `#~| ` を付ける。`#`、`#.`、`#:`、`#,` はそのまま書く。msgcat は廃止エントリをファイルの末尾に移し、msgstr が空の廃止エントリは出力しない。
  - **改行**: CRLF の入力を読み込めて、出力は LF になる。
  - **エスケープ**: `\a`、`\b`、`\f`、`\v`、`\r`、`\t`、`\n`、`\\`、`\"` を書き出す。そのほかの制御文字（`\001`、`\x7f`）は、エスケープせずにそのままのバイトで書き出す。
  - **`--no-wrap` と `#, no-wrap`**: `\n` の直後でだけ区切る。
- **Implications**: ライターの整形規則は、上の実測結果をそのまま契約にする。msgcat が行う並べ替え（廃止エントリを末尾に移すこと、未翻訳の廃止エントリを捨てること）は、本部品では行わない。要件 5.1 と 7.1 は、エントリを渡された順序のまま保持することを求めているためである。gettext の出力はもともとこの並びになっているので、7.3 のバイト単位の一致には影響しない。

### msgfmt --check の判定
- **Context**: 5.6 と 9.6 で保証する範囲を確かめる。
- **Findings**:
  - ヘッダの項目が既定値のままだったり、項目が欠けていたりしても、警告になるだけで終了コードは 0 になる。テンプレート（fuzzy のヘッダ）も、初期化した翻訳ファイルも合格する。
  - 致命的なエラーになるもの: 翻訳済みのエントリで msgid と msgstr の改行の始まり・終わりが一致しない。複数形のエントリがあるのに、ヘッダに Plural-Forms がない。msgctxt と msgid の組が重複している。
  - `\a` などのエスケープは警告になるだけで、エラーにはならない。
- **Implications**: 5.6 の前提（重複がない、改行が一致する、複数形には Plural-Forms がある）を、テストの入力の条件にする。リーダーは改行の不一致を警告として報告する（4.10）。

### Unicode のデータ
- **Context**: UAX #14 の行分割クラスと、UAX #11 の文字幅を、外部ライブラリなしで得る。
- **Sources Consulted**: UAX #14（Unicode Line Breaking Algorithm）の Unicode 15.0 版、UAX #11（East Asian Width）、Unicode Character Database 15.0.0 の `LineBreak.txt`、`EastAsianWidth.txt`、`DerivedGeneralCategory.txt`。libunistring 1.1 の対応 Unicode バージョン（15.0）。
- **Findings**:
  - libunistring 1.1 は Unicode 15.0 の表を持つ。Ubuntu 24.04（CI の `ubuntu-latest`）の gettext はこれを使う。
  - UAX #14 の規則は、行分割クラスの組の表（ペアテーブル）と、いくつかの文脈規則（空白の連続、結合文字、ZWJ、地域指示子）で実装できる。
  - UTF-8 では、文字幅があいまいな文字（East Asian Width が A）は 1 桁になる。結合文字（Mn、Me）と書式文字（Cf）は 0 桁になる。
- **Implications**: Unicode 15.0.0 のデータから C の表を生成するスクリプトを `tools/` に置く。生成した C のファイルはリポジトリに含める。ビルドとテストでは Python を必要としない。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| 行単位の字句解析 + エントリ単位の状態機械 | 1 行ずつ分類し（コメント、キーワード、続きの文字列、空行）、エントリの文法を状態機械で組み立てる | PO は行指向なので単純。エラーの行番号が自然に得られる。次のエントリの始まりで回復しやすい | 1 行に複数のキーワードを書く PO（まれ）は受け付けない | 採用。gettext の出力と一般的なエディタの出力は、すべて 1 行 1 キーワードである |
| 文字単位の字句解析器 + 再帰下降 | gettext の po-gram と同じく、トークンの列を文法で解析する | 自由な書式を受け付ける | エラーからの回復と行の対応付けが複雑になる | 不採用 |

## Design Decisions

### Decision: 折り返しは UAX #14 のペアテーブルで実装する
- **Context**: 6.1、6.2、7.3。日本語を含む文字列の折り返し位置を gettext と一致させる必要がある。
- **Alternatives Considered**:
  1. 空白の後だけで区切る — 英語では一致するが、日本語の段落では折り返しの位置が gettext と異なり、7.3 を満たせない。
  2. 空白の後と、CJK の文字の間で区切る簡易規則 — 句読点や括弧の禁則が合わない。
  3. UAX #14（Unicode 15.0）のペアテーブルと文脈規則 — libunistring と同じ方式で、一致させやすい。
- **Selected Approach**: 3。行分割クラスの表と、文字幅の表を生成して持つ。
- **Rationale**: 7.3 の一致を、日本語の文書でも検証できる。表は Unicode のデータから機械的に生成するので、手書きの誤りが入りにくい。
- **Trade-offs**: 生成した表の分だけソースが大きくなる（数十 KB）。gettext が使う libunistring と Unicode のバージョンが変わると、まれな文字で結果が変わりうる。
- **Follow-up**: msgcat で作った日本語の折り返しの正解ファイルで確かめる。CI では msgcat が正解ファイルを変えないこと（正解ファイルが gettext の出力そのものであること）も確かめる。

### Decision: gettext と libunistring のコードを流用しない
- **Context**: Monde は MIT License である。gettext と libunistring は GPL / LGPL である。
- **Selected Approach**: 規則は UAX #14 / UAX #11 の文書と、msgcat の出力の観察から実装する。gettext と libunistring のソースはコピーしない。Unicode のデータファイルは Unicode License v3 であり、生成した表にはその表記を添える。

### Decision: エントリは構造体の公開フィールドと、少数の操作関数で表す
- **Context**: 1.x。po-merge と translation-units が、エントリを作って加工する。
- **Alternatives Considered**:
  1. 不透明な型と、取得・設定の関数の組 — API が大きくなり、上位の部品から使いにくい。
  2. 公開の構造体（`mnd_buf_t` のフィールド）と、所有権と不変条件にかかわる操作だけを関数にする。
- **Selected Approach**: 2。文字列は `mnd_buf_t` で持ち、利用者は `mnd_buf_*` で直接書き換える。配列の追加（エントリ、参照、コメント、フラグ、msgstr）は関数を通す。
- **Trade-offs**: 不変条件（`has_msgctxt` が偽なら `msgctxt` は空、など）を利用者が崩しうる。ライターは `has_*` だけを見て書くので、崩れても出力は決まる。

### Decision: 検索と重複の検査は線形探索とする
- **Context**: 1.7、4.7、5.5。性能目標はない（tech.md）。
- **Selected Approach**: `mnd_po_catalog_find` は線形探索。重複の検査は O(n²) の比較とする。
- **Rationale**: 一般的な文書は数百〜数千のエントリで、O(n²) でも 1 秒よりずっと短い。索引の構造は、po-merge が必要とした時点で追加できる（公開 API を変えない）。
- **Follow-up**: po-merge の設計で、件数が多いときの計算量を再確認する。

### Decision: 文字列の検査（NUL と不正な UTF-8）はアンエスケープの後に行う
- **Context**: 3.2 の 8 進数・16 進数のエスケープは、任意のバイトを作れる。`\0` や `\xff` を受け付けると、ライターが不正な UTF-8 のファイルや、gettext が扱えない文字列を書き出してしまう。
- **Selected Approach**: アンエスケープした文字列に NUL が含まれる場合と、UTF-8 として不正な場合は、その行のエラーにする（4.8 を、エスケープで作られたバイトにも当てはめる）。

### Decision: フラグの書き出しでは `fuzzy` を先頭に置く
- **Context**: 1.6 で、フラグを追加すると末尾に付く。po-merge が `c-format` のあるエントリに `fuzzy` を付けると、gettext と異なる順序（`c-format, fuzzy`）になる。
- **Selected Approach**: ライターは `fuzzy` を先頭に書き、ほかのフラグは保持している順に書く。gettext の出力（`fuzzy` が先頭）を読んで書き直した結果は変わらない（7.3）。

### Decision: 互換性テストは 3 種類に分ける
- **Selected Approach**:
  1. **正解ファイルとの比較（全環境）**: `tests/po/data/` の、msgcat が書き出した PO を読み込んで書き出し、バイト単位で比較する（Naos）。
  2. **正解ファイルの検証（gettext がある環境）**: `msgcat <正解ファイル>` の出力が正解ファイルと一致することを CTest で確かめる。gettext のバージョンが変わって正解ファイルが古くなったことを検出する。
  3. **msgfmt --check（gettext がある環境）**: テスト用の小さな実行ファイルで、ライターと初期化の出力をファイルに書き、`msgfmt --check` を CTest から実行する。
- **Rationale**: Windows と macOS では gettext を前提にできない。1 は全環境で動き、2 と 3 は CI の Ubuntu で動く。

## Risks & Mitigations
- **libunistring の版の違いで折り返しが変わる** — 表の Unicode バージョンを 15.0 に固定し、research.md と生成スクリプトに記録する。CI の正解ファイルの検証（上記 2）で差を検出する。
- **UAX #14 のペアテーブルの転記の誤り** — 英語、日本語の句読点と括弧、全角英数字、絵文字、ハイフン、URL を含む正解ファイルで確かめる。
- **gettext が受け付けて、本部品がエラーにする入力**（1 行に複数のキーワード、`#~` の後のキーワードの省略形など） — 一般的なツールの出力では現れない。見つかった場合は、リーダーの文法を広げる。
- **エラーからの回復で、エラーが連鎖する** — 回復の位置を「空行、コメントの行、`msgctxt` / `msgid` の行」に限定し、回復した直後のエラーを 1 つのエントリの中で 1 件に絞る。

## References
- [GNU gettext manual: The Format of PO Files](https://www.gnu.org/software/gettext/manual/html_node/PO-Files.html) — エントリの文法とコメントの種類
- [UAX #14: Unicode Line Breaking Algorithm](https://www.unicode.org/reports/tr14/) — 行分割のクラスと規則
- [UAX #11: East Asian Width](https://www.unicode.org/reports/tr11/) — 文字幅
- [Unicode Character Database 15.0.0](https://www.unicode.org/Public/15.0.0/ucd/) — `LineBreak.txt`、`EastAsianWidth.txt`、`extracted/DerivedGeneralCategory.txt`
