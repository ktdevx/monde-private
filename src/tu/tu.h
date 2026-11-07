#ifndef MONDE_TU_TU_H
#define MONDE_TU_TU_H

/*
 * 翻訳単位（translation-units）の公開ヘッダ: 翻訳セグメント・オプション・統計の型、
 * 走査（mnd_tu_scan）・抽出（mnd_tu_extract）・再構成（mnd_tu_translate）と文書のアクセサ。
 * 文書とセグメントは mnd_tu_scan に渡した data を指し続け、複製しない
 */

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/eol.h"
#include "foundation/source.h"
#include "po/po.h"

#include <stdbool.h>
#include <stddef.h>

/* 走査（Scanner） */

typedef enum mnd_tu_origin
{
    MND_TU_ORIGIN_MARKDOWN,      /* 段落または見出し */
    MND_TU_ORIGIN_FRONT_MATTER   /* フロントマターのスカラー値 */
} mnd_tu_origin_t;

/* 翻訳セグメント。読み取り用。文字列と範囲は mnd_tu_doc_t が所有する */
typedef struct mnd_tu_segment
{
    mnd_tu_origin_t origin;
    mnd_span_t span;             /* 置き換える原文の範囲（data の先頭基準） */
    size_t line;                 /* 1 起点の行番号（#: に書く） */
    mnd_buf_t msgid;             /* 論理テキスト（msgid の規則） */
    mnd_buf_t kind;              /* 要素の種類（#. に書く。ラベルの規則） */
    size_t lead_newlines;        /* フロントマターのみ: 論理値から除いた先頭の \n の数 */
    size_t trail_newlines;       /* フロントマターのみ: 論理値から除いた末尾の \n の数 */
} mnd_tu_segment_t;

typedef struct mnd_tu_options
{
    const char *path;                      /* 診断に付けるパス。NULL 可 */
    const char *const *front_matter_keys;  /* 翻訳対象のキーパス（そのまま yaml に渡す）。count == 0 なら NULL 可 */
    size_t front_matter_key_count;
} mnd_tu_options_t;

#define MND_TU_OPTIONS_INIT { NULL, NULL, 0 }

typedef struct mnd_tu_doc mnd_tu_doc_t;  /* 不透明。原文を指す */

/*
 * data[0..len)（BOM 除去後）を走査し、*out に文書を設定して true を返す。
 * 失敗したら *out を NULL にし、diags にエラーを追加して false を返す（確保したものはすべて解放済み）。
 * 警告だけなら true。
 * options は NULL 可（MND_TU_OPTIONS_INIT と同じ）。data は len == 0 なら NULL 可。out と diags は NULL でない。
 * 成功時、セグメントは span.start の昇順（フロントマター → 本文）で重ならず、
 * msgid は空でなく \n で始まらず、NUL を含まない（含むセグメントがあれば最初のものについて
 * エラー NUL in source text で失敗する）。
 * 文書は data を指し続けるので、呼び出し側は mnd_tu_doc_free まで data を変更も解放もしない
 */
bool mnd_tu_scan(const char *data, size_t len, const mnd_tu_options_t *options,
                 mnd_tu_doc_t **out, mnd_diag_list_t *diags);

size_t mnd_tu_doc_segment_count(const mnd_tu_doc_t *doc);
/* index < count。返したセグメントは mnd_tu_doc_free まで有効 */
const mnd_tu_segment_t *mnd_tu_doc_segment(const mnd_tu_doc_t *doc, size_t index);
void mnd_tu_doc_free(mnd_tu_doc_t *doc);  /* NULL に対しても安全 */

/* 抽出（Extractor） */

/*
 * doc のセグメントから PO エントリを作り、out の末尾に加えて true を返す。ヘッダエントリは加えない。
 * ref_file は参照コメント（#:）に書くファイル名（NULL でない）。
 * 同じ msgid はひとつのエントリにまとめ、out に既にある msgctxt のない同じ msgid のエントリがあればそれに統合する。
 * 加えるエントリの順は各 msgid の最初の出現の順。refs は出現順、extracted_comments は重複なし。
 * メモリ不足なら diags にエラーを追加し、out を呼び出し前の状態に戻して false を返す
 * （加えたエントリを取り除き、既存のエントリに足した参照と抽出コメントも取り除く）。doc は変えない
 */
bool mnd_tu_extract(const mnd_tu_doc_t *doc, const char *ref_file,
                    mnd_po_catalog_t *out, mnd_diag_list_t *diags);

/* 再構成（Rebuilder） */

typedef struct mnd_tu_translate_options
{
    bool use_fuzzy;            /* fuzzy のエントリの訳を採用する */
    mnd_eol_t eol;             /* 原文より多い訳文の行の境界に使う改行コード。MND_EOL_NONE なら LF */
    const char *catalog_path;  /* 訳文の診断に付ける翻訳ファイルのパス。NULL 可 */
} mnd_tu_translate_options_t;

#define MND_TU_TRANSLATE_OPTIONS_INIT { false, MND_EOL_LF, NULL }

typedef struct mnd_tu_stats
{
    size_t total;         /* セグメントの出現の数 */
    size_t translated;    /* 翻訳済み */
    size_t fuzzy;         /* fuzzy（採用の有無を問わない） */
    size_t untranslated;  /* エントリはあるが msgstr が空 */
    size_t missing;       /* カタログにない */
} mnd_tu_stats_t;

#define MND_TU_STATS_INIT { 0, 0, 0, 0, 0 }

/*
 * doc のセグメントを catalog の訳文で置き換えた内容を out（初期化済み。内容は捨てる）に入れ、true を返す。
 * doc を作った data が生きていること。options は NULL 可（MND_TU_TRANSLATE_OPTIONS_INIT と同じ）。
 * stats は NULL 可。doc、catalog、out、diags は NULL でない。
 * 成功時、out はセグメント以外のすべてのバイトが data と同じで、stats->total はセグメントの数。
 * 構造を変え得る訳文は diags に警告を追加し、出力は変えない。
 * 訳文の不正な UTF-8 / NUL とメモリ不足は diags にエラーを追加し、out を空に、*stats を 0 にして false を返す
 */
bool mnd_tu_translate(const mnd_tu_doc_t *doc, const mnd_po_catalog_t *catalog,
                      const mnd_tu_translate_options_t *options,
                      mnd_buf_t *out, mnd_tu_stats_t *stats, mnd_diag_list_t *diags);

#endif
