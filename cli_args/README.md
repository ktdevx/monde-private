# CLOpts

CLOpts は、C で書かれたコマンドライン引数の解析ライブラリです。

## 概要

- 呼び出し側が `static const` の構造体で書いたコマンドの宣言に基づいて、引数列を解析します。サブコマンドなし、1 段のサブコマンド、入れ子のサブコマンドを扱えます。
- オプションは、値を取らないフラグ（`CLOPTS_FLAG`）、単一の値（`CLOPTS_VALUE`）、カンマ区切りのリスト値（`CLOPTS_LIST`）の 3 種類です。長い名前（`--name`）と 1 文字の短い名前（`-n`）、必須の指定、既定値、子孫のコマンドへの引き継ぎを宣言できます。フラグは、`--no-<name>` で無効にできる否定できるフラグとして宣言でき、既定で有効にもできます（[否定できるフラグ](#否定できるフラグ)）。位置引数の個数の下限と上限も宣言できます。
- フラグが指定された回数を取得できます。`-vvv` のように重ねた回数で、詳しさなどの段階を指定させられます（[フラグの出現回数](#フラグの出現回数)）。
- オプションは、ヘルプに出さない隠しオプションとして宣言できます（[隠しオプション](#隠しオプション)）。長い名前には、同じオプションを指定できる別名を付けられます（[オプションの別名](#オプションの別名)）。
- `-h`、`--help`、子コマンドを持つコマンドでの `help` の語、ルートコマンドの `--version` を組み込みで解釈し、ヘルプ、バージョン、使い方の案内の文字列を宣言から生成します。
- エラーは標準エラー出力に書かず、プロセスも終了しません。英語の UTF-8 のメッセージをエラーの列（`clopts_errors_t`）に追加し、戻り値で結果を返します。表示の方法は利用側が決めます。
- Windows では、プロセスのコマンドラインを UTF-16 から UTF-8 に変換した引数列を取得できます。
- 実行時の依存は C の標準ライブラリと OS のライブラリ（Windows の `shell32`）だけです。

ビルドすると静的ライブラリのターゲット `clopts` と、その別名 `CLOpts::clopts` ができます。利用側は `CLOpts::clopts` にリンクしてください。

### 公開ヘッダ

| ヘッダ | 内容 |
|--------|------|
| `clopts/clopts.h` | 宣言の型（`clopts_program_t`、`clopts_command_t`、`clopts_option_t`、`clopts_positional_t`）、解析（`clopts_parse`）、解析結果の参照（`clopts_value`、`clopts_flag`、`clopts_flag_count`、`clopts_is_set`、`clopts_positional_at` など） |
| `clopts/errors.h` | エラーの列 `clopts_errors_t` の読み取り（`clopts_errors_count`、`clopts_errors_at`、`clopts_errors_dropped`）と解放（`clopts_errors_free`） |
| `clopts/help.h` | ヘルプ、バージョン、使い方の案内、コマンドの経路の生成（`clopts_format_*`）と、その解放（`clopts_text_free`） |
| `clopts/argv.h` | UTF-8 の引数列の取得（`clopts_argv_get`）と解放（`clopts_argv_free`） |

公開ヘッダは `extern "C"` で囲んであるので、C++ からも取り込めます。

### エラーの返し方

| 状況 | 戻り値 | エラーの列 |
|------|--------|------------|
| 利用者の入力の誤り | `CLOPTS_USAGE_ERROR` | 1 件 |
| 宣言の誤り（プログラムの不具合） | `CLOPTS_DECL_ERROR` | 1 件以上 |
| 解析中のメモリ不足 | `CLOPTS_OUT_OF_MEMORY` | 1 件（記録できなければ `clopts_errors_dropped` が `true`） |
| 引数列の取得の失敗 | `false` | 1 件（記録できなければ `clopts_errors_dropped` が `true`） |
| 文字列の生成でのメモリ不足 | `NULL` | 使わない |

解析に成功すると `CLOPTS_OK`、ヘルプやバージョンの要求なら `CLOPTS_HELP`、`CLOPTS_VERSION`、子コマンドを持つコマンドで子コマンドが指定されなければ `CLOPTS_NO_COMMAND` を返します。利用者の入力の誤りのメッセージは、`greet: unknown option '--x'` のようにコマンドの経路で始まります。

## 対応する環境

- CMake 3.21 以上
- C99 のコンパイラ（MSVC では `/std:c11` で C99 の範囲を使います）
- 継続的な検証を行っている環境
  - Windows: MSVC、MinGW-w64
  - macOS: Apple Clang
  - Linux: GCC、Clang

テストには、テストフレームワーク [Naos](https://github.com/vabelorg/naos) の v0.4.0 を使います。テストを有効にして構成すると、CMake の FetchContent で取得します。ライブラリだけを使う場合は不要です。

## 単独構成でのビルドとテスト

このディレクトリを最上位のプロジェクトとして構成すると、既定でテストもビルドし、CTest に登録します。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

テストには、単体テスト、このディレクトリが外のファイルに依存していないことの検査、インストールして `find_package(CLOpts)` で使えることの検証が含まれます。いずれもラベル `clopts` が付いています。

ネットワークに接続できない環境では、手元にある Naos のソースの場所を指定すると、そのソースを使います。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_SOURCE_DIR_NAOS=/path/to/naos
```

### 構成のオプション

| オプション | 既定値 | 内容 |
|------------|--------|------|
| `CLOPTS_BUILD_TESTS` | 最上位なら `ON`、取り込まれたなら `OFF` | テストをビルドし、CTest に登録する。`OFF` なら Naos を取得しない |
| `CLOPTS_INSTALL` | 最上位なら `ON`、取り込まれたなら `OFF` | インストールの規則と、`find_package` 用のパッケージ構成ファイルを定義する |
| `CLOPTS_WARNINGS_AS_ERRORS` | 最上位なら `ON`、取り込まれたなら `OFF` | CLOpts のソースの警告をエラーとして扱う |

警告の水準と言語規格の指定は、CLOpts のターゲットだけに付けます。`CMAKE_C_FLAGS` などの全体に効く変数は変更しないので、取り込んだ側のビルドには影響しません。

## 取り込んで使う方法

### add_subdirectory

CLOpts のディレクトリを自分のプロジェクトの中に置き、`add_subdirectory` で取り込みます。オプションを変えたい場合は、取り込む前に通常の変数として設定します。

```cmake
# 例: third_party/clopts に置いた場合
set(CLOPTS_BUILD_TESTS OFF)
set(CLOPTS_WARNINGS_AS_ERRORS ON)
add_subdirectory(third_party/clopts)

target_link_libraries(my_app PRIVATE CLOpts::clopts)
```

取り込んだ構成で `CLOPTS_BUILD_TESTS` を `ON` にする場合は、取り込む側で `enable_testing()`（または `include(CTest)`）を呼んでおいてください。取り込む側がすでに Naos を用意している（`naos_add_executable` が定義済みの）場合は、Naos を取得せずにそれを使います。

### FetchContent

```cmake
include(FetchContent)

set(CLOPTS_BUILD_TESTS OFF)
FetchContent_Declare(CLOpts
  GIT_REPOSITORY <CLOpts のリポジトリの URL>
  GIT_TAG <タグまたはコミット>
)
FetchContent_MakeAvailable(CLOpts)

target_link_libraries(my_app PRIVATE CLOpts::clopts)
```

どちらの方法でも、公開ヘッダの場所と、必要な OS のライブラリは `CLOpts::clopts` から伝わります。

## インストールして find_package(CLOpts) で使う方法

単独構成でビルドし、任意の場所にインストールします。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCLOPTS_BUILD_TESTS=OFF
cmake --build build --config Release
cmake --install build --config Release --prefix /path/to/prefix
```

インストールされるファイルは次のとおりです（テスト、内部ヘッダ、Naos はインストールしません）。

| 対象 | インストール先 |
|------|----------------|
| 静的ライブラリ | `<prefix>/${CMAKE_INSTALL_LIBDIR}` |
| 公開ヘッダ | `<prefix>/${CMAKE_INSTALL_INCLUDEDIR}/clopts` |
| `CLOptsConfig.cmake`、`CLOptsConfigVersion.cmake`、`CLOptsTargets*.cmake` | `<prefix>/${CMAKE_INSTALL_LIBDIR}/cmake/CLOpts` |
| `LICENSE` | `<prefix>/${CMAKE_INSTALL_DATADIR}/licenses/CLOpts` |

パッケージ構成ファイルには絶対パスを埋め込まないので、インストールした後にディレクトリごと別の場所へ移しても使えます。

利用側のプロジェクトでは、`find_package` で見つけた `CLOpts::clopts` にリンクします。

```cmake
cmake_minimum_required(VERSION 3.21)
project(greet LANGUAGES C)

find_package(CLOpts 0.3 CONFIG REQUIRED)

add_executable(greet main.c)
target_link_libraries(greet PRIVATE CLOpts::clopts)
```

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/prefix -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

1.0 より前のため、バージョンの照合は `SameMinorVersion` です。`find_package(CLOpts 0.3)` は 0.3.x に一致し、マイナーバージョンの違う CLOpts は見つからないものとして扱われます。

## 最小の使用例

オプション 2 つと位置引数 1 つを持つ、サブコマンドのないプログラムです。プロセスの引数を UTF-8 の引数列として取得して解析し、解析の結果に応じてヘルプ、バージョン、エラーを表示します。

```c
#include "clopts/argv.h"
#include "clopts/clopts.h"
#include "clopts/errors.h"
#include "clopts/help.h"

#include <stdbool.h>
#include <stdio.h>

static const clopts_option_t root_options[] = {
    { .long_name = "greeting", .short_name = 'g', .kind = CLOPTS_VALUE, .value_name = "WORD",
      .help = "Greeting word", .default_value = "Hello" },
    { .long_name = "excited", .short_name = 'e', .kind = CLOPTS_FLAG, .help = "End with '!'" },
};

static const clopts_positional_t root_positionals[] = {
    { .name = "NAME", .help = "Name to greet" },
};

static const clopts_command_t root_command = {
    .description = "Print a greeting.",
    .options = root_options,
    .option_count = sizeof(root_options) / sizeof(root_options[0]),
    .positionals = root_positionals,
    .positional_count = 1,
    .min_positionals = 1,
    .max_positionals = 1,
};

static const clopts_program_t program = {
    .name = "greet",
    .version = "1.0.0",
    .root = &root_command,
};

/* エラーの列の各メッセージを標準エラー出力に書く */
static void print_errors(const clopts_errors_t *errors)
{
    size_t i;

    for (i = 0; i < clopts_errors_count(errors); ++i)
    {
        fprintf(stderr, "error: %s\n", clopts_errors_at(errors, i));
    }
    if (clopts_errors_dropped(errors))
    {
        fputs("error: some errors could not be recorded\n", stderr);
    }
}

/* 生成した文字列を stream に書いて解放する。NULL ならメモリ不足として false を返す */
static bool write_text(FILE *stream, char *text)
{
    if (text == NULL)
    {
        fputs("error: out of memory\n", stderr);
        return false;
    }
    fputs(text, stream);
    clopts_text_free(text);
    return true;
}

int main(int argc, char **argv)
{
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;
    clopts_argv_t args = CLOPTS_ARGV_INIT;
    clopts_result_t result = CLOPTS_RESULT_INIT;
    int exit_code = 1;

    if (!clopts_argv_get(argc, argv, &args, &errors))
    {
        print_errors(&errors);
        clopts_errors_free(&errors);
        return 1;
    }

    switch (clopts_parse(&program, args.argc, args.argv, &result, &errors))
    {
    case CLOPTS_OK:
        printf("%s, %s%s\n", clopts_value(&result, "greeting"), clopts_positional_at(&result, 0),
               clopts_flag(&result, "excited") ? "!" : ".");
        exit_code = 0;
        break;
    case CLOPTS_HELP:
        exit_code = write_text(stdout, clopts_format_help(&result)) ? 0 : 1;
        break;
    case CLOPTS_VERSION:
        exit_code = write_text(stdout, clopts_format_version(&program)) ? 0 : 1;
        break;
    case CLOPTS_USAGE_ERROR:
        print_errors(&errors);
        write_text(stderr, clopts_format_usage_hint(&result));
        exit_code = 2;
        break;
    default:
        /* CLOPTS_NO_COMMAND はサブコマンドを宣言したときだけ起きる */
        print_errors(&errors);
        exit_code = 1;
        break;
    }

    clopts_result_free(&result);
    clopts_argv_free(&args);
    clopts_errors_free(&errors);
    return exit_code;
}
```

実行の例です。

```console
$ greet World
Hello, World.
$ greet --greeting Hi -e World
Hi, World!
$ greet --unknown World
error: greet: unknown option '--unknown'
Run 'greet --help' for usage.
```

`greet --help` は、宣言から生成したヘルプを表示します。

## 否定できるフラグ

フラグ（`CLOPTS_FLAG`）の宣言で `negatable` を `true` にすると、`--<name>` で有効に、`--no-<name>` で無効にできます。シェルの別名や先に書いた指定で有効になったフラグを、コマンドラインの後ろで打ち消すための機能です。さらに `default_enabled` を `true` にすると、指定がないときに有効なフラグになります。

```c
static const clopts_option_t options[] = {
    { .long_name = "color", .short_name = 'c', .kind = CLOPTS_FLAG, .help = "Colorize the output",
      .negatable = true },
    { .long_name = "sync", .kind = CLOPTS_FLAG, .help = "Flush after each file", .negatable = true,
      .default_enabled = true },
};
```

`negatable` と `default_enabled` の既定は、どちらも `false`（否定できない、既定で無効）です。指示付き初期化子で書いた既存の宣言は、そのままビルドでき、挙動も変わりません。C++ の指示付き初期化子は宣言の順に書く必要があります。`inherited` より後ろのメンバーの順は、`negatable`、`default_enabled`、`hidden`、`aliases` です。

解析の規則は次のとおりです。

- 肯定の形（`--color`、`-c`、短い名前のまとめ書きの中の `c`）と否定の形（`--no-color`）を合わせて何回指定しても、エラーにはならず、引数列で最後に現れた指定が残ります。子孫に引き継ぐフラグでは、子コマンドの名前の前後にまたがる指定にも同じ規則を当てはめます。
- `--no-<name>` は、まず `no-<name>` という長い名前のオプションとして探し、見つからなければ `<name>` の否定の形として探します。このため、`no-cache` のような名前のフラグは、これまでどおり `--no-cache` で指定できます。
- 否定の形は長い名前だけで、短い名前の否定の形はありません。`--no-color=1` のように値を付けると `option '--no-color' does not take a value` に、否定できないフラグ、値付きオプション、リスト値の `--no-<name>` は `unknown option '--no-<name>'` になります。

結果は、ほかのフラグと同じ 2 つの関数で取得します。

| 関数 | 戻り値 |
|------|--------|
| `clopts_flag(&result, "color")` | フラグが最終的に有効なら `true`。指定がなければ既定の状態（`default_enabled` の値） |
| `clopts_is_set(&result, "color")` | 利用者が肯定の形か否定の形で明示的に指定したなら `true` |

既定で有効のフラグも、最終的な状態は `clopts_flag` だけで得られます。既定の状態か明示的な指定かを区別したいときだけ、`clopts_is_set` を併せて使ってください。否定できないフラグでは、2 つの関数の戻り値は常に一致します。

ヘルプでは、否定できるフラグの長い名前を `--[no-]<name>` の形で示し、既定で有効のフラグの説明には ` (default: on)` を付けます。上の宣言をルートコマンドに付けた場合、ヘルプの `Options:` は次のようになります。

```text
Options:
  -c, --[no-]color  Colorize the output
      --[no-]sync   Flush after each file (default: on)
  -h, --help        Show this help
      --version     Show version information
```

次の宣言は宣言の誤り（`CLOPTS_DECL_ERROR`）になります。

- フラグではないオプション（`CLOPTS_VALUE`、`CLOPTS_LIST`）の `negatable` を `true` にする
- 否定できるフラグではないオプション（否定できないフラグ、`CLOPTS_VALUE`、`CLOPTS_LIST`）の `default_enabled` を `true` にする
- 否定できるフラグ `<name>` と、長い名前が `no-<name>` のオプションを、同じコマンド、またはコマンドとその祖先に宣言する（祖先の側のオプションが引き継がないものでも誤りです）

フラグに `default_value` を付けると宣言の誤りになる点は、否定できるフラグでも同じです。既定で有効にするには `default_enabled` を使ってください。

## フラグの出現回数

フラグの出現回数は、利用者がそのフラグを肯定の形で指定した回数です。`clopts_flag_count` で取得します。`-v` を重ねるほど詳しいメッセージを出す、といった段階の指定に使えます。どのフラグの出現回数も取得でき、宣言に書き足すものはありません。

```c
static const clopts_option_t options[] = {
    { .long_name = "verbose", .short_name = 'v', .kind = CLOPTS_FLAG, .help = "Print more messages",
      .inherited = true },
    { .long_name = "color", .short_name = 'c', .kind = CLOPTS_FLAG, .help = "Colorize the output",
      .negatable = true },
    { .long_name = "sync", .kind = CLOPTS_FLAG, .help = "Flush after each file", .negatable = true,
      .default_enabled = true },
};
```

```c
size_t verbosity = clopts_flag_count(&result, "verbose");

if (verbosity >= 2)
{
    puts("debug messages are on");
}
```

数え方の規則は次のとおりです。

- 肯定の形の指定を、1 つにつき 1 回と数えます。長い名前（`--verbose`）、短い名前（`-v`）、短い名前のまとめ書きの中の 1 文字ずつ（`-vvv` は 3 回）、別名（[オプションの別名](#オプションの別名)）のどれも同じです。
- 否定できるフラグの否定の形（`--no-color`）は、出現回数を 0 に戻します。その後ろの肯定の形は、また 1 から数えます。
- 指定がなければ 0 です。既定で有効のフラグは、解析に成功したとき（`CLOPTS_OK`）に 1 になります。
- 子孫に引き継ぐフラグでは、子コマンドの名前の前と後の指定を合わせて数えます。
- `clopts_flag` が `true` を返すのは、出現回数が 1 以上のときで、そのときだけです。
- 値付きオプション、リスト値、宣言のない名前、別名の名前には 0 を返します。

上の宣言をルートコマンドに付け、子コマンド `run` を持たせたプログラム `tool` での例です。

| 引数列 | `clopts_flag_count` |
|--------|---------------------|
| `tool run` | `verbose` は 0、`color` は 0、`sync` は 1 |
| `tool -vvv run` | `verbose` は 3 |
| `tool -v run --verbose -v` | `verbose` は 3 |
| `tool -c -c run` | `color` は 2 |
| `tool -c -c --no-color -c run` | `color` は 1 |
| `tool -c --no-color run` | `color` は 0 |
| `tool --sync --sync run` | `sync` は 2 |
| `tool --no-sync run` | `sync` は 0 |

## 隠しオプション

隠しオプションは、ヘルプに出さないオプションです。オプションの宣言で `hidden` を `true` にします。廃止の予定のオプションや実験中のオプションを、利用者に案内せずに受け付けるための機能です。フラグ、値付きオプション、リスト値のどれにも指定できます。

```c
static const clopts_option_t options[] = {
    { .long_name = "verbose", .short_name = 'v', .kind = CLOPTS_FLAG, .help = "Print more messages" },
    { .long_name = "trace-parser", .kind = CLOPTS_FLAG, .help = "Trace the parser", .hidden = true },
    { .long_name = "legacy-format", .kind = CLOPTS_VALUE, .value_name = "NAME",
      .help = "Use an old output format", .hidden = true },
};
```

`hidden` の既定は `false`（ヘルプに出す）です。指示付き初期化子で書いた既存の宣言は、そのままビルドでき、挙動も変わりません。

隠しオプションは、ヘルプのオプションの一覧に出ず、一覧の列の幅の計算にも入りません。子孫に引き継ぐ隠しオプションは、子孫のコマンドのヘルプにも出ません。`-h, --help` と、ルートコマンドの `--version` は常に出ます。上の宣言をルートコマンドに付けた場合、ヘルプの `Options:` は次のようになります。

```text
Options:
  -v, --verbose  Print more messages
  -h, --help     Show this help
      --version  Show version information
```

子コマンドを持つコマンドの有効なオプションがすべて隠しオプションなら、ヘルプの使い方の行は、オプションを 1 つも宣言しないコマンドと同じになります。

ヘルプ以外の扱いは、ほかのオプションと変わりません。指定は同じ規則で解析され、結果は同じ関数で取得でき、指定の誤りは `option '--legacy-format' requires a value` のように同じ文面のエラーになります。

必須のオプション（`required` が `true`）の `hidden` を `true` にすると、宣言の誤り（`CLOPTS_DECL_ERROR`）になります。

## オプションの別名

別名は、オプションの長い名前の代わりに指定できる名前です。オプションの宣言の `aliases` に、カンマ区切りの 1 つの文字列で書きます。オプションの名前を変えた後も、古い名前を使った手順やスクリプトを動かし続けるための機能です。カンマで区切るので、別名はカンマを含められません。

```c
static const clopts_option_t options[] = {
    { .long_name = "output", .short_name = 'o', .kind = CLOPTS_VALUE, .value_name = "FILE",
      .help = "Write the result to FILE", .required = true, .aliases = "out,outfile" },
    { .long_name = "color", .kind = CLOPTS_FLAG, .help = "Colorize the output", .negatable = true,
      .aliases = "colour" },
};
```

`aliases` は `clopts_option_t` の最後のメンバーで、既定は `NULL`（別名なし）です。`hidden` と同じく、指示付き初期化子で書いた既存の宣言には影響しません。

照合と取得の規則は次のとおりです。

- `--<別名>` と `--<別名>=<値>` を、元の長い名前で指定した場合と同じに扱います。否定できるフラグでは、`--no-<別名>` も否定の形として受け付けます。別名も長い名前と同じく全体が一致したときだけ受け付け、前方一致や大文字と小文字の違う指定は `unknown option` になります。
- 元の長い名前、短い名前、別名による指定は、同じオプションの繰り返しです。値は最後の指定が残り、リスト値は連結し、フラグは最後の指定が残り、出現回数は合わせて数えます。必須のオプションは、別名による指定でも満たされます。
- 結果は、元の長い名前でだけ取得します。`clopts_value(&result, "out")` のように別名の名前を渡すと、宣言のない名前と同じ結果になります。
- 指定の誤りのエラーは、利用者が書いた別名を示します。必須のオプションの欠落のエラーは、元の長い名前を示します。
- ヘルプには別名を出しません。別名を持つオプションの行は、別名を持たない場合と同じです。

上の宣言をルートコマンドに付け、解析に成功したら `output` の値と `color` の状態を表示するプログラム `pack` での例です。

```console
$ pack --out a.txt
output: a.txt, color: off
$ pack -o a.txt --outfile=b.txt --colour
output: b.txt, color: on
$ pack --out a.txt --colour --no-colour
output: a.txt, color: off
$ pack --out
error: pack: option '--out' requires a value
Run 'pack --help' for usage.
$ pack --colour
error: pack: missing required option '--output'
Run 'pack --help' for usage.
```

次の宣言は宣言の誤り（`CLOPTS_DECL_ERROR`）になります。以下の「名前」は、長い名前と別名の両方を指します。

- 長い名前として受け付けない形の別名を書く。空の別名、先頭が `-` の別名、`=` を含む別名です。`aliases` が空文字列の場合と、カンマが先頭や末尾にある場合、連続している場合は、空の別名として扱います
- 組み込みで解釈する名前（`help`、`version`）を別名にする
- 同じ名前を、同じコマンドの 2 つのオプション、1 つのオプションの中（長い名前と別名、または 2 つの別名）、コマンドとその祖先のオプション（祖先の側のオプションが引き継がないものでも誤りです）に宣言する
- 否定できるフラグの名前 `<name>` と、名前 `no-<name>` を、同じコマンド、またはコマンドとその祖先に宣言する。否定できるフラグが自分の別名として `no-<name>` を持つ場合も含みます


## ライセンス

CLOpts は MIT ライセンスで提供します。全文はこのディレクトリの [LICENSE](LICENSE) を参照してください。
