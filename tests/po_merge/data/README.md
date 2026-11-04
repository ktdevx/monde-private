# po_merge のテストデータ

このディレクトリには、golden スイート（`tests/po_merge/golden_test.c`）が比較に使う正解ファイルを置く。ケースごとのディレクトリ `<case>/` に、次の 3 つのファイルを置く。

| ファイル | 内容 |
|----------|------|
| `old.po` | 既存の翻訳ファイル（`msgcat` 0.21 で整形済み） |
| `template.pot` | テンプレート（`msgcat` 0.21 で整形済み） |
| `expected.po` | `msgmerge --previous`（`nofuzzy` は `--no-fuzzy-matching` も付ける）の出力そのもの |

golden スイートは、`old.po` と `template.pot` を読み込み、`nofuzzy` はあいまい一致を無効（`fuzzy_matching = false`）に、ほかは既定の設定でマージし、`mnd_po_write` の既定の設定（折り返しあり、LF）で書き出した結果が `expected.po` とバイト単位で一致することを確かめる。入力は `msgcat` の正規形なので、読み込んで書き出すと元のファイルに一致することも確かめる。テストは `msgmerge` を実行せず、ここに置いた出力だけを使うので、gettext のない環境でも同じ結果になる。

## 作った環境

| 項目 | 値 |
|------|----|
| OS | Ubuntu 24.04 |
| `msgmerge --version` の 1 行目 | `msgmerge (GNU gettext-tools) 0.21` |
| `msgcat --version` の 1 行目 | `msgcat (GNU gettext-tools) 0.21` |
| gettext のパッケージ | `gettext` 0.21-14ubuntu2 |
| libunistring のパッケージ | `libunistring5` 1.1-2build1.1（`dpkg -s libunistring5` の Version） |

折り返しの位置と文字幅は libunistring に依存する。gettext や libunistring を上げると、`msgcat` と `msgmerge` の出力が変わりうる。比較に使う gettext の版を変えるときは、設計の Revalidation Triggers に従って正解ファイルを作り直し、マージの規則を確かめ直す。

## 作り方

1. 各ケースの内容（下記）を、折り返しを気にせずに手で書いた PO（入力）を用意する。手で書いた入力はリポジトリに含めない。
2. 入力を既定の設定の `msgcat` で整形する。

   ```sh
   msgcat <手で書いた old>.po -o <case>/old.po
   msgcat <手で書いた template>.pot -o <case>/template.pot
   ```

   `nofuzzy/old.po` と `nofuzzy/template.pot` は、`fuzzy/` の同名のファイルの複製である（内容は同じ）。

3. 各ケースのディレクトリで `msgmerge` を実行する。

   ```sh
   # exact、fuzzy、obsolete、header、no_header、plural
   msgmerge --previous -o expected.po old.po template.pot
   # nofuzzy
   msgmerge --previous --no-fuzzy-matching -o expected.po old.po template.pot
   ```

4. 確認として、`old.po`、`template.pot`、`expected.po` が `msgcat` をもう一度かけても変わらないことを確かめる。`msgfmt --check -o /dev/null` は、`header/old.po`（小文字の項目名を使うため `Last-Translator` と `Language` がないという警告）と `no_header/` の `old.po`・`expected.po`（ヘッダがないためのエラー）を除いて、エラーも警告も出さない。これらは意図した入力である。

正解ファイルを変えるときは、`expected.po` を直接編集しない。入力（`old.po`、`template.pot`）を編集して `msgcat` で自分自身を書き直し（例: `msgcat old.po -o old.po`）、手順 3 の `msgmerge` で `expected.po` を作り直す。`fuzzy/` の入力を変えたら、`nofuzzy/` の入力も複製し直す。

### 正解ファイルに入れないもの

本部品が msgmerge と意図して異なる入力（設計の Open Questions / Risks、research.md の「msgmerge と意図して異なる点」）は、どのケースにも含めない。

- テンプレートの廃止エントリ、既存の翻訳ファイルで同じキーを持つ通常のエントリと廃止エントリ
- `no-wrap` 以外のフラグの組み合わせで順序が変わるもの、未知のフラグ。`c-format` は、書式の検査で fuzzy にならない訳（`%s` を保つ訳）を持つ完全一致のエントリでだけ使う
- 書式文字列や `range` の検査で fuzzy になるもの
- Language の推測が起こるヘッダ（既存の翻訳ファイルのヘッダは、常に Language を持つ）
- 壊れた Plural-Forms の式、空の msgid で msgctxt を持つテンプレートのエントリ
- 長さの和が数万バイトを超える、大きく異なる msgid
- 未翻訳のまま fuzzy になるエントリ（訳のない既存のエントリとの複数形の食い違いなど）。po-format のライターは msgstr が空でも `#, fuzzy` を書くが、gettext 0.21 は書かない

## 各ケースの内容

すべてのファイルは UTF-8 で、改行は LF である。`no_header` 以外の `old.po` は、UTF-8 の charset と Language を宣言するヘッダを持つ。`template.pot` は、xgettext の形のヘッダ（`#, fuzzy`、空の Language）を持つ。

| ケース | 内容 |
|--------|------|
| `exact` | 完全一致。翻訳者コメント（空の `#` を含む）の保持と、抽出コメント・参照の更新。`c-format` のエントリ。`no-wrap` の追加（テンプレートだけが持つ）と削除（既存だけが持つ。訳を折り返して書く）。fuzzy の既存エントリの完全一致（前回の msgid を保つものと、前回の msgid のないもの）。fuzzy でない既存エントリの前回の msgid（`#|`）を消す。訳のない既存エントリの完全一致。msgctxt（`menu`、`dialog`、空の msgctxt、msgctxt なし）で区別される同じ msgid。`\n`・`\t`・`\"`・`\\` を含む文字列。どれにも一致しない新規のエントリ |
| `fuzzy` | あいまい一致（前回の msgid と、既存の翻訳者コメントを保ち、参照をテンプレートから取る）。fuzzy の既存エントリを候補にしたもの（その前回の msgid を引き継ぐ）。msgctxt の異なる候補（`#| msgctxt` を持つ）。同点の 2 つの候補（`Copy file A` と `Copy file B`。先の候補が選ばれ、後の候補は廃止エントリになる）。4 文字未満の msgid（`Cut`、`閉じる`）。日本語の msgid。廃止エントリの候補（出力に廃止エントリとして残らない）。訳のない既存のエントリは候補にならず、削除される。どの候補とも一致しない新規のエントリ |
| `obsolete` | 完全一致と新規。廃止エントリ化（翻訳者コメントを保ち、抽出コメントと参照を消す。fuzzy と前回の msgid、msgctxt を保つ）。訳のない既存のエントリの削除。廃止エントリの復活（完全一致、msgctxt を持つ完全一致、あいまい一致）。元からの廃止エントリ（fuzzy と `#~|` を持つものを含む）。廃止エントリは既存の翻訳ファイルの順に末尾に並ぶ |
| `header` | ヘッダの更新。既存のヘッダは項目の順が異なり、未知の項目（`X-Generator`、`X-Poedit-SourceCharset`）と Plural-Forms を持ち、項目名の大文字・小文字が異なるもの（`language:`、`last-translator:`）を含み、Report-Msgid-Bugs-To を持たない。結果は既知の項目を決まった順と綴りに並べ、ほかの行を元の順に続ける。Report-Msgid-Bugs-To と POT-Creation-Date をテンプレートの値にし、テンプレートだけの未知の項目（`X-Template-Only`）は加えない。ヘッダの fuzzy と翻訳者コメントは既存から保つ |
| `no_header` | テンプレートにだけヘッダがある。結果はヘッダを持たない。完全一致、訳のない完全一致、あいまい一致、新規、廃止エントリ化を含む（ASCII だけの内容） |
| `plural` | ドイツ語（`nplurals=2`）の翻訳ファイル。単数形 → 複数形（訳を `msgstr[0]` と `msgstr[1]` に複製して fuzzy）、複数形 → 単数形（`msgstr[0]` を msgstr にして fuzzy。前回の msgid_plural を持つ）、msgid_plural の変更（fuzzy。前回の msgid_plural を持つ）、複数形の完全一致、新規の複数形エントリ（`msgstr[0]` と `msgstr[1]`）、単数形の既存エントリとの複数形のあいまい一致、複数形の廃止エントリ化 |
| `nofuzzy` | `fuzzy` と同じ入力を、`--no-fuzzy-matching` で処理した結果。あいまい一致したエントリは新規になり、使われなかった訳のある既存エントリは廃止エントリになる（9.3） |

## チェックアウトの設定

正解ファイルはバイト単位で比べるので、Git が改行を変換しないように `.gitattributes` で `*.po -text` と `*.pot -text` としている（Windows の `core.autocrlf` でも LF のまま取り出す）。golden スイートは、読み込んだファイルに CR がないことも確かめる。
