#ifndef MONDE_PO_MERGE_MERGE_INTERNAL_H
#define MONDE_PO_MERGE_MERGE_INTERNAL_H

/*
 * モジュール内部の宣言。src/po_merge/ の .c と tests/po_merge/ だけが include する。
 * 部品ごとの節に分ける。下位の部品（Hash、Similarity）を先に置く。
 * 依存の方向: Hash → KeyIndex / Fuzzy、Similarity → Fuzzy、Builder / Header / KeyIndex / Fuzzy → Merge
 */

#include "foundation/buf.h"
#include "po/po.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Hash（merge_hash.c） */

/* ハッシュ表の 1 つの枠。used が偽の枠は空 */
typedef struct mnd_merge_hash_slot
{
    const unsigned char *key;  /* 呼び出し側の領域を指す（複製しない）。len == 0 なら NULL 可 */
    size_t len;
    uint64_t hash;             /* key の FNV-1a（64 ビット） */
    size_t value;
    bool used;
} mnd_merge_hash_slot_t;

/*
 * バイト列（ポインタと長さ）をキー、size_t を値とする開番地法のハッシュ表。
 * 容量は 0 または 2 の累乗で、使用率を 1/2 以下に保つ
 */
typedef struct mnd_merge_hash
{
    mnd_merge_hash_slot_t *slots;  /* cap 個。cap == 0 なら NULL */
    size_t cap;
    size_t count;                  /* 使用中の枠の数 */
} mnd_merge_hash_t;

void mnd_merge_hash_init(mnd_merge_hash_t *hash);
void mnd_merge_hash_free(mnd_merge_hash_t *hash);  /* 初期状態に戻す。初期状態・解放済みに対しても安全 */
/* key が既にあれば *existing に既存の値を入れて *inserted を偽にする。キーは複製しない */
bool mnd_merge_hash_insert(mnd_merge_hash_t *hash, const void *key, size_t len,
                           size_t value, bool *inserted, size_t *existing);
bool mnd_merge_hash_find(const mnd_merge_hash_t *hash, const void *key, size_t len,
                         size_t *value);

/* Similarity（merge_similarity.c） */

#define MND_MERGE_PATTERN_WORD_BITS 64
#define MND_MERGE_PATTERN_BYTE_KINDS 256

/*
 * 問い合わせの文字列のバイトごとの位置のビットマスク。全フィールドを 0（NULL）にした状態が空で、
 * mnd_merge_pattern_set は空の状態からも、以前の set の後からも呼べる（領域を再利用する）
 */
typedef struct mnd_merge_pattern
{
    size_t len;        /* 問い合わせのバイト長 */
    size_t words;      /* マスク 1 種あたりの語数 ⌈len / 64⌉ */
    size_t word_cap;   /* マスク 1 種あたりの確保済みの語数（words 以上） */
    uint64_t *masks;   /* 256 × word_cap 語。バイト b の語 w は masks[(b * words) + w] */
    uint64_t *vector;  /* LCS の計算の作業領域（word_cap 語）。mnd_merge_lcs が書き換える */
} mnd_merge_pattern_t;

bool mnd_merge_pattern_set(mnd_merge_pattern_t *pattern, const char *s, size_t len);
void mnd_merge_pattern_free(mnd_merge_pattern_t *pattern);  /* 空の状態に戻す */
size_t mnd_merge_lcs(const mnd_merge_pattern_t *pattern, const char *s, size_t len);
/* 2 × numerator / sum を double で返す（sum == 0 なら 1.0） */
double mnd_merge_ratio(size_t numerator, size_t sum);
/* 長さと出現数による上限の分子 */
size_t mnd_merge_upper_numerator(const char *a, size_t alen, const char *b, size_t blen);

/* KeyIndex（merge_index.c） */

/* キーの msgctxt と msgid の区切り */
#define MND_MERGE_KEY_SEPARATOR '\x04'

/*
 * キー（msgctxt の有無、msgctxt、msgid）からカタログの添字を引く索引。
 * キーは「msgctxt の有無の 1 バイト + msgctxt + 0x04 + msgid」の連結として索引が所有し、
 * hash のキーはその連結を指す
 */
typedef struct mnd_merge_key_index
{
    const mnd_po_catalog_t *catalog;  /* 索引の対象（所有しない） */
    mnd_merge_hash_t hash;            /* キー → catalog の添字 */
    mnd_buf_t *keys;                  /* catalog の添字ごとのキーの連結（key_count 個。未登録は空） */
    size_t key_count;                 /* init の時点の catalog->count */
} mnd_merge_key_index_t;

bool mnd_merge_key_index_init(mnd_merge_key_index_t *index, const mnd_po_catalog_t *catalog);
void mnd_merge_key_index_free(mnd_merge_key_index_t *index);
/* entry_index のキーを登録する。既にあれば登録せず *existing に先の添字を入れて true、*added を偽にする */
bool mnd_merge_key_index_add(mnd_merge_key_index_t *index, size_t entry_index,
                             bool *added, size_t *existing);
bool mnd_merge_key_index_find(const mnd_merge_key_index_t *index, const mnd_po_entry_t *key,
                              size_t *entry_index);

/* Fuzzy（merge_fuzzy.c） */

/* 4-gram の文字数（UTF-8 の文字） */
#define MND_MERGE_FUZZY_GRAM_CHARS 4
/* バイト長ごとの列を持つ msgid のバイト長の上限 */
#define MND_MERGE_FUZZY_SHORT_MAX_LEN 28

/* 候補の番号（candidates の添字）の列 */
typedef struct mnd_merge_index_list
{
    size_t *items;
    size_t count;
    size_t cap;
} mnd_merge_index_list_t;

/*
 * あいまい一致の候補の索引と、検索の作業領域。
 * 候補の番号は candidates の添字で、番号の昇順は catalog の添字の昇順に等しい
 */
typedef struct mnd_merge_fuzzy
{
    const mnd_po_catalog_t *catalog;  /* 候補を持つカタログ（所有しない） */
    size_t *candidates;               /* 候補の catalog の添字（昇順。candidate_count 個） */
    size_t candidate_count;
    mnd_merge_hash_t grams;           /* 4-gram（候補の msgid の内部を指す）→ gram_lists の添字 */
    mnd_merge_index_list_t *gram_lists;  /* 4-gram ごとの候補の番号の列（昇順、重複なし） */
    size_t gram_list_count;
    size_t gram_list_cap;
    /* msgid のバイト長が 0〜28 の候補の番号の列（バイト長ごと、昇順） */
    mnd_merge_index_list_t by_length[MND_MERGE_FUZZY_SHORT_MAX_LEN + 1];
    /* 作業領域（検索の結果に影響しない） */
    size_t *counts;                   /* 候補の番号ごとの数え上げ（candidate_count 個。検索の外では 0） */
    size_t *touched;                  /* 数え上げが 1 以上になった候補の番号（candidate_count 個の領域） */
    size_t touched_count;
    mnd_merge_pattern_t pattern;      /* 問い合わせの msgid のマスク */
} mnd_merge_fuzzy_t;

/* catalog の訳のあるエントリ（ヘッダを除く）を登録する */
bool mnd_merge_fuzzy_init(mnd_merge_fuzzy_t *fuzzy, const mnd_po_catalog_t *catalog);
void mnd_merge_fuzzy_free(mnd_merge_fuzzy_t *fuzzy);
/*
 * query の msgctxt と msgid に最も近い候補を探す。見つかれば *found に catalog の添字を入れて
 * *has_found を真にする。false はメモリ不足だけを表す。検索は fuzzy の登録内容を変えない
 */
bool mnd_merge_fuzzy_search(mnd_merge_fuzzy_t *fuzzy, const mnd_po_entry_t *query,
                            bool *has_found, size_t *found);

/* Builder（merge_entry.c） */

/* src を dst（初期化済み）に複製する。失敗時は dst を初期状態に戻して false */
bool mnd_merge_entry_copy(mnd_po_entry_t *dst, const mnd_po_entry_t *src);
/* combine(d, r, force_fuzzy)。nplurals は 1 以上 */
bool mnd_merge_combine(mnd_po_entry_t *dst, const mnd_po_entry_t *old_entry,
                       const mnd_po_entry_t *template_entry, bool force_fuzzy, size_t nplurals);
/* fresh(r) */
bool mnd_merge_fresh(mnd_po_entry_t *dst, const mnd_po_entry_t *template_entry, size_t nplurals);
/* 廃止エントリの複製（抽出コメントと参照を除く） */
bool mnd_merge_obsolete(mnd_po_entry_t *dst, const mnd_po_entry_t *old_entry);
/* 手順 6: fuzzy でない、または訳がなければ前回の msgid を消す */
void mnd_merge_tidy_previous(mnd_po_entry_t *entry);
/* 訳がある（msgstr が 1 個以上で、最初の msgstr が空でない） */
bool mnd_merge_has_translation(const mnd_po_entry_t *entry);

/* Header（merge_header.c） */

/* Plural-Forms から nplurals を読めない、または範囲外のときの値 */
#define MND_MERGE_NPLURALS_DEFAULT 2
/* nplurals として受け付ける上限 */
#define MND_MERGE_NPLURALS_MAX 100

/* old_header と template_header（NULL 可）から dst（初期化済み）にヘッダを作る */
bool mnd_merge_header(mnd_po_entry_t *dst, const mnd_po_entry_t *old_header,
                      const mnd_po_entry_t *template_header);
/* header（NULL 可）の Plural-Forms から nplurals を返す。1〜100 以外は 2 */
size_t mnd_merge_nplurals(const mnd_po_entry_t *header);

#endif
