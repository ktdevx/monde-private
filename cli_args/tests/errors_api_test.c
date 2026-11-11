#include <naos/naos.h>

#include "clopts/errors.h"
#include "clopts_buf.h"
#include "clopts_internal.h"

#include <stdio.h>
#include <stdlib.h>

/* エラーの列（clopts_errors_t）の単体テスト（3.3、3.5） */

NAOS_TEST(errors_init_macro_is_empty)
{
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&errors));
    NAOS_CHECK_NULL(clopts_errors_at(&errors, 0));
    NAOS_CHECK_FALSE(clopts_errors_dropped(&errors));
    NAOS_CHECK_NULL(errors.messages);
    NAOS_CHECK_UINT_EQ(0, errors.cap);
}

NAOS_TEST(errors_init_function_resets_to_empty)
{
    clopts_errors_t errors;

    errors.messages = NULL;
    errors.count = 3;
    errors.cap = 7;
    errors.dropped = true;
    clopts_errors_init(&errors);
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&errors));
    NAOS_CHECK_NULL(errors.messages);
    NAOS_CHECK_UINT_EQ(0, errors.cap);
    NAOS_CHECK_FALSE(clopts_errors_dropped(&errors));
}

NAOS_TEST(errors_free_on_init_is_safe_twice)
{
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    clopts_errors_free(&errors);
    clopts_errors_free(&errors);
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&errors));
    NAOS_CHECK_NULL(errors.messages);
    NAOS_CHECK_FALSE(clopts_errors_dropped(&errors));
}

NAOS_TEST(errors_add_keeps_order_and_formats)
{
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    clopts_errors_add(&errors, "unknown option '%s'", "--foo");
    clopts_errors_add(&errors, "argument %d is bad", 3);
    clopts_errors_add(&errors, "plain");
    NAOS_REQUIRE_UINT_EQ(3, clopts_errors_count(&errors));
    NAOS_CHECK_STR_EQ("unknown option '--foo'", clopts_errors_at(&errors, 0));
    NAOS_CHECK_STR_EQ("argument 3 is bad", clopts_errors_at(&errors, 1));
    NAOS_CHECK_STR_EQ("plain", clopts_errors_at(&errors, 2));
    NAOS_CHECK_FALSE(clopts_errors_dropped(&errors));

    clopts_errors_free(&errors);
}

NAOS_TEST(errors_add_many_grows)
{
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;
    size_t i;
    char expected[32];

    for (i = 0; i < 100; i++)
    {
        clopts_errors_add(&errors, "error %u", (unsigned)i);
    }

    NAOS_REQUIRE_UINT_EQ(100, clopts_errors_count(&errors));
    NAOS_CHECK_UINT_GE(errors.cap, errors.count);
    for (i = 0; i < 100; i++)
    {
        snprintf(expected, sizeof(expected), "error %u", (unsigned)i);
        NAOS_CHECK_STR_EQ(expected, clopts_errors_at(&errors, i));
    }

    clopts_errors_free(&errors);
}

NAOS_TEST(errors_at_out_of_range_is_null)
{
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    clopts_errors_add(&errors, "only");
    NAOS_REQUIRE_UINT_EQ(1, clopts_errors_count(&errors));
    NAOS_CHECK_NOT_NULL(clopts_errors_at(&errors, 0));
    NAOS_CHECK_NULL(clopts_errors_at(&errors, 1));
    NAOS_CHECK_NULL(clopts_errors_at(&errors, (size_t)-1));

    clopts_errors_free(&errors);
}

NAOS_TEST(errors_free_after_add_returns_to_init_and_is_safe_twice)
{
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    clopts_errors_add(&errors, "first");
    clopts_errors_add(&errors, "second");
    errors.dropped = true;
    clopts_errors_free(&errors);
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&errors));
    NAOS_CHECK_NULL(errors.messages);
    NAOS_CHECK_UINT_EQ(0, errors.cap);
    NAOS_CHECK_FALSE(clopts_errors_dropped(&errors));
    NAOS_CHECK_NULL(clopts_errors_at(&errors, 0));

    clopts_errors_free(&errors);
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&errors));
}

NAOS_TEST(errors_dropped_reads_flag)
{
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    /*
     * メモリ不足を起こす手段は設計にないため、記録できなかったときに
     * ライブラリが立てる dropped を直接立て、読み取りの関数が返すことを確かめる
     */
    clopts_errors_add(&errors, "kept");
    NAOS_CHECK_FALSE(clopts_errors_dropped(&errors));
    errors.dropped = true;
    NAOS_CHECK_TRUE(clopts_errors_dropped(&errors));

    /* dropped が立っていても、記録済みのメッセージと件数は変わらず、追加もできる */
    NAOS_CHECK_UINT_EQ(1, clopts_errors_count(&errors));
    NAOS_CHECK_STR_EQ("kept", clopts_errors_at(&errors, 0));
    clopts_errors_add(&errors, "after");
    NAOS_CHECK_UINT_EQ(2, clopts_errors_count(&errors));
    NAOS_CHECK_STR_EQ("after", clopts_errors_at(&errors, 1));
    NAOS_CHECK_TRUE(clopts_errors_dropped(&errors));

    clopts_errors_free(&errors);
}

NAOS_SUITE(errors_api,
    NAOS_ADD_TEST(errors_init_macro_is_empty),
    NAOS_ADD_TEST(errors_init_function_resets_to_empty),
    NAOS_ADD_TEST(errors_free_on_init_is_safe_twice),
    NAOS_ADD_TEST(errors_add_keeps_order_and_formats),
    NAOS_ADD_TEST(errors_add_many_grows),
    NAOS_ADD_TEST(errors_at_out_of_range_is_null),
    NAOS_ADD_TEST(errors_free_after_add_returns_to_init_and_is_safe_twice),
    NAOS_ADD_TEST(errors_dropped_reads_flag)
)

/* 内部バッファ（clopts_buf_t）の単体テスト。公開しないため src/ の内部ヘッダを使う */

NAOS_TEST(buf_init_is_empty_string)
{
    clopts_buf_t buf = CLOPTS_BUF_INIT;

    NAOS_CHECK_STR_EQ("", clopts_buf_cstr(&buf));
    NAOS_CHECK_UINT_EQ(0, buf.len);
    clopts_buf_free(&buf);
    clopts_buf_free(&buf);
    NAOS_CHECK_NULL(buf.data);
}

NAOS_TEST(buf_append_concatenates)
{
    clopts_buf_t buf = CLOPTS_BUF_INIT;
    size_t i;

    NAOS_REQUIRE_TRUE(clopts_buf_append_str(&buf, "Usage: "));
    NAOS_REQUIRE_TRUE(clopts_buf_append_str(&buf, ""));
    NAOS_REQUIRE_TRUE(clopts_buf_append_str(&buf, "tool"));
    NAOS_REQUIRE_TRUE(clopts_buf_append_byte(&buf, (unsigned char)'\n'));
    NAOS_CHECK_STR_EQ("Usage: tool\n", clopts_buf_cstr(&buf));
    NAOS_CHECK_UINT_EQ(12, buf.len);

    for (i = 0; i < 100; i++)
    {
        NAOS_REQUIRE_TRUE(clopts_buf_append_byte(&buf, (unsigned char)'x'));
    }

    NAOS_CHECK_UINT_EQ(112, buf.len);
    NAOS_CHECK_UINT_GE(buf.cap, buf.len + 1);

    clopts_buf_free(&buf);
    NAOS_CHECK_NULL(buf.data);
    NAOS_CHECK_UINT_EQ(0, buf.len);
    NAOS_CHECK_UINT_EQ(0, buf.cap);
}

NAOS_TEST(buf_take_moves_ownership)
{
    clopts_buf_t buf = CLOPTS_BUF_INIT;
    char *text;

    NAOS_REQUIRE_TRUE(clopts_buf_append_str(&buf, "tool 1.0\n"));
    text = clopts_buf_take(&buf);
    NAOS_REQUIRE_NOT_NULL(text);
    NAOS_CHECK_STR_EQ("tool 1.0\n", text);
    NAOS_CHECK_NULL(buf.data);
    NAOS_CHECK_UINT_EQ(0, buf.len);
    NAOS_CHECK_UINT_EQ(0, buf.cap);
    NAOS_CHECK_STR_EQ("", clopts_buf_cstr(&buf));
    free(text);
    clopts_buf_free(&buf);
}

NAOS_TEST(buf_take_on_empty_returns_empty_copy)
{
    clopts_buf_t buf = CLOPTS_BUF_INIT;
    char *text;

    text = clopts_buf_take(&buf);
    NAOS_REQUIRE_NOT_NULL(text);
    NAOS_CHECK_STR_EQ("", text);
    free(text);
    clopts_buf_free(&buf);
}

NAOS_SUITE(buf,
    NAOS_ADD_TEST(buf_init_is_empty_string),
    NAOS_ADD_TEST(buf_append_concatenates),
    NAOS_ADD_TEST(buf_take_moves_ownership),
    NAOS_ADD_TEST(buf_take_on_empty_returns_empty_copy)
)
