#ifndef MONDE_MARKDOWN_MD_ENTITY_H
#define MONDE_MARKDOWN_MD_ENTITY_H

/* Entity: 実体名の表と検索の内部の宣言（md_entity.c、md_entity_table.c） */

#include <stdbool.h>
#include <stddef.h>

/*
 * 名前の表し方: 表と検索で扱う名前は、実体参照 `&name;` の `&` と `;` の間の部分（例: `amp`）。
 * 表は WHATWG の entities.json のうち `;` で終わるキーだけから作る。`;` のないキー（`&amp` などの
 * 古い形）は表に入れない。したがって、検索の対象は `;` で閉じた実体参照だけであり、
 * 呼び出し側は、名前の直後に `;` があることを確かめてから検索する。名前に `&` や `;` を含めて
 * 渡すと見つからない
 */

/*
 * 実体名の最大のバイト数（`&` と `;` を除く）。表の最長の名前（`CounterClockwiseContourIntegral`）は
 * 31 バイト。インラインの解析は、これより長い名前を検索せずに実体参照ではないと判定できる。
 * tools/gen_md_entities.py は表の名前がこれを超えないことを確かめる
 */
#define MND_MD_ENTITY_NAME_MAX 31

/*
 * 1 つの実体が表す UTF-8 の列の最大のバイト数。表の最長の列は 6 バイト（2 つのコードポイント）。
 * mnd_md_char_ref_t.text に収まる。tools/gen_md_entities.py は表の列がこれを超えないことを確かめる
 */
#define MND_MD_ENTITY_UTF8_MAX 6

/*
 * 実体名 name[0..len)（NUL で終端しなくてよい。`&` と `;` を含めない）を表から二分探索する。
 * 見つかれば、表す UTF-8 の列の先頭を *utf8 に、バイト数（1 以上 MND_MD_ENTITY_UTF8_MAX 以下）を
 * *utf8_len に書いて true を返す。*utf8 は表の中を指し、NUL で終端しない（プログラムの終了まで有効）。
 * 見つからなければ（len が 0 または MND_MD_ENTITY_NAME_MAX を超える場合を含む）、*utf8 と *utf8_len を
 * 変えずに false を返す。名前は大文字と小文字を区別する。len が 0 なら name は NULL でもよい
 */
bool mnd_md_entity_lookup(const char *name, size_t len, const unsigned char **utf8, size_t *utf8_len);

/*
 * 実体名の表（md_entity_table.c。tools/gen_md_entities.py が entities.json から生成する）。
 * 名前のバイト列の昇順（strcmp と同じ順）に、重複なしで並べる。
 * name[0..name_len) が名前（ASCII の英数字。name[name_len] は NUL）、utf8[0..len) が表す UTF-8 の列。
 * 要素数は mnd_md_entity_count で表す
 */
typedef struct mnd_md_entity
{
    char name[MND_MD_ENTITY_NAME_MAX + 1];
    unsigned char name_len;
    unsigned char len;
    unsigned char utf8[MND_MD_ENTITY_UTF8_MAX];
} mnd_md_entity_t;

extern const mnd_md_entity_t mnd_md_entity_entries[];
extern const size_t mnd_md_entity_count;

#endif
