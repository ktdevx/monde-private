/* スイート roundtrip: 代表的なフロントマターによる往復（8.2、8.3） */

#include <naos/naos.h>

#include "foundation/source.h"
#include "yaml/yaml_front_matter.h"
#include "yaml/yaml_parse.h"

#include <stddef.h>
#include <string.h>

/* 雛形: 範囲が指す元のバイト列を取り出して並べると原文に戻る（往復の検証の基本の操作） */
NAOS_TEST(roundtrip_scaffold_span_text_concatenation)
{
    static const char data[] = "---\ntitle: x\n---\nbody\n";
    size_t len = sizeof(data) - 1;
    mnd_span_t head = { 0, 4 };
    mnd_span_t rest = { 4, len };
    mnd_slice_t a;
    mnd_slice_t b;

    NAOS_REQUIRE_TRUE(mnd_span_text(data, len, head, &a));
    NAOS_REQUIRE_TRUE(mnd_span_text(data, len, rest, &b));
    NAOS_REQUIRE_UINT_EQ(len, a.len + b.len);
    NAOS_CHECK_MEM_EQ(data, a.len, a.data, a.len);
    NAOS_CHECK_MEM_EQ(data + a.len, b.len, b.data, b.len);
}

NAOS_SUITE(roundtrip,
    NAOS_ADD_TEST(roundtrip_scaffold_span_text_concatenation)
)
