/* スイート front_matter: 走査の規則（フロントマター）と文書のライフサイクル（1.8〜1.12、2.6、2.7、7.1〜7.3） */

#include <naos/naos.h>

#include "tu/tu.h"

#include <stddef.h>

/* オプションの既定値はパスなし・キーパスなし。文書の解放は NULL に対しても安全 */
NAOS_TEST(front_matter_options_init_has_no_keys)
{
    mnd_tu_options_t options = MND_TU_OPTIONS_INIT;

    NAOS_CHECK_TRUE(options.path == NULL);
    NAOS_CHECK_TRUE(options.front_matter_keys == NULL);
    NAOS_CHECK_UINT_EQ(0, options.front_matter_key_count);
    mnd_tu_doc_free(NULL);
}

NAOS_SUITE(front_matter,
    NAOS_ADD_TEST(front_matter_options_init_has_no_keys)
)
