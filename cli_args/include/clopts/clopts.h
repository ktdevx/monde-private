#ifndef CLOPTS_CLOPTS_H
#define CLOPTS_CLOPTS_H

#include "clopts/errors.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 汎用のコマンドライン引数パーサ。呼び出し側が static const で書く宣言に基づいて、
 * ルートコマンドを起点とするコマンドの木と、オプションと位置引数を解析する
 */

typedef enum clopts_kind
{
    CLOPTS_FLAG,   /* 値を取らない */
    CLOPTS_VALUE,  /* 単一の値。繰り返すと後勝ち */
    CLOPTS_LIST    /* カンマ区切りのリスト値。繰り返すと連結 */
} clopts_kind_t;

typedef struct clopts_option
{
    const char *long_name;      /* 必須。先頭の "--" を含まない（例: "output"） */
    char short_name;            /* 1 文字の ASCII 英数字。ないなら '\0' */
    clopts_kind_t kind;
    const char *value_name;     /* ヘルプ上の値の表示名（例: "FILE"）。NULL なら "VALUE"。フラグでは使わない */
    const char *help;           /* 説明（英語、1 行）。NULL 可 */
    bool required;              /* フラグでは false でなければならない */
    const char *default_value;  /* 既定値。NULL ならなし。リスト値はカンマ区切りで書く。フラグでは NULL */
    bool inherited;             /* true なら、宣言したコマンドの子孫でも有効（1.5） */
    bool negatable;             /* true なら "--no-<long_name>" で無効にできる。フラグだけに指定できる（14.1） */
    bool default_enabled;       /* true なら、指定がないときに有効とする。否定できるフラグだけに指定できる（14.14） */
    bool hidden;                /* true なら、ヘルプに出さない。必須のオプションには指定できない（16.1） */
    const char *aliases;        /* 長い名前の別名。複数はカンマ区切りで書く（例: "out,outfile"）。NULL ならなし（17.1） */
} clopts_option_t;

typedef struct clopts_positional
{
    const char *name;  /* ヘルプとメッセージ上の名前（例: "INPUT"）。山括弧を含まない */
    const char *help;  /* 説明。NULL 可 */
} clopts_positional_t;

#define CLOPTS_UNLIMITED ((size_t)-1)

typedef struct clopts_command clopts_command_t;

/*
 * コマンドの宣言。指示付き初期化子で書き、書かなかったメンバーの 0 が「なし」を表す。
 * サブコマンドなしは root->command_count == 0、1 段はルートの子がすべて末端コマンド、
 * 入れ子は子コマンドがさらに commands を持つ（1.3、1.4）
 */
struct clopts_command
{
    const char *name;                         /* 子コマンド名。ルートコマンドでは使わない（NULL 可） */
    const char *summary;                      /* 親のヘルプの一覧に出す 1 行の説明。ルートコマンドでは使わない */
    const char *description;                  /* このコマンドのヘルプの説明。NULL なら summary を使う */
    const clopts_option_t *options;         /* option_count == 0 なら NULL 可 */
    size_t option_count;
    const clopts_positional_t *positionals; /* 末端コマンドだけが持てる。positional_count == 0 なら NULL 可 */
    size_t positional_count;
    size_t min_positionals;
    size_t max_positionals;                   /* CLOPTS_UNLIMITED なら上限なし */
    const clopts_command_t *commands;       /* 子コマンド。command_count == 0 なら末端コマンド（NULL 可） */
    size_t command_count;
};

typedef struct clopts_program
{
    const char *name;                /* プログラム名。ルートコマンドの名前として使う */
    const char *version;             /* バージョン文字列 */
    const clopts_command_t *root;  /* ルートコマンド。必須 */
} clopts_program_t;

typedef enum clopts_status
{
    CLOPTS_OK,             /* 解析に成功。末端コマンドが選択されている */
    CLOPTS_HELP,           /* ヘルプの要求。対象は結果の経路の最後のコマンド */
    CLOPTS_VERSION,        /* バージョンの要求 */
    CLOPTS_NO_COMMAND,     /* 子を持つコマンドで子コマンドが指定されなかった。対象は経路の最後のコマンド */
    CLOPTS_USAGE_ERROR,    /* 利用者の入力の誤り。エラーを 1 件追加済み */
    CLOPTS_DECL_ERROR,     /* 宣言の誤り（プログラムの不具合）。エラーを 1 件以上追加済み */
    CLOPTS_OUT_OF_MEMORY   /* メモリ不足。エラーを 1 件追加済み（記録できなければ dropped） */
} clopts_status_t;

typedef struct clopts_value
{
    bool explicit_set;   /* 利用者が明示的に指定した（否定の形を含む） */
    const char *value;   /* 単一の値: 引数列か既定値を指す。なければ NULL */
    char **items;        /* リスト値: 確保した項目の配列 */
    size_t item_count;
    size_t item_cap;
    bool enabled;        /* フラグ: count > 0 と常に一致する */
    size_t count;        /* フラグ: 出現回数。最後の否定の形より後ろの肯定の形の数。未指定なら既定で有効のフラグは 1、ほかは 0。フラグ以外では 0（15.2〜15.4） */
} clopts_value_t;

/* 経路上の 1 つの段 */
typedef struct clopts_level
{
    const clopts_command_t *command;
    clopts_value_t *values;   /* command->option_count 個。0 個なら NULL */
} clopts_level_t;

typedef struct clopts_result
{
    clopts_status_t status;
    const clopts_program_t *program;  /* 解析に使った宣言。解析前は NULL */
    clopts_level_t *levels;           /* levels[0] がルートコマンド、最後が選択中のコマンド */
    size_t level_count;
    size_t level_cap;
    const char **positionals;           /* 引数列を指す */
    size_t positional_count;
    size_t positional_cap;
} clopts_result_t;

#define CLOPTS_RESULT_INIT { CLOPTS_NO_COMMAND, NULL, NULL, 0, 0, NULL, 0, 0 }

void clopts_result_init(clopts_result_t *result);

/* 確保した領域を解放して初期状態に戻す。初期状態・解放済みに対しても安全 */
void clopts_result_free(clopts_result_t *result);

/* 経路（2.8）。level_count == 0（DECL_ERROR、OUT_OF_MEMORY、解析前）なら NULL / 0 */
const clopts_command_t *clopts_command(const clopts_result_t *result);   /* 選択されたコマンド（最後の段） */
size_t clopts_depth(const clopts_result_t *result);                        /* 段の数。ルートだけなら 1 */
const clopts_command_t *clopts_command_at(const clopts_result_t *result, size_t level); /* level < depth */

/* 以下、long_name が経路上のどのコマンドでも宣言されていない場合は false / NULL / 0 を返す（5.7） */
bool clopts_is_set(const clopts_result_t *result, const char *long_name);   /* どちらかの形で明示的に指定された（5.6、14.5） */
bool clopts_flag(const clopts_result_t *result, const char *long_name);     /* フラグが有効である。未指定なら既定の状態（14.5、14.15） */
/* フラグの出現回数。フラグではないオプション、経路上で宣言されていない名前、別名には 0 を返す（15.1、15.7、17.8） */
size_t clopts_flag_count(const clopts_result_t *result, const char *long_name);
const char *clopts_value(const clopts_result_t *result, const char *long_name); /* 単一の値または既定値 */
size_t clopts_list_count(const clopts_result_t *result, const char *long_name);
const char *clopts_list_at(const clopts_result_t *result, const char *long_name, size_t index);
size_t clopts_positional_count(const clopts_result_t *result);
const char *clopts_positional_at(const clopts_result_t *result, size_t index); /* index < count */

/*
 * argv[0] を読み飛ばし、argv[1]〜argv[argc - 1] を解析する。
 * result は初期化済みであること。以前の内容は解放してから書き込む。
 * 戻り値は result->status と同じ。エラーは errors の末尾に追加する（errors は初期化済みであること）。
 */
clopts_status_t clopts_parse(const clopts_program_t *program, int argc, char *const *argv,
                             clopts_result_t *result, clopts_errors_t *errors);

#ifdef __cplusplus
}
#endif

#endif
