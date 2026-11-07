/* スイート markdown: 走査の規則（Markdown）（1.1〜1.7、2.1〜2.5、2.8、3.5） */

#include <naos/naos.h>

#include "foundation/diag.h"
#include "tu/tu.h"

#include <stdbool.h>
#include <stddef.h>

/* 空の文書の走査は成功し、セグメントを作らない */
NAOS_TEST(markdown_empty_document_has_no_segments)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_tu_doc_t *doc = NULL;

    NAOS_CHECK_TRUE(mnd_tu_scan(NULL, 0, NULL, &doc, &diags));
    NAOS_CHECK_TRUE(doc != NULL);
    NAOS_CHECK_UINT_EQ(0, mnd_tu_doc_segment_count(doc));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_tu_doc_free(doc);
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(markdown,
    NAOS_ADD_TEST(markdown_empty_document_has_no_segments)
)
