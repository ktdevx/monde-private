#!/usr/bin/env python3
"""UCD 15.0.0 のデータファイルから src/po/po_ucd_tables.c を生成する。

Python 3 の標準ライブラリだけを使い、ネットワークからは取得しない。
データファイルは https://www.unicode.org/Public/15.0.0/ucd/ から手で取得し、
リポジトリの外に置く（リポジトリには含めない）。

使い方:
    python3 tools/gen_po_ucd.py LineBreak.txt EastAsianWidth.txt \\
        extracted/DerivedGeneralCategory.txt -o src/po/po_ucd_tables.c

-o を省略すると標準出力に書く。同じデータからは常に同じ内容を生成する。
"""

import argparse
import re
import sys

UNICODE_VERSION = "15.0.0"
MAX_CP = 0x10FFFF

# THIRD_PARTY_NOTICES の素材の ID（licenses/<ID>.txt に対応する）
NOTICES_ID = "unicode-ucd"

# po_internal.h の mnd_po_lb_t にある名前（XX は表に載せない）
LB_CLASSES = (
    "OP", "CL", "CP", "QU", "GL", "NS", "EX", "SY", "IS", "PR", "PO", "NU",
    "AL", "HL", "ID", "IN", "HY", "BA", "BB", "B2", "ZW", "CM", "WJ", "H2",
    "H3", "JL", "JV", "JT", "RI", "EB", "EM", "ZWJ",
    "SP", "BK", "CR", "LF", "NL", "AI", "SA", "CJ", "SG", "XX", "CB",
)

# LineBreak-15.0.0.txt の先頭の説明にある、明示されないコードポイントの既定値。
# 既定値を先に入れ、明示された値で上書きする
LB_DEFAULTS = (
    (0x0000, MAX_CP, "XX"),
    (0x3400, 0x4DBF, "ID"),
    (0x4E00, 0x9FFF, "ID"),
    (0xF900, 0xFAFF, "ID"),
    (0x20000, 0x2FFFD, "ID"),
    (0x30000, 0x3FFFD, "ID"),
    (0x1F000, 0x1FAFF, "ID"),
    (0x1FC00, 0x1FFFD, "ID"),
    (0x20A0, 0x20CF, "PR"),
)

# EastAsianWidth-15.0.0.txt の先頭の説明にある既定値
EAW_DEFAULTS = (
    (0x0000, MAX_CP, "N"),
    (0x3400, 0x4DBF, "W"),
    (0x4E00, 0x9FFF, "W"),
    (0xF900, 0xFAFF, "W"),
    (0x20000, 0x2FFFD, "W"),
    (0x30000, 0x3FFFD, "W"),
)

# DerivedGeneralCategory-15.0.0.txt の @missing
GC_DEFAULT = "Cn"

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

LINE_RE = re.compile(r"^([0-9A-F]{4,6})(?:\.\.([0-9A-F]{4,6}))?\s*;\s*([A-Za-z0-9_]+)\s*(?:#.*)?$")


class GenError(Exception):
    pass


def read_property_file(path, expected_name):
    """データファイルを読み、(first, last, value) の列を返す。"""
    with open(path, encoding="utf-8") as f:
        lines = f.read().splitlines()
    expected_first = "# " + expected_name + "-" + UNICODE_VERSION + ".txt"
    if (not lines) or (lines[0].strip() != expected_first):
        raise GenError("%s: first line is not %r" % (path, expected_first))
    entries = []
    for number, line in enumerate(lines, 1):
        stripped = line.strip()
        if (not stripped) or stripped.startswith("#"):
            continue
        m = LINE_RE.match(stripped)
        if m is None:
            raise GenError("%s:%d: cannot parse line" % (path, number))
        first = int(m.group(1), 16)
        last = int(m.group(2), 16) if m.group(2) else first
        if (first > last) or (last > MAX_CP):
            raise GenError("%s:%d: bad range" % (path, number))
        entries.append((first, last, m.group(3)))
    return entries


def build_map(defaults, entries):
    """既定値を入れてから明示された値で上書きした、コードポイントごとの値の列を返す。"""
    values = [None] * (MAX_CP + 1)
    for first, last, value in defaults:
        values[first:last + 1] = [value] * (last - first + 1)
    for first, last, value in entries:
        values[first:last + 1] = [value] * (last - first + 1)
    return values


def to_ranges(values, keep):
    """keep(value) が真のコードポイントを、同じ値の連続ごとに (first, last, value) にまとめる。"""
    ranges = []
    cp = 0
    while cp <= MAX_CP:
        value = values[cp]
        start = cp
        while (cp + 1 <= MAX_CP) and (values[cp + 1] == value):
            cp += 1
        if keep(value):
            ranges.append((start, cp, value))
        cp += 1
    return ranges


def compute_width(gc, eaw):
    """設計の規則による文字幅（0、1、2）の列を返す。"""
    width = [1] * (MAX_CP + 1)
    for cp in range(MAX_CP + 1):
        if eaw[cp] in ("W", "F"):
            width[cp] = 2
    for cp in range(MAX_CP + 1):
        if gc[cp] in ("Mn", "Me", "Cf"):
            width[cp] = 0
    for first, last in ((0x0000, 0x001F), (0x007F, 0x009F), (0x1160, 0x11FF)):
        for cp in range(first, last + 1):
            width[cp] = 0
    return width


def format_table(name, ranges, format_value):
    out = []
    out.append("const mnd_po_ucd_range_t %s_ranges[] = {" % name)
    for first, last, value in ranges:
        out.append("    { 0x%04X, 0x%04X, %s }," % (first, last, format_value(value)))
    out.append("};")
    out.append("const size_t %s_count = %d;" % (name, len(ranges)))
    return out


def generate(lb_path, eaw_path, gc_path):
    lb_entries = read_property_file(lb_path, "LineBreak")
    eaw_entries = read_property_file(eaw_path, "EastAsianWidth")
    gc_entries = read_property_file(gc_path, "DerivedGeneralCategory")

    for _, _, value in lb_entries:
        if value not in LB_CLASSES:
            raise GenError("%s: unknown Line_Break value %r" % (lb_path, value))

    lb = build_map(LB_DEFAULTS, lb_entries)
    eaw = build_map(EAW_DEFAULTS, eaw_entries)
    gc = build_map(((0, MAX_CP, GC_DEFAULT),), gc_entries)
    width = compute_width(gc, eaw)
    mark = [gc[cp] in ("Mn", "Mc") for cp in range(MAX_CP + 1)]

    lb_ranges = to_ranges(lb, lambda v: v != "XX")
    width_ranges = to_ranges(width, lambda v: v != 1)
    mark_ranges = to_ranges(mark, lambda v: v)

    out = []
    out.append("/*")
    out.append(" * Ucd の表。tools/gen_po_ucd.py が生成したファイルであり、手で編集しない。")
    out.append(" *")
    out.append(" * Unicode のバージョン: %s" % UNICODE_VERSION)
    out.append(" * 生成元: Unicode Character Database %s" % UNICODE_VERSION)
    out.append(" *   https://www.unicode.org/Public/%s/ucd/LineBreak.txt" % UNICODE_VERSION)
    out.append(" *   https://www.unicode.org/Public/%s/ucd/EastAsianWidth.txt" % UNICODE_VERSION)
    out.append(" *   https://www.unicode.org/Public/%s/ucd/extracted/DerivedGeneralCategory.txt"
               % UNICODE_VERSION)
    out.append(" * 再生成: python3 tools/gen_po_ucd.py LineBreak.txt EastAsianWidth.txt"
               " DerivedGeneralCategory.txt \\")
    out.append(" *     -o src/po/po_ucd_tables.c")
    out.append(" *")
    out.append(" * THIRD_PARTY_NOTICES: %s" % NOTICES_ID)
    out.append(" *   表示の索引: リポジトリの THIRD_PARTY_NOTICES と licenses/%s.txt" % NOTICES_ID)
    out.append(" *")
    out.append(" * 表の内容:")
    out.append(" *   mnd_po_ucd_lb: 解決前の行分割クラス（XX 以外）。明示されないコードポイントには")
    out.append(" *     LineBreak.txt の既定値（ID、PR）を入れてある")
    out.append(" *   mnd_po_ucd_width: 文字幅が 0 または 2 のコードポイント。一般カテゴリが Mn、Me、Cf、")
    out.append(" *     U+0000〜U+001F、U+007F〜U+009F、U+1160〜U+11FF は 0。East Asian Width が W、F は 2")
    out.append(" *   mnd_po_ucd_mark: 一般カテゴリが Mn または Mc のコードポイント")
    out.append(" *")
    out.append(" * 表はデータファイルから導いたものであり、次の Unicode License v3 に従う。")
    out.append(" *")
    for line in LICENSE.splitlines():
        out.append((" * " + line).rstrip())
    out.append(" */")
    out.append("")
    out.append('#include "po/po_internal.h"')
    out.append("")
    out.append("#include <stddef.h>")
    out.append("")
    out.extend(format_table("mnd_po_ucd_lb", lb_ranges, lambda v: "MND_PO_LB_" + v))
    out.append("")
    out.extend(format_table("mnd_po_ucd_width", width_ranges, lambda v: "%d" % v))
    out.append("")
    out.extend(format_table("mnd_po_ucd_mark", mark_ranges, lambda v: "1"))
    return "\n".join(out) + "\n"


def main(argv):
    parser = argparse.ArgumentParser(
        description="Generate src/po/po_ucd_tables.c from UCD %s data files." % UNICODE_VERSION)
    parser.add_argument("line_break", help="path to LineBreak.txt")
    parser.add_argument("east_asian_width", help="path to EastAsianWidth.txt")
    parser.add_argument("general_category", help="path to extracted/DerivedGeneralCategory.txt")
    parser.add_argument("-o", "--output", help="output file (default: standard output)")
    args = parser.parse_args(argv)

    try:
        text = generate(args.line_break, args.east_asian_width, args.general_category)
    except (GenError, OSError, UnicodeDecodeError) as e:
        sys.stderr.write("gen_po_ucd.py: %s\n" % e)
        return 1

    if args.output:
        with open(args.output, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    else:
        sys.stdout.buffer.write(text.encode("utf-8"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
