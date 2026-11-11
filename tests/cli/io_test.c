/* スイート io: -o の省略、-、- の重複、同じパスへの上書き、失敗時に出力を作らない、改行コード、BOM（2.x、3.x） */

#include <naos/naos.h>

#include "test_util.h"

#include "cli/cli.h"
#include "cli/cli_clopts.h"
#include "cli/cli_commands.h"
#include "cli/cli_io.h"
#include "clopts/clopts.h"
#include "clopts/errors.h"
#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/eol.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define IO_ARGV_MAX_ARGS 8
#define IO_ARGV_MAX_LEN 64

/* IO を直接呼ぶテストの環境。in / out / err は作業ディレクトリのファイルをバイナリモードで開く（テストコードのみ fopen を使う） */
typedef struct io_env
{
    mnd_cli_env_t env;
    mnd_buf_t in_path;
    mnd_buf_t out_path;
    mnd_buf_t err_path;
} io_env_t;

/*
 * 作業ディレクトリの <name>.in（内容は in_bytes の in_len バイト）を "rb" で、
 * <name>.out を out_mode で、<name>.err を "wb" で開いて env を組む
 */
static bool io_env_open(io_env_t *ctx, const char *name, const void *in_bytes, size_t in_len,
                        const char *out_mode)
{
    mnd_buf_t file = MND_BUF_INIT;
    bool ok;

    memset(ctx, 0, sizeof(*ctx));
    ok = (mnd_buf_append_str(&file, name) && mnd_buf_append_str(&file, ".in")
          && test_write_file(mnd_buf_cstr(&file), in_bytes, in_len)
          && test_work_path(mnd_buf_cstr(&file), &ctx->in_path));
    mnd_buf_clear(&file);
    ok = (ok && mnd_buf_append_str(&file, name) && mnd_buf_append_str(&file, ".out")
          && test_write_file(mnd_buf_cstr(&file), NULL, 0)
          && test_work_path(mnd_buf_cstr(&file), &ctx->out_path));
    mnd_buf_clear(&file);
    ok = (ok && mnd_buf_append_str(&file, name) && mnd_buf_append_str(&file, ".err")
          && test_work_path(mnd_buf_cstr(&file), &ctx->err_path));
    mnd_buf_free(&file);
    if (!ok)
    {
        return false;
    }

    ctx->env.in = fopen(mnd_buf_cstr(&ctx->in_path), "rb");
    ctx->env.out = fopen(mnd_buf_cstr(&ctx->out_path), out_mode);
    ctx->env.err = fopen(mnd_buf_cstr(&ctx->err_path), "wb");
    ctx->env.version = TEST_CLI_VERSION;
    ctx->env.getenv = test_env_getenv;
    ctx->env.now = test_clock_now;
    return ((ctx->env.in != NULL) && (ctx->env.out != NULL) && (ctx->env.err != NULL));
}

/* ストリームを閉じてパスを解放する */
static void io_env_close(io_env_t *ctx)
{
    if (ctx->env.in != NULL)
    {
        fclose(ctx->env.in);
    }
    if (ctx->env.out != NULL)
    {
        fclose(ctx->env.out);
    }
    if (ctx->env.err != NULL)
    {
        fclose(ctx->env.err);
    }
    ctx->env.in = NULL;
    ctx->env.out = NULL;
    ctx->env.err = NULL;
    mnd_buf_free(&ctx->in_path);
    mnd_buf_free(&ctx->out_path);
    mnd_buf_free(&ctx->err_path);
}

/* 診断の一覧がちょうど 1 件のエラーで、そのパスが path（NULL 可）であり、メッセージが空でないことを確かめる */
static void check_single_error(const mnd_diag_list_t *diags, const char *path)
{
    const mnd_diag_t *diag;

    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(diags));
    diag = mnd_diag_list_at(diags, 0);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, diag->severity);
    if (path == NULL)
    {
        NAOS_CHECK_NULL(diag->path);
    }
    else
    {
        NAOS_REQUIRE_NOT_NULL(diag->path);
        NAOS_CHECK_STR_EQ(path, diag->path);
    }
    NAOS_REQUIRE_NOT_NULL(diag->message);
    NAOS_CHECK_TRUE(diag->message[0] != '\0');
}

/* 雛形: MND_CLI_INPUT_INIT が空の入力を表す */
NAOS_TEST(io_scaffold_input_init_is_empty)
{
    mnd_cli_input_t input = MND_CLI_INPUT_INIT;

    NAOS_CHECK_NULL(input.path);
    NAOS_CHECK_UINT_EQ(0, input.content.len);
    NAOS_CHECK_FALSE(input.had_bom);
    NAOS_CHECK_INT_EQ(MND_EOL_NONE, input.eol.dominant);
    mnd_cli_input_free(&input);
}

/* ファイルから読み、パス・内容・BOM なし・改行の情報を返す（2.1、3.1） */
NAOS_TEST(io_read_input_from_file)
{
    static const char text[] = "# Title\r\n\r\nBody\n";
    io_env_t ctx;
    mnd_buf_t path = MND_BUF_INIT;
    mnd_cli_input_t input = MND_CLI_INPUT_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_REQUIRE_TRUE(io_env_open(&ctx, "io_read_input_from_file", NULL, 0, "wb"));
    NAOS_REQUIRE_TRUE(test_write_file("io_read_input_from_file.md", text, sizeof(text) - 1));
    NAOS_REQUIRE_TRUE(test_work_path("io_read_input_from_file.md", &path));

    NAOS_CHECK_TRUE(mnd_cli_read_input(&ctx.env, mnd_buf_cstr(&path), &input, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_REQUIRE_NOT_NULL(input.path);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&path), input.path);
    NAOS_CHECK_MEM_EQ(text, sizeof(text) - 1, input.content.data, input.content.len);
    NAOS_CHECK_FALSE(input.had_bom);
    NAOS_CHECK_INT_EQ(MND_EOL_CRLF, input.eol.dominant);
    NAOS_CHECK_TRUE(input.eol.mixed);
    NAOS_CHECK_UINT_EQ(1, input.eol.lf_count);
    NAOS_CHECK_UINT_EQ(2, input.eol.crlf_count);

    mnd_cli_input_free(&input);
    mnd_diag_list_free(&diags);
    mnd_buf_free(&path);
    io_env_close(&ctx);
}

/* "-" なら環境の in から読み、パスは "-"（2.4、3.8） */
NAOS_TEST(io_read_input_from_stdin)
{
    static const char text[] = "line 1\r\nline 2\r\n";
    io_env_t ctx;
    mnd_cli_input_t input = MND_CLI_INPUT_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_REQUIRE_TRUE(io_env_open(&ctx, "io_read_input_from_stdin", text, sizeof(text) - 1, "wb"));

    NAOS_CHECK_TRUE(mnd_cli_read_input(&ctx.env, "-", &input, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_REQUIRE_NOT_NULL(input.path);
    NAOS_CHECK_STR_EQ("-", input.path);
    NAOS_CHECK_MEM_EQ(text, sizeof(text) - 1, input.content.data, input.content.len);
    NAOS_CHECK_FALSE(input.had_bom);
    NAOS_CHECK_INT_EQ(MND_EOL_CRLF, input.eol.dominant);
    NAOS_CHECK_FALSE(input.eol.mixed);

    mnd_cli_input_free(&input);
    mnd_diag_list_free(&diags);
    io_env_close(&ctx);
}

/* ファイルの BOM を取り除き、had_bom で報告する（3.1） */
NAOS_TEST(io_read_input_strips_bom_from_file)
{
    static const char bytes[] = "\xEF\xBB\xBFtext\n";
    io_env_t ctx;
    mnd_buf_t path = MND_BUF_INIT;
    mnd_cli_input_t input = MND_CLI_INPUT_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_REQUIRE_TRUE(io_env_open(&ctx, "io_read_input_strips_bom_from_file", NULL, 0, "wb"));
    NAOS_REQUIRE_TRUE(test_write_file("io_read_input_strips_bom_from_file.md", bytes, sizeof(bytes) - 1));
    NAOS_REQUIRE_TRUE(test_work_path("io_read_input_strips_bom_from_file.md", &path));

    NAOS_CHECK_TRUE(mnd_cli_read_input(&ctx.env, mnd_buf_cstr(&path), &input, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_CHECK_TRUE(input.had_bom);
    NAOS_CHECK_MEM_EQ("text\n", 5, input.content.data, input.content.len);
    NAOS_CHECK_INT_EQ(MND_EOL_LF, input.eol.dominant);

    mnd_cli_input_free(&input);
    mnd_diag_list_free(&diags);
    mnd_buf_free(&path);
    io_env_close(&ctx);
}

/* 標準入力の BOM も取り除き、had_bom で報告する（3.1） */
NAOS_TEST(io_read_input_strips_bom_from_stdin)
{
    static const char bytes[] = "\xEF\xBB\xBFtext";
    io_env_t ctx;
    mnd_cli_input_t input = MND_CLI_INPUT_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_REQUIRE_TRUE(io_env_open(&ctx, "io_read_input_strips_bom_from_stdin", bytes, sizeof(bytes) - 1, "wb"));

    NAOS_CHECK_TRUE(mnd_cli_read_input(&ctx.env, "-", &input, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_CHECK_TRUE(input.had_bom);
    NAOS_CHECK_MEM_EQ("text", 4, input.content.data, input.content.len);
    NAOS_CHECK_INT_EQ(MND_EOL_NONE, input.eol.dominant);

    mnd_cli_input_free(&input);
    mnd_diag_list_free(&diags);
    io_env_close(&ctx);
}

/* 存在しない入力: 指定したパスと原因を示すエラーを 1 つ加えて失敗し、内容は空（2.9） */
NAOS_TEST(io_read_input_missing_file_reports_path_and_cause)
{
    io_env_t ctx;
    mnd_buf_t path = MND_BUF_INIT;
    mnd_cli_input_t input = MND_CLI_INPUT_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const mnd_diag_t *diag;

    NAOS_REQUIRE_TRUE(io_env_open(&ctx, "io_read_input_missing_file_reports_path_and_cause", NULL, 0, "wb"));
    NAOS_REQUIRE_TRUE(test_remove_file("io_read_input_missing_file_reports_path_and_cause.md"));
    NAOS_REQUIRE_TRUE(test_work_path("io_read_input_missing_file_reports_path_and_cause.md", &path));

    NAOS_CHECK_FALSE(mnd_cli_read_input(&ctx.env, mnd_buf_cstr(&path), &input, &diags));
    check_single_error(&diags, mnd_buf_cstr(&path));
    diag = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_NOT_NULL(strstr(diag->message, "cannot open file for reading: "));
    NAOS_CHECK_UINT_EQ(0, input.content.len);
    NAOS_CHECK_FALSE(input.had_bom);

    mnd_cli_input_free(&input);
    mnd_diag_list_free(&diags);
    mnd_buf_free(&path);
    io_env_close(&ctx);
}

/* 不正な UTF-8 の標準入力: パス "-" のエラーで失敗する（3.2） */
NAOS_TEST(io_read_input_invalid_utf8_stdin_reports_dash)
{
    static const char bytes[] = "ok\n\xFF\n";
    io_env_t ctx;
    mnd_cli_input_t input = MND_CLI_INPUT_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_REQUIRE_TRUE(io_env_open(&ctx, "io_read_input_invalid_utf8_stdin_reports_dash", bytes, sizeof(bytes) - 1,
                                  "wb"));

    NAOS_CHECK_FALSE(mnd_cli_read_input(&ctx.env, "-", &input, &diags));
    check_single_error(&diags, "-");
    NAOS_CHECK_UINT_EQ(2, mnd_diag_list_at(&diags, 0)->line);
    NAOS_CHECK_UINT_EQ(0, input.content.len);

    mnd_cli_input_free(&input);
    mnd_diag_list_free(&diags);
    io_env_close(&ctx);
}

/* 出力先 "-": 環境の out にバイト列をそのまま書く（2.4、3.8） */
NAOS_TEST(io_write_output_dash_writes_to_out)
{
    static const char data[] = "a\r\nb\n";
    io_env_t ctx;
    mnd_buf_t written = MND_BUF_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_REQUIRE_TRUE(io_env_open(&ctx, "io_write_output_dash_writes_to_out", NULL, 0, "wb"));

    NAOS_CHECK_TRUE(mnd_cli_write_output(&ctx.env, "-", data, sizeof(data) - 1, false, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    io_env_close(&ctx);
    NAOS_REQUIRE_TRUE(test_read_file("io_write_output_dash_writes_to_out.out", &written));
    NAOS_CHECK_MEM_EQ(data, sizeof(data) - 1, written.data, written.len);

    mnd_buf_free(&written);
    mnd_diag_list_free(&diags);
}

/* 出力先の未指定（NULL）: 環境の out に書く（2.3） */
NAOS_TEST(io_write_output_unspecified_writes_to_out)
{
    static const char data[] = "msgid \"\"\n";
    io_env_t ctx;
    mnd_buf_t written = MND_BUF_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_REQUIRE_TRUE(io_env_open(&ctx, "io_write_output_unspecified_writes_to_out", NULL, 0, "wb"));

    NAOS_CHECK_TRUE(mnd_cli_write_output(&ctx.env, NULL, data, sizeof(data) - 1, false, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    io_env_close(&ctx);
    NAOS_REQUIRE_TRUE(test_read_file("io_write_output_unspecified_writes_to_out.out", &written));
    NAOS_CHECK_MEM_EQ(data, sizeof(data) - 1, written.data, written.len);

    mnd_buf_free(&written);
    mnd_diag_list_free(&diags);
}

/* 出力先 "-" で BOM を付ける（3.6） */
NAOS_TEST(io_write_output_dash_adds_bom)
{
    static const char expected[] = "\xEF\xBB\xBFtext\n";
    io_env_t ctx;
    mnd_buf_t written = MND_BUF_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_REQUIRE_TRUE(io_env_open(&ctx, "io_write_output_dash_adds_bom", NULL, 0, "wb"));

    NAOS_CHECK_TRUE(mnd_cli_write_output(&ctx.env, "-", "text\n", 5, true, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    io_env_close(&ctx);
    NAOS_REQUIRE_TRUE(test_read_file("io_write_output_dash_adds_bom.out", &written));
    NAOS_CHECK_MEM_EQ(expected, sizeof(expected) - 1, written.data, written.len);

    mnd_buf_free(&written);
    mnd_diag_list_free(&diags);
}

/* 出力先のファイル: 既存の内容を置き換え、out には何も書かない（2.2、2.6、3.7） */
NAOS_TEST(io_write_output_file_replaces_existing)
{
    static const char data[] = "new\r\n";
    io_env_t ctx;
    mnd_buf_t path = MND_BUF_INIT;
    mnd_buf_t written = MND_BUF_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_REQUIRE_TRUE(io_env_open(&ctx, "io_write_output_file_replaces_existing", NULL, 0, "wb"));
    NAOS_REQUIRE_TRUE(test_write_file("io_write_output_file_replaces_existing.po", "old content that is longer\n", 27));
    NAOS_REQUIRE_TRUE(test_work_path("io_write_output_file_replaces_existing.po", &path));

    NAOS_CHECK_TRUE(mnd_cli_write_output(&ctx.env, mnd_buf_cstr(&path), data, sizeof(data) - 1, false, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    io_env_close(&ctx);
    NAOS_REQUIRE_TRUE(test_read_file("io_write_output_file_replaces_existing.po", &written));
    NAOS_CHECK_MEM_EQ(data, sizeof(data) - 1, written.data, written.len);
    NAOS_REQUIRE_TRUE(test_read_file("io_write_output_file_replaces_existing.out", &written));
    NAOS_CHECK_UINT_EQ(0, written.len);

    mnd_buf_free(&written);
    mnd_buf_free(&path);
    mnd_diag_list_free(&diags);
}

/* 出力先のファイルに BOM を付ける（3.6） */
NAOS_TEST(io_write_output_file_adds_bom)
{
    static const char expected[] = "\xEF\xBB\xBF# T\n";
    io_env_t ctx;
    mnd_buf_t path = MND_BUF_INIT;
    mnd_buf_t written = MND_BUF_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_REQUIRE_TRUE(io_env_open(&ctx, "io_write_output_file_adds_bom", NULL, 0, "wb"));
    NAOS_REQUIRE_TRUE(test_work_path("io_write_output_file_adds_bom.md", &path));

    NAOS_CHECK_TRUE(mnd_cli_write_output(&ctx.env, mnd_buf_cstr(&path), "# T\n", 4, true, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    io_env_close(&ctx);
    NAOS_REQUIRE_TRUE(test_read_file("io_write_output_file_adds_bom.md", &written));
    NAOS_CHECK_MEM_EQ(expected, sizeof(expected) - 1, written.data, written.len);

    mnd_buf_free(&written);
    mnd_buf_free(&path);
    mnd_diag_list_free(&diags);
}

/* 書けない出力先（存在しないディレクトリの下）: パスと原因を示すエラーで失敗する（2.9） */
NAOS_TEST(io_write_output_unwritable_file_reports_path_and_cause)
{
    io_env_t ctx;
    mnd_buf_t path = MND_BUF_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const mnd_diag_t *diag;

    NAOS_REQUIRE_TRUE(io_env_open(&ctx, "io_write_output_unwritable_file_reports_path_and_cause", NULL, 0, "wb"));
    NAOS_REQUIRE_TRUE(test_work_path("io_write_output_unwritable_file_reports_path_and_cause.missing-dir/out.po",
                                     &path));

    NAOS_CHECK_FALSE(mnd_cli_write_output(&ctx.env, mnd_buf_cstr(&path), "x\n", 2, false, &diags));
    check_single_error(&diags, mnd_buf_cstr(&path));
    diag = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_NOT_NULL(strstr(diag->message, "cannot open file for writing: "));

    mnd_buf_free(&path);
    mnd_diag_list_free(&diags);
    io_env_close(&ctx);
}

/* 書けない out（"rb" で開いたストリーム）: パス "-" のエラーで失敗する（2.9） */
NAOS_TEST(io_write_output_unwritable_out_reports_dash)
{
    io_env_t ctx;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_REQUIRE_TRUE(io_env_open(&ctx, "io_write_output_unwritable_out_reports_dash", NULL, 0, "rb"));

    NAOS_CHECK_FALSE(mnd_cli_write_output(&ctx.env, "-", "x\n", 2, false, &diags));
    check_single_error(&diags, "-");

    mnd_diag_list_free(&diags);
    io_env_close(&ctx);
}

/* 改行コードの決定: LF のみ → LF（3.3） */
NAOS_TEST(io_choose_eol_lf)
{
    static const char text[] = "a\nb\n";
    mnd_eol_info_t info = mnd_eol_detect(text, sizeof(text) - 1);

    NAOS_CHECK_INT_EQ(MND_EOL_LF, mnd_cli_choose_eol(&info));
}

/* 改行コードの決定: CRLF のみ → CRLF（3.3） */
NAOS_TEST(io_choose_eol_crlf)
{
    static const char text[] = "a\r\nb\r\n";
    mnd_eol_info_t info = mnd_eol_detect(text, sizeof(text) - 1);

    NAOS_CHECK_INT_EQ(MND_EOL_CRLF, mnd_cli_choose_eol(&info));
}

/* 改行コードの決定: 混在 → 多いほう、同数なら先に現れたほう（3.4） */
NAOS_TEST(io_choose_eol_mixed_uses_dominant)
{
    static const char more_crlf[] = "a\nb\r\nc\r\n";
    static const char tie_lf_first[] = "a\nb\r\n";
    static const char tie_crlf_first[] = "a\r\nb\n";
    mnd_eol_info_t info;

    info = mnd_eol_detect(more_crlf, sizeof(more_crlf) - 1);
    NAOS_CHECK_INT_EQ(MND_EOL_CRLF, mnd_cli_choose_eol(&info));
    info = mnd_eol_detect(tie_lf_first, sizeof(tie_lf_first) - 1);
    NAOS_CHECK_INT_EQ(MND_EOL_LF, mnd_cli_choose_eol(&info));
    info = mnd_eol_detect(tie_crlf_first, sizeof(tie_crlf_first) - 1);
    NAOS_CHECK_INT_EQ(MND_EOL_CRLF, mnd_cli_choose_eol(&info));
}

/* 改行コードの決定: 改行なし → LF（3.5） */
NAOS_TEST(io_choose_eol_none_is_lf)
{
    mnd_eol_info_t info = mnd_eol_detect("no newline", 10);

    NAOS_CHECK_INT_EQ(MND_EOL_NONE, info.dominant);
    NAOS_CHECK_INT_EQ(MND_EOL_LF, mnd_cli_choose_eol(&info));
}

/* "-" の重複の検査のために argv の複製を解析する */
typedef struct io_argv
{
    char storage[IO_ARGV_MAX_ARGS][IO_ARGV_MAX_LEN];
    char *argv[IO_ARGV_MAX_ARGS + 1];
    int argc;
} io_argv_t;

/* NULL で終わる tokens を copy に複製して mnd_cli_program で解析し、MND_ARGS_OK を確かめる */
static void io_parse(const char *const *tokens, io_argv_t *copy, mnd_args_result_t *result)
{
    mnd_args_program_t program = mnd_cli_program(TEST_CLI_VERSION);
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_args_status_t status;

    copy->argc = 0;
    while (tokens[copy->argc] != NULL)
    {
        NAOS_REQUIRE_TRUE(copy->argc < IO_ARGV_MAX_ARGS);
        NAOS_REQUIRE_TRUE(strlen(tokens[copy->argc]) < IO_ARGV_MAX_LEN);
        strcpy(copy->storage[copy->argc], tokens[copy->argc]);
        copy->argv[copy->argc] = copy->storage[copy->argc];
        copy->argc++;
    }
    copy->argv[copy->argc] = NULL;

    status = mnd_args_parse(&program, copy->argc, copy->argv, result, &errors);
    mnd_cli_clopts_errors_to_diags(&errors, &diags);
    clopts_errors_free(&errors);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
    NAOS_REQUIRE_INT_EQ(MND_ARGS_OK, status);
}

/* 位置引数の "-" が 1 つ以下なら成功し、診断を加えない。-o - は位置引数ではないので数えない（2.4、2.5） */
NAOS_TEST(io_check_stdin_once_accepts_single_dash)
{
    static const char *const one_dash[] = { "monde", "translate", "-", "ja.po", "-o", "-", NULL };
    static const char *const no_dash[] = { "monde", "update", "new.pot", "old.po", NULL };
    io_argv_t copy;
    mnd_args_result_t result;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    mnd_args_result_init(&result);
    io_parse(one_dash, &copy, &result);
    NAOS_CHECK_TRUE(mnd_cli_check_stdin_once(&result, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));

    io_parse(no_dash, &copy, &result);
    NAOS_CHECK_TRUE(mnd_cli_check_stdin_once(&result, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));

    mnd_args_result_free(&result);
    mnd_diag_list_free(&diags);
}

/* 位置引数の "-" が 2 つなら、パスなしの使い方のエラーを 1 つ加えて失敗する（2.5） */
NAOS_TEST(io_check_stdin_once_rejects_duplicate_dash)
{
    static const char *const two_dashes[] = { "monde", "update", "-", "-", NULL };
    io_argv_t copy;
    mnd_args_result_t result;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const mnd_diag_t *diag;

    mnd_args_result_init(&result);
    io_parse(two_dashes, &copy, &result);
    NAOS_CHECK_FALSE(mnd_cli_check_stdin_once(&result, &diags));
    check_single_error(&diags, NULL);
    diag = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_NOT_NULL(strstr(diag->message, "standard input ('-') can be used only once"));

    mnd_args_result_free(&result);
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(io,
    NAOS_ADD_TEST(io_scaffold_input_init_is_empty),
    NAOS_ADD_TEST(io_read_input_from_file),
    NAOS_ADD_TEST(io_read_input_from_stdin),
    NAOS_ADD_TEST(io_read_input_strips_bom_from_file),
    NAOS_ADD_TEST(io_read_input_strips_bom_from_stdin),
    NAOS_ADD_TEST(io_read_input_missing_file_reports_path_and_cause),
    NAOS_ADD_TEST(io_read_input_invalid_utf8_stdin_reports_dash),
    NAOS_ADD_TEST(io_write_output_dash_writes_to_out),
    NAOS_ADD_TEST(io_write_output_unspecified_writes_to_out),
    NAOS_ADD_TEST(io_write_output_dash_adds_bom),
    NAOS_ADD_TEST(io_write_output_file_replaces_existing),
    NAOS_ADD_TEST(io_write_output_file_adds_bom),
    NAOS_ADD_TEST(io_write_output_unwritable_file_reports_path_and_cause),
    NAOS_ADD_TEST(io_write_output_unwritable_out_reports_dash),
    NAOS_ADD_TEST(io_choose_eol_lf),
    NAOS_ADD_TEST(io_choose_eol_crlf),
    NAOS_ADD_TEST(io_choose_eol_mixed_uses_dominant),
    NAOS_ADD_TEST(io_choose_eol_none_is_lf),
    NAOS_ADD_TEST(io_check_stdin_once_accepts_single_dash),
    NAOS_ADD_TEST(io_check_stdin_once_rejects_duplicate_dash)
)
