/*
 * po_merge の性能テスト（10.1）。Naos を使わない独立した実行ファイル。
 *
 * 固定の種の擬似乱数と単語の表から、平均 200 バイトの英語風の msgid を持つ 2,000 エントリの
 * 既存のカタログ（すべて翻訳済み）を作る。テンプレートは同じ 2,000 エントリのうち 200 エントリの
 * msgid の一部の語を置き換えたものとし、マージだけの時間を clock() で測る。
 * 第 1 引数の構成名が Release のときは 1.0 秒を超えたら失敗し、ほかの構成では時間を表示するだけにする。
 * 参考として、全 2,000 エントリの msgid を変えた場合（最悪の場合）の時間も表示する（判定しない）。
 */

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "po/po.h"
#include "po_merge/merge.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define PERF_ENTRY_COUNT 2000
#define PERF_CHANGED_INTERVAL 10   /* 基準の場合は 10 エントリに 1 つ（計 200）の msgid を変える */
#define PERF_REPLACED_WORDS 3      /* 変えるエントリで置き換える語の数 */
#define PERF_MAX_WORDS 64          /* 1 つの msgid の語の数の上限 */
#define PERF_MIN_MSGID_BYTES 150   /* msgid の長さの目標の下限（150〜250 バイトで平均 200 バイト） */
#define PERF_MSGID_BYTES_RANGE 101
#define PERF_LIMIT_SECONDS 1.0

static const char *const perf_words[] = {
    "the", "document", "translation", "file", "update", "source", "paragraph", "heading",
    "list", "item", "code", "block", "link", "image", "table", "reference",
    "should", "will", "can", "must", "be", "is", "are", "was",
    "when", "if", "and", "or", "with", "without", "for", "from",
    "into", "each", "every", "user", "translator", "template", "catalog", "entry",
    "message", "original", "text", "changed", "new", "old", "fuzzy", "match",
    "build", "output", "input", "directory", "command", "option", "default", "value",
    "markdown", "section", "example", "chapter", "release", "version", "project", "guide"
};

#define PERF_WORD_COUNT (sizeof(perf_words) / sizeof(perf_words[0]))

/* 固定の種の擬似乱数（xorshift32）。実行ごとに同じ列を返す */
static uint32_t perf_random_state = 2463534242u;

static uint32_t perf_random(void)
{
    uint32_t x = perf_random_state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    perf_random_state = x;

    return x;
}

/* 0 以上 n 未満の擬似乱数（n > 0） */
static size_t perf_random_below(size_t n)
{
    return (size_t)(perf_random() % (uint32_t)n);
}

/* 語の添字の列 words[0..count) を空白で区切って buf に書く。false はメモリ不足 */
static bool perf_join_words(const size_t *words, size_t count, mnd_buf_t *buf)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        if ((i > 0) && !mnd_buf_append_byte(buf, ' '))
        {
            return false;
        }

        if (!mnd_buf_append_str(buf, perf_words[words[i]]))
        {
            return false;
        }
    }

    return mnd_buf_append_byte(buf, '.');
}

/* 目標の長さに達するまで語を選び、words に入れて語の数を返す */
static size_t perf_make_words(size_t *words)
{
    size_t target = PERF_MIN_MSGID_BYTES + perf_random_below(PERF_MSGID_BYTES_RANGE);
    size_t len = 0;
    size_t count = 0;

    while ((len < target) && (count < PERF_MAX_WORDS))
    {
        words[count] = perf_random_below(PERF_WORD_COUNT);
        len += strlen(perf_words[words[count]]) + 1;
        count++;
    }

    return count;
}

/* words のうち PERF_REPLACED_WORDS 個の位置の語を、元と異なる語に置き換える */
static void perf_replace_words(size_t *words, size_t count)
{
    size_t k;

    for (k = 0; k < PERF_REPLACED_WORDS; k++)
    {
        size_t pos = perf_random_below(count);
        size_t shift = 1 + perf_random_below(PERF_WORD_COUNT - 1);

        words[pos] = (words[pos] + shift) % PERF_WORD_COUNT;
    }
}

/* catalog にヘッダを加える。false はメモリ不足 */
static bool perf_add_header(mnd_po_catalog_t *catalog, bool translated)
{
    static const char header[] =
        "Project-Id-Version: perf 1.0\n"
        "Content-Type: text/plain; charset=UTF-8\n"
        "Content-Transfer-Encoding: 8bit\n";
    static const char translated_header[] =
        "Project-Id-Version: perf 1.0\n"
        "Language: ja\n"
        "Content-Type: text/plain; charset=UTF-8\n"
        "Content-Transfer-Encoding: 8bit\n"
        "Plural-Forms: nplurals=1; plural=0;\n";
    mnd_po_entry_t *entry = mnd_po_catalog_append(catalog);
    const char *text = translated ? translated_header : header;

    if (entry == NULL)
    {
        return false;
    }

    return mnd_po_strings_push(&entry->msgstr, text, strlen(text));
}

/*
 * catalog の末尾に msgid が words のエントリを加える。translated なら訳（msgid の前に「訳: 」を付けたもの）を、
 * そうでなければ空の msgstr を持たせる。line は参照の行番号に使う。false はメモリ不足
 */
static bool perf_add_entry(mnd_po_catalog_t *catalog, const size_t *words, size_t count,
                           bool translated, size_t line)
{
    mnd_po_entry_t *entry = mnd_po_catalog_append(catalog);
    mnd_buf_t msgstr = MND_BUF_INIT;
    bool ok;

    if (entry == NULL)
    {
        return false;
    }

    if (!perf_join_words(words, count, &entry->msgid)
        || !mnd_po_refs_push(&entry->refs, "docs/guide.md", line))
    {
        return false;
    }

    if (!translated)
    {
        return mnd_po_strings_push(&entry->msgstr, "", 0);
    }

    ok = mnd_buf_append_str(&msgstr, "訳: ")
         && mnd_buf_append(&msgstr, entry->msgid.data, entry->msgid.len)
         && mnd_po_strings_push(&entry->msgstr, mnd_buf_cstr(&msgstr), msgstr.len);
    mnd_buf_free(&msgstr);

    return ok;
}

/*
 * 既存のカタログ old_catalog（全エントリが翻訳済み）と、2 つのテンプレートを作る。
 * baseline は 10 エントリに 1 つの msgid の語を置き換えたもの、worst は全エントリの語を置き換えたもの。
 * total_bytes に既存の msgid の合計バイト数を返す。false はメモリ不足
 */
static bool perf_build_catalogs(mnd_po_catalog_t *old_catalog, mnd_po_catalog_t *baseline,
                                mnd_po_catalog_t *worst, size_t *total_bytes)
{
    size_t words[PERF_MAX_WORDS];
    size_t changed[PERF_MAX_WORDS];
    size_t i;

    *total_bytes = 0;
    if (!perf_add_header(old_catalog, true) || !perf_add_header(baseline, false)
        || !perf_add_header(worst, false))
    {
        return false;
    }

    for (i = 0; i < PERF_ENTRY_COUNT; i++)
    {
        size_t count = perf_make_words(words);
        size_t line = (i * 4) + 1;

        memcpy(changed, words, count * sizeof(words[0]));
        perf_replace_words(changed, count);
        if (!perf_add_entry(old_catalog, words, count, true, line)
            || !perf_add_entry(baseline, ((i % PERF_CHANGED_INTERVAL) == 0) ? changed : words,
                               count, false, line)
            || !perf_add_entry(worst, changed, count, false, line))
        {
            return false;
        }

        *total_bytes += old_catalog->entries[old_catalog->count - 1].msgid.len;
    }

    return true;
}

/* diags の各診断を標準エラー出力に表示する */
static void perf_print_diags(const mnd_diag_list_t *diags)
{
    size_t count = mnd_diag_list_count(diags);
    size_t i;

    for (i = 0; i < count; i++)
    {
        const mnd_diag_t *diag = mnd_diag_list_at(diags, i);

        fprintf(stderr, "%s:%zu: %s\n", (diag->path != NULL) ? diag->path : "(unknown)",
                diag->line, diag->message);
    }
}

/*
 * template_catalog と old_catalog をマージし、マージだけの時間（秒）を *seconds に、統計を *stats に返す。
 * マージに失敗したら診断を表示して false を返す
 */
static bool perf_measure(const char *name, const mnd_po_catalog_t *template_catalog,
                         const mnd_po_catalog_t *old_catalog, mnd_merge_stats_t *stats,
                         double *seconds)
{
    mnd_po_catalog_t out = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    clock_t start;
    clock_t end;
    bool ok;

    start = clock();
    ok = mnd_merge_catalogs(template_catalog, "template.pot", old_catalog, "old.po", NULL,
                            &out, stats, &diags);
    end = clock();
    *seconds = (double)(end - start) / (double)CLOCKS_PER_SEC;

    if (!ok)
    {
        fprintf(stderr, "%s: mnd_merge_catalogs failed\n", name);
        perf_print_diags(&diags);
    }
    else
    {
        printf("%s: %.3f s (matched %zu, fuzzy %zu, added %zu, obsoleted %zu, output %zu entries)\n",
               name, *seconds, stats->matched, stats->fuzzy, stats->added, stats->obsoleted,
               out.count);
    }

    mnd_po_catalog_free(&out);
    mnd_diag_list_free(&diags);

    return ok;
}

/*
 * 統計が作ったデータどおりかを確かめる（測った処理が実際にマージをしたことの確認）。
 * expected_matched 個が完全一致し、残りはすべてあいまい一致になり、新規と廃止エントリがないことを期待する
 */
static bool perf_check_stats(const char *name, const mnd_merge_stats_t *stats,
                             size_t expected_matched)
{
    size_t expected_fuzzy = PERF_ENTRY_COUNT - expected_matched;

    if ((stats->matched != expected_matched) || (stats->fuzzy != expected_fuzzy)
        || (stats->added != 0) || (stats->obsoleted != 0))
    {
        fprintf(stderr,
                "%s: unexpected stats (expected matched %zu, fuzzy %zu, added 0, obsoleted 0)\n",
                name, expected_matched, expected_fuzzy);

        return false;
    }

    return true;
}

int main(int argc, char **argv)
{
    const char *config = ((argc > 1) && (argv[1][0] != '\0')) ? argv[1] : "(none)";
    bool judge = (strcmp(config, "Release") == 0);
    mnd_po_catalog_t old_catalog = MND_PO_CATALOG_INIT;
    mnd_po_catalog_t baseline = MND_PO_CATALOG_INIT;
    mnd_po_catalog_t worst = MND_PO_CATALOG_INIT;
    mnd_merge_stats_t stats = MND_MERGE_STATS_INIT;
    size_t total_bytes = 0;
    double baseline_seconds = 0.0;
    double worst_seconds = 0.0;
    bool ok = true;

    if (!perf_build_catalogs(&old_catalog, &baseline, &worst, &total_bytes))
    {
        fprintf(stderr, "out of memory while building catalogs\n");
        ok = false;
    }
    else
    {
        printf("config: %s, %d entries, average msgid %.1f bytes\n", config, PERF_ENTRY_COUNT,
               (double)total_bytes / (double)PERF_ENTRY_COUNT);

        if (!perf_measure("baseline (200 changed)", &baseline, &old_catalog, &stats,
                          &baseline_seconds)
            || !perf_check_stats("baseline", &stats,
                                 PERF_ENTRY_COUNT - (PERF_ENTRY_COUNT / PERF_CHANGED_INTERVAL)))
        {
            ok = false;
        }

        /* 最悪の場合は参考として時間を表示するだけで、判定しない */
        if (!perf_measure("worst case (all 2000 changed, not judged)", &worst, &old_catalog, &stats,
                          &worst_seconds)
            || !perf_check_stats("worst case", &stats, 0))
        {
            ok = false;
        }

        if (ok && judge)
        {
            if (baseline_seconds > PERF_LIMIT_SECONDS)
            {
                fprintf(stderr, "baseline merge took %.3f s, over the limit %.1f s\n",
                        baseline_seconds, PERF_LIMIT_SECONDS);
                ok = false;
            }
            else
            {
                printf("baseline is within the limit %.1f s\n", PERF_LIMIT_SECONDS);
            }
        }
        else if (ok)
        {
            printf("not a Release build: the time is not judged\n");
        }
    }

    mnd_po_catalog_free(&old_catalog);
    mnd_po_catalog_free(&baseline);
    mnd_po_catalog_free(&worst);

    return ok ? 0 : 1;
}
