/* translate ハンドラ: .md と .po を読み、翻訳済み Markdown を書く */

#include "cli/cli_commands.h"
#include "cli/cli_internal.h"
#include "cli/cli_io.h"
#include "cli/cli_report.h"

#include "clopts/clopts.h"
#include "foundation/buf.h"
#include "foundation/diag.h"
#include "po/po.h"
#include "po/po_read.h"
#include "tu/tu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

/* メモリ不足の診断 */
#define TRANSLATE_OUT_OF_MEMORY_MESSAGE "out of memory"

/*
 * --front-matter-keys の項目を、解析結果の文字列を指す配列として *keys に作る（count == 0 なら NULL）。
 * 配列は呼び出し側が free する。メモリ不足なら診断を追加して false
 */
static bool collect_keys(const clopts_result_t *result, const char ***keys, size_t *count,
                         mnd_diag_list_t *diags)
{
    size_t n = clopts_list_count(result, MND_CLI_OPT_FRONT_MATTER_KEYS);
    const char **items;
    size_t i;

    *keys = NULL;
    *count = 0;
    if (n == 0)
    {
        return true;
    }

    items = (const char **)malloc(n * sizeof(*items));
    if (items == NULL)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, TRANSLATE_OUT_OF_MEMORY_MESSAGE);
        return false;
    }
    for (i = 0; i < n; i++)
    {
        items[i] = clopts_list_at(result, MND_CLI_OPT_FRONT_MATTER_KEYS, i);
    }

    *keys = items;
    *count = n;
    return true;
}

bool mnd_cli_cmd_translate(mnd_cli_ctx_t *ctx)
{
    const char *input_path = clopts_positional_at(ctx->result, 0);
    const char *catalog_path = clopts_positional_at(ctx->result, 1);
    const char *output_path = clopts_value(ctx->result, MND_CLI_OPT_OUTPUT);
    mnd_cli_input_t input = MND_CLI_INPUT_INIT;
    mnd_cli_input_t catalog_input = MND_CLI_INPUT_INIT;
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_tu_options_t options = MND_TU_OPTIONS_INIT;
    mnd_tu_translate_options_t translate_options = MND_TU_TRANSLATE_OPTIONS_INIT;
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;
    mnd_buf_t output = MND_BUF_INIT;
    mnd_tu_doc_t *doc = NULL;
    const char **keys = NULL;
    size_t key_count = 0;
    bool ok;

    /* 標準入力は 1 回しか読めない。2 つとも "-" なら使い方のエラー（2.5） */
    if (!mnd_cli_check_stdin_once(ctx->result, ctx->diags))
    {
        ctx->usage_error = true;
        return false;
    }

    /* 出力先が入力と同じパスでもよいよう、両方を読み終えてから先へ進む */
    ok = mnd_cli_read_input(ctx->env, input_path, &input, ctx->diags)
         && mnd_cli_read_input(ctx->env, catalog_path, &catalog_input, ctx->diags)
         && collect_keys(ctx->result, &keys, &key_count, ctx->diags);

    if (ok)
    {
        /* 診断のパスは指定されたパスのまま（標準入力は "-"） */
        options.path = input.path;
        options.front_matter_keys = keys;
        options.front_matter_key_count = key_count;
        translate_options.use_fuzzy = clopts_flag(ctx->result, MND_CLI_OPT_USE_FUZZY);
        /* 訳文の行の境界は原文の改行コードに合わせる */
        translate_options.eol = mnd_cli_choose_eol(&input.eol);
        translate_options.catalog_path = catalog_input.path;

        /*
         * 構造を変え得る訳文は警告だけで成功として扱う（7.7）。PO と訳文のエラーでは出力を書かない（7.8）。
         * BOM は原文に合わせる（3.6）
         */
        ok = mnd_po_read(mnd_buf_cstr(&catalog_input.content), catalog_input.content.len, catalog_input.path,
                         &catalog, ctx->diags)
             && mnd_tu_scan(mnd_buf_cstr(&input.content), input.content.len, &options, &doc, ctx->diags)
             && mnd_tu_translate(doc, &catalog, &translate_options, &output, &stats, ctx->diags)
             && mnd_cli_write_output(ctx->env, output_path, mnd_buf_cstr(&output), output.len, input.had_bom,
                                     ctx->diags);
    }

    if (ok)
    {
        mnd_cli_report_translate(ctx->env->err, ctx->quiet, &stats);
    }

    mnd_buf_free(&output);
    mnd_tu_doc_free(doc);
    mnd_po_catalog_free(&catalog);
    free(keys);
    mnd_cli_input_free(&catalog_input);
    mnd_cli_input_free(&input);

    return ok;
}
