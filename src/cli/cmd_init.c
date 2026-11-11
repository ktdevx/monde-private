/* init ハンドラ: .pot を読み、言語を決めて .po を書く */

#include "cli/cli_commands.h"
#include "cli/cli_internal.h"
#include "cli/cli_io.h"
#include "cli/cli_language.h"
#include "cli/cli_report.h"
#include "cli/cli_time.h"

#include "clopts/clopts.h"
#include "foundation/buf.h"
#include "foundation/diag.h"
#include "po/po.h"
#include "po/po_init.h"
#include "po/po_read.h"
#include "po/po_write.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* メモリ不足の診断（Language と同じ文面） */
#define INIT_OUT_OF_MEMORY_MESSAGE "out of memory"

/*
 * 言語コードを決める。失敗が使い方のエラー（決まらない、不正な値）なら ctx->usage_error を立てる。
 * mnd_cli_resolve_language はメモリ不足でも false を返すため、新たに加わった診断が
 * メモリ不足の文面か、診断を記録できなかった（dropped）場合は使い方のエラーにしない
 */
static bool resolve_language(mnd_cli_ctx_t *ctx, mnd_cli_language_t *language)
{
    const char *explicit_code = clopts_value(ctx->result, MND_CLI_OPT_LANGUAGE);
    size_t before = mnd_diag_list_count(ctx->diags);
    bool dropped_before = ctx->diags->dropped;
    size_t after;

    if (mnd_cli_resolve_language(ctx->env, explicit_code, language, ctx->diags))
    {
        return true;
    }

    after = mnd_diag_list_count(ctx->diags);
    if ((ctx->diags->dropped && !dropped_before)
        || ((after > before)
            && (strcmp(mnd_diag_list_at(ctx->diags, after - 1)->message, INIT_OUT_OF_MEMORY_MESSAGE) == 0)))
    {
        return false;
    }

    ctx->usage_error = true;
    return false;
}

/* catalog（テンプレート）を現在日時と言語コードで翻訳ファイルに変える。失敗なら診断を追加して false */
static bool initialize(const mnd_cli_ctx_t *ctx, mnd_po_catalog_t *catalog, const char *code)
{
    char now[MND_PO_DATE_SIZE];

    return mnd_cli_format_now(ctx->env, now, ctx->diags)
           && mnd_po_init(catalog, code, now, ctx->diags);
}

bool mnd_cli_cmd_init(mnd_cli_ctx_t *ctx)
{
    const char *template_path = clopts_positional_at(ctx->result, 0);
    const char *output_path = clopts_value(ctx->result, MND_CLI_OPT_OUTPUT);
    mnd_cli_language_t language = { NULL, NULL };
    mnd_cli_input_t input = MND_CLI_INPUT_INIT;
    mnd_po_write_options_t write_options = MND_PO_WRITE_OPTIONS_INIT;
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_buf_t output = MND_BUF_INIT;
    bool ok;

    ok = resolve_language(ctx, &language)
         && mnd_cli_read_input(ctx->env, template_path, &input, ctx->diags);

    if (ok)
    {
        write_options.no_wrap = clopts_flag(ctx->result, MND_CLI_OPT_NO_WRAP);
        write_options.eol = mnd_cli_choose_eol(&input.eol);

        /* 診断のパスは指定されたパスのまま（標準入力は "-"） */
        ok = mnd_po_read(mnd_buf_cstr(&input.content), input.content.len, input.path, &catalog, ctx->diags)
             && initialize(ctx, &catalog, language.code)
             && mnd_po_write(&catalog, &write_options, &output, ctx->diags)
             && mnd_cli_write_output(ctx->env, output_path, mnd_buf_cstr(&output), output.len, false,
                                     ctx->diags);
    }

    if (ok)
    {
        /* 環境変数から決めたときだけ出どころを示す（5.6） */
        if (language.variable != NULL)
        {
            mnd_cli_report_init_language_source(ctx->env->err, ctx->quiet, language.code, language.variable);
        }
        /* 初期化後は必ずヘッダエントリがあるため、それを除いた件数 */
        mnd_cli_report_init(ctx->env->err, ctx->quiet, catalog.count - 1, language.code);
    }

    mnd_buf_free(&output);
    mnd_po_catalog_free(&catalog);
    mnd_cli_input_free(&input);
    free((void *)language.code);

    return ok;
}
