#ifndef MONDE_CLI_ARGS_ARGS_H
#define MONDE_CLI_ARGS_ARGS_H

#include "foundation/diag.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * 汎用のコマンドライン引数パーサ。呼び出し側が static const で書く宣言に基づいて、
 * サブコマンドとオプションと位置引数を解析する
 */

typedef enum mnd_args_kind
{
    MND_ARGS_FLAG,   /* 値を取らない */
    MND_ARGS_VALUE,  /* 単一の値。繰り返すと後勝ち */
    MND_ARGS_LIST    /* カンマ区切りのリスト値。繰り返すと連結 */
} mnd_args_kind_t;

typedef struct mnd_args_option
{
    const char *long_name;      /* 必須。先頭の "--" を含まない（例: "output"） */
    char short_name;            /* 1 文字の ASCII 英数字。ないなら '\0' */
    mnd_args_kind_t kind;
    const char *value_name;     /* ヘルプ上の値の表示名（例: "FILE"）。NULL なら "VALUE"。フラグでは使わない */
    const char *help;           /* 説明（英語、1 行）。NULL 可 */
    bool required;              /* フラグでは false でなければならない */
    const char *default_value;  /* 既定値。NULL ならなし。リスト値はカンマ区切りで書く。フラグでは NULL */
} mnd_args_option_t;

typedef struct mnd_args_positional
{
    const char *name;  /* ヘルプとメッセージ上の名前（例: "INPUT"）。山括弧を含まない */
    const char *help;  /* 説明。NULL 可 */
} mnd_args_positional_t;

#define MND_ARGS_UNLIMITED ((size_t)-1)

typedef struct mnd_args_command
{
    const char *name;                         /* サブコマンド名 */
    const char *summary;                      /* 全体のヘルプに出す 1 行の説明 */
    const char *description;                  /* サブコマンドのヘルプの説明。NULL なら summary を使う */
    const mnd_args_option_t *options;         /* option_count == 0 なら NULL 可 */
    size_t option_count;
    const mnd_args_positional_t *positionals; /* positional_count == 0 なら NULL 可 */
    size_t positional_count;
    size_t min_positionals;
    size_t max_positionals;                   /* MND_ARGS_UNLIMITED なら上限なし */
} mnd_args_command_t;

typedef struct mnd_args_program
{
    const char *name;         /* プログラム名（例: メッセージ上の "tool"） */
    const char *description;  /* 全体のヘルプの説明。NULL 可 */
    const char *version;      /* バージョン文字列 */
    const mnd_args_command_t *commands;
    size_t command_count;
} mnd_args_program_t;

typedef enum mnd_args_status
{
    MND_ARGS_OK,             /* 解析に成功。command は選択されたサブコマンド */
    MND_ARGS_HELP,           /* ヘルプの要求。command が NULL なら全体、そうでなければそのサブコマンド */
    MND_ARGS_VERSION,        /* バージョンの要求 */
    MND_ARGS_NO_COMMAND,     /* 引数がない。全体の使い方を示すべき状態 */
    MND_ARGS_USAGE_ERROR,    /* 利用者の入力の誤り。診断を 1 件追加済み */
    MND_ARGS_DECL_ERROR,     /* 宣言の誤り（プログラムの不具合）。診断を 1 件以上追加済み */
    MND_ARGS_OUT_OF_MEMORY   /* メモリ不足。診断を追加済み（記録できなければ dropped） */
} mnd_args_status_t;

typedef struct mnd_args_value
{
    bool explicit_set;   /* 利用者が明示的に指定した */
    const char *value;   /* 単一の値: 引数列か既定値を指す。なければ NULL */
    char **items;        /* リスト値: 確保した項目の配列 */
    size_t item_count;
    size_t item_cap;
} mnd_args_value_t;

typedef struct mnd_args_result
{
    mnd_args_status_t status;
    const mnd_args_command_t *command;  /* OK、HELP（サブコマンドのヘルプ）、選択後の USAGE_ERROR で非 NULL */
    mnd_args_value_t *values;           /* command->option_count 個。command が NULL なら NULL */
    const char **positionals;           /* 引数列を指す */
    size_t positional_count;
    size_t positional_cap;
} mnd_args_result_t;

#define MND_ARGS_RESULT_INIT { MND_ARGS_NO_COMMAND, NULL, NULL, NULL, 0, 0 }

void mnd_args_result_init(mnd_args_result_t *result);

/* 確保した領域を解放して初期状態に戻す。初期状態・解放済みに対しても安全 */
void mnd_args_result_free(mnd_args_result_t *result);

/* 以下、long_name が選択されたサブコマンドで宣言されていない場合は false / NULL / 0 を返す */
bool mnd_args_is_set(const mnd_args_result_t *result, const char *long_name);   /* 明示的な指定（5.6） */
bool mnd_args_flag(const mnd_args_result_t *result, const char *long_name);     /* フラグが指定された */
const char *mnd_args_value(const mnd_args_result_t *result, const char *long_name); /* 単一の値または既定値 */
size_t mnd_args_list_count(const mnd_args_result_t *result, const char *long_name);
const char *mnd_args_list_at(const mnd_args_result_t *result, const char *long_name, size_t index);
size_t mnd_args_positional_count(const mnd_args_result_t *result);
const char *mnd_args_positional_at(const mnd_args_result_t *result, size_t index); /* index < count */

/*
 * argv[0] を読み飛ばし、argv[1]〜argv[argc - 1] を解析する。
 * result は初期化済みであること。以前の内容は解放してから書き込む。
 * 戻り値は result->status と同じ。
 */
mnd_args_status_t mnd_args_parse(const mnd_args_program_t *program, int argc, char *const *argv,
                                 mnd_args_result_t *result, mnd_diag_list_t *diags);

#endif
