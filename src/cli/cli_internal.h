#ifndef MONDE_CLI_CLI_INTERNAL_H
#define MONDE_CLI_CLI_INTERNAL_H

/*
 * ハンドラが共有する文脈の型（mnd_cli_ctx_t）とサブコマンドのハンドラの宣言。
 * モジュール内部のヘッダ（src/cli/ と tests/cli/ だけが include する）
 */

#include "cli/cli.h"
#include "cli/cli_commands.h"
#include "clopts/clopts.h"
#include "foundation/diag.h"

#include <stdbool.h>

/* Run がハンドラに渡す文脈。解析結果と診断の一覧の所有と解放は Run が行う */
typedef struct mnd_cli_ctx
{
    const mnd_cli_env_t *env;          /* 実行環境 */
    const clopts_result_t *result;   /* 引数の解析結果（CLOPTS_OK） */
    mnd_diag_list_t *diags;            /* 診断の一覧。ハンドラの成否にかかわらず Run が表示する */
    bool quiet;                        /* --quiet: 統計と情報を抑止する */
    bool usage_error;                  /* 失敗が使い方のエラーなら true（Run は 1 ではなく 2 を返す） */
} mnd_cli_ctx_t;

/* サブコマンドのハンドラ（mnd_cli_handler_fn の形）。成功なら true */
bool mnd_cli_cmd_extract(mnd_cli_ctx_t *ctx);
bool mnd_cli_cmd_init(mnd_cli_ctx_t *ctx);
bool mnd_cli_cmd_update(mnd_cli_ctx_t *ctx);
bool mnd_cli_cmd_translate(mnd_cli_ctx_t *ctx);

/* サブコマンド名からハンドラを引く関数の型（mnd_cli_find_handler の形） */
typedef mnd_cli_handler_fn (*mnd_cli_find_handler_fn)(const char *name);

/*
 * mnd_cli_run の本体。ハンドラを find_handler で引く（mnd_cli_run は mnd_cli_find_handler を渡す）。
 * テストがハンドラを差し替えるための継ぎ目
 */
int mnd_cli_run_with(const mnd_cli_env_t *env, int argc, char *const *argv, mnd_cli_find_handler_fn find_handler);

#endif
