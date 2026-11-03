#include <naos/naos.h>

#include <stdlib.h>
#include <string.h>

#include "foundation/diag.h"

NAOS_TEST(list_init_macro_is_empty)
{
    mnd_diag_list list = MND_DIAG_LIST_INIT;

    NAOS_CHECK_NULL(list.items);
    NAOS_CHECK_UINT_EQ(0, list.count);
    NAOS_CHECK_UINT_EQ(0, list.cap);
    NAOS_CHECK_FALSE(list.dropped);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&list));
    NAOS_CHECK_FALSE(mnd_diag_list_has_error(&list));
}

NAOS_TEST(list_init_resets_to_empty)
{
    mnd_diag dummy;
    mnd_diag_list list;

    list.items = &dummy;
    list.count = 3;
    list.cap = 5;
    list.dropped = true;
    mnd_diag_list_init(&list);

    NAOS_CHECK_NULL(list.items);
    NAOS_CHECK_UINT_EQ(0, list.count);
    NAOS_CHECK_UINT_EQ(0, list.cap);
    NAOS_CHECK_FALSE(list.dropped);
}

NAOS_TEST(add_stores_all_fields)
{
    mnd_diag_list list = MND_DIAG_LIST_INIT;
    const mnd_diag *d;

    mnd_diag_add(&list, MND_SEVERITY_ERROR, "docs/guide.md", 12,
                 "diag-test unexpected '%s' at byte offset %d", "x", 7);

    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&list));
    NAOS_CHECK_FALSE(list.dropped);
    d = mnd_diag_list_at(&list, 0);
    NAOS_REQUIRE_NOT_NULL(d);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
    NAOS_REQUIRE_NOT_NULL(d->message);
    NAOS_CHECK_STR_EQ("diag-test unexpected 'x' at byte offset 7", d->message);
    NAOS_REQUIRE_NOT_NULL(d->path);
    NAOS_CHECK_STR_EQ("docs/guide.md", d->path);
    NAOS_CHECK_UINT_EQ(12, d->line);

    mnd_diag_list_free(&list);
}

NAOS_TEST(add_copies_message_and_path)
{
    mnd_diag_list list = MND_DIAG_LIST_INIT;
    char path[] = "a/b.md";
    char arg[] = "word";
    const mnd_diag *d;

    mnd_diag_add(&list, MND_SEVERITY_WARNING, path, 1, "diag-test %s", arg);
    path[0] = 'z';
    arg[0] = 'z';

    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&list));
    d = mnd_diag_list_at(&list, 0);
    NAOS_REQUIRE_NOT_NULL(d);
    NAOS_REQUIRE_NOT_NULL(d->path);
    NAOS_REQUIRE_NOT_NULL(d->message);
    NAOS_CHECK_TRUE(d->path != path);
    NAOS_CHECK_STR_EQ("a/b.md", d->path);
    NAOS_CHECK_STR_EQ("diag-test word", d->message);

    mnd_diag_list_free(&list);
}

NAOS_TEST(add_without_path_and_line)
{
    mnd_diag_list list = MND_DIAG_LIST_INIT;
    const mnd_diag *d;

    mnd_diag_add(&list, MND_SEVERITY_WARNING, NULL, 0, "diag-test no location");

    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&list));
    d = mnd_diag_list_at(&list, 0);
    NAOS_REQUIRE_NOT_NULL(d);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_WARNING, d->severity);
    NAOS_CHECK_NULL(d->path);
    NAOS_CHECK_UINT_EQ(0, d->line);
    NAOS_REQUIRE_NOT_NULL(d->message);
    NAOS_CHECK_STR_EQ("diag-test no location", d->message);

    mnd_diag_list_free(&list);
}

NAOS_TEST(add_keeps_insertion_order)
{
    mnd_diag_list list = MND_DIAG_LIST_INIT;
    const mnd_diag *d;

    mnd_diag_add(&list, MND_SEVERITY_WARNING, "f.md", 10, "diag-test item %d", 1);
    mnd_diag_add(&list, MND_SEVERITY_ERROR, "g.md", 20, "diag-test item %d", 2);
    mnd_diag_add(&list, MND_SEVERITY_WARNING, NULL, 30, "diag-test item %d", 3);

    NAOS_REQUIRE_UINT_EQ(3, mnd_diag_list_count(&list));

    d = mnd_diag_list_at(&list, 0);
    NAOS_REQUIRE_NOT_NULL(d);
    NAOS_REQUIRE_NOT_NULL(d->message);
    NAOS_CHECK_STR_EQ("diag-test item 1", d->message);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_WARNING, d->severity);
    NAOS_REQUIRE_NOT_NULL(d->path);
    NAOS_CHECK_STR_EQ("f.md", d->path);
    NAOS_CHECK_UINT_EQ(10, d->line);

    d = mnd_diag_list_at(&list, 1);
    NAOS_REQUIRE_NOT_NULL(d);
    NAOS_REQUIRE_NOT_NULL(d->message);
    NAOS_CHECK_STR_EQ("diag-test item 2", d->message);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
    NAOS_REQUIRE_NOT_NULL(d->path);
    NAOS_CHECK_STR_EQ("g.md", d->path);
    NAOS_CHECK_UINT_EQ(20, d->line);

    d = mnd_diag_list_at(&list, 2);
    NAOS_REQUIRE_NOT_NULL(d);
    NAOS_REQUIRE_NOT_NULL(d->message);
    NAOS_CHECK_STR_EQ("diag-test item 3", d->message);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_WARNING, d->severity);
    NAOS_CHECK_NULL(d->path);
    NAOS_CHECK_UINT_EQ(30, d->line);

    mnd_diag_list_free(&list);
}

NAOS_TEST(add_many_grows_list)
{
    mnd_diag_list list = MND_DIAG_LIST_INIT;
    const mnd_diag *d;
    unsigned i;

    for (i = 0; i < 100u; i++) {
        mnd_diag_add(&list, MND_SEVERITY_WARNING, NULL, (size_t)i, "diag-test n%u", i);
    }

    NAOS_REQUIRE_UINT_EQ(100, mnd_diag_list_count(&list));
    NAOS_CHECK_FALSE(list.dropped);
    d = mnd_diag_list_at(&list, 0);
    NAOS_REQUIRE_NOT_NULL(d);
    NAOS_REQUIRE_NOT_NULL(d->message);
    NAOS_CHECK_STR_EQ("diag-test n0", d->message);
    d = mnd_diag_list_at(&list, 99);
    NAOS_REQUIRE_NOT_NULL(d);
    NAOS_REQUIRE_NOT_NULL(d->message);
    NAOS_CHECK_STR_EQ("diag-test n99", d->message);
    NAOS_CHECK_UINT_EQ(99, d->line);

    mnd_diag_list_free(&list);
}

NAOS_TEST(add_long_message_is_not_truncated)
{
    enum { LONG_LEN = 10000 };
    mnd_diag_list list = MND_DIAG_LIST_INIT;
    const mnd_diag *d;
    char *arg;

    arg = (char *)malloc(LONG_LEN + 1);
    /* 未初期化の領域を const void * で渡すと GCC が -Wmaybe-uninitialized を出すため、比較で確かめる */
    NAOS_REQUIRE_TRUE(arg != NULL);
    memset(arg, 'q', LONG_LEN);
    arg[LONG_LEN] = '\0';

    mnd_diag_add(&list, MND_SEVERITY_ERROR, NULL, 0, "[%s]", arg);

    if (mnd_diag_list_count(&list) == 1) {
        d = mnd_diag_list_at(&list, 0);
        NAOS_CHECK_NOT_NULL(d);
        if (d != NULL && d->message != NULL) {
            NAOS_CHECK_UINT_EQ(LONG_LEN + 2, strlen(d->message));
            NAOS_CHECK_INT_EQ('[', d->message[0]);
            NAOS_CHECK_INT_EQ(']', d->message[LONG_LEN + 1]);
            NAOS_CHECK_MEM_EQ(arg, LONG_LEN, d->message + 1, LONG_LEN);
        } else {
            NAOS_CHECK_TRUE(d != NULL && d->message != NULL);
        }
    } else {
        NAOS_CHECK_UINT_EQ(1, mnd_diag_list_count(&list));
    }

    free(arg);
    mnd_diag_list_free(&list);
}

NAOS_TEST(add_formats_percent_literal)
{
    mnd_diag_list list = MND_DIAG_LIST_INIT;
    const mnd_diag *d;

    mnd_diag_add(&list, MND_SEVERITY_WARNING, NULL, 0, "diag-test 100%% %s", "%d");

    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&list));
    d = mnd_diag_list_at(&list, 0);
    NAOS_REQUIRE_NOT_NULL(d);
    NAOS_REQUIRE_NOT_NULL(d->message);
    NAOS_CHECK_STR_EQ("diag-test 100% %d", d->message);

    mnd_diag_list_free(&list);
}

NAOS_TEST(has_error_false_for_warnings_only)
{
    mnd_diag_list list = MND_DIAG_LIST_INIT;

    mnd_diag_add(&list, MND_SEVERITY_WARNING, NULL, 0, "diag-test warn 1");
    mnd_diag_add(&list, MND_SEVERITY_WARNING, "w.md", 3, "diag-test warn 2");

    NAOS_REQUIRE_UINT_EQ(2, mnd_diag_list_count(&list));
    NAOS_CHECK_FALSE(mnd_diag_list_has_error(&list));

    mnd_diag_list_free(&list);
}

NAOS_TEST(has_error_true_with_one_error)
{
    mnd_diag_list list = MND_DIAG_LIST_INIT;

    mnd_diag_add(&list, MND_SEVERITY_WARNING, NULL, 0, "diag-test warn 1");
    mnd_diag_add(&list, MND_SEVERITY_ERROR, "e.md", 4, "diag-test error");
    mnd_diag_add(&list, MND_SEVERITY_WARNING, NULL, 0, "diag-test warn 2");

    NAOS_REQUIRE_UINT_EQ(3, mnd_diag_list_count(&list));
    NAOS_CHECK_TRUE(mnd_diag_list_has_error(&list));

    mnd_diag_list_free(&list);
}

NAOS_TEST(has_error_true_when_dropped)
{
    mnd_diag_list list = MND_DIAG_LIST_INIT;

    list.dropped = true;
    NAOS_CHECK_TRUE(mnd_diag_list_has_error(&list));

    mnd_diag_add(&list, MND_SEVERITY_WARNING, NULL, 0, "diag-test warn");
    NAOS_CHECK_TRUE(mnd_diag_list_has_error(&list));

    mnd_diag_list_free(&list);
    NAOS_CHECK_FALSE(list.dropped);
    NAOS_CHECK_FALSE(mnd_diag_list_has_error(&list));
}

NAOS_TEST(free_resets_and_is_repeatable)
{
    mnd_diag_list list = MND_DIAG_LIST_INIT;

    mnd_diag_list_free(&list);
    NAOS_CHECK_NULL(list.items);
    NAOS_CHECK_UINT_EQ(0, list.count);

    mnd_diag_add(&list, MND_SEVERITY_ERROR, "p.md", 1, "diag-test %d", 1);
    mnd_diag_add(&list, MND_SEVERITY_WARNING, NULL, 0, "diag-test %d", 2);
    NAOS_REQUIRE_UINT_EQ(2, mnd_diag_list_count(&list));

    mnd_diag_list_free(&list);
    NAOS_CHECK_NULL(list.items);
    NAOS_CHECK_UINT_EQ(0, list.count);
    NAOS_CHECK_UINT_EQ(0, list.cap);
    NAOS_CHECK_FALSE(list.dropped);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&list));
    NAOS_CHECK_FALSE(mnd_diag_list_has_error(&list));

    mnd_diag_list_free(&list);
    NAOS_CHECK_NULL(list.items);

    mnd_diag_add(&list, MND_SEVERITY_WARNING, NULL, 0, "diag-test reuse");
    NAOS_CHECK_UINT_EQ(1, mnd_diag_list_count(&list));
    mnd_diag_list_free(&list);
}

NAOS_SUITE(diag,
    NAOS_ADD_TEST(list_init_macro_is_empty),
    NAOS_ADD_TEST(list_init_resets_to_empty),
    NAOS_ADD_TEST(add_stores_all_fields),
    NAOS_ADD_TEST(add_copies_message_and_path),
    NAOS_ADD_TEST(add_without_path_and_line),
    NAOS_ADD_TEST(add_keeps_insertion_order),
    NAOS_ADD_TEST(add_many_grows_list),
    NAOS_ADD_TEST(add_long_message_is_not_truncated),
    NAOS_ADD_TEST(add_formats_percent_literal),
    NAOS_ADD_TEST(has_error_false_for_warnings_only),
    NAOS_ADD_TEST(has_error_true_with_one_error),
    NAOS_ADD_TEST(has_error_true_when_dropped),
    NAOS_ADD_TEST(free_resets_and_is_repeatable)
)
