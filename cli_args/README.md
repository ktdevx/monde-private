# CLOpts

CLOpts は、C で書かれたコマンドライン引数の解析ライブラリです。

## 概要

- 呼び出し側が `static const` の構造体で書いたコマンドの宣言に基づいて、引数列を解析します。サブコマンドなし、1 段のサブコマンド、入れ子のサブコマンドを扱えます。
- オプションは、値を取らないフラグ（`CLOPTS_FLAG`）、単一の値（`CLOPTS_VALUE`）、カンマ区切りのリスト値（`CLOPTS_LIST`）の 3 種類です。長い名前（`--name`）と 1 文字の短い名前（`-n`）、必須の指定、既定値、子孫のコマンドへの引き継ぎを宣言できます。位置引数の個数の下限と上限も宣言できます。
- `-h`、`--help`、子コマンドを持つコマンドでの `help` の語、ルートコマンドの `--version` を組み込みで解釈し、ヘルプ、バージョン、使い方の案内の文字列を宣言から生成します。
- エラーは標準エラー出力に書かず、プロセスも終了しません。英語の UTF-8 のメッセージをエラーの列（`clopts_errors_t`）に追加し、戻り値で結果を返します。表示の方法は利用側が決めます。
- Windows では、プロセスのコマンドラインを UTF-16 から UTF-8 に変換した引数列を取得できます。
- 実行時の依存は C の標準ライブラリと OS のライブラリ（Windows の `shell32`）だけです。

ビルドすると静的ライブラリのターゲット `clopts` と、その別名 `CLOpts::clopts` ができます。利用側は `CLOpts::clopts` にリンクしてください。

### 公開ヘッダ

| ヘッダ | 内容 |
|--------|------|
| `clopts/clopts.h` | 宣言の型（`clopts_program_t`、`clopts_command_t`、`clopts_option_t`、`clopts_positional_t`）、解析（`clopts_parse`）、解析結果の参照（`clopts_value`、`clopts_flag`、`clopts_positional_at` など） |
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

find_package(CLOpts 0.1 CONFIG REQUIRED)

add_executable(greet main.c)
target_link_libraries(greet PRIVATE CLOpts::clopts)
```

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/prefix -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

1.0 より前のため、バージョンの照合は `SameMinorVersion` です。`find_package(CLOpts 0.1)` は 0.1.x に一致し、マイナーバージョンの違う CLOpts は見つからないものとして扱われます。

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

## ライセンス

CLOpts は MIT ライセンスで提供します。全文はこのディレクトリの [LICENSE](LICENSE) を参照してください。
