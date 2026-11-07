/* スイート roundtrip: spec.txt の全例 × original / CRLF × 空カタログ / msgstr = msgid をプロセス内で検証（10.1〜10.3） */

#include <naos/naos.h>

#include "../markdown/spec_reader.h"

/* 雛形: spec.txt のパスが渡されている */
NAOS_TEST(roundtrip_scaffold_spec_file_is_given)
{
    NAOS_CHECK_STR_NE("", MND_MD_SPEC_FILE);
}

NAOS_SUITE(roundtrip,
    NAOS_ADD_TEST(roundtrip_scaffold_spec_file_is_given)
)
