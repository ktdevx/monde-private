#ifndef MONDE_CLI_ARGS_ARGS_H
#define MONDE_CLI_ARGS_ARGS_H

#include "clopts/errors.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * 汎用のコマンドライン引数パーサ。呼び出し側が static const で書く宣言に基づいて、
 * ルートコマンドを起点とするコマンドの木と、オプションと位置引数を解析する
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
    bool inherited;             /* true なら、宣言したコマンドの子孫でも有効（1.5） */
} mnd_args_option_t;

typedef struct mnd_args_positional
{
    const char *name;  /* ヘルプとメッセージ上の名前（例: "INPUT"）。山括弧を含まない */
    const char *help;  /* 説明。NULL 可 */
} mnd_args_positional_t;

#define MND_ARGS_UNLIMITED ((size_t)-1)

typedef struct mnd_args_command mnd_args_command_t;

/*
 * コマンドの宣言。指示付き初期化子で書き、書かなかったメンバーの 0 が「なし」を表す。
 * サブコマンドなしは root->command_count == 0、1 段はルートの子がすべて末端コマンド、
 * 入れ子は子コマンドがさらに commands を持つ（1.3、1.4）
 */
struct mnd_args_command
{
    const char *name;                         /* 子コマンド名。ルートコマンドでは使わない（NULL 可） */
    const char *summary;                      /* 親のヘルプの一覧に出す 1 行の説明。ルートコマンドでは使わない */
    const char *description;                  /* このコマンドのヘルプの説明。NULL なら summary を使う */
    const mnd_args_option_t *options;         /* option_count == 0 なら NULL 可 */
    size_t option_count;
    const mnd_args_positional_t *positionals; /* 末端コマンドだけが持てる。positional_count == 0 なら NULL 可 */
    size_t positional_count;
    size_t min_positionals;
    size_t max_positionals;                   /* MND_ARGS_UNLIMITED なら上限なし */
    const mnd_args_command_t *commands;       /* 子コマンド。command_count == 0 なら末端コマンド（NULL 可） */
    size_t command_count;
};

typedef struct mnd_args_program
{
    const char *name;                /* プログラム名。ルートコマンドの名前として使う */
    const char *version;             /* バージョン文字列 */
    const mnd_args_command_t *root;  /* ルートコマンド。必須 */
} mnd_args_program_t;

typedef enum mnd_args_status
{
    MND_ARGS_OK,             /* 解析に成功。末端コマンドが選択されている */
    MND_ARGS_HELP,           /* ヘルプの要求。対象は結果の経路の最後のコマンド */
    MND_ARGS_VERSION,        /* バージョンの要求 */
    MND_ARGS_NO_COMMAND,     /* 子を持つコマンドで子コマンドが指定されなかった。対象は経路の最後のコマンド */
    MND_ARGS_USAGE_ERROR,    /* 利用者の入力の誤り。エラーを 1 件追加済み */
    MND_ARGS_DECL_ERROR,     /* 宣言の誤り（プログラムの不具合）。エラーを 1 件以上追加済み */
    MND_ARGS_OUT_OF_MEMORY   /* メモリ不足。エラーを 1 件追加済み（記録できなければ dropped） */
} mnd_args_status_t;

typedef struct mnd_args_value
{
    bool explicit_set;   /* 利用者が明示的に指定した */
    const char *value;   /* 単一の値: 引数列か既定値を指す。なければ NULL */
    char **items;        /* リスト値: 確保した項目の配列 */
    size_t item_count;
    size_t item_cap;
} mnd_args_value_t;

/* 経路上の 1 つの段 */
typedef struct mnd_args_level
{
    const mnd_args_command_t *command;
    mnd_args_value_t *values;   /* command->option_count 個。0 個なら NULL */
} mnd_args_level_t;

typedef struct mnd_args_result
{
    mnd_args_status_t status;
    const mnd_args_program_t *program;  /* 解析に使った宣言。解析前は NULL */
    mnd_args_level_t *levels;           /* levels[0] がルートコマンド、最後が選択中のコマンド */
    size_t level_count;
    size_t level_cap;
    const char **positionals;           /* 引数列を指す */
    size_t positional_count;
    size_t positional_cap;
} mnd_args_result_t;

#define MND_ARGS_RESULT_INIT { MND_ARGS_NO_COMMAND, NULL, NULL, 0, 0, NULL, 0, 0 }

void mnd_args_result_init(mnd_args_result_t *result);

/* 確保した領域を解放して初期状態に戻す。初期状態・解放済みに対しても安全 */
void mnd_args_result_free(mnd_args_result_t *result);

/* 経路（2.8）。level_count == 0（DECL_ERROR、OUT_OF_MEMORY、解析前）なら NULL / 0 */
const mnd_args_command_t *mnd_args_command(const mnd_args_result_t *result);   /* 選択されたコマンド（最後の段） */
size_t mnd_args_depth(const mnd_args_result_t *result);                        /* 段の数。ルートだけなら 1 */
const mnd_args_command_t *mnd_args_command_at(const mnd_args_result_t *result, size_t level); /* level < depth */

/* 以下、long_name が経路上のどのコマンドでも宣言されていない場合は false / NULL / 0 を返す（5.7） */
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
 * 戻り値は result->status と同じ。エラーは errors の末尾に追加する（errors は初期化済みであること）。
 */
mnd_args_status_t mnd_args_parse(const mnd_args_program_t *program, int argc, char *const *argv,
                                 mnd_args_result_t *result, clopts_errors_t *errors);

#endif
