/*
 * html_render: 構文木を commonmark.js 0.31 の HTML 出力（lib/render/html.js）と同じ規則で HTML にする。
 * 文字列は構文木の範囲から mnd_md_logical_text と mnd_md_unescape で作るため、範囲の誤りは HTML の
 * 不一致として現れる。木は親へのポインタでたどり、再帰を使わない
 */

#include "html_render.h"

#include "foundation/buf.h"
#include "markdown/md.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* NUL の代わりに出力する U+FFFD の UTF-8 */
static const char REPLACEMENT[] = "\357\277\275";
#define REPLACEMENT_LEN 3

typedef struct renderer
{
    const mnd_md_doc_t *doc;
    mnd_buf_t *out;
    mnd_buf_t text;     /* 作業用: 範囲の論理的な文字列 */
    mnd_buf_t plain;    /* 作業用: アンエスケープした文字列 */
    mnd_buf_t url;      /* 作業用: パーセントエンコードした URL */
    /*
     * 最後に出力した文字列が "\n" だけか（commonmark.js の lastOut === "\n"）。
     * cr はこれが偽のときだけ改行を出す。初期値は真
     */
    bool last_newline;
    size_t disable_tags; /* 画像の中の深さ。0 より大きい間はタグを出さない（alt の文字列だけを出す） */
} renderer_t;

/* 出力 */

/* s[0..len) を出力する。escape が真なら & < > " をエスケープする。NUL は U+FFFD にする */
static bool emit(renderer_t *r, const char *s, size_t len, bool escape)
{
    const char *rep = NULL;
    size_t run = 0;
    size_t rep_len = 0;
    size_t i;

    for (i = 0; i < len; i++)
    {
        switch (s[i])
        {
        case '\0':
            rep = REPLACEMENT;
            rep_len = REPLACEMENT_LEN;
            break;
        case '&':
            rep = escape ? "&amp;" : NULL;
            rep_len = 5;
            break;
        case '<':
            rep = escape ? "&lt;" : NULL;
            rep_len = 4;
            break;
        case '>':
            rep = escape ? "&gt;" : NULL;
            rep_len = 4;
            break;
        case '"':
            rep = escape ? "&quot;" : NULL;
            rep_len = 6;
            break;
        default:
            rep = NULL;
            break;
        }

        if (rep != NULL)
        {
            if (!mnd_buf_append(r->out, s + run, i - run) || !mnd_buf_append(r->out, rep, rep_len))
            {
                return false;
            }

            run = i + 1;
        }
    }

    if (!mnd_buf_append(r->out, s + run, len - run))
    {
        return false;
    }

    r->last_newline = ((len == 1) && (s[0] == '\n'));

    return true;
}

/* commonmark.js の lit（エスケープしない） */
static bool lit(renderer_t *r, const char *s, size_t len)
{
    return emit(r, s, len, false);
}

static bool lit_str(renderer_t *r, const char *s)
{
    return lit(r, s, strlen(s));
}

/* commonmark.js の out（エスケープする） */
static bool out_text(renderer_t *r, const char *s, size_t len)
{
    return emit(r, s, len, true);
}

/* commonmark.js の tag。画像の中では何も出さない */
static bool tag(renderer_t *r, const char *s)
{
    if (r->disable_tags > 0)
    {
        return true;
    }

    return lit_str(r, s);
}

/* commonmark.js の cr。最後の出力が "\n" でなければ改行を出す */
static bool cr(renderer_t *r)
{
    if (r->last_newline)
    {
        return true;
    }

    return lit(r, "\n", 1);
}

/* 文字列の組み立て */

/* node を含み、行を持つ最も近いリーフ（node 自身を含む）。なければ NULL */
static const mnd_md_node_t *find_leaf(const mnd_md_node_t *node)
{
    while ((node != NULL) && (node->lines == NULL))
    {
        node = node->parent;
    }

    return node;
}

static size_t max_size(size_t a, size_t b)
{
    return (a > b) ? a : b;
}

static size_t min_size(size_t a, size_t b)
{
    return (a < b) ? a : b;
}

/* mnd_md_logical_text が行 line を論理的な文字列に含めるか（md_text.c の line_in_span と同じ条件） */
static bool line_included(const mnd_md_line_t *line, mnd_span_t span)
{
    size_t cs = line->content.start;
    size_t ce = line->content.end;

    if (cs == ce)
    {
        return ((span.start <= cs) && (cs <= span.end));
    }

    return (max_size(span.start, cs) < min_size(span.end, ce));
}

static bool append_newlines(mnd_buf_t *buf, size_t count)
{
    while (count > 0)
    {
        if (!mnd_buf_append_byte(buf, '\n'))
        {
            return false;
        }

        count--;
    }

    return true;
}

/*
 * leaf の行に含まれる範囲 span の論理的な文字列を buf の後ろに追加する。
 * mnd_md_logical_text は、空でない内容と行の境目でだけ接する行を含めないため、範囲が行の境目で
 * 始まるか終わると、その境目の "\n" が落ちる（tasks.md の 4.1 の注意）。範囲が覆う境目
 * （行 i の内容の終わりから行 i + 1 の内容の始まりまで）のうち、落ちた分の "\n" を前後に補う
 */
static bool span_text(renderer_t *r, const mnd_md_node_t *leaf, mnd_span_t span, mnd_buf_t *buf)
{
    size_t before = 0;
    size_t after = 0;
    size_t first = 0;
    size_t last = 0;
    size_t i;
    bool any = false;

    if ((leaf == NULL) || (leaf->lines == NULL) || (leaf->line_count == 0))
    {
        return mnd_md_logical_text(r->doc, leaf, span, buf);
    }

    for (i = 0; i < leaf->line_count; i++)
    {
        if (line_included(&leaf->lines[i], span))
        {
            if (!any)
            {
                first = i;
            }

            last = i;
            any = true;
        }
    }

    for (i = 0; (i + 1) < leaf->line_count; i++)
    {
        if ((span.start > leaf->lines[i].content.end) || (span.end < leaf->lines[i + 1].content.start))
        {
            continue;
        }

        if (!any || (i < first))
        {
            before++;
        }
        else if (i >= last)
        {
            after++;
        }
    }

    return (append_newlines(buf, before) && mnd_md_logical_text(r->doc, leaf, span, buf)
            && append_newlines(buf, after));
}

static bool is_hex(unsigned char c)
{
    return (((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F')));
}

/* mdurl の encode がそのまま残す ASCII（英数字と ;/?:@&=+$,-_.!~*'()#） */
static bool url_keeps(unsigned char c)
{
    if (((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')))
    {
        return true;
    }

    return ((c != '\0') && (strchr(";/?:@&=+$,-_.!~*'()#", c) != NULL));
}

static bool append_percent(mnd_buf_t *buf, unsigned char c)
{
    static const char HEX[] = "0123456789ABCDEF";
    char bytes[3];

    bytes[0] = '%';
    bytes[1] = HEX[c >> 4];
    bytes[2] = HEX[c & 0x0F];

    return mnd_buf_append(buf, bytes, 3);
}

/*
 * commonmark.js の normalizeURI（mdurl の encode）と同じ規則で s[0..len) をパーセントエンコードし、
 * buf の後ろに追加する。既存の %XX は残し、ASCII 以外は UTF-8 のバイトごとに %XX にする。
 * NUL は U+FFFD として扱う（commonmark.js は入力の NUL を U+FFFD に置き換えてから解析する）
 */
static bool encode_url(const char *s, size_t len, mnd_buf_t *buf)
{
    const unsigned char *u = (const unsigned char *)s;
    size_t i = 0;
    size_t k;

    while (i < len)
    {
        if ((u[i] == '%') && ((i + 2) < len) && is_hex(u[i + 1]) && is_hex(u[i + 2]))
        {
            if (!mnd_buf_append(buf, s + i, 3))
            {
                return false;
            }

            i += 3;
            continue;
        }

        if (u[i] == '\0')
        {
            for (k = 0; k < REPLACEMENT_LEN; k++)
            {
                if (!append_percent(buf, (unsigned char)REPLACEMENT[k]))
                {
                    return false;
                }
            }
        }
        else if (url_keeps(u[i]))
        {
            if (!mnd_buf_append_byte(buf, u[i]))
            {
                return false;
            }
        }
        else if (!append_percent(buf, u[i]))
        {
            return false;
        }

        i++;
    }

    return true;
}

/*
 * s[pos..len) が JavaScript の \s（String.prototype.trim と split(/\s+/) の空白）で始まるなら、
 * そのバイト数を返す。始まらなければ 0
 */
static size_t js_space_len(const unsigned char *s, size_t pos, size_t len)
{
    size_t rest = len - pos;

    if ((s[pos] == ' ') || ((s[pos] >= '\t') && (s[pos] <= '\r')))
    {
        return 1;
    }

    if ((rest >= 2) && (s[pos] == 0xC2) && (s[pos + 1] == 0xA0))
    {
        return 2;
    }

    if (rest < 3)
    {
        return 0;
    }

    /* U+1680、U+2000〜U+200A、U+2028、U+2029、U+202F、U+205F、U+3000、U+FEFF */
    if (((s[pos] == 0xE1) && (s[pos + 1] == 0x9A) && (s[pos + 2] == 0x80))
        || ((s[pos] == 0xE2) && (s[pos + 1] == 0x80)
            && ((s[pos + 2] <= 0x8A) || (s[pos + 2] == 0xA8) || (s[pos + 2] == 0xA9) || (s[pos + 2] == 0xAF)))
        || ((s[pos] == 0xE2) && (s[pos + 1] == 0x81) && (s[pos + 2] == 0x9F))
        || ((s[pos] == 0xE3) && (s[pos + 1] == 0x80) && (s[pos + 2] == 0x80))
        || ((s[pos] == 0xEF) && (s[pos + 1] == 0xBB) && (s[pos + 2] == 0xBF)))
    {
        return 3;
    }

    return 0;
}

/* ブロック */

/* リーフの範囲 span の論理的な文字列を r->text に入れる */
static bool load_text(renderer_t *r, const mnd_md_node_t *leaf, mnd_span_t span)
{
    mnd_buf_clear(&r->text);

    return span_text(r, leaf, span, &r->text);
}

/*
 * コードブロック。内容は各行の論理的な文字列に "\n" を付けたもの。言語のクラスは、情報文字列
 * （JavaScript の trim の後）をアンエスケープして、最初の空白までの語にする
 */
static bool render_code_block(renderer_t *r, const mnd_md_node_t *node)
{
    const mnd_md_code_block_t *code = &node->as.code_block;
    const unsigned char *info = NULL;
    const char *word = NULL;
    size_t start;
    size_t end;
    size_t n;
    size_t len = 0;

    if (!cr(r) || !tag(r, "<pre><code"))
    {
        return false;
    }

    if (code->has_info)
    {
        info = (const unsigned char *)mnd_md_doc_data(r->doc);
        start = code->info.start;
        end = code->info.end;
        while ((start < end) && ((n = js_space_len(info, start, end)) > 0))
        {
            start += n;
        }

        mnd_buf_clear(&r->plain);
        if (!mnd_md_unescape((const char *)info + start, end - start, &r->plain))
        {
            return false;
        }

        word = mnd_buf_cstr(&r->plain);
        while ((len < r->plain.len) && (js_space_len((const unsigned char *)word, len, r->plain.len) == 0))
        {
            len++;
        }
    }

    if (len > 0)
    {
        if (!tag(r, " class=\"language-") || !out_text(r, word, len) || !tag(r, "\""))
        {
            return false;
        }
    }

    /* 行のないコードブロック（内容のないフェンス）は空。行を持たないノードの範囲は原文のまま返るので渡さない */
    mnd_buf_clear(&r->text);
    if (!tag(r, ">") || ((node->line_count > 0) && !load_text(r, node, node->span)))
    {
        return false;
    }

    if ((node->line_count > 0) && !mnd_buf_append_byte(&r->text, '\n'))
    {
        return false;
    }

    return (out_text(r, mnd_buf_cstr(&r->text), r->text.len) && tag(r, "</code></pre>") && cr(r));
}

/* HTML ブロック。内容は行の論理的な文字列から、末尾の (\n *)+ を除いたもの（commonmark.js と同じ） */
static bool render_html_block(renderer_t *r, const mnd_md_node_t *node)
{
    const char *s = NULL;
    size_t len;
    size_t k;

    if (!load_text(r, node, node->span))
    {
        return false;
    }

    s = mnd_buf_cstr(&r->text);
    len = r->text.len;
    for (;;)
    {
        k = len;
        while ((k > 0) && (s[k - 1] == ' '))
        {
            k--;
        }

        if ((k == 0) || (s[k - 1] != '\n'))
        {
            break;
        }

        len = k - 1;
    }

    return (cr(r) && lit(r, s, len) && cr(r));
}

/* 順序付きのリストの開きのタグ */
static bool render_list_open(renderer_t *r, const mnd_md_node_t *node)
{
    char start[64];

    if (!cr(r))
    {
        return false;
    }

    if (!node->as.list.ordered)
    {
        if (!tag(r, "<ul>"))
        {
            return false;
        }
    }
    else if (node->as.list.start != 1)
    {
        (void)snprintf(start, sizeof(start), "<ol start=\"%lu\">", node->as.list.start);
        if (!tag(r, start))
        {
            return false;
        }
    }
    else if (!tag(r, "<ol>"))
    {
        return false;
    }

    return cr(r);
}

/* タイトなリストの項目の直下の段落は <p> で囲まない */
static bool in_tight_list(const mnd_md_node_t *node)
{
    const mnd_md_node_t *grandparent = (node->parent != NULL) ? node->parent->parent : NULL;

    return ((grandparent != NULL) && (grandparent->kind == MND_MD_LIST) && grandparent->as.list.tight);
}

static bool render_heading(renderer_t *r, const mnd_md_node_t *node, bool entering)
{
    char name[8];
    int level = node->as.heading.level;

    if ((level < 1) || (level > 6))
    {
        level = 1;
    }

    (void)snprintf(name, sizeof(name), entering ? "<h%d>" : "</h%d>", level);
    if (entering)
    {
        return (cr(r) && tag(r, name));
    }

    return (tag(r, name) && cr(r));
}

/* インライン */

/* コードスパン。行の境目は空白にし、両端が空白で空白だけでなければ 1 つずつ除く */
static bool render_code_span(renderer_t *r, const mnd_md_node_t *node)
{
    char *s = NULL;
    size_t start = 0;
    size_t len;
    size_t i;
    bool all_space = true;

    if (!load_text(r, find_leaf(node), node->as.code_span.content))
    {
        return false;
    }

    s = r->text.data;
    len = r->text.len;
    for (i = 0; i < len; i++)
    {
        if (s[i] == '\n')
        {
            s[i] = ' ';
        }

        if (s[i] != ' ')
        {
            all_space = false;
        }
    }

    if ((len >= 2) && (s[0] == ' ') && (s[len - 1] == ' ') && !all_space)
    {
        start = 1;
        len -= 2;
    }

    return (tag(r, "<code>") && out_text(r, mnd_buf_cstr(&r->text) + start, len) && tag(r, "</code>"));
}

/*
 * リンクと画像のリンク先とタイトルの文字列を作る。参照形式の範囲は定義のノードの中にあるので、
 * 定義をリーフとして論理的な文字列を得る。リンク先は r->url（アンエスケープしてパーセントエンコード）、
 * タイトルは r->plain（アンエスケープ）に入れる
 */
static bool load_link(renderer_t *r, const mnd_md_node_t *node)
{
    const mnd_md_link_t *link = &node->as.link;
    const mnd_md_node_t *leaf = NULL;

    if ((link->ref_kind != MND_MD_REF_INLINE) && (link->def != NULL))
    {
        leaf = link->def;
    }
    else
    {
        leaf = find_leaf(node);
    }

    mnd_buf_clear(&r->plain);
    mnd_buf_clear(&r->url);
    if (!load_text(r, leaf, link->dest) || !mnd_md_unescape(mnd_buf_cstr(&r->text), r->text.len, &r->plain)
        || !encode_url(mnd_buf_cstr(&r->plain), r->plain.len, &r->url))
    {
        return false;
    }

    mnd_buf_clear(&r->plain);
    if (link->has_title)
    {
        if (!load_text(r, leaf, link->title) || !mnd_md_unescape(mnd_buf_cstr(&r->text), r->text.len, &r->plain))
        {
            return false;
        }
    }

    return true;
}

static bool render_link_open(renderer_t *r, const mnd_md_node_t *node)
{
    if (r->disable_tags > 0)
    {
        return true;
    }

    if (!load_link(r, node))
    {
        return false;
    }

    if (!lit_str(r, "<a href=\"") || !out_text(r, mnd_buf_cstr(&r->url), r->url.len))
    {
        return false;
    }

    if ((r->plain.len > 0)
        && (!lit_str(r, "\" title=\"") || !out_text(r, mnd_buf_cstr(&r->plain), r->plain.len)))
    {
        return false;
    }

    return lit_str(r, "\">");
}

/* 画像の開き。src と alt の始まりを出し、子（alt）の間はタグを出さない */
static bool render_image_open(renderer_t *r, const mnd_md_node_t *node)
{
    if (r->disable_tags == 0)
    {
        mnd_buf_clear(&r->url);
        if (!load_link(r, node) || !lit_str(r, "<img src=\"")
            || !out_text(r, mnd_buf_cstr(&r->url), r->url.len) || !lit_str(r, "\" alt=\""))
        {
            return false;
        }
    }

    r->disable_tags++;

    return true;
}

/* 画像の閉じ。タイトルは子を出したあとで作り直す（子の出力で作業用のバッファが変わるため） */
static bool render_image_close(renderer_t *r, const mnd_md_node_t *node)
{
    r->disable_tags--;
    if (r->disable_tags > 0)
    {
        return true;
    }

    if (!load_link(r, node))
    {
        return false;
    }

    if ((r->plain.len > 0)
        && (!lit_str(r, "\" title=\"") || !out_text(r, mnd_buf_cstr(&r->plain), r->plain.len)))
    {
        return false;
    }

    return lit_str(r, "\" />");
}

/* 自動リンク。リンク先はアンエスケープせず、メールアドレスには mailto: を付ける */
static bool render_autolink(renderer_t *r, const mnd_md_node_t *node)
{
    if (!load_text(r, find_leaf(node), node->as.autolink.dest))
    {
        return false;
    }

    if (r->disable_tags == 0)
    {
        mnd_buf_clear(&r->url);
        if ((node->as.autolink.email && !mnd_buf_append_str(&r->url, "mailto:"))
            || !encode_url(mnd_buf_cstr(&r->text), r->text.len, &r->url) || !lit_str(r, "<a href=\"")
            || !out_text(r, mnd_buf_cstr(&r->url), r->url.len) || !lit_str(r, "\">"))
        {
            return false;
        }
    }

    return (out_text(r, mnd_buf_cstr(&r->text), r->text.len) && tag(r, "</a>"));
}

/* 範囲の論理的な文字列をそのまま、またはエスケープして出す（テキスト、生の HTML） */
static bool render_span(renderer_t *r, const mnd_md_node_t *node, bool escape)
{
    if (!load_text(r, find_leaf(node), node->span))
    {
        return false;
    }

    return emit(r, mnd_buf_cstr(&r->text), r->text.len, escape);
}

/* 走査 */

/* node に入るときの出力 */
static bool enter_node(renderer_t *r, const mnd_md_node_t *node)
{
    switch (node->kind)
    {
    case MND_MD_BLOCK_QUOTE:
        return (cr(r) && tag(r, "<blockquote>") && cr(r));
    case MND_MD_LIST:
        return render_list_open(r, node);
    case MND_MD_ITEM:
        return tag(r, "<li>");
    case MND_MD_PARAGRAPH:
        return (in_tight_list(node) || (cr(r) && tag(r, "<p>")));
    case MND_MD_HEADING:
        return render_heading(r, node, true);
    case MND_MD_THEMATIC_BREAK:
        return (cr(r) && tag(r, "<hr />") && cr(r));
    case MND_MD_CODE_BLOCK:
        return render_code_block(r, node);
    case MND_MD_HTML_BLOCK:
        return render_html_block(r, node);
    case MND_MD_TEXT:
        return render_span(r, node, true);
    case MND_MD_SOFT_BREAK:
        return lit(r, "\n", 1);
    case MND_MD_HARD_BREAK:
        return (tag(r, "<br />") && cr(r));
    case MND_MD_ESCAPE:
        return out_text(r, &node->as.escape.ch, 1);
    case MND_MD_CHAR_REF:
        return out_text(r, node->as.char_ref.text, node->as.char_ref.len);
    case MND_MD_CODE_SPAN:
        return render_code_span(r, node);
    case MND_MD_EMPH:
        return tag(r, "<em>");
    case MND_MD_STRONG:
        return tag(r, "<strong>");
    case MND_MD_LINK:
        return render_link_open(r, node);
    case MND_MD_IMAGE:
        return render_image_open(r, node);
    case MND_MD_AUTOLINK:
        return render_autolink(r, node);
    case MND_MD_HTML_INLINE:
        return render_span(r, node, false);
    default:
        /* 文書、リンク参照定義、方言の種類（子だけを出力する） */
        return true;
    }
}

/* node から出るときの出力 */
static bool leave_node(renderer_t *r, const mnd_md_node_t *node)
{
    switch (node->kind)
    {
    case MND_MD_BLOCK_QUOTE:
        return (cr(r) && tag(r, "</blockquote>") && cr(r));
    case MND_MD_LIST:
        return (cr(r) && tag(r, node->as.list.ordered ? "</ol>" : "</ul>") && cr(r));
    case MND_MD_ITEM:
        return (tag(r, "</li>") && cr(r));
    case MND_MD_PARAGRAPH:
        return (in_tight_list(node) || (tag(r, "</p>") && cr(r)));
    case MND_MD_HEADING:
        return render_heading(r, node, false);
    case MND_MD_EMPH:
        return tag(r, "</em>");
    case MND_MD_STRONG:
        return tag(r, "</strong>");
    case MND_MD_LINK:
        return tag(r, "</a>");
    case MND_MD_IMAGE:
        return render_image_close(r, node);
    default:
        return true;
    }
}

bool mnd_md_html_render(const mnd_md_doc_t *doc, mnd_buf_t *out)
{
    renderer_t r;
    const mnd_md_node_t *root = mnd_md_doc_root(doc);
    const mnd_md_node_t *node = root;
    bool entering = true;
    bool ok = true;

    memset(&r, 0, sizeof(r));
    r.doc = doc;
    r.out = out;
    r.last_newline = true;
    mnd_buf_init(&r.text);
    mnd_buf_init(&r.plain);
    mnd_buf_init(&r.url);

    /* 親へのポインタで、入るときと出るときを順にたどる */
    while (ok && (node != NULL))
    {
        if (entering)
        {
            ok = enter_node(&r, node);
            if (ok && (node->first_child != NULL))
            {
                node = node->first_child;
                continue;
            }
        }

        ok = ok && leave_node(&r, node);
        if (node == root)
        {
            break;
        }

        if (node->next != NULL)
        {
            node = node->next;
            entering = true;
        }
        else
        {
            node = node->parent;
            entering = false;
        }
    }

    mnd_buf_free(&r.text);
    mnd_buf_free(&r.plain);
    mnd_buf_free(&r.url);

    return ok;
}
