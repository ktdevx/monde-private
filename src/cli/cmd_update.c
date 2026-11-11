/* update ハンドラ: .pot と既存の .po を読み、マージした .po を書く */

#include "cli/cli_commands.h"
#include "cli/cli_internal.h"
#include "cli/cli_io.h"
#include "cli/cli_report.h"

#include "clopts/clopts.h"
#include "foundation/buf.h"
#include "foundation/diag.h"
#include "po/po.h"
#include "po/po_read.h"
#include "po/po_write.h"
#include "po_merge/merge.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * 読み込んだ 2 つの入力を PO として解析する。両方のファイルのエラーを報告するため、
 * テンプレートが失敗しても既存の翻訳ファイルを解析する（6.6）。診断のパスは指定されたパスのまま
 */
static bool parse_both(mnd_cli_ctx_t *ctx, const mnd_cli_input_t *template_input, const mnd_cli_input_t *old_input,
                       mnd_po_catalog_t *template_catalog, mnd_po_catalog_t *old_catalog)
{
    bool template_ok = mnd_po_read(mnd_buf_cstr(&template_input->content), template_input->content.len,
                                   template_input->path, template_catalog, ctx->diags);
    bool old_ok = mnd_po_read(mnd_buf_cstr(&old_input->content), old_input->content.len, old_input->path,
                              old_catalog, ctx->diags);

    return template_ok && old_ok;
}

bool mnd_cli_cmd_update(mnd_cli_ctx_t *ctx)
{
    const char *template_path = mnd_args_positional_at(ctx->result, 0);
    const char *old_path = mnd_args_positional_at(ctx->result, 1);
    const char *output_path = mnd_args_value(ctx->result, MND_CLI_OPT_OUTPUT);
    mnd_cli_input_t template_input = MND_CLI_INPUT_INIT;
    mnd_cli_input_t old_input = MND_CLI_INPUT_INIT;
    mnd_po_catalog_t template_catalog = MND_PO_CATALOG_INIT;
    mnd_po_catalog_t old_catalog = MND_PO_CATALOG_INIT;
    mnd_po_catalog_t merged = MND_PO_CATALOG_INIT;
    mnd_merge_options_t merge_options = MND_MERGE_OPTIONS_INIT;
    mnd_merge_stats_t stats = MND_MERGE_STATS_INIT;
    mnd_po_write_options_t write_options = MND_PO_WRITE_OPTIONS_INIT;
    mnd_buf_t output = MND_BUF_INIT;
    bool ok;

    /* 標準入力は 1 回しか読めない。2 つとも "-" なら使い方のエラー（2.5） */
    if (!mnd_cli_check_stdin_once(ctx->result, ctx->diags))
    {
        ctx->usage_error = true;
        return false;
    }

    /* 出力先が既存の翻訳ファイルと同じでもよいよう、両方を読み終えてから先へ進む（2.7） */
    ok = mnd_cli_read_input(ctx->env, template_path, &template_input, ctx->diags)
         && mnd_cli_read_input(ctx->env, old_path, &old_input, ctx->diags);

    if (ok)
    {
        merge_options.fuzzy_matching = !mnd_args_flag(ctx->result, MND_CLI_OPT_NO_FUZZY_MATCHING);
        write_options.no_wrap = mnd_args_flag(ctx->result, MND_CLI_OPT_NO_WRAP);
        /* 改行コードは既存の翻訳ファイルに合わせる（3.3） */
        write_options.eol = mnd_cli_choose_eol(&old_input.eol);

        /* 出力は成功した後に 1 回だけ書く。失敗なら出力先（既存の翻訳ファイルを含む）は変わらない（2.8、6.7） */
        ok = parse_both(ctx, &template_input, &old_input, &template_catalog, &old_catalog)
             && mnd_merge_catalogs(&template_catalog, template_input.path, &old_catalog, old_input.path,
                                   &merge_options, &merged, &stats, ctx->diags)
             && mnd_po_write(&merged, &write_options, &output, ctx->diags)
             && mnd_cli_write_output(ctx->env, output_path, mnd_buf_cstr(&output), output.len, false,
                                     ctx->diags);
    }

    if (ok)
    {
        mnd_cli_report_update(ctx->env->err, ctx->quiet, &stats);
    }

    mnd_buf_free(&output);
    mnd_po_catalog_free(&merged);
    mnd_po_catalog_free(&old_catalog);
    mnd_po_catalog_free(&template_catalog);
    mnd_cli_input_free(&old_input);
    mnd_cli_input_free(&template_input);

    return ok;
}
