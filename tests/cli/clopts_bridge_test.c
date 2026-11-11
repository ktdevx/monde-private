/* スイート clopts_bridge: CLOpts のエラーの列から Monde の診断の列への変換（7.4） */

#include <naos/naos.h>

#include "cli/cli_clopts.h"
#include "clopts/errors.h"
#include "foundation/diag.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * 変換の入力は公開の構造体を直接組み立てて作る（追加の関数は CLOpts の内部だけが持つ）。
 * メッセージは静的な配列を指すので、clopts_errors_free は呼ばない
 */

/* diags の index 番目が、パスなし・行番号 0・メッセージ message のエラーの診断であること */
static void check_error_diag(const mnd_diag_list_t *diags, size_t index, const char *message)
{
    const mnd_diag_t *diag;

    NAOS_REQUIRE_TRUE(index < mnd_diag_list_count(diags));
    diag = mnd_diag_list_at(diags, index);
    NAOS_CHECK_INT_EQ((int)MND_SEVERITY_ERROR, (int)diag->severity);
    NAOS_CHECK_NULL(diag->path);
    NAOS_CHECK_UINT_EQ(0, diag->line);
    NAOS_REQUIRE_NOT_NULL(diag->message);
    NAOS_CHECK_STR_EQ(message, diag->message);
}

/* エラーが 0 件なら診断を加えず、dropped にもしない */
NAOS_TEST(clopts_bridge_empty_adds_nothing)
{
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    mnd_cli_clopts_errors_to_diags(&errors, &diags);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_CHECK_FALSE(diags.dropped);
    NAOS_CHECK_FALSE(mnd_diag_list_has_error(&diags));

    mnd_diag_list_free(&diags);
}

/* 複数件のエラーを、同じ順序で、既存の診断の後ろに追加する */
NAOS_TEST(clopts_bridge_multiple_keeps_order_after_existing)
{
    static char first[] = "monde: unknown option '--bogus'";
    static char second[] = "monde extract: missing argument <input>";
    static char third[] = "third message";
    char *messages[] = { first, second, third };
    clopts_errors_t errors = { messages, 3, 3, false };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    mnd_diag_add(&diags, MND_SEVERITY_WARNING, "doc.md", 4, "existing");
    mnd_cli_clopts_errors_to_diags(&errors, &diags);

    NAOS_REQUIRE_UINT_EQ(4, mnd_diag_list_count(&diags));
    NAOS_CHECK_INT_EQ((int)MND_SEVERITY_WARNING, (int)mnd_diag_list_at(&diags, 0)->severity);
    NAOS_CHECK_STR_EQ("existing", mnd_diag_list_at(&diags, 0)->message);
    check_error_diag(&diags, 1, first);
    check_error_diag(&diags, 2, second);
    check_error_diag(&diags, 3, third);
    NAOS_CHECK_FALSE(diags.dropped);

    mnd_diag_list_free(&diags);
}

/* メッセージ中の % を書式として解釈せず、そのまま写す */
NAOS_TEST(clopts_bridge_percent_is_not_format)
{
    static char message[] = "invalid value '100%s%d%n%%' for --width";
    char *messages[] = { message };
    clopts_errors_t errors = { messages, 1, 1, false };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    mnd_cli_clopts_errors_to_diags(&errors, &diags);
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
    check_error_diag(&diags, 0, "invalid value '100%s%d%n%%' for --width");

    mnd_diag_list_free(&diags);
}

/* 記録できなかったエラーがあれば、診断の列も dropped にする（0 件でも、メッセージがあっても） */
NAOS_TEST(clopts_bridge_propagates_dropped)
{
    static char message[] = "out of memory while parsing arguments";
    char *messages[] = { message };
    clopts_errors_t only_dropped = { NULL, 0, 0, true };
    clopts_errors_t with_message = { messages, 1, 1, true };
    mnd_diag_list_t a = MND_DIAG_LIST_INIT;
    mnd_diag_list_t b = MND_DIAG_LIST_INIT;

    mnd_cli_clopts_errors_to_diags(&only_dropped, &a);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&a));
    NAOS_CHECK_TRUE(a.dropped);
    NAOS_CHECK_TRUE(mnd_diag_list_has_error(&a));

    mnd_cli_clopts_errors_to_diags(&with_message, &b);
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&b));
    check_error_diag(&b, 0, message);
    NAOS_CHECK_TRUE(b.dropped);

    mnd_diag_list_free(&a);
    mnd_diag_list_free(&b);
}

/* dropped でないエラーの列は、すでに dropped の診断の列を元に戻さない */
NAOS_TEST(clopts_bridge_keeps_existing_dropped)
{
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    diags.dropped = true;
    mnd_cli_clopts_errors_to_diags(&errors, &diags);
    NAOS_CHECK_TRUE(diags.dropped);

    mnd_diag_list_free(&diags);
}

NAOS_SUITE(clopts_bridge,
    NAOS_ADD_TEST(clopts_bridge_empty_adds_nothing),
    NAOS_ADD_TEST(clopts_bridge_multiple_keeps_order_after_existing),
    NAOS_ADD_TEST(clopts_bridge_percent_is_not_format),
    NAOS_ADD_TEST(clopts_bridge_propagates_dropped),
    NAOS_ADD_TEST(clopts_bridge_keeps_existing_dropped)
)
