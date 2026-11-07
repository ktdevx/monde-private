/* スイート language: 優先順位、"." / "@" の除去、C / POSIX、空、不正な値（5.2〜5.7） */

#include <naos/naos.h>

#include "cli/cli_language.h"

#include <stddef.h>

/* 雛形: 言語コードの結果の型を組み立てられる */
NAOS_TEST(language_scaffold_result_type)
{
    mnd_cli_language_t language = { NULL, NULL };

    NAOS_CHECK_NULL(language.code);
    NAOS_CHECK_NULL(language.variable);
}

NAOS_SUITE(language,
    NAOS_ADD_TEST(language_scaffold_result_type)
)
