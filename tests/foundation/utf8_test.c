#include <naos/naos.h>

#include "foundation/utf8.h"

/* sizeof の被演算子は評価されないため、未定義の関数でも宣言の形だけを確認できる */
NAOS_TEST(validate_returns_bool)
{
    size_t offset = 0;

    NAOS_CHECK_UINT_EQ(sizeof(bool), sizeof(mnd_utf8_validate("", 0, &offset)));
}

NAOS_SUITE(utf8,
    NAOS_ADD_TEST(validate_returns_bool)
)
