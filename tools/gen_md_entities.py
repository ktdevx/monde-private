#!/usr/bin/env python3
"""WHATWG HTML の entities.json から src/markdown/md_entity_table.c を生成する。

Python 3 の標準ライブラリだけを使い、ネットワークからは取得しない。ビルドとテストからは呼ばない。
entities.json は https://html.spec.whatwg.org/entities.json から手で取得し、
リポジトリの外に置く（リポジトリには含めない）。

使い方:
    python3 tools/gen_md_entities.py entities.json -o src/markdown/md_entity_table.c

-o を省略すると標準出力に書く。同じデータからは常に同じ内容を生成する。

`;` で終わるキーだけを使い、名前は `&` と `;` を除いた部分として持つ（md_entity.h を参照）。
`;` のないキー（`&amp` などの古い形）は表に入れない。
"""

import argparse
import json
import re
import sys

SOURCE_URL = "https://html.spec.whatwg.org/entities.json"

# THIRD_PARTY_NOTICES の素材の ID（licenses/<ID>.txt に対応する）
NOTICES_ID = "whatwg-entities"

COPYRIGHT = "Copyright © WHATWG (Apple, Google, Mozilla, Microsoft)"

# BSD 3-Clause License の全文（licenses/whatwg-entities.txt の本文と同じ）
LICENSE = """\
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

# md_entity.h の MND_MD_ENTITY_NAME_MAX と MND_MD_ENTITY_UTF8_MAX
NAME_MAX = 31
UTF8_MAX = 6

MAX_CP = 0x10FFFF

KEY_RE = re.compile(r"^&([A-Za-z0-9]+)(;?)$")


class GenError(Exception):
    pass


def reject_duplicates(pairs):
    """JSON のオブジェクトで同じキーが 2 回現れたら誤りにする（json の既定では後のものが黙って残る）。"""
    result = {}
    for key, value in pairs:
        if key in result:
            raise GenError("duplicate key %r" % key)
        result[key] = value
    return result


def read_entities(path):
    """entities.json を読み、`;` で終わるキーの {名前のバイト列: UTF-8 のバイト列} を返す。"""
    with open(path, "rb") as f:
        data = json.loads(f.read().decode("utf-8"), object_pairs_hook=reject_duplicates)
    if not isinstance(data, dict):
        raise GenError("%s: top level is not an object" % path)

    entities = {}
    for key, value in data.items():
        m = KEY_RE.match(key)
        if m is None:
            raise GenError("%s: unexpected key %r" % (path, key))
        if not isinstance(value, dict):
            raise GenError("%s: %s: value is not an object" % (path, key))
        codepoints = value.get("codepoints")
        characters = value.get("characters")
        if ((not isinstance(codepoints, list)) or (not codepoints)
                or (not all(isinstance(cp, int) and not isinstance(cp, bool) for cp in codepoints))):
            raise GenError("%s: %s: bad codepoints" % (path, key))
        if any((cp < 0) or (cp > MAX_CP) or (0xD800 <= cp <= 0xDFFF) for cp in codepoints):
            raise GenError("%s: %s: code point out of range" % (path, key))
        if (not isinstance(characters, str)) or (characters != "".join(chr(cp) for cp in codepoints)):
            raise GenError("%s: %s: characters do not match codepoints" % (path, key))
        if m.group(2) != ";":
            # `;` のない古い形は使わない
            continue

        name = m.group(1).encode("ascii")
        utf8 = characters.encode("utf-8")
        if len(name) > NAME_MAX:
            raise GenError("%s: %s: name longer than %d bytes" % (path, key, NAME_MAX))
        if len(utf8) > UTF8_MAX:
            raise GenError("%s: %s: characters longer than %d bytes" % (path, key, UTF8_MAX))
        if name in entities:
            raise GenError("%s: duplicate key %r" % (path, key))
        entities[name] = utf8
    if not entities:
        raise GenError("%s: empty table" % path)
    return entities


def format_entries(entities):
    out = []
    out.append("const mnd_md_entity_t mnd_md_entity_entries[] = {")
    # バイト列の昇順は strcmp の順と同じ
    for name in sorted(entities):
        utf8 = entities[name]
        out.append('    { "%s", %d, %d, { %s } },'
                   % (name.decode("ascii"), len(name), len(utf8), ", ".join("0x%02X" % b for b in utf8)))
    out.append("};")
    out.append("const size_t mnd_md_entity_count = %d;" % len(entities))
    return out


def generate(path):
    entities = read_entities(path)

    out = []
    out.append("/*")
    out.append(" * Entity の表。tools/gen_md_entities.py が生成したファイルであり、手で編集しない。")
    out.append(" *")
    out.append(" * 生成元: WHATWG HTML Living Standard の名前付き文字参照の一覧")
    out.append(" *   %s" % SOURCE_URL)
    out.append(" * 再生成: python3 tools/gen_md_entities.py entities.json -o src/markdown/md_entity_table.c")
    out.append(" *")
    out.append(" * THIRD_PARTY_NOTICES: %s" % NOTICES_ID)
    out.append(" *   表示の索引: リポジトリの THIRD_PARTY_NOTICES と licenses/%s.txt" % NOTICES_ID)
    out.append(" *")
    out.append(" * 表の内容:")
    out.append(" *   entities.json のキーのうち `;` で終わるものだけを、`&` と `;` を除いた名前の")
    out.append(" *   バイト列の昇順（strcmp の順）に並べる。各項目は { 名前, 名前のバイト数,")
    out.append(" *   表す列のバイト数, 表す列の UTF-8 } の形。`;` のない古い形のキーは含めない")
    out.append(" *")
    out.append(" * 表は entities.json から導いたものである。WHATWG は HTML Living Standard を")
    out.append(" * CC BY 4.0 で提供し、ソースコードに取り込んだ部分は BSD 3-Clause License で提供する。")
    out.append(" * 表はソースコードに取り込んだ部分に当たり、次の BSD 3-Clause License に従う。")
    out.append(" *")
    out.append(" * " + COPYRIGHT)
    out.append(" *")
    for line in LICENSE.splitlines():
        out.append((" * " + line).rstrip())
    out.append(" */")
    out.append("")
    out.append('#include "markdown/md_entity.h"')
    out.append("")
    out.append("#include <stddef.h>")
    out.append("")
    out.extend(format_entries(entities))
    return "\n".join(out) + "\n"


def main(argv):
    parser = argparse.ArgumentParser(
        description="Generate src/markdown/md_entity_table.c from WHATWG entities.json.")
    parser.add_argument("entities", help="path to entities.json (%s)" % SOURCE_URL)
    parser.add_argument("-o", "--output", help="output file (default: standard output)")
    args = parser.parse_args(argv)

    try:
        text = generate(args.entities)
    except (GenError, OSError, UnicodeError, ValueError) as e:
        sys.stderr.write("gen_md_entities.py: %s\n" % e)
        return 1

    if args.output:
        with open(args.output, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    else:
        sys.stdout.buffer.write(text.encode("utf-8"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
