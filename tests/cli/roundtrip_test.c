/* スイート roundtrip: spec.txt の全例 × original / CRLF × 空カタログ / msgstr = msgid をプロセス内で検証（10.1〜10.4） */

#include <naos/naos.h>

#include "../markdown/spec_reader.h"

#include "cli/cli.h"
#include "foundation/buf.h"
#include "foundation/diag.h"
#include "po/po.h"
#include "po/po_read.h"
#include "po/po_write.h"
#include "test_util.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* spec.txt（タグ 0.31.2）の例の数 */
#define ROUNDTRIP_EXAMPLE_COUNT 652u

#define ROUNDTRIP_MAX_ARGS 8
#define ROUNDTRIP_MAX_LEN 1024
#define ROUNDTRIP_MAX_NAME 128

/* 入力の変種 */
typedef enum roundtrip_variant
{
    ROUNDTRIP_ORIGINAL,  /* spec.txt の例のまま（LF） */
    ROUNDTRIP_CRLF       /* LF を CRLF に変換したもの */
} roundtrip_variant_t;

/* translate に渡すカタログ */
typedef enum roundtrip_check
{
    ROUNDTRIP_EXTRACT_OUTPUT,   /* extract の出力そのもの（msgstr はすべて空） */
    ROUNDTRIP_MSGSTR_IS_MSGID   /* extract の出力の msgstr をすべて msgid と同じにしたもの */
} roundtrip_check_t;

/* test_cli_run に渡す argv（argv[argc] == NULL） */
typedef struct roundtrip_argv
{
    char storage[ROUNDTRIP_MAX_ARGS][ROUNDTRIP_MAX_LEN];
    char *argv[ROUNDTRIP_MAX_ARGS + 1];
    int argc;
} roundtrip_argv_t;

/* 1 つの検査で使うファイル名（作業ディレクトリからの相対。テスト名を接頭辞にして一意にする） */
typedef struct roundtrip_names
{
    char md[ROUNDTRIP_MAX_NAME];         /* 入力の Markdown */
    char pot[ROUNDTRIP_MAX_NAME];        /* extract の出力 */
    char po[ROUNDTRIP_MAX_NAME];         /* msgstr = msgid のカタログ */
    char run_extract[ROUNDTRIP_MAX_NAME];   /* extract の test_cli_run の名前 */
    char run_translate[ROUNDTRIP_MAX_NAME]; /* translate の test_cli_run の名前 */
} roundtrip_names_t;

/* ---- 補助 ---- */

static const char *variant_name(roundtrip_variant_t variant)
{
    return (variant == ROUNDTRIP_CRLF) ? "crlf" : "original";
}

static const char *check_name(roundtrip_check_t check)
{
    return (check == ROUNDTRIP_MSGSTR_IS_MSGID) ? "msgstr = msgid" : "extract output";
}

/* dst（大きさ ROUNDTRIP_MAX_NAME）に prefix と suffix をつなげて入れる。収まらなければ false */
static bool make_name(char *dst, const char *prefix, const char *suffix)
{
    if ((strlen(prefix) + strlen(suffix)) >= ROUNDTRIP_MAX_NAME)
    {
        return false;
    }
    strcpy(dst, prefix);
    strcat(dst, suffix);

    return true;
}

static bool make_names(const char *prefix, roundtrip_names_t *names)
{
    return (make_name(names->md, prefix, ".md")
            && make_name(names->pot, prefix, ".pot")
            && make_name(names->po, prefix, ".po")
            && make_name(names->run_extract, prefix, "_extract")
            && make_name(names->run_translate, prefix, "_translate"));
}

/* args（NULL で終わる）を "monde <command>" に続けて copy に入れる。収まらなければ false */
static bool build_argv(roundtrip_argv_t *copy, const char *command, const char *const *args)
{
    size_t i;

    strcpy(copy->storage[0], "monde");
    strcpy(copy->storage[1], command);
    copy->argc = 2;
    for (i = 0; args[i] != NULL; i++)
    {
        if ((copy->argc >= ROUNDTRIP_MAX_ARGS) || (strlen(args[i]) >= ROUNDTRIP_MAX_LEN))
        {
            return false;
        }
        strcpy(copy->storage[copy->argc], args[i]);
        copy->argc++;
    }
    for (i = 0; i < (size_t)copy->argc; i++)
    {
        copy->argv[i] = copy->storage[i];
    }
    copy->argv[copy->argc] = NULL;

    return true;
}

/* 変種の入力を out に作る（CRLF なら LF を CRLF に変換する。spec.txt に CR はない） */
static bool make_input(const mnd_buf_t *markdown, roundtrip_variant_t variant, mnd_buf_t *out)
{
    const char *data = mnd_buf_cstr(markdown);
    size_t i;

    mnd_buf_clear(out);
    if (variant == ROUNDTRIP_ORIGINAL)
    {
        return mnd_buf_append(out, data, markdown->len);
    }

    for (i = 0; i < markdown->len; i++)
    {
        if ((data[i] == '\n') && !mnd_buf_append_byte(out, (unsigned char)'\r'))
        {
            return false;
        }
        if (!mnd_buf_append_byte(out, (unsigned char)data[i]))
        {
            return false;
        }
    }

    return true;
}

/*
 * pot_name のカタログを読み、ヘッダ以外のすべてのエントリの msgstr を msgid と同じにして po_name に書く。
 * 失敗したら理由を返し、成功なら NULL
 */
static const char *write_identity_catalog(const char *pot_name, const char *po_name)
{
    mnd_buf_t text = MND_BUF_INIT;
    mnd_buf_t written = MND_BUF_INIT;
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_po_write_options_t options = MND_PO_WRITE_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const char *reason = NULL;
    size_t i;

    if (!test_read_file(pot_name, &text))
    {
        reason = "cannot read the extracted catalog";
    }
    else if (!mnd_po_read(mnd_buf_cstr(&text), text.len, pot_name, &catalog, &diags))
    {
        reason = "cannot parse the extracted catalog";
    }

    for (i = 0; (reason == NULL) && (i < catalog.count); i++)
    {
        mnd_po_entry_t *entry = &catalog.entries[i];

        if ((entry->msgid.len == 0) && !entry->has_msgctxt)
        {
            continue; /* ヘッダ */
        }
        if (entry->has_msgid_plural || (entry->msgstr.count != 1))
        {
            reason = "unexpected plural entry in the extracted catalog";
            break;
        }
        mnd_buf_clear(&entry->msgstr.items[0]);
        if (!mnd_buf_append(&entry->msgstr.items[0], mnd_buf_cstr(&entry->msgid), entry->msgid.len))
        {
            reason = "out of memory";
        }
    }

    if ((reason == NULL) && !mnd_po_write(&catalog, &options, &written, &diags))
    {
        reason = "cannot write the msgstr = msgid catalog";
    }
    if ((reason == NULL) && !test_write_file(po_name, mnd_buf_cstr(&written), written.len))
    {
        reason = "cannot save the msgstr = msgid catalog";
    }

    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&catalog);
    mnd_buf_free(&written);
    mnd_buf_free(&text);

    return reason;
}

/*
 * 1 つの例について、input を names->md に書き、入口を通して extract → translate を行い、出力が input と
 * バイト単位で一致するかを確かめる。一致すれば NULL、しなければ理由を返す
 */
static const char *check_example(const mnd_buf_t *input, roundtrip_check_t check, const roundtrip_names_t *names)
{
    roundtrip_argv_t copy;
    mnd_buf_t md_path = MND_BUF_INIT;
    mnd_buf_t pot_path = MND_BUF_INIT;
    mnd_buf_t po_path = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;
    const char *args[4];
    const char *reason = NULL;

    if (!test_write_file(names->md, mnd_buf_cstr(input), input->len)
        || !test_work_path(names->md, &md_path)
        || !test_work_path(names->pot, &pot_path)
        || !test_work_path(names->po, &po_path))
    {
        reason = "cannot prepare the input file";
    }

    /* monde extract <md> -o <pot> */
    if (reason == NULL)
    {
        args[0] = mnd_buf_cstr(&md_path);
        args[1] = "-o";
        args[2] = mnd_buf_cstr(&pot_path);
        args[3] = NULL;
        if (!build_argv(&copy, "extract", args)
            || !test_cli_run(names->run_extract, NULL, 0, copy.argc, copy.argv, &result))
        {
            reason = "cannot run extract";
        }
        else if (result.exit_code != MND_CLI_EXIT_OK)
        {
            reason = "extract failed";
        }
    }

    if ((reason == NULL) && (check == ROUNDTRIP_MSGSTR_IS_MSGID))
    {
        reason = write_identity_catalog(names->pot, names->po);
    }

    /* monde translate <md> <catalog> */
    if (reason == NULL)
    {
        args[0] = mnd_buf_cstr(&md_path);
        args[1] = (check == ROUNDTRIP_MSGSTR_IS_MSGID) ? mnd_buf_cstr(&po_path) : mnd_buf_cstr(&pot_path);
        args[2] = NULL;
        if (!build_argv(&copy, "translate", args)
            || !test_cli_run(names->run_translate, NULL, 0, copy.argc, copy.argv, &result))
        {
            reason = "cannot run translate";
        }
        else if (result.exit_code != MND_CLI_EXIT_OK)
        {
            reason = "translate failed";
        }
        else if ((result.out.len != input->len)
                 || ((input->len > 0) && (memcmp(mnd_buf_cstr(&result.out), mnd_buf_cstr(input), input->len) != 0)))
        {
            reason = "output differs from the input";
        }
    }

    test_cli_result_free(&result);
    mnd_buf_free(&po_path);
    mnd_buf_free(&pot_path);
    mnd_buf_free(&md_path);

    return reason;
}

/*
 * spec.txt の全例について、variant の入力を check のカタログでラウンドトリップする。
 * 一致しない例は番号・変種・検査の種類・理由をメモに出して失敗にし（検査は続ける）、全例を確かめた後に
 * 一致しなかった数を出す
 */
static void run_roundtrip(const char *test_name, roundtrip_variant_t variant, roundtrip_check_t check)
{
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    roundtrip_names_t names;
    mnd_buf_t input = MND_BUF_INIT;
    size_t failures = 0;
    size_t i;
    bool loaded;

    test_env_set(NULL, 0);
    NAOS_REQUIRE_TRUE(make_names(test_name, &names));

    loaded = mnd_md_spec_load(MND_MD_SPEC_FILE, &spec, &diags);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
    NAOS_REQUIRE_TRUE(loaded);
    NAOS_REQUIRE_UINT_EQ(ROUNDTRIP_EXAMPLE_COUNT, spec.count);

    for (i = 0; i < spec.count; i++)
    {
        const mnd_md_spec_example_t *ex = &spec.examples[i];
        const char *reason;

        if (!make_input(&ex->markdown, variant, &input))
        {
            reason = "out of memory";
        }
        else
        {
            reason = check_example(&input, check, &names);
        }

        if (reason != NULL)
        {
            NAOS_NOTE("example %zu (line %zu, section \"%s\"), variant %s, check %s: %s", ex->number,
                      ex->start_line, mnd_buf_cstr(&ex->section), variant_name(variant), check_name(check), reason);
            NAOS_CHECK_TRUE(false); /* メモは次の検査の失敗で表示される。失敗にして続ける */
            failures++;
        }
    }

    NAOS_NOTE("roundtrip %s / %s: %zu of %zu examples failed", variant_name(variant), check_name(check), failures,
              spec.count);
    NAOS_CHECK_UINT_EQ(0, failures);

    (void)test_remove_file(names.md);
    (void)test_remove_file(names.pot);
    (void)test_remove_file(names.po);

    mnd_buf_free(&input);
    mnd_md_spec_free(&spec);
}

/* ---- テスト ---- */

/* 原文: extract の出力（msgstr は空）で translate すると入力と一致する（10.1、10.4） */
NAOS_TEST(roundtrip_original_extract_output)
{
    run_roundtrip("roundtrip_original_extract_output", ROUNDTRIP_ORIGINAL, ROUNDTRIP_EXTRACT_OUTPUT);
}

/* 原文: msgstr をすべて msgid と同じにしたカタログで translate すると入力と一致する（10.2、10.4） */
NAOS_TEST(roundtrip_original_msgstr_is_msgid)
{
    run_roundtrip("roundtrip_original_msgstr_is_msgid", ROUNDTRIP_ORIGINAL, ROUNDTRIP_MSGSTR_IS_MSGID);
}

/* CRLF: extract の出力（msgstr は空）で translate すると入力と一致する（10.3、10.4） */
NAOS_TEST(roundtrip_crlf_extract_output)
{
    run_roundtrip("roundtrip_crlf_extract_output", ROUNDTRIP_CRLF, ROUNDTRIP_EXTRACT_OUTPUT);
}

/* CRLF: msgstr をすべて msgid と同じにしたカタログで translate すると入力と一致する（10.2、10.3、10.4） */
NAOS_TEST(roundtrip_crlf_msgstr_is_msgid)
{
    run_roundtrip("roundtrip_crlf_msgstr_is_msgid", ROUNDTRIP_CRLF, ROUNDTRIP_MSGSTR_IS_MSGID);
}

NAOS_SUITE(roundtrip,
    NAOS_ADD_TEST(roundtrip_original_extract_output),
    NAOS_ADD_TEST(roundtrip_original_msgstr_is_msgid),
    NAOS_ADD_TEST(roundtrip_crlf_extract_output),
    NAOS_ADD_TEST(roundtrip_crlf_msgstr_is_msgid)
)
