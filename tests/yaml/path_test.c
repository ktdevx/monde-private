/* スイート path: キーパスによる照合（4.1〜4.8、6.9） */

#include <naos/naos.h>

#include "yaml/yaml.h"
#include "yaml/yaml_path.h"

#include <stddef.h>

/* 雛形: 照合の結果はスカラーとマッピングを kind で区別できる */
NAOS_TEST(path_scaffold_kinds_are_distinct)
{
    mnd_yaml_matches_t matches = { NULL, 0 };

    NAOS_CHECK_TRUE(matches.nodes == NULL);
    NAOS_CHECK_UINT_EQ(0, matches.count);
    NAOS_CHECK_INT_NE(MND_YAML_SCALAR, MND_YAML_MAPPING);
    NAOS_CHECK_INT_NE(MND_YAML_SCALAR, MND_YAML_SEQUENCE);
    NAOS_CHECK_INT_NE(MND_YAML_MAPPING, MND_YAML_SEQUENCE);
}

NAOS_SUITE(path,
    NAOS_ADD_TEST(path_scaffold_kinds_are_distinct)
)
