/* スイート block: 解析の入口とブロックのコレクション（2.1、2.2、2.4〜2.10、6.2〜6.4） */

#include <naos/naos.h>

#include "yaml/yaml_parse.h"

#include <stddef.h>

/* 雛形: 既定のオプションはパスも確保関数も持たない */
NAOS_TEST(block_scaffold_options_init_is_default)
{
    mnd_yaml_options_t options = MND_YAML_OPTIONS_INIT;

    NAOS_CHECK_NULL(options.path);
    NAOS_CHECK_TRUE(options.allocator == NULL);
}

NAOS_SUITE(block,
    NAOS_ADD_TEST(block_scaffold_options_init_is_default)
)
