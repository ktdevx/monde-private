/* スイート report: 診断の書式、dropped、統計の書式と翻訳率の丸め、--quiet（1.6、5.6、8.x、9.1〜9.4） */

#include <naos/naos.h>

#include "test_util.h"

#include "cli/cli_commands.h"
#include "cli/cli_report.h"
#include "cli_args/args.h"
#include "foundation/buf.h"
#include "foundation/diag.h"
#include "po_merge/merge.h"
#include "tu/tu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* 作業ディレクトリの <name>.err を "wb" で開く（テストコードのみ fopen を使う）。失敗なら NULL */
static FILE *report_open(const char *name)
{
    mnd_buf_t file = MND_BUF_INIT;
    mnd_buf_t path = MND_BUF_INIT;
    FILE *fp = NULL;

    if (mnd_buf_append_str(&file, name) && mnd_buf_append_str(&file, ".err")
        && test_work_path(mnd_buf_cstr(&file), &path))
    {
        fp = fopen(mnd_buf_cstr(&path), "wb");
    }

    mnd_buf_free(&file);
    mnd_buf_free(&path);

    return fp;
}

/* fp を閉じ、作業ディレクトリの <name>.err の内容を out に読む */
static bool report_close_read(FILE *fp, const char *name, mnd_buf_t *out)
{
    mnd_buf_t file = MND_BUF_INIT;
    bool ok = (fclose(fp) == 0);

    ok = (ok && mnd_buf_append_str(&file, name) && mnd_buf_append_str(&file, ".err")
          && test_read_file(mnd_buf_cstr(&file), out));
    mnd_buf_free(&file);

    return ok;
}

/* 書かれたバイト列が expected とバイト単位で一致することを確かめる */
#define REPORT_CHECK_BYTES(expected, buf)                                        \
    do                                                                           \
    {                                                                            \
        NAOS_CHECK_UINT_EQ(strlen(expected), (buf).len);                         \
        NAOS_CHECK_STR_EQ((expected), mnd_buf_cstr(&(buf)));                     \
    } while (0)

/* program のサブコマンド name の宣言。なければ NULL */
static const mnd_args_command_t *report_find_command(const mnd_args_program_t *program, const char *name)
{
    size_t i;

    for (i = 0; i < program->command_count; i++)
    {
        if (strcmp(program->commands[i].name, name) == 0)
        {
            return &program->commands[i];
        }
    }

    return NULL;
}

/* 診断の 3 形式（パスと行、パスのみ、どちらもなし）と警告が、渡された順に 1 件 1 行で出る（9.1〜9.4） */
NAOS_TEST(report_diags_forms_in_order)
{
    static const char *const name = "report_diags_forms_in_order";
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t text = MND_BUF_INIT;
    FILE *fp;

    mnd_diag_add(&diags, MND_SEVERITY_ERROR, "doc.md", 12, "bad thing %d", 1);
    mnd_diag_add(&diags, MND_SEVERITY_WARNING, "doc.po", 3, "odd thing");
    mnd_diag_add(&diags, MND_SEVERITY_ERROR, "missing.md", 0, "cannot open file");
    mnd_diag_add(&diags, MND_SEVERITY_WARNING, "-", 0, "path only warning");
    mnd_diag_add(&diags, MND_SEVERITY_ERROR, NULL, 0, "out of memory");
    mnd_diag_add(&diags, MND_SEVERITY_WARNING, NULL, 0, "no path warning");
    mnd_diag_add(&diags, MND_SEVERITY_ERROR, NULL, 7, "line without path");
    NAOS_REQUIRE_UINT_EQ(7, mnd_diag_list_count(&diags));

    fp = report_open(name);
    NAOS_REQUIRE_NOT_NULL(fp);
    mnd_cli_report_diags(fp, &diags);
    NAOS_REQUIRE_TRUE(report_close_read(fp, name, &text));

    REPORT_CHECK_BYTES("doc.md:12: error: bad thing 1\n"
                       "doc.po:3: warning: odd thing\n"
                       "missing.md: error: cannot open file\n"
                       "-: warning: path only warning\n"
                       "monde: error: out of memory\n"
                       "monde: warning: no path warning\n"
                       "monde: error: line without path\n",
                       text);

    mnd_buf_free(&text);
    mnd_diag_list_free(&diags);
}

/* 診断がなければ何も書かない */
NAOS_TEST(report_diags_empty_writes_nothing)
{
    static const char *const name = "report_diags_empty_writes_nothing";
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t text = MND_BUF_INIT;
    FILE *fp;

    fp = report_open(name);
    NAOS_REQUIRE_NOT_NULL(fp);
    mnd_cli_report_diags(fp, &diags);
    NAOS_REQUIRE_TRUE(report_close_read(fp, name, &text));

    NAOS_CHECK_UINT_EQ(0, text.len);

    mnd_buf_free(&text);
    mnd_diag_list_free(&diags);
}

/* dropped なら記録できた診断の後に固定のメッセージを足す（Error Handling） */
NAOS_TEST(report_diags_dropped_appends_fixed_message)
{
    static const char *const name = "report_diags_dropped_appends_fixed_message";
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t text = MND_BUF_INIT;
    FILE *fp;

    mnd_diag_add(&diags, MND_SEVERITY_ERROR, "a.po", 2, "first");
    diags.dropped = true;

    fp = report_open(name);
    NAOS_REQUIRE_NOT_NULL(fp);
    mnd_cli_report_diags(fp, &diags);
    NAOS_REQUIRE_TRUE(report_close_read(fp, name, &text));

    REPORT_CHECK_BYTES("a.po:2: error: first\n"
                       "monde: error: some diagnostics were dropped (out of memory)\n",
                       text);

    mnd_buf_free(&text);
    mnd_diag_list_free(&diags);
}

/* 診断が 1 件もなくても dropped なら固定のメッセージだけを書く */
NAOS_TEST(report_diags_dropped_without_items)
{
    static const char *const name = "report_diags_dropped_without_items";
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t text = MND_BUF_INIT;
    FILE *fp;

    diags.dropped = true;

    fp = report_open(name);
    NAOS_REQUIRE_NOT_NULL(fp);
    mnd_cli_report_diags(fp, &diags);
    NAOS_REQUIRE_TRUE(report_close_read(fp, name, &text));

    REPORT_CHECK_BYTES("monde: error: some diagnostics were dropped (out of memory)\n", text);

    mnd_buf_free(&text);
    mnd_diag_list_free(&diags);
}

/* 使い方のエラー: "monde: error: <message>" の次にサブコマンドの案内（1.6） */
NAOS_TEST(report_usage_error_with_command)
{
    static const char *const name = "report_usage_error_with_command";
    mnd_args_program_t program = mnd_cli_program(TEST_CLI_VERSION);
    const mnd_args_command_t *command = report_find_command(&program, "update");
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t text = MND_BUF_INIT;
    FILE *fp;

    NAOS_REQUIRE_NOT_NULL(command);
    mnd_diag_add(&diags, MND_SEVERITY_ERROR, NULL, 0, "unknown option '--bogus'");

    fp = report_open(name);
    NAOS_REQUIRE_NOT_NULL(fp);
    mnd_cli_report_usage_error(fp, &program, command, &diags);
    NAOS_REQUIRE_TRUE(report_close_read(fp, name, &text));

    REPORT_CHECK_BYTES("monde: error: unknown option '--bogus'\n"
                       "Run 'monde update --help' for usage.\n",
                       text);

    mnd_buf_free(&text);
    mnd_diag_list_free(&diags);
}

/* 使い方のエラー: サブコマンドが決まらないときは全体の案内。パスがあっても "monde:" で始める（1.6） */
NAOS_TEST(report_usage_error_without_command)
{
    static const char *const name = "report_usage_error_without_command";
    mnd_args_program_t program = mnd_cli_program(TEST_CLI_VERSION);
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t text = MND_BUF_INIT;
    FILE *fp;

    mnd_diag_add(&diags, MND_SEVERITY_ERROR, NULL, 0, "unknown command 'bogus'");
    mnd_diag_add(&diags, MND_SEVERITY_ERROR, "-", 0, "'-' is used more than once");

    fp = report_open(name);
    NAOS_REQUIRE_NOT_NULL(fp);
    mnd_cli_report_usage_error(fp, &program, NULL, &diags);
    NAOS_REQUIRE_TRUE(report_close_read(fp, name, &text));

    REPORT_CHECK_BYTES("monde: error: unknown command 'bogus'\n"
                       "monde: error: '-' is used more than once\n"
                       "Run 'monde --help' for usage.\n",
                       text);

    mnd_buf_free(&text);
    mnd_diag_list_free(&diags);
}

/* extract / init / 言語の出どころ / update の統計の書式（5.6、8.1〜8.3、8.8） */
NAOS_TEST(report_stats_formats)
{
    static const char *const name = "report_stats_formats";
    mnd_merge_stats_t merge = MND_MERGE_STATS_INIT;
    mnd_buf_t text = MND_BUF_INIT;
    FILE *fp;

    merge.matched = 10;
    merge.fuzzy = 2;
    merge.added = 3;
    merge.obsoleted = 4;

    fp = report_open(name);
    NAOS_REQUIRE_NOT_NULL(fp);
    mnd_cli_report_extract(fp, false, 42);
    mnd_cli_report_init_language_source(fp, false, "ja_JP", "LANG");
    mnd_cli_report_init(fp, false, 42, "ja_JP");
    mnd_cli_report_update(fp, false, &merge);
    mnd_cli_report_extract(fp, false, 0);
    NAOS_REQUIRE_TRUE(report_close_read(fp, name, &text));

    REPORT_CHECK_BYTES("extract: 42 messages\n"
                       "init: using language 'ja_JP' from LANG\n"
                       "init: 42 messages, language ja_JP\n"
                       "update: 10 matched, 2 fuzzy, 3 added, 4 obsoleted\n"
                       "extract: 0 messages\n",
                       text);

    mnd_buf_free(&text);
}

/* translate の統計: 2/3 は 66.7%、fuzzy は翻訳済みに数えない（8.4、8.5） */
NAOS_TEST(report_translate_rate_two_thirds)
{
    static const char *const name = "report_translate_rate_two_thirds";
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;
    mnd_buf_t text = MND_BUF_INIT;
    FILE *fp;

    stats.total = 3;
    stats.translated = 2;
    stats.fuzzy = 1;

    fp = report_open(name);
    NAOS_REQUIRE_NOT_NULL(fp);
    mnd_cli_report_translate(fp, false, &stats);
    NAOS_REQUIRE_TRUE(report_close_read(fp, name, &text));

    REPORT_CHECK_BYTES("translate: 2 translated, 1 fuzzy, 0 untranslated, 0 missing, 3 total, 66.7% translated\n",
                       text);

    mnd_buf_free(&text);
}

/* 翻訳率の丸め: 1/3 → 33.3%、1/16 → 6.3%（6.25 を四捨五入で切り上げ）、3/3 → 100.0%、0/5 → 0.0%（8.5） */
NAOS_TEST(report_translate_rate_rounding)
{
    static const char *const name = "report_translate_rate_rounding";
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;
    mnd_buf_t text = MND_BUF_INIT;
    FILE *fp;

    fp = report_open(name);
    NAOS_REQUIRE_NOT_NULL(fp);

    stats.total = 3;
    stats.translated = 1;
    stats.untranslated = 1;
    stats.missing = 1;
    mnd_cli_report_translate(fp, false, &stats);

    memset(&stats, 0, sizeof(stats));
    stats.total = 16;
    stats.translated = 1;
    stats.fuzzy = 15;
    mnd_cli_report_translate(fp, false, &stats);

    memset(&stats, 0, sizeof(stats));
    stats.total = 3;
    stats.translated = 3;
    mnd_cli_report_translate(fp, false, &stats);

    memset(&stats, 0, sizeof(stats));
    stats.total = 5;
    stats.fuzzy = 5;
    mnd_cli_report_translate(fp, false, &stats);

    memset(&stats, 0, sizeof(stats));
    stats.total = 2000;
    stats.translated = 1999;
    stats.missing = 1;
    mnd_cli_report_translate(fp, false, &stats);

    NAOS_REQUIRE_TRUE(report_close_read(fp, name, &text));

    REPORT_CHECK_BYTES("translate: 1 translated, 0 fuzzy, 1 untranslated, 1 missing, 3 total, 33.3% translated\n"
                       "translate: 1 translated, 15 fuzzy, 0 untranslated, 0 missing, 16 total, 6.3% translated\n"
                       "translate: 3 translated, 0 fuzzy, 0 untranslated, 0 missing, 3 total, 100.0% translated\n"
                       "translate: 0 translated, 5 fuzzy, 0 untranslated, 0 missing, 5 total, 0.0% translated\n"
                       "translate: 1999 translated, 0 fuzzy, 0 untranslated, 1 missing, 2000 total, "
                       "100.0% translated\n",
                       text);

    mnd_buf_free(&text);
}

/* translate の合計が 0 なら翻訳率を出さず、翻訳対象がないことを示す（8.6） */
NAOS_TEST(report_translate_total_zero)
{
    static const char *const name = "report_translate_total_zero";
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;
    mnd_buf_t text = MND_BUF_INIT;
    FILE *fp;

    fp = report_open(name);
    NAOS_REQUIRE_NOT_NULL(fp);
    mnd_cli_report_translate(fp, false, &stats);
    NAOS_REQUIRE_TRUE(report_close_read(fp, name, &text));

    REPORT_CHECK_BYTES("translate: no translatable messages\n", text);

    mnd_buf_free(&text);
}

/* quiet は統計と情報だけを抑止し、診断は抑止しない（8.7、5.6） */
NAOS_TEST(report_quiet_suppresses_only_stats)
{
    static const char *const name = "report_quiet_suppresses_only_stats";
    mnd_merge_stats_t merge = MND_MERGE_STATS_INIT;
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t text = MND_BUF_INIT;
    FILE *fp;

    merge.matched = 1;
    stats.total = 3;
    stats.translated = 2;
    mnd_diag_add(&diags, MND_SEVERITY_WARNING, "doc.md", 5, "structure may change");

    fp = report_open(name);
    NAOS_REQUIRE_NOT_NULL(fp);
    mnd_cli_report_extract(fp, true, 42);
    mnd_cli_report_init_language_source(fp, true, "ja_JP", "LANG");
    mnd_cli_report_init(fp, true, 42, "ja_JP");
    mnd_cli_report_update(fp, true, &merge);
    mnd_cli_report_translate(fp, true, &stats);
    memset(&stats, 0, sizeof(stats));
    mnd_cli_report_translate(fp, true, &stats);
    mnd_cli_report_diags(fp, &diags);
    NAOS_REQUIRE_TRUE(report_close_read(fp, name, &text));

    REPORT_CHECK_BYTES("doc.md:5: warning: structure may change\n", text);

    mnd_buf_free(&text);
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(report,
    NAOS_ADD_TEST(report_diags_forms_in_order),
    NAOS_ADD_TEST(report_diags_empty_writes_nothing),
    NAOS_ADD_TEST(report_diags_dropped_appends_fixed_message),
    NAOS_ADD_TEST(report_diags_dropped_without_items),
    NAOS_ADD_TEST(report_usage_error_with_command),
    NAOS_ADD_TEST(report_usage_error_without_command),
    NAOS_ADD_TEST(report_stats_formats),
    NAOS_ADD_TEST(report_translate_rate_two_thirds),
    NAOS_ADD_TEST(report_translate_rate_rounding),
    NAOS_ADD_TEST(report_translate_total_zero),
    NAOS_ADD_TEST(report_quiet_suppresses_only_stats)
)
