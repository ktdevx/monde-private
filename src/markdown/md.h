#ifndef MONDE_MARKDOWN_MD_H
#define MONDE_MARKDOWN_MD_H

/*
 * CommonMark 0.31.2 の構文木（Model）と、範囲が指す論理的な文字列・アンエスケープ（Text）。
 * 構文木は内容を複製せず、内容を指す位置（オフセット）だけを持つ。公開の型は読み取り用である
 */

#include "foundation/buf.h"
#include "foundation/source.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum mnd_md_kind
{
    /* ブロック */
    MND_MD_DOCUMENT,
    MND_MD_BLOCK_QUOTE,
    MND_MD_LIST,
    MND_MD_ITEM,
    MND_MD_PARAGRAPH,
    MND_MD_HEADING,         /* ATX と Setext（heading.setext で区別） */
    MND_MD_THEMATIC_BREAK,
    MND_MD_CODE_BLOCK,      /* インデントとフェンス（code_block.fenced で区別） */
    MND_MD_HTML_BLOCK,
    MND_MD_LINK_REF_DEF,
    /* インライン */
    MND_MD_TEXT,
    MND_MD_SOFT_BREAK,
    MND_MD_HARD_BREAK,
    MND_MD_ESCAPE,
    MND_MD_CHAR_REF,
    MND_MD_CODE_SPAN,
    MND_MD_EMPH,
    MND_MD_STRONG,
    MND_MD_LINK,
    MND_MD_IMAGE,
    MND_MD_AUTOLINK,
    MND_MD_HTML_INLINE,
    /* 方言が定める種類はこの値以上を使う */
    MND_MD_KIND_DIALECT = 0x100
} mnd_md_kind_t;

typedef struct mnd_md_line
{
    size_t line;            /* 行番号（1 起点。content.start の行） */
    mnd_span_t prefix;      /* [物理的な行の先頭, content.start) */
    mnd_span_t content;     /* 行末記号を含まない */
    unsigned char pad;      /* 内容の前に補う空白の数（0〜3） */
    unsigned char eol_len;  /* 行末記号のバイト数（0、1、2） */
    bool lazy;              /* 遅延継続行 */
} mnd_md_line_t;

typedef struct mnd_md_node mnd_md_node_t;

typedef struct mnd_md_heading
{
    int level;              /* 1〜6 */
    bool setext;
    mnd_span_t content;     /* 見出しの記号を除いた内容（3.5） */
} mnd_md_heading_t;

typedef struct mnd_md_list
{
    bool ordered;
    char marker;            /* '-'、'+'、'*'、または順序付きの '.'、')' */
    unsigned long start;    /* 順序付きの開始番号（0〜999999999） */
    bool tight;
} mnd_md_list_t;

typedef struct mnd_md_item
{
    mnd_span_t marker;      /* リスト記号（番号を含む）の範囲 */
    size_t content_indent;  /* 内容の開始の桁（継続行に必要なインデント） */
} mnd_md_item_t;

typedef struct mnd_md_code_block
{
    bool fenced;
    char fence_char;        /* '`' または '~'（fenced のとき） */
    size_t fence_len;
    size_t fence_indent;    /* 開きフェンスのインデントの桁数 */
    bool has_info;
    mnd_span_t info;        /* 前後の空白を除いた情報文字列（エスケープは未処理） */
    bool closed;            /* 閉じフェンスがある */
} mnd_md_code_block_t;

typedef struct mnd_md_html_block
{
    int type;               /* 開始条件の種類 1〜7 */
} mnd_md_html_block_t;

typedef struct mnd_md_link_ref_def
{
    mnd_span_t label;       /* [ と ] の内側 */
    mnd_span_t dest;        /* <> で囲まれていれば、その内側 */
    bool dest_angle;
    bool has_title;
    mnd_span_t title;       /* 引用符・括弧の内側 */
    char title_delim;       /* '"'、'\''、'('（has_title のとき） */
    const char *norm_label; /* 正規化したラベル（UTF-8、NUL 終端） */
} mnd_md_link_ref_def_t;

typedef enum mnd_md_ref_kind
{
    MND_MD_REF_INLINE,      /* [text](dest "title") */
    MND_MD_REF_FULL,        /* [text][label] */
    MND_MD_REF_COLLAPSED,   /* [text][] */
    MND_MD_REF_SHORTCUT     /* [text] */
} mnd_md_ref_kind_t;

typedef struct mnd_md_link
{
    mnd_md_ref_kind_t ref_kind;
    mnd_span_t text;        /* リンクテキスト、または画像の alt（[ と ] の内側） */
    mnd_span_t label;       /* 参照ラベル（FULL は 2 つ目の [] の内側、COLLAPSED と SHORTCUT は text と同じ） */
    const mnd_md_node_t *def; /* 参照形式のとき、対応するリンク参照定義のノード */
    mnd_span_t dest;        /* リンク先（参照形式では定義の dest） */
    bool dest_angle;
    bool has_title;
    mnd_span_t title;       /* タイトル（参照形式では定義の title） */
    char title_delim;
} mnd_md_link_t;

typedef struct mnd_md_code_span
{
    mnd_span_t content;     /* バッククォートの並びの内側（正規化は未処理） */
    size_t ticks;
} mnd_md_code_span_t;

typedef struct mnd_md_emph
{
    char delim;             /* '*' または '_' */
    mnd_span_t open;        /* 開きの区切り記号 */
    mnd_span_t close;       /* 閉じの区切り記号 */
} mnd_md_emph_t;

typedef struct mnd_md_autolink
{
    bool email;
    mnd_span_t dest;        /* < と > の内側 */
} mnd_md_autolink_t;

typedef struct mnd_md_escape
{
    char ch;                /* エスケープされた ASCII の句読点 */
} mnd_md_escape_t;

typedef struct mnd_md_char_ref
{
    bool numeric;
    unsigned char len;      /* text のバイト数（1〜6） */
    char text[8];           /* 表す文字の UTF-8（不正な数値は U+FFFD） */
} mnd_md_char_ref_t;

typedef struct mnd_md_hard_break
{
    bool backslash;         /* バックスラッシュによる改行 */
} mnd_md_hard_break_t;

/*
 * 構文木のノード。文書のアリーナに確保する。
 * 不変条件: span は内容の範囲内。子は親の内側。兄弟は重ならずに原文の順に並ぶ（3.2、3.3）。
 * リンク・画像・強調・見出し・段落・方言のインラインを持つリーフは、インラインのノードを子に持つ。
 * 画像の子は alt のインラインである
 */
struct mnd_md_node
{
    mnd_md_kind_t kind;
    mnd_span_t span;
    size_t start_line;
    size_t end_line;
    mnd_md_node_t *parent;
    mnd_md_node_t *first_child;
    mnd_md_node_t *last_child;
    mnd_md_node_t *prev;
    mnd_md_node_t *next;
    const mnd_md_line_t *lines; /* リーフの行（それ以外は NULL） */
    size_t line_count;
    union
    {
        mnd_md_heading_t heading;
        mnd_md_list_t list;
        mnd_md_item_t item;
        mnd_md_code_block_t code_block;
        mnd_md_html_block_t html_block;
        mnd_md_link_ref_def_t link_ref_def;
        mnd_md_link_t link;         /* MND_MD_LINK と MND_MD_IMAGE */
        mnd_md_code_span_t code_span;
        mnd_md_emph_t emph;         /* MND_MD_EMPH と MND_MD_STRONG */
        mnd_md_autolink_t autolink;
        mnd_md_escape_t escape;
        mnd_md_char_ref_t char_ref;
        mnd_md_hard_break_t hard_break;
        void *dialect_data;         /* 方言の種類のノード（方言が定める） */
    } as;
};

typedef struct mnd_md_doc mnd_md_doc_t; /* 不透明。内容へのポインタ、根、アリーナを持つ */

const mnd_md_node_t *mnd_md_doc_root(const mnd_md_doc_t *doc);
const char *mnd_md_doc_data(const mnd_md_doc_t *doc);
size_t mnd_md_doc_len(const mnd_md_doc_t *doc);
void mnd_md_doc_free(mnd_md_doc_t *doc);  /* NULL に対しても安全 */

/* 前順の次のノード。root の部分木を出たら NULL */
const mnd_md_node_t *mnd_md_node_next(const mnd_md_node_t *node, const mnd_md_node_t *root);

/*
 * leaf の行に含まれる範囲 span の論理的な文字列を out の後ろに追加する。
 * 各行のプレフィックスを飛ばし、pad の数の空白を補い、行の境目を "\n" にする。
 * leaf には span を含む、行を持つノード（段落、見出し、コードブロック、リンク参照定義など）を渡す。
 * インラインのノードの範囲には、それを含むリーフを渡す。
 * span が leaf の行の内容と重ならない部分は無視する。leaf が行を持たない場合は、span のバイト列を
 * そのまま追加する。
 * メモリ不足のときは false を返し、out にそれまでに追加した内容は残る
 */
bool mnd_md_logical_text(const mnd_md_doc_t *doc, const mnd_md_node_t *leaf, mnd_span_t span,
                         mnd_buf_t *out);

/*
 * text[0..len) のバックスラッシュエスケープと実体参照・数値文字参照を文字に戻して out の後ろに追加する。
 * メモリ不足のときは false を返し、out にそれまでに追加した内容は残る
 */
bool mnd_md_unescape(const char *text, size_t len, mnd_buf_t *out);

#endif
