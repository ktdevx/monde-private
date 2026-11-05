#!/usr/bin/env python3
"""UCD 15.0.0 のデータファイルから src/markdown/md_unicode_tables.c を生成する。

Python 3 の標準ライブラリだけを使い、ネットワークからは取得しない。ビルドとテストからは呼ばない。
データファイルは https://www.unicode.org/Public/15.0.0/ucd/ から手で取得し、
リポジトリの外に置く（リポジトリには含めない）。

使い方:
    python3 tools/gen_md_ucd.py extracted/DerivedGeneralCategory.txt CaseFolding.txt \\
        -o src/markdown/md_unicode_tables.c

-o を省略すると標準出力に書く。同じデータからは常に同じ内容を生成する。
"""

import argparse
import re
import sys

UNICODE_VERSION = "15.0.0"
MAX_CP = 0x10FFFF

# md_unicode.h の MND_MD_UNICODE_FOLD_MAX
FOLD_MAX = 6

# 句読点として扱う一般カテゴリ（P と S）
PUNCT_CATEGORIES = ("Pc", "Pd", "Ps", "Pe", "Pi", "Pf", "Po", "Sm", "Sc", "Sk", "So")
# 空白として扱う一般カテゴリ
SPACE_CATEGORIES = ("Zs",)
# 使う畳み込みの状態（共通と完全）
FOLD_STATUSES = ("C", "F")

LICENSE = """\
UNICODE LICENSE V3

COPYRIGHT AND PERMISSION NOTICE

Copyright © 1991-2022 Unicode, Inc.

NOTICE TO USER: Carefully read the following legal agreement. BY
DOWNLOADING, INSTALLING, COPYING OR OTHERWISE USING DATA FILES, AND/OR
SOFTWARE, YOU UNEQUIVOCALLY ACCEPT, AND AGREE TO BE BOUND BY, ALL OF THE
TERMS AND CONDITIONS OF THIS AGREEMENT. IF YOU DO NOT AGREE, DO NOT
DOWNLOAD, INSTALL, COPY, DISTRIBUTE OR USE THE DATA FILES OR SOFTWARE.

Permission is hereby granted, free of charge, to any person obtaining a
copy of data files and any associated documentation (the "Data Files") or
software and any associated documentation (the "Software") to deal in the
Data Files or Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, and/or sell
copies of the Data Files or Software, and to permit persons to whom the
Data Files or Software are furnished to do so, provided that either (a)
this copyright and permission notice appear with all copies of the Data
Files or Software, or (b) this copyright and permission notice appear in
associated Documentation.

THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
THIRD PARTY RIGHTS.

IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS NOTICE
BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES,
OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE DATA
FILES OR SOFTWARE.

Except as contained in this notice, the name of a copyright holder shall
not be used in advertising or otherwise to promote the sale, use or other
dealings in these Data Files or Software without prior written
authorization of the copyright holder.
"""

GC_LINE_RE = re.compile(r"^([0-9A-F]{4,6})(?:\.\.([0-9A-F]{4,6}))?\s*;\s*([A-Za-z]{2})\s*(?:#.*)?$")
CF_LINE_RE = re.compile(r"^([0-9A-F]{4,6});\s*([CFST]);\s*([0-9A-F]{4,6}(?: [0-9A-F]{4,6})*);\s*(?:#.*)?$")


class GenError(Exception):
    pass


def read_lines(path, expected_name):
    """データファイルを読み、先頭の行を確かめてから、(行番号, 内容) の列を返す（空行と注釈を除く）。"""
    with open(path, encoding="utf-8") as f:
        lines = f.read().splitlines()
    expected_first = "# " + expected_name + "-" + UNICODE_VERSION + ".txt"
    if (not lines) or (lines[0].strip() != expected_first):
        raise GenError("%s: first line is not %r" % (path, expected_first))
    result = []
    for number, line in enumerate(lines, 1):
        stripped = line.strip()
        if (not stripped) or stripped.startswith("#"):
            continue
        result.append((number, stripped))
    return result


def read_general_category(path):
    """DerivedGeneralCategory.txt を読み、(first, last, category) の列を返す。"""
    entries = []
    for number, line in read_lines(path, "DerivedGeneralCategory"):
        m = GC_LINE_RE.match(line)
        if m is None:
            raise GenError("%s:%d: cannot parse line" % (path, number))
        first = int(m.group(1), 16)
        last = int(m.group(2), 16) if m.group(2) else first
        if (first > last) or (last > MAX_CP):
            raise GenError("%s:%d: bad range" % (path, number))
        entries.append((first, last, m.group(3)))
    return entries


def read_case_folding(path):
    """CaseFolding.txt を読み、状態が C または F の対応を {cp: (target, ...)} で返す。"""
    folds = {}
    for number, line in read_lines(path, "CaseFolding"):
        m = CF_LINE_RE.match(line)
        if m is None:
            raise GenError("%s:%d: cannot parse line" % (path, number))
        if m.group(2) not in FOLD_STATUSES:
            continue
        cp = int(m.group(1), 16)
        targets = tuple(int(t, 16) for t in m.group(3).split())
        if (cp > MAX_CP) or any(t > MAX_CP for t in targets):
            raise GenError("%s:%d: bad code point" % (path, number))
        if cp in folds:
            # C と F は同じコードポイントに同時には付かない
            raise GenError("%s:%d: duplicate mapping for %04X" % (path, number, cp))
        folds[cp] = targets
    return folds


def to_ranges(entries, categories):
    """categories に含まれる一般カテゴリの範囲を、並べ替えて隣り合うものをまとめた (first, last) の列にする。"""
    selected = sorted((first, last) for first, last, gc in entries if gc in categories)
    ranges = []
    for first, last in selected:
        if ranges and (first <= ranges[-1][1] + 1):
            if last > ranges[-1][1]:
                ranges[-1] = (ranges[-1][0], last)
        else:
            ranges.append((first, last))
    return ranges


def utf8_bytes(targets):
    data = "".join(chr(t) for t in targets).encode("utf-8")
    if len(data) > FOLD_MAX:
        raise GenError("folded sequence longer than %d bytes" % FOLD_MAX)
    return data


def format_ranges(name, ranges):
    out = []
    out.append("const mnd_md_unicode_range_t %s_ranges[] = {" % name)
    for first, last in ranges:
        out.append("    { 0x%04X, 0x%04X }," % (first, last))
    out.append("};")
    out.append("const size_t %s_count = %d;" % (name, len(ranges)))
    return out


def format_folds(folds):
    out = []
    out.append("const mnd_md_unicode_fold_entry_t mnd_md_unicode_fold_entries[] = {")
    for cp in sorted(folds):
        data = utf8_bytes(folds[cp])
        out.append("    { 0x%04X, %d, { %s } }," % (cp, len(data), ", ".join("0x%02X" % b for b in data)))
    out.append("};")
    out.append("const size_t mnd_md_unicode_fold_count = %d;" % len(folds))
    return out


def generate(gc_path, cf_path):
    gc_entries = read_general_category(gc_path)
    folds = read_case_folding(cf_path)

    punct_ranges = to_ranges(gc_entries, PUNCT_CATEGORIES)
    space_ranges = to_ranges(gc_entries, SPACE_CATEGORIES)
    if (not punct_ranges) or (not space_ranges) or (not folds):
        raise GenError("empty table")

    out = []
    out.append("/*")
    out.append(" * Unicode の表。tools/gen_md_ucd.py が生成したファイルであり、手で編集しない。")
    out.append(" *")
    out.append(" * Unicode のバージョン: %s" % UNICODE_VERSION)
    out.append(" * 生成元: Unicode Character Database %s" % UNICODE_VERSION)
    out.append(" *   https://www.unicode.org/Public/%s/ucd/extracted/DerivedGeneralCategory.txt"
               % UNICODE_VERSION)
    out.append(" *   https://www.unicode.org/Public/%s/ucd/CaseFolding.txt" % UNICODE_VERSION)
    out.append(" * 再生成: python3 tools/gen_md_ucd.py DerivedGeneralCategory.txt CaseFolding.txt \\")
    out.append(" *     -o src/markdown/md_unicode_tables.c")
    out.append(" *")
    out.append(" * 表の内容:")
    out.append(" *   mnd_md_unicode_punct: 一般カテゴリが P（Pc、Pd、Ps、Pe、Pi、Pf、Po）または")
    out.append(" *     S（Sm、Sc、Sk、So）の範囲")
    out.append(" *   mnd_md_unicode_space: 一般カテゴリが Zs の範囲")
    out.append(" *   mnd_md_unicode_fold: CaseFolding.txt の状態 C と F の対応。畳み込んだ列を UTF-8 で持つ")
    out.append(" *")
    out.append(" * 表はデータファイルから導いたものであり、次の Unicode License v3 に従う。")
    out.append(" *")
    for line in LICENSE.splitlines():
        out.append((" * " + line).rstrip())
    out.append(" */")
    out.append("")
    out.append('#include "markdown/md_unicode.h"')
    out.append("")
    out.append("#include <stddef.h>")
    out.append("")
    out.extend(format_ranges("mnd_md_unicode_punct", punct_ranges))
    out.append("")
    out.extend(format_ranges("mnd_md_unicode_space", space_ranges))
    out.append("")
    out.extend(format_folds(folds))
    return "\n".join(out) + "\n"


def main(argv):
    parser = argparse.ArgumentParser(
        description="Generate src/markdown/md_unicode_tables.c from UCD %s data files." % UNICODE_VERSION)
    parser.add_argument("general_category", help="path to extracted/DerivedGeneralCategory.txt")
    parser.add_argument("case_folding", help="path to CaseFolding.txt")
    parser.add_argument("-o", "--output", help="output file (default: standard output)")
    args = parser.parse_args(argv)

    try:
        text = generate(args.general_category, args.case_folding)
    except (GenError, OSError, UnicodeDecodeError) as e:
        sys.stderr.write("gen_md_ucd.py: %s\n" % e)
        return 1

    if args.output:
        with open(args.output, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    else:
        sys.stdout.buffer.write(text.encode("utf-8"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
