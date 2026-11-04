# po のテストデータ

このディレクトリには、roundtrip スイート（`tests/po/roundtrip_test.c`）が比較に使う正解ファイルを置く。正解ファイルは、すべて GNU gettext の `msgcat` が書き出した PO そのものである。roundtrip スイートは、正解ファイルを読み込んで書き出した結果が、正解ファイルとバイト単位で一致することを確かめる。例外として、`fuzzy_empty.po` の入力 `fuzzy_empty_in.po` と `obsolete_empty.po` の入力 `obsolete_empty_in.po` だけは手で書いた PO で、正解ファイルではない（下記の「fuzzy_empty_in.po」「obsolete_empty_in.po」）。テストは `msgcat` を実行せず、ここに置いた出力だけを使うので、gettext のない環境でも同じ結果になる。

## 作った環境

| 項目 | 値 |
|------|----|
| OS | Ubuntu 24.04.5 LTS |
| `msgcat --version` の 1 行目 | `msgcat (GNU gettext-tools) 0.21` |
| gettext のパッケージ | `gettext` 0.21-14ubuntu2 |
| libunistring のパッケージ | `libunistring5` 1.1-2build1.1（`dpkg -s libunistring5` の Version） |

折り返しの位置と文字幅は libunistring に依存する。gettext や libunistring を上げると、`msgcat` の出力が変わりうる。その場合は、設計の Revalidation Triggers に従って Unicode の表と正解ファイルを見直す。

## 作り方

1. 各ファイルの内容（下記）を、折り返しを気にせずに手で書いた PO（入力）を用意する。入力はリポジトリに含めない。
2. 既定の設定の `msgcat` で書き出す。

   ```sh
   msgcat <入力>.po -o basic.po          # wrap_latin.po、wrap_cjk.po、refs.po、nowrap_flag.po も同じ
   msgcat --no-wrap wrap_cjk.po -o nowrap_option.po
   ```

3. 確認として、各ファイルに `msgfmt --check -o /dev/null <ファイル>` を実行し、エラーも警告も出ないことを確かめる。

`fuzzy_empty.po` と `obsolete_empty.po` は例外で、入力 `fuzzy_empty_in.po` と `obsolete_empty_in.po` をリポジトリに含め、次のコマンドで作る。

```sh
msgcat fuzzy_empty_in.po -o fuzzy_empty.po
msgcat obsolete_empty_in.po -o obsolete_empty.po
```

`msgcat` の出力は、もう一度 `msgcat` にかけても変わらない（`nowrap_option.po` は `msgcat --no-wrap` で変わらない）。正解ファイルを変えるときは、正解ファイルを直接編集してから、同じコマンドで自分自身を書き直す（例: `msgcat basic.po -o basic.po`）。`wrap_cjk.po` を変えたら、`nowrap_option.po` も作り直す。`fuzzy_empty.po` と `obsolete_empty.po` は直接編集せず、それぞれの入力（`*_in.po`）を編集してから上のコマンドで作り直す。

### msgcat が入力から変えること

正解ファイルは `msgcat` の出力なので、入力とは次の点が異なる。Monde のライターはエントリの順序とフラグの順序を保つので、正解ファイルを読み込んで書き出すと、正解ファイルと一致する。

- fuzzy 以外のフラグを、gettext の決めた順に並べ替える（例: `#, python-format, c-format` は `#, c-format, python-format` になる）。
- 未翻訳のエントリ（最初の msgstr が空のもの。複数形では `msgstr[0]` が空なら `msgstr[1]` に訳があっても未翻訳）の `fuzzy` を捨てる。
- 廃止エントリを末尾に移し、未翻訳の廃止エントリ（最初の msgstr が空のもの。複数形では `msgstr[0]` が空なら `msgstr[1]` に訳があっても捨てる）をコメント・フラグ・`#~|` を含めて捨てる。Monde のライターも同じ廃止エントリを書かない（5.8）が、エントリの順序は変えないので、書く廃止エントリを末尾に置いた入力でだけ `msgcat` の出力と一致する。
- 文字列を 79 桁で折り返し、参照を 79 バイトで折り返す。

### 正解ファイルに入れないもの

- **空白を含むファイル名の参照**: gettext 0.22 以降は U+2068 と U+2069 で囲んで書くが、0.21 は囲まずに書き、読むときに空白で分けてしまう。このため、0.21 の出力としての正解ファイルは作れない。Monde のライターが囲んで書くことと、リーダーが囲みを解くことは、write・read スイートと、roundtrip スイートの `roundtrip_refs_outside_golden_files_read_back`（書き出して読み直す往復）で確かめる。
- **libunistring 1.1 と文字幅が異なる文字**: Unicode 15.0 で追加された絵文字、CJK のブロック内の未割り当てのコードポイント、U+D7B0〜U+D7FB、U+00AD など。正解ファイルには、幅の定まった文字（日本語、韓国語、中国語、全角・半角の英数字とカタカナ、Unicode 13 までの絵文字）だけを使う。

## 各ファイルの内容

すべてのファイルは、UTF-8 の charset を宣言するヘッダを持ち、`msgfmt --check` を通る。改行は LF である。

| ファイル | 内容 |
|----------|------|
| `basic.po` | ファイルの先頭の翻訳者コメント（空の `#` を含む）と完全なヘッダ（Plural-Forms あり）。翻訳者コメント、抽出コメント（空の `#.` を含む）、参照、`c-format` のフラグ。複数形（`msgstr[0]` だけのものと未翻訳のもの）。msgctxt（`menu`、`button`、空の msgctxt）で区別される同じ msgid。前回の msgctxt と msgid（`#|`）を持つ fuzzy のエントリ。未翻訳のエントリ。`\t`、`\"`、`\\` と途中の `\n` を含む文字列。廃止エントリ（`#~|` を持つ fuzzy のもの、msgctxt と複数形を持つものを含む） |
| `wrap_latin.po` | 英語の折り返し。79 桁ちょうどと 80 桁の文字列、複数行に折り返す長い文、ハイフン・URL・パス・句読点・数字・記号・`…` や引用符・アクセント付きの文字の前後の分割、分割の機会のない 100 文字の語（W を超えてもキーワードの行に書く。末尾が `\n` のものを含む）、空白で区切られた 2 つの長い語、`\"`・`\\`・`\t` のエスケープを含む行（分割の機会はエスケープ後の文字列で決まる）、`\n` の直前の空白、段落と字下げを含む複数行の文字列。長い msgctxt、長い前回の msgid、複数形、`#~ ` の付いた廃止エントリの折り返し |
| `wrap_cjk.po` | 日本語・韓国語・中国語の折り返し。句読点（`。`、`、`）と括弧（`「」`、`（）`、`『』`、`【】`）、小さい仮名（`っ`、`ャ`）と長音（`ー`）、日本語と英数字の混在、`\n` を含む文字列、全角の英数字と全角の空白、半角カタカナ（1 桁）、絵文字（2 桁）、肌の色の修飾子・国旗・ZWJ の列、`\"` と `\t` と `\\` を含む日本語の文、空白のない長い仮名の列。fuzzy のエントリと廃止エントリ。msgctxt を持つ各エントリは、79 バイトで折り返す長さの参照を持つ |
| `refs.po` | 参照の折り返し。39 個の参照、行番号のない参照、行番号の有無が混在する参照、日本語のファイル名（106 バイトで 79 桁になる行は、バイト数で数えて折り返す）、2 つ目の参照を加えると 80 バイトになるものと 79 バイトになるもの、1 つで 79 バイトを超える参照、空のファイル名（`:5`）、行番号の部分が正の数でない参照（`g.c:0`、`h.c:x`、`i.c:`。ファイル名として読み、そのまま書き戻す）、翻訳済みのエントリと廃止エントリの参照 |
| `nowrap_flag.po` | `#, no-wrap` のエントリ。長い 1 行の文字列、途中に `\n` を含む文字列、`c-format` との組み合わせ（`msgcat` の順の `#, c-format, no-wrap`）、fuzzy と前回の msgid、長い msgctxt、複数形、廃止エントリ。比較のため、フラグのないエントリも 1 つ含む。参照は `no-wrap` があっても折り返す |
| `nowrap_option.po` | `wrap_cjk.po` を `msgcat --no-wrap` で書き出したもの。文字列は `\n` の直後でだけ分かれるが、参照は既定と同じく 79 バイトで折り返す |
| `fuzzy_empty_in.po` | 正解ファイルではなく、`fuzzy_empty.po` の入力として手で書いた PO。ファイルの先頭の翻訳者コメントと完全なヘッダ（Plural-Forms は `nplurals=2`）。`fuzzy` を持ち最初の msgstr が空のエントリ（単数形、`#, fuzzy, c-format` で翻訳者コメント・抽出コメントを持つもの、前回の msgid（`#|`）を持つもの、msgctxt を持つもの、複数形で `msgstr[0]` だけが空のもの（`c-format` 付き）、すべての msgstr が空の複数形）。比較のため、`msgstr[1]` だけが空の fuzzy の複数形、訳のある fuzzy のエントリ（単数形で `#|` と `c-format` を持つもの、複数形）、翻訳済みのエントリ、未翻訳のエントリ、fuzzy の廃止エントリと翻訳済みの廃止エントリも含む。最初の msgstr が空の廃止エントリは含めない（`obsolete_empty_in.po` で確かめる） |
| `fuzzy_empty.po` | `fuzzy_empty_in.po` を `msgcat` で書き出したもの。最初の msgstr が空のエントリから `fuzzy` だけが消え（ほかのフラグと `#|` は残る）、ほかのフラグがなければ `#,` の行がなくなる。roundtrip スイートは、`fuzzy_empty_in.po` を読み込んで既定の設定で書き出した結果がこのファイルと一致することを確かめ、互換性テスト `po_msgcat_normalize.fuzzy_empty` は `msgcat fuzzy_empty_in.po` の出力がこのファイルと一致することを確かめる（5.7） |
| `obsolete_empty_in.po` | 正解ファイルではなく、`obsolete_empty.po` の入力として手で書いた PO。ファイルの先頭の翻訳者コメントと完全なヘッダ（Plural-Forms は `nplurals=2`）。書かれない廃止エントリ（最初の msgstr が空のもの）を、ヘッダの直後、通常のエントリの途中（2 件の連続を含む）、末尾の廃止エントリの間と最後に置く: 単数形、翻訳者コメント・抽出コメント・参照・`c-format` を持つもの、`fuzzy` と前回の msgid（`#~\|`）を持つもの、msgctxt を持つもの、すべての msgstr が空の複数形（`c-format` 付き）、`msgstr[0]` だけが空の複数形。比較のため、通常のエントリ（翻訳済み、`c-format` 付き、未翻訳、複数形）と、書かれる廃止エントリ（翻訳者コメントと参照を持つ訳のあるもの、`msgstr[1]` だけが空の複数形、`#~\|` を持つ訳のある fuzzy のもの）を含む。書かれる廃止エントリは、`msgcat` が並べ替えないように通常のエントリの後ろに置く |
| `obsolete_empty.po` | `obsolete_empty_in.po` を `msgcat` で書き出したもの。最初の msgstr が空の廃止エントリが、コメント・フラグ・`#~\|` を含めて 1 行も残らず、エントリの間の空行も 1 つだけになる。roundtrip スイートは、`obsolete_empty_in.po` を読み込んで既定の設定で書き出した結果がこのファイルと一致し、それを読み直したカタログが書かなかった廃止エントリを除いて最初のカタログと一致することを確かめ、互換性テスト `po_msgcat_normalize.obsolete_empty` は `msgcat obsolete_empty_in.po` の出力がこのファイルと一致することを確かめる（5.8） |

## チェックアウトの設定

正解ファイルはバイト単位で比べるので、Git が改行を変換しないように `.gitattributes` で `*.po -text` としている（Windows の `core.autocrlf` でも LF のまま取り出す）。roundtrip スイートは、読み込んだ正解ファイルに CR がないことも確かめる。
