/* スイート format: 訳文の整形（5.1〜5.8） */

#include <naos/naos.h>

#include "foundation/buf.h"
#include "yaml/yaml.h"
#include "yaml/yaml_format.h"

#include <stddef.h>

/* 雛形: 5 種類のスタイルと 3 種類のチョンプが区別でき、既定値（0）が PLAIN と CLIP */
NAOS_TEST(format_scaffold_styles_and_chomps)
{
    NAOS_CHECK_INT_EQ(0, MND_YAML_PLAIN);
    NAOS_CHECK_INT_EQ(0, MND_YAML_CHOMP_CLIP);
    NAOS_CHECK_INT_NE(MND_YAML_SINGLE_QUOTED, MND_YAML_DOUBLE_QUOTED);
    NAOS_CHECK_INT_NE(MND_YAML_LITERAL, MND_YAML_FOLDED);
    NAOS_CHECK_INT_NE(MND_YAML_CHOMP_STRIP, MND_YAML_CHOMP_KEEP);
}

NAOS_SUITE(format,
    NAOS_ADD_TEST(format_scaffold_styles_and_chomps)
)
