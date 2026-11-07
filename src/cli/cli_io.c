/* IO: 入力の読み込み、出力の書き込み、改行コードの決定、"-" の重複の検査 */

#include "cli/cli_io.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/eol.h"

#include <stdbool.h>
#include <stddef.h>

/* 仮の実装: 診断を 1 つ追加して失敗を返す */
bool mnd_cli_read_input(const mnd_cli_env_t *env, const char *path, mnd_cli_input_t *out,
                        mnd_diag_list_t *diags)
{
    (void)env;
    (void)out;
    mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0, "reading input is not implemented");
    return false;
}

void mnd_cli_input_free(mnd_cli_input_t *input)
{
    mnd_buf_free(&input->content);
}

/* 仮の実装: 診断を 1 つ追加して失敗を返す */
bool mnd_cli_write_output(const mnd_cli_env_t *env, const char *path, const char *data, size_t len,
                          bool add_bom, mnd_diag_list_t *diags)
{
    (void)env;
    (void)data;
    (void)len;
    (void)add_bom;
    mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0, "writing output is not implemented");
    return false;
}

/* 仮の実装: 常に LF を返す */
mnd_eol_t mnd_cli_choose_eol(const mnd_eol_info_t *info)
{
    (void)info;
    return MND_EOL_LF;
}

/* 仮の実装: 診断を 1 つ追加して失敗を返す */
bool mnd_cli_check_stdin_once(const mnd_args_result_t *result, mnd_diag_list_t *diags)
{
    (void)result;
    mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "checking standard input usage is not implemented");
    return false;
}
