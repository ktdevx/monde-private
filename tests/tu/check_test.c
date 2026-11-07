/* スイート check: 構造の検査（StructureCheck）（4.11） */

#include <naos/naos.h>

#include "tu/tu_internal.h"

#include <stdbool.h>
#include <stddef.h>

/* 通常の 1 行の文は構造を変えない */
NAOS_TEST(check_plain_sentence_is_ok)
{
    static const char text[] = "Hello, world.";
    size_t line_index = 0;

    NAOS_CHECK_INT_EQ((int)MND_TU_CHECK_OK,
                      (int)mnd_tu_check_structure(text, sizeof(text) - 1, false, &line_index));
}

NAOS_SUITE(check,
    NAOS_ADD_TEST(check_plain_sentence_is_ok)
)
