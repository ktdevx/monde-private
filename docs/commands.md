# コマンドリファレンス

この章では、`monde` の各サブコマンドの引数とオプション、成功時に表示する統計、エラーと警告の表示、終了コード、入出力のファイルの扱いを説明します。使い方の引用は、`monde` の `--help` の出力をそのまま載せたものです。2 つの翻訳フローの中での使い方は [翻訳フロー](workflow.md) を、初版で対応していない記法は [既知の制限](limitations.md) を参照してください。

- [全体の使い方](#全体の使い方)
- [`monde extract`](#monde-extract)
- [`monde init`](#monde-init)
- [`monde update`](#monde-update)
- [`monde translate`](#monde-translate)
- [統計の表示](#統計の表示)
- [診断の表示と終了コード](#診断の表示と終了コード)
- [文字コードと改行コード](#文字コードと改行コード)
- [翻訳の対象になる文字列](#翻訳の対象になる文字列)
- [出力先のファイルの扱い](#出力先のファイルの扱い)

## 全体の使い方

`monde --help` の出力は次のとおりです。

<!-- monde-check: help -->
```text
Usage: monde <command> [<options>] [<arguments>]

Translate Markdown documents through gettext .pot/.po files.

Commands:
  extract    Create a translation template (.pot) from a Markdown file
  init       Create a new translation file (.po) from a template
  update     Merge a new template into an existing translation file
  translate  Create a translated Markdown file from a translation file
  help       Show help for a command

Options:
  -h, --help     Show this help
      --version  Show version information

Run 'monde <command> --help' for help on a command.
```

- `monde --help`、`monde -h`、`monde help` は、上の全体の使い方を標準出力に書き、終了コード 0 で終了します。
- `monde <command> --help`、`monde <command> -h`、`monde help <command>` は、そのサブコマンドの使い方を標準出力に書き、終了コード 0 で終了します（例: `monde help init` は `monde init --help` と同じ内容を書きます）。
- `monde --version` は、プログラム名とバージョンからなる 1 行（`monde X.Y.Z` の形。例: `monde 0.1.0`）を標準出力に書き、終了コード 0 で終了します。
- 引数を付けずに `monde` だけを実行すると、全体の使い方を標準エラー出力に書き、終了コード 2 で終了します。
- 未知のサブコマンドやオプション、オプションの値の欠落、位置引数の過不足などの引数の誤りがあると、誤りを示すメッセージと、ヘルプの表示方法を示す 1 行を標準エラー出力に書き、終了コード 2 で終了します。

  ```text
  monde: error: unknown option '--nope'
  Run 'monde extract --help' for usage.
  ```

すべてのサブコマンドで、オプションと位置引数はどの順序で指定してもかまいません（`monde extract -o hello.pot hello.md` と `monde extract hello.md -o hello.pot` は同じ結果になります）。位置引数の順序（`update` の `<TEMPLATE> <PO>` など）は、使い方に書かれた順のとおりに指定します。

### 4 つのサブコマンドに共通のこと

- `-o, --output <FILE>`: 結果を `FILE` に書きます。省略すると、結果を標準出力に書きます。`-o -` と指定しても標準出力に書きます。
- 位置引数の入力ファイルにちょうど `-` を指定すると、そのファイルの代わりに標準入力から読みます。標準入力は 1 回しか読めないため、`update` と `translate` で 2 つの入力の両方に `-` を指定すると、使い方のエラー（終了コード 2）になります。
- `-q, --quiet`: 成功時の統計と情報の表示を抑止します（[統計の表示](#統計の表示) を参照）。警告とエラーは抑止しません。
- 結果（`.pot`、`.po`、翻訳済みの Markdown）は標準出力または `-o` のファイルに書き、統計と診断は標準エラー出力に書きます。統計と診断が結果に混ざることはありません。

## `monde extract`

原文の Markdown から翻訳テンプレート（`.pot`）を作ります。

<!-- monde-check: help extract -->
```text
Usage: monde extract [<options>] <INPUT>

Extract translatable text from a Markdown file and write a translation template (.pot).

Arguments:
  <INPUT>  Markdown file to extract from ('-' for standard input)

Options:
  -o, --output <FILE>                       Write the result to FILE (default: standard output)
  -q, --quiet                               Do not print statistics or informational messages
      --front-matter-keys <KEY>[,<KEY>...]  Extract front matter values at these key paths
      --project-id-version <NAME>           Value of the Project-Id-Version header (default: PACKAGE VERSION)
      --msgid-bugs-address <ADDRESS>        Value of the Report-Msgid-Bugs-To header (default: empty)
      --no-wrap                             Do not wrap long lines in the output
  -h, --help                                Show this help
```

- 位置引数: `<INPUT>`（原文の Markdown ファイル）を 1 つ指定します。`-` なら標準入力から読みます。
- `-o, --output <FILE>`: 省略すると標準出力に書きます。
- `--front-matter-keys <KEY>[,<KEY>...]`: YAML フロントマターのうち、指定したキーパスの値を翻訳の対象にします。省略すると、フロントマターは翻訳の対象にならず、その値は `.pot` に入りません。キーパスの書き方は [翻訳フロー](workflow.md) の「YAML フロントマターの値を翻訳する」の節を参照してください。`translate` にも同じ値を指定する必要があります。キーパスの書式が不正なとき、またはフロントマターの YAML を解析できないときは、エラーを表示して終了コード 1 で終了します。
- `--no-wrap`: 出力の長い行を折り返しません。省略すると、長い msgid などを複数の行に折り返して書きます。

`.pot` のヘッダには、次の値が書かれます。

| ヘッダ | 値 | 指定するオプション | 省略時の値 |
| --- | --- | --- | --- |
| `Project-Id-Version` | プロジェクトの名前とバージョン | `--project-id-version <NAME>` | `PACKAGE VERSION` |
| `Report-Msgid-Bugs-To` | 原文の誤りの報告先 | `--msgid-bugs-address <ADDRESS>` | 空 |
| `POT-Creation-Date` | 実行時のローカル日時とタイムゾーンのオフセット（`YYYY-MM-DD HH:MM+ZZZZ`） | なし | 常に実行時の日時 |

各エントリの参照コメント（`#:`）のファイル名には、コマンドラインで指定した `<INPUT>` のパスがそのまま（区切り文字も変換せずに）使われます。標準入力から読んだときは `-` になります。原文に翻訳する文字列が 1 つもないときは、ヘッダだけの `.pot` を書き、終了コード 0 で終了します（統計は `extract: 0 messages`）。

## `monde init`

翻訳テンプレート（`.pot`）から、言語ごとの新しい翻訳ファイル（`.po`）を作ります。

<!-- monde-check: help init -->
```text
Usage: monde init [<options>] <TEMPLATE>

Create a new translation file (.po) for a language from a translation template (.pot).

Arguments:
  <TEMPLATE>  Translation template (.pot) to start from ('-' for standard input)

Options:
  -o, --output <FILE>    Write the result to FILE (default: standard output)
  -q, --quiet            Do not print statistics or informational messages
  -l, --language <CODE>  Language code for the Language header (default: from LC_ALL, LC_MESSAGES or LANG)
      --no-wrap          Do not wrap long lines in the output
  -h, --help             Show this help
```

- 位置引数: `<TEMPLATE>`（翻訳テンプレート）を 1 つ指定します。`-` なら標準入力から読みます。
- `-o, --output <FILE>`: 省略すると標準出力に書きます。
- `--no-wrap`: 出力の長い行を折り返しません。省略すると折り返します。

テンプレートのすべてのエントリを、訳（msgstr）が空のまま引き継ぎます。ヘッダには `Language`（言語コード）と `PO-Revision-Date`（実行時のローカル日時とタイムゾーンのオフセット）を書き、ヘッダの fuzzy の印を外します。テンプレートが PO として読めない（構文の誤り、UTF-8 以外の charset、重複するエントリなど）ときは、見つかったすべてのエラーを表示して終了コード 1 で終了します。

### 言語コードの決め方

ヘッダの `Language` に書く言語コードは、次の順で決めます。

1. `-l <CODE>` または `--language <CODE>` を指定したときは、その値をそのまま使います。`.` や `@` を含んでいても取り除きません。
2. `-l/--language` を省略したときは、環境変数 `LC_ALL`、`LC_MESSAGES`、`LANG` をこの順に見て、最初に設定されている値を使います。値のうち `.` 以降と `@` 以降を除いた部分が言語コードになります（例: `ja_JP.UTF-8` → `ja_JP`、`fr_FR@euro` → `fr_FR`）。
   - 値が空のとき、`.` と `@` 以降を除いた部分が空、`C` または `POSIX` のとき（`C.UTF-8` を含む）は、その環境変数は設定されていないものとして、次の環境変数を見ます。
   - 環境変数から言語コードを決めたときは、どの環境変数のどの値を使ったかを `init: using language 'L' from VAR` の書式で標準エラー出力に 1 行で表示します（`-q` で抑止できます）。
3. `-l/--language` を省略し、3 つの環境変数のどれからも言語コードを決められないときは、次の使い方のエラーを表示して終了コード 2 で終了します。`-l` で言語コードを指定してください。

   ```text
   monde: error: cannot determine the language; specify --language
   ```

言語コードが空のとき、または空白、制御文字、ASCII 以外の文字を含むときは、`monde: error: invalid language code '<値>' from <指定元>` の形の使い方のエラーを表示して終了コード 2 で終了します（`<指定元>` は `--language` または環境変数の名前）。環境変数から最初に決まった値が不正なときは、次の環境変数を見ずにエラーになります。

## `monde update`

更新後の翻訳テンプレート（`.pot`）を既存の翻訳ファイル（`.po`）にマージし、既存の訳を引き継いだ翻訳ファイルを作ります。

<!-- monde-check: help update -->
```text
Usage: monde update [<options>] <TEMPLATE> <PO>

Merge a new translation template (.pot) into an existing translation file (.po), keeping existing translations.

Arguments:
  <TEMPLATE>  New translation template (.pot) ('-' for standard input)
  <PO>        Existing translation file (.po) to merge ('-' for standard input)

Options:
  -o, --output <FILE>      Write the result to FILE (default: standard output)
  -q, --quiet              Do not print statistics or informational messages
      --no-fuzzy-matching  Do not use fuzzy matching; reuse translations of exact matches only
      --no-wrap            Do not wrap long lines in the output
  -h, --help               Show this help
```

- 位置引数: `<TEMPLATE>`（更新後の翻訳テンプレート）と `<PO>`（既存の翻訳ファイル）を、この順に 2 つ指定します。どちらか一方に `-` を指定すると、そのファイルを標準入力から読みます（両方には指定できません）。
- `-o, --output <FILE>`: 省略すると標準出力に書き、既存の `.po` は変わりません。既存の `.po` を更新するには、`-o` に `<PO>` と同じパスを指定します（[出力先のファイルの扱い](#出力先のファイルの扱い) を参照）。
- `--no-fuzzy-matching`: あいまい一致を行わず、msgid が完全に一致するエントリの訳だけを引き継ぎます。省略すると、あいまい一致を行い、似た msgid の訳を fuzzy の印と前回の msgid（`#|`）付きで引き継ぎます。
- `--no-wrap`: 出力の長い行を折り返しません。省略すると折り返します。

マージの規則は GNU gettext の `msgmerge --previous` と同じです。ヘッダ（`Language`、`PO-Revision-Date`、`Last-Translator` など）は既存の `.po` のものを引き継ぎ、`PO-Revision-Date` は書き換えません。テンプレートまたは既存の `.po` が PO として読めないとき、または重複するエントリを含むときは、どちらのファイルのエラーかが分かる形ですべてのエラーを表示し、終了コード 1 で終了します。マージの結果の読み方は [翻訳フロー](workflow.md) の「原文更新」の節を参照してください。

## `monde translate`

原文の Markdown と翻訳ファイル（`.po`）から、翻訳済みの Markdown を作ります。

<!-- monde-check: help translate -->
```text
Usage: monde translate [<options>] <INPUT> <PO>

Replace translatable text in a Markdown file with translations from a translation file (.po).

Arguments:
  <INPUT>  Markdown file to translate ('-' for standard input)
  <PO>     Translation file (.po) with the translations ('-' for standard input)

Options:
  -o, --output <FILE>                       Write the result to FILE (default: standard output)
  -q, --quiet                               Do not print statistics or informational messages
      --front-matter-keys <KEY>[,<KEY>...]  Translate front matter values at these key paths
      --use-fuzzy                           Use non-empty translations of fuzzy entries
  -h, --help                                Show this help
```

- 位置引数: `<INPUT>`（原文の Markdown ファイル）と `<PO>`（訳を入れた翻訳ファイル）を、この順に 2 つ指定します。どちらか一方に `-` を指定すると、そのファイルを標準入力から読みます（両方には指定できません）。
- `-o, --output <FILE>`: 省略すると標準出力に書きます。
- `--front-matter-keys <KEY>[,<KEY>...]`: 指定したキーパスに一致する YAML フロントマターの値も訳に置き換えます。省略すると、フロントマターは原文のまま出力されます。`extract` に指定したものと同じ値を指定します。
- `--use-fuzzy`: fuzzy のエントリのうち、訳が空でないものを訳として採用します。省略すると、fuzzy のエントリに対応する箇所は原文のまま出力されます。

原文のうち翻訳する文字列の範囲だけを `.po` の訳に置き換え、それ以外のバイト列を原文のまま出力します。`.po` の訳が空のエントリ、`.po` にないエントリ、（`--use-fuzzy` を指定しないときの）fuzzy のエントリに対応する箇所は、原文のまま出力されます。`.po` の廃止エントリと、msgctxt の付いたエントリは置き換えに使いません。訳を 1 つも入れていない `.po`（`extract` の出力をそのまま渡した場合を含む）では、原文とバイト単位で同じ内容を出力します。

訳文に空行が含まれるとき、訳文のいずれかの行がブロックを始め得る記号（`#`、`>`、リストの記号、コードフェンス、区切り線など）で始まるとき、1 行の見出しの訳文に改行が含まれるときは、文書の構造が変わり得るため警告を表示します。このときも訳文をそのまま使って出力を書き、終了コード 0 で終了します。

```text
hello.md:5: warning: translation line 1 starts with a block marker (msgid at line 5)
```

`.po` が PO として読めないとき、または訳文が正しい UTF-8 でないときは、見つかったすべてのエラーを表示し、出力を書かずに終了コード 1 で終了します。

## 統計の表示

各サブコマンドは、成功したときに結果の件数を標準エラー出力に 1 行で表示します。文面は英語で、次の固定の書式です（`N`、`A`〜`E`、`P.Q`、`L`、`VAR` のところに値が入ります）。

| サブコマンド | 書式 | 内容 |
| --- | --- | --- |
| `extract` | `extract: N messages` | `.pot` のエントリの数（ヘッダを除く） |
| `init` | `init: N messages, language L` | 作った `.po` のエントリの数と、ヘッダに書いた言語コード |
| `init` | `init: using language 'L' from VAR` | `-l` を省略したときに、言語コード `L` を環境変数 `VAR` から決めたこと（上の行より先に表示） |
| `update` | `update: A matched, B fuzzy, C added, D obsoleted` | 完全一致で引き継いだ数、fuzzy として引き継いだ数、新しく加わった数、廃止エントリにした数 |
| `translate` | `translate: A translated, B fuzzy, C untranslated, D missing, E total, P.Q% translated` | 文書の中の翻訳する文字列の出現の数を、訳で置き換えた、fuzzy、訳が空、`.po` にない、の状態ごとに数えたものと合計、翻訳率 |

- `translate` の件数は、`.po` のエントリの数ではなく、文書の中で翻訳する文字列が出現する箇所の数です。同じ文字列が 2 回出現すれば 2 と数えます。
- 翻訳率は `A ÷ E` を百分率で、小数点以下 1 桁に丸めて表示します。fuzzy の箇所は、`--use-fuzzy` の有無にかかわらず翻訳済みに数えません。
- `translate` で翻訳する文字列が文書に 1 つもない（合計が 0）ときは、翻訳率の代わりに `translate: no translatable messages` と表示します。
- 各項目の読み方は [翻訳フロー](workflow.md) の各段階の説明を参照してください。

`-q`（`--quiet`）を指定すると、上の表の統計と `init: using language ...` の情報の表示を抑止します。警告とエラーの表示は、`-q` を指定しても抑止しません。

## 診断の表示と終了コード

エラーと警告（診断）は、英語のメッセージで、1 件につき 1 行ずつ標準エラー出力に表示します。複数の診断があるときは、すべてを順に表示します。表示の形式は次の 3 つです。

| 形式 | 使われるとき | 例 |
| --- | --- | --- |
| `<path>:<line>: error: <message>`、`<path>:<line>: warning: <message>` | ファイルの中の位置（1 から数える行番号）が分かる診断 | `hello.ja.po:12: error: unterminated string` |
| `<path>: error: <message>` | ファイルは分かるが行が決まらない診断（ファイルを開けないなど） | `hello.md: error: cannot open file for reading: No such file or directory` |
| `monde: error: <message>` | 特定のファイルに結び付かない診断（引数の誤りなど） | `monde: error: missing argument <INPUT>` |

`<path>` は、コマンドラインで指定したパスそのものです。標準入力から読んだ入力は `-` と表示されます。未知のサブコマンドやオプション、値の欠落、位置引数の過不足などの引数の誤りのときは、メッセージの後に、ヘルプの表示方法を示す 1 行（`Run 'monde --help' for usage.` または `Run 'monde <command> --help' for usage.`）を表示します。

終了コードの意味は次のとおりです。

| 終了コード | 意味 |
| --- | --- |
| `0` | 成功。警告だけが表示された場合も含みます。 |
| `1` | 処理の失敗。入力の内容のエラー（Markdown や `.pot` / `.po` の誤り、正しくない UTF-8、フロントマターの解析の失敗など）、ファイルの読み書きのエラー、メモリの確保の失敗。 |
| `2` | 引数の誤り。引数なしの実行、未知のサブコマンドやオプション、値の欠落、位置引数の過不足、2 つ以上の入力への `-` の指定、`init` で言語コードを決められない、または言語コードが不正。 |

終了コードが 0 でないときは、標準出力に結果の一部を書きません。

## 文字コードと改行コード

- 入力はすべて UTF-8 として読みます。正しい UTF-8 でない入力は、パスと位置を示すエラーになり、終了コード 1 で終了します。
- 入力の先頭に BOM（バイト順マーク）があれば、取り除いてから処理します。
- `translate` の入力の Markdown に BOM があるときは、出力の翻訳済みの Markdown の先頭にも BOM を付けます。
- `extract`、`init`、`update` の出力（`.pot` / `.po`）は、入力の BOM の有無にかかわらず BOM なしで書きます。
- 出力の改行コードは、主となる入力の改行コードに合わせます。主となる入力は、`extract` と `translate` では Markdown、`init` では `.pot`、`update` では既存の `.po`（`<PO>`）です。
  - 主となる入力に LF と CRLF が混ざっているときは、多いほうに合わせます（同じ数なら先に現れたほう）。
  - 主となる入力に改行が 1 つもないときは、LF にします。
- 標準入力から読むときと標準出力に書くときも、改行コードを変換せず、バイト列をそのまま扱います（Windows でも同じです）。

## 翻訳の対象になる文字列

`extract` が `.pot` に取り出し、`translate` が訳に置き換える文字列（翻訳する文字列）は、次の規則で決まります。

- 翻訳の単位はブロックです。見出し（`#` の見出しと、下線の見出し）の内容、段落（リストの項目の中と引用の中の段落を含む）の内容が、それぞれ 1 つの文字列（1 つの msgid）になります。段落が複数の行にわたるときは、引用の `>` やリストの記号、インデントを除いた各行を改行でつないだものが msgid になります。
- 強調、リンク、画像、コードスパン、インライン HTML、バックスラッシュのエスケープ、文字参照などのインラインの記法は、原文の書き方のまま msgid に含まれます（例: `See [GNU gettext](https://www.gnu.org/software/gettext/) for *details*.`）。訳文の中でも同じ記法を書きます。
- 画像の alt は、それを含む段落や見出しの msgid の一部（`![alt](https://example.com/image.png)` の形）として翻訳の対象になります。alt だけが別の msgid になることはありません。
- リンク先の URL は msgid の中に含まれますが、訳す対象ではありません。訳文でも原文と同じ URL を書きます。
- コードブロック（フェンスとインデントのもの）、HTML ブロック、リンク参照定義、区切り線は翻訳の対象にならず、原文のまま出力されます。内容が空または空白だけの見出しと段落も対象になりません。
- YAML フロントマターは、`--front-matter-keys` で指定したキーパスに一致する値だけが翻訳の対象になります。指定しなければ、フロントマター全体が原文のまま出力されます。
- 文書の中で同じ文字列が何度出現しても、`.pot` / `.po` では 1 つのエントリ（1 つの msgid）にまとめられます。エントリの参照コメント（`#:`）には、出現した箇所の行番号がすべて付きます。`translate` では、そのエントリの訳がすべての出現に使われます。

初版で対応していない Markdown の記法（GFM の表など）の扱いは [既知の制限](limitations.md) を参照してください。

## 出力先のファイルの扱い

- `-o` で指定した出力先のファイルがすでに存在するときは、確認を求めずに内容を置き換えます。
- 出力先が入力のいずれかと同じパスでもかまいません（`update` で既存の `.po` を上書きする場合など）。すべての入力を読み終えてから出力を書くため、入力の内容は失われません。
- 処理がエラーで失敗したとき（終了コード 1 または 2）は、出力先のファイルを作成も変更もしません。出力先は、処理が成功したときにだけ書かれます。既存のファイルを出力先に指定していても、失敗したときは元の内容のまま残ります。
- 入力ファイルが存在しない、読めない、または出力先に書けないときは、そのパスと原因を示すエラーを表示し、終了コード 1 で終了します。
