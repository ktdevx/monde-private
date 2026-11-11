#ifndef MONDE_CLI_CLI_IO_H
#define MONDE_CLI_CLI_IO_H

/*
 * IO: 入力の読み込み（ファイル / 標準入力）、出力の書き込み（ファイル / 標準出力）、改行コードの決定。
 * モジュール内部のヘッダ（src/cli/ と tests/cli/ だけが include する）
 */

#include "cli/cli.h"
#include "clopts/clopts.h"
#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/eol.h"

#include <stdbool.h>
#include <stddef.h>

/* 読み込んだ入力。content は BOM 除去後 */
typedef struct mnd_cli_input
{
    const char *path;      /* 指定されたパス。標準入力なら "-" */
    mnd_buf_t content;
    bool had_bom;
    mnd_eol_info_t eol;    /* content の改行の情報 */
} mnd_cli_input_t;

#define MND_CLI_INPUT_INIT { NULL, MND_BUF_INIT, false, { MND_EOL_NONE, false, 0, 0 } }

/* path が "-" なら env->in から、そうでなければファイルから読む。失敗: 診断を追加して false */
bool mnd_cli_read_input(const mnd_cli_env_t *env, const char *path, mnd_cli_input_t *out,
                        mnd_diag_list_t *diags);
void mnd_cli_input_free(mnd_cli_input_t *input);

/*
 * 出力先。path が NULL または "-" なら env->out に、そうでなければファイルに 1 回で書く。
 * fflush の失敗も失敗として扱う。失敗: 診断を追加して false
 */
bool mnd_cli_write_output(const mnd_cli_env_t *env, const char *path, const char *data, size_t len,
                          bool add_bom, mnd_diag_list_t *diags);

/* 主となる入力の改行コードから出力の改行コードを決める（NONE → LF） */
mnd_eol_t mnd_cli_choose_eol(const mnd_eol_info_t *info);

/* 位置引数のうち "-" が 2 つ以上なら使い方のエラーの診断を追加して false */
bool mnd_cli_check_stdin_once(const mnd_args_result_t *result, mnd_diag_list_t *diags);

#endif
