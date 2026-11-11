/* IO: 入力の読み込み、出力の書き込み、改行コードの決定、"-" の重複の検査 */

#include "cli/cli_io.h"

#include "clopts/clopts.h"
#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/eol.h"
#include "foundation/file_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 標準入出力を表すパス（診断にもこのまま使う） */
#define MND_CLI_STDIO_PATH "-"

/* path がちょうど "-" なら true */
static bool mnd_cli_is_stdio(const char *path)
{
    return ((path != NULL) && (strcmp(path, MND_CLI_STDIO_PATH) == 0));
}

bool mnd_cli_read_input(const mnd_cli_env_t *env, const char *path, mnd_cli_input_t *out,
                        mnd_diag_list_t *diags)
{
    bool ok;

    out->path = path;
    if (mnd_cli_is_stdio(path))
    {
        ok = mnd_file_read_stream(env->in, MND_CLI_STDIO_PATH, &out->content, &out->had_bom, diags);
    }
    else
    {
        ok = mnd_file_read(path, &out->content, &out->had_bom, diags);
    }

    /* 失敗時は content が空になるため、改行の情報は「改行なし」になる */
    out->eol = mnd_eol_detect(mnd_buf_cstr(&out->content), out->content.len);
    return ok;
}

void mnd_cli_input_free(mnd_cli_input_t *input)
{
    mnd_buf_free(&input->content);
}

bool mnd_cli_write_output(const mnd_cli_env_t *env, const char *path, const char *data, size_t len,
                          bool add_bom, mnd_diag_list_t *diags)
{
    /* foundation の書き込みは 1 回で書き、fflush（ファイルは fclose）の失敗も失敗として扱う */
    if ((path == NULL) || mnd_cli_is_stdio(path))
    {
        return mnd_file_write_stream(env->out, MND_CLI_STDIO_PATH, data, len, add_bom, diags);
    }
    return mnd_file_write(path, data, len, add_bom, diags);
}

mnd_eol_t mnd_cli_choose_eol(const mnd_eol_info_t *info)
{
    if (info->dominant == MND_EOL_NONE)
    {
        return MND_EOL_LF;
    }
    return info->dominant;
}

bool mnd_cli_check_stdin_once(const clopts_result_t *result, mnd_diag_list_t *diags)
{
    size_t count = clopts_positional_count(result);
    size_t dashes = 0;
    size_t i;

    for (i = 0; i < count; i++)
    {
        if (mnd_cli_is_stdio(clopts_positional_at(result, i)))
        {
            dashes++;
        }
    }

    if (dashes >= 2)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                     "standard input ('-') can be used only once, but %zu inputs are '-'", dashes);
        return false;
    }
    return true;
}
