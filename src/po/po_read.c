/* Reader: 行の分類、エントリの状態機械、エラーからの回復、読み込み後の検査 */

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/source.h"
#include "foundation/utf8.h"
#include "po/po.h"
#include "po/po_internal.h"
#include "po/po_read.h"

#include <stdint.h>
#include <string.h>

/* 診断に表示する入力の値の最大のバイト数 */
#define MND_PO_READ_SHOW_MAX 40

/* U+2068（FIRST STRONG ISOLATE）と U+2069（POP DIRECTIONAL ISOLATE）の UTF-8 */
#define MND_PO_READ_FSI "\xE2\x81\xA8"
#define MND_PO_READ_PDI "\xE2\x81\xA9"
#define MND_PO_READ_ISOLATE_LEN 3

/* 行の分類（設計の「行の分類」の表） */
typedef enum mnd_po_line_kind
{
    MND_PO_LINE_BLANK,       /* 空行 */
    MND_PO_LINE_IGNORED,     /* 残りが空の #~、#|、#~| */
    MND_PO_LINE_TRANSLATOR,  /* # */
    MND_PO_LINE_EXTRACTED,   /* #. */
    MND_PO_LINE_REFERENCE,   /* #: */
    MND_PO_LINE_FLAGS,       /* #, */
    MND_PO_LINE_KEYWORD,     /* キーワードと文字列 */
    MND_PO_LINE_STRING       /* 続きの文字列 */
} mnd_po_line_kind_t;

typedef enum mnd_po_keyword
{
    MND_PO_KEYWORD_MSGCTXT,
    MND_PO_KEYWORD_MSGID,
    MND_PO_KEYWORD_MSGID_PLURAL,
    MND_PO_KEYWORD_MSGSTR,
    MND_PO_KEYWORD_MSGSTR_INDEXED  /* msgstr[n] */
} mnd_po_keyword_t;

/* 分類した 1 行。Keyword と String の文字列はアンエスケープ済みで value に入る */
typedef struct mnd_po_line
{
    mnd_po_line_kind_t kind;
    size_t number;             /* 1 起点の行番号 */
    bool obsolete;             /* #~ または #~| で始まる */
    bool previous;             /* #| または #~| で始まる */
    mnd_slice_t text;          /* Translator、Extracted、Reference、Flags の内容 */
    mnd_po_keyword_t keyword;  /* Keyword のとき */
    size_t index;              /* MSGSTR_INDEXED のときの n */
    mnd_buf_t value;           /* Keyword、String の文字列 */
} mnd_po_line_t;

typedef enum mnd_po_lex_status
{
    MND_PO_LEX_OK,
    MND_PO_LEX_ERROR,     /* 診断を追加済み */
    MND_PO_LEX_NO_MEMORY
} mnd_po_lex_status_t;

typedef struct mnd_po_reader
{
    const char *path;
    mnd_diag_list_t *diags;
    mnd_po_catalog_t *out;
    bool failed;          /* エラーを 1 つ以上報告した */
    mnd_po_entry_t entry; /* 組み立て中のエントリ */
    bool entry_started;   /* entry にコメントかキーワードがある */
    bool has_keyword;     /* entry にキーワードがある */
    bool has_msgstr;      /* entry に msgstr がある */
    mnd_buf_t *target;    /* 続きの文字列を連結する先。なければ NULL */
} mnd_po_reader_t;

static bool mnd_po_read_is_blank(char c)
{
    return (c == ' ') || (c == '\t');
}

static bool mnd_po_read_is_digit(char c)
{
    return (c >= '0') && (c <= '9');
}

static bool mnd_po_read_starts_with(const char *s, size_t len, const char *prefix)
{
    size_t prefix_len = strlen(prefix);

    return (len >= prefix_len) && (memcmp(s, prefix, prefix_len) == 0);
}

static void mnd_po_read_error(mnd_po_reader_t *reader, size_t line, const char *message)
{
    mnd_diag_add(reader->diags, MND_SEVERITY_ERROR, reader->path, line, "%s", message);
    reader->failed = true;
}

static void mnd_po_read_no_memory(mnd_po_reader_t *reader)
{
    mnd_po_read_error(reader, 0, "out of memory");
}

/* 正しい UTF-8 の先頭バイトから、その文字のバイト数を返す */
static size_t mnd_po_read_char_len(unsigned char lead)
{
    if (lead < 0x80)
    {
        return 1;
    }

    if (lead < 0xE0)
    {
        return 2;
    }

    if (lead < 0xF0)
    {
        return 3;
    }

    return 4;
}

/*
 * 表示に使う値を out（NUL 終端）に作る。s[0..len) は正しい UTF-8。
 * 文字の途中で切らずに MND_PO_READ_SHOW_MAX バイトまでとし、制御文字（C0、DEL、C1）を '?' にする
 */
static void mnd_po_read_show(const char *s, size_t len, char out[MND_PO_READ_SHOW_MAX + 1])
{
    size_t i = 0;
    size_t n = 0;

    while (i < len)
    {
        unsigned char c = (unsigned char)s[i];
        size_t char_len = mnd_po_read_char_len(c);
        bool control = (c < 0x20) || (c == 0x7F)
                       || ((c == 0xC2) && (i + 1 < len) && ((unsigned char)s[i + 1] < 0xA0));

        if (char_len > (len - i))
        {
            char_len = len - i;
        }

        if (control)
        {
            if (n + 1 > MND_PO_READ_SHOW_MAX)
            {
                break;
            }

            out[n] = '?';
            n++;
        }
        else
        {
            if (n + char_len > MND_PO_READ_SHOW_MAX)
            {
                break;
            }

            memcpy(out + n, s + i, char_len);
            n += char_len;
        }

        i += char_len;
    }

    out[n] = '\0';
}

/* アンエスケープの失敗を報告する。lit[0..len) は引用符の内側 */
static mnd_po_lex_status_t mnd_po_read_unescape_error(mnd_po_reader_t *reader, size_t line,
                                                      mnd_po_unescape_status_t status,
                                                      const char *lit, size_t len,
                                                      size_t bad_offset)
{
    char shown[MND_PO_READ_SHOW_MAX + 1];

    switch (status)
    {
    case MND_PO_UNESCAPE_BAD_ESCAPE:
        if ((bad_offset + 1) < len)
        {
            size_t char_len = mnd_po_read_char_len((unsigned char)lit[bad_offset + 1]);

            if (char_len > (len - (bad_offset + 1)))
            {
                char_len = len - (bad_offset + 1);
            }

            mnd_po_read_show(lit + bad_offset + 1, char_len, shown);
        }
        else
        {
            shown[0] = '\0';
        }

        mnd_diag_add(reader->diags, MND_SEVERITY_ERROR, reader->path, line,
                     "invalid escape sequence '\\%s'", shown);
        reader->failed = true;

        return MND_PO_LEX_ERROR;
    case MND_PO_UNESCAPE_NUL:
        mnd_po_read_error(reader, line, "string contains a NUL byte");

        return MND_PO_LEX_ERROR;
    case MND_PO_UNESCAPE_BAD_UTF8:
        mnd_po_read_error(reader, line, "invalid UTF-8 sequence");

        return MND_PO_LEX_ERROR;
    case MND_PO_UNESCAPE_NO_MEMORY:
    case MND_PO_UNESCAPE_OK:
    default:
        break;
    }

    return MND_PO_LEX_NO_MEMORY;
}

/*
 * s[0..len)（s[0] == '"'）を文字列リテラルとして解釈し、アンエスケープした値を value に加える。
 * 閉じる '"' の後ろは空白とタブだけを許す
 */
static mnd_po_lex_status_t mnd_po_read_literal(mnd_po_reader_t *reader, size_t line,
                                               const char *s, size_t len, mnd_buf_t *value)
{
    size_t i = 1;
    size_t close;
    size_t bad_offset = 0;
    mnd_po_unescape_status_t status;

    while ((i < len) && (s[i] != '"'))
    {
        if ((s[i] == '\\') && ((i + 1) < len))
        {
            i += 2;
        }
        else
        {
            i++;
        }
    }

    if (i >= len)
    {
        mnd_po_read_error(reader, line, "unterminated string");

        return MND_PO_LEX_ERROR;
    }

    close = i;
    for (i = close + 1; i < len; i++)
    {
        if (!mnd_po_read_is_blank(s[i]))
        {
            mnd_po_read_error(reader, line, "unexpected text after string");

            return MND_PO_LEX_ERROR;
        }
    }

    status = mnd_po_unescape(s + 1, close - 1, value, &bad_offset);
    if (status != MND_PO_UNESCAPE_OK)
    {
        return mnd_po_read_unescape_error(reader, line, status, s + 1, close - 1, bad_offset);
    }

    return MND_PO_LEX_OK;
}

/* word[0..len) が msgstr[n] なら true を返し、*index に n を入れる */
static bool mnd_po_read_msgstr_index(const char *word, size_t len, size_t *index)
{
    const size_t prefix_len = 7;  /* "msgstr[" */
    size_t value = 0;
    size_t i;

    if ((len < (prefix_len + 2)) || !mnd_po_read_starts_with(word, len, "msgstr[")
        || (word[len - 1] != ']'))
    {
        return false;
    }

    for (i = prefix_len; i < (len - 1); i++)
    {
        size_t digit;

        if (!mnd_po_read_is_digit(word[i]))
        {
            return false;
        }

        digit = (size_t)(word[i] - '0');
        if (value > ((SIZE_MAX - digit) / 10))
        {
            return false;
        }

        value = (value * 10) + digit;
    }

    *index = value;

    return true;
}

/* word[0..len) をキーワードとして照合する。未知なら false */
static bool mnd_po_read_match_keyword(const char *word, size_t len, mnd_po_keyword_t *keyword,
                                      size_t *index)
{
    static const struct
    {
        const char *name;
        mnd_po_keyword_t keyword;
    } names[] = {
        { "msgctxt", MND_PO_KEYWORD_MSGCTXT },
        { "msgid", MND_PO_KEYWORD_MSGID },
        { "msgid_plural", MND_PO_KEYWORD_MSGID_PLURAL },
        { "msgstr", MND_PO_KEYWORD_MSGSTR }
    };
    size_t i;

    for (i = 0; i < (sizeof(names) / sizeof(names[0])); i++)
    {
        if ((strlen(names[i].name) == len) && (memcmp(names[i].name, word, len) == 0))
        {
            *keyword = names[i].keyword;
            *index = 0;

            return true;
        }
    }

    if (mnd_po_read_msgstr_index(word, len, index))
    {
        *keyword = MND_PO_KEYWORD_MSGSTR_INDEXED;

        return true;
    }

    return false;
}

/*
 * キーワードの行、または続きの文字列の行 s[0..len)（先頭の空白を除いたもの）を解釈する。
 * line->previous が真なら、キーワードは msgctxt、msgid、msgid_plural だけを受け付ける
 */
static mnd_po_lex_status_t mnd_po_read_lex_code(mnd_po_reader_t *reader, const char *s,
                                                size_t len, mnd_po_line_t *line)
{
    char shown[MND_PO_READ_SHOW_MAX + 1];
    size_t word_len = 0;
    size_t rest;

    if (s[0] == '"')
    {
        line->kind = MND_PO_LINE_STRING;

        return mnd_po_read_literal(reader, line->number, s, len, &line->value);
    }

    while ((word_len < len) && !mnd_po_read_is_blank(s[word_len]))
    {
        word_len++;
    }

    if (!mnd_po_read_match_keyword(s, word_len, &line->keyword, &line->index)
        || (line->previous && (line->keyword != MND_PO_KEYWORD_MSGCTXT)
            && (line->keyword != MND_PO_KEYWORD_MSGID)
            && (line->keyword != MND_PO_KEYWORD_MSGID_PLURAL)))
    {
        mnd_po_read_show(s, word_len, shown);
        mnd_diag_add(reader->diags, MND_SEVERITY_ERROR, reader->path, line->number,
                     "unknown keyword '%s'", shown);
        reader->failed = true;

        return MND_PO_LEX_ERROR;
    }

    rest = word_len;
    while ((rest < len) && mnd_po_read_is_blank(s[rest]))
    {
        rest++;
    }

    if ((rest >= len) || (s[rest] != '"'))
    {
        mnd_po_read_show(s, word_len, shown);
        mnd_diag_add(reader->diags, MND_SEVERITY_ERROR, reader->path, line->number,
                     "missing string after '%s'", shown);
        reader->failed = true;

        return MND_PO_LEX_ERROR;
    }

    line->kind = MND_PO_LINE_KEYWORD;

    return mnd_po_read_literal(reader, line->number, s + rest, len - rest, &line->value);
}

/* "#~"、"#|"、"#~|" の後ろの残りを解釈する。残りが空なら無視する */
static mnd_po_lex_status_t mnd_po_read_lex_marked(mnd_po_reader_t *reader, const char *s,
                                                  size_t len, mnd_po_line_t *line)
{
    size_t i = 0;

    while ((i < len) && mnd_po_read_is_blank(s[i]))
    {
        i++;
    }

    if (i >= len)
    {
        line->kind = MND_PO_LINE_IGNORED;

        return MND_PO_LEX_OK;
    }

    return mnd_po_read_lex_code(reader, s + i, len - i, line);
}

/* コメントの内容 s[0..len) を line->text に設定する。NUL を含めば誤り */
static mnd_po_lex_status_t mnd_po_read_lex_comment(mnd_po_reader_t *reader, const char *s,
                                                   size_t len, mnd_po_line_kind_t kind,
                                                   mnd_po_line_t *line)
{
    if ((len > 0) && (memchr(s, '\0', len) != NULL))
    {
        mnd_po_read_error(reader, line->number, "string contains a NUL byte");

        return MND_PO_LEX_ERROR;
    }

    line->kind = kind;
    line->text.data = s;
    line->text.len = len;

    return MND_PO_LEX_OK;
}

/* s[0..len)（行末の改行と CR を除いた 1 行）を分類する */
static mnd_po_lex_status_t mnd_po_read_lex(mnd_po_reader_t *reader, const char *s, size_t len,
                                           mnd_po_line_t *line)
{
    size_t i = 0;

    line->obsolete = false;
    line->previous = false;
    line->text.data = NULL;
    line->text.len = 0;
    mnd_buf_clear(&line->value);

    while ((i < len) && mnd_po_read_is_blank(s[i]))
    {
        i++;
    }

    s += i;
    len -= i;
    if (len == 0)
    {
        line->kind = MND_PO_LINE_BLANK;

        return MND_PO_LEX_OK;
    }

    if (s[0] != '#')
    {
        return mnd_po_read_lex_code(reader, s, len, line);
    }

    if (mnd_po_read_starts_with(s, len, "#~|"))
    {
        line->obsolete = true;
        line->previous = true;

        return mnd_po_read_lex_marked(reader, s + 3, len - 3, line);
    }

    if (mnd_po_read_starts_with(s, len, "#~"))
    {
        line->obsolete = true;

        return mnd_po_read_lex_marked(reader, s + 2, len - 2, line);
    }

    if (mnd_po_read_starts_with(s, len, "#|"))
    {
        line->previous = true;

        return mnd_po_read_lex_marked(reader, s + 2, len - 2, line);
    }

    if (mnd_po_read_starts_with(s, len, "#."))
    {
        size_t skip = ((len > 2) && (s[2] == ' ')) ? 3 : 2;

        return mnd_po_read_lex_comment(reader, s + skip, len - skip, MND_PO_LINE_EXTRACTED,
                                       line);
    }

    if (mnd_po_read_starts_with(s, len, "#:"))
    {
        return mnd_po_read_lex_comment(reader, s + 2, len - 2, MND_PO_LINE_REFERENCE, line);
    }

    if (mnd_po_read_starts_with(s, len, "#,"))
    {
        return mnd_po_read_lex_comment(reader, s + 2, len - 2, MND_PO_LINE_FLAGS, line);
    }

    /* "#" の後ろが空白なら 1 つ除く。そのほかの文字なら "#" の後ろの全部 */
    {
        size_t skip = ((len > 1) && (s[1] == ' ')) ? 2 : 1;

        return mnd_po_read_lex_comment(reader, s + skip, len - skip, MND_PO_LINE_TRANSLATOR,
                                       line);
    }
}

/*
 * text[0..len) から name_start 以降の最後の ':' を探し、その後ろが 1 文字以上の 10 進数で、
 * 値が 1 以上 SIZE_MAX 以下なら true を返す。*colon に ':' の位置、*number に値を入れる
 */
static bool mnd_po_read_ref_line(const char *text, size_t len, size_t name_start, size_t *colon,
                                 size_t *number)
{
    size_t i = len;
    size_t value = 0;

    while ((i > name_start) && (text[i - 1] != ':'))
    {
        i--;
    }

    if ((i == name_start) || (i == len))
    {
        return false;
    }

    *colon = i - 1;
    for (; i < len; i++)
    {
        size_t digit;

        if (!mnd_po_read_is_digit(text[i]))
        {
            return false;
        }

        digit = (size_t)(text[i] - '0');
        if (value > ((SIZE_MAX - digit) / 10))
        {
            return false;
        }

        value = (value * 10) + digit;
    }

    if (value == 0)
    {
        return false;
    }

    *number = value;

    return true;
}

/* s[from..len) から U+2069 を探し、その位置を返す。なければ len */
static size_t mnd_po_read_find_pdi(const char *s, size_t len, size_t from)
{
    size_t i;

    for (i = from; (i + MND_PO_READ_ISOLATE_LEN) <= len; i++)
    {
        if (memcmp(s + i, MND_PO_READ_PDI, MND_PO_READ_ISOLATE_LEN) == 0)
        {
            return i;
        }
    }

    return len;
}

/*
 * 参照の列 s[0..len) を空白とタブで区切って entry に加える。U+2068 で始まる語は U+2069 までを
 * ファイル名とし（U+2068 と U+2069 は除く）、行番号はその後ろの部分からだけ探す。
 * U+2069 がなければ、U+2068 を除いた語を通常の語として扱う
 */
static bool mnd_po_read_refs(const char *s, size_t len, mnd_po_entry_t *entry)
{
    mnd_buf_t file = MND_BUF_INIT;
    size_t i = 0;
    bool ok = true;

    while (ok)
    {
        size_t start;
        size_t name_start = 0;
        size_t colon = 0;
        size_t number = 0;

        while ((i < len) && mnd_po_read_is_blank(s[i]))
        {
            i++;
        }

        if (i >= len)
        {
            break;
        }

        mnd_buf_clear(&file);
        start = i;
        if (mnd_po_read_starts_with(s + i, len - i, MND_PO_READ_FSI))
        {
            size_t inner = i + MND_PO_READ_ISOLATE_LEN;
            size_t close = mnd_po_read_find_pdi(s, len, inner);

            if (close < len)
            {
                ok = mnd_buf_append(&file, s + inner, close - inner);
                name_start = file.len;
                start = close + MND_PO_READ_ISOLATE_LEN;
            }
            else
            {
                start = inner;
            }
        }

        i = start;
        while ((i < len) && !mnd_po_read_is_blank(s[i]))
        {
            i++;
        }

        ok = ok && mnd_buf_append(&file, s + start, i - start);
        if (!ok)
        {
            break;
        }

        if (mnd_po_read_ref_line(mnd_buf_cstr(&file), file.len, name_start, &colon, &number))
        {
            file.data[colon] = '\0';
            file.len = colon;
        }
        else
        {
            number = 0;
        }

        ok = mnd_po_refs_push(&entry->refs, mnd_buf_cstr(&file), number);
    }

    mnd_buf_free(&file);

    return ok;
}

/* フラグの列 s[0..len) をカンマで区切り、前後の空白を除いて entry に加える。空の項目は捨てる */
static bool mnd_po_read_flags(const char *s, size_t len, mnd_po_entry_t *entry)
{
    mnd_buf_t flag = MND_BUF_INIT;
    size_t i = 0;
    bool ok = true;

    while (ok && (i <= len))
    {
        size_t start = i;
        size_t end;

        while ((i < len) && (s[i] != ','))
        {
            i++;
        }

        end = i;
        while ((start < end) && mnd_po_read_is_blank(s[start]))
        {
            start++;
        }

        while ((end > start) && mnd_po_read_is_blank(s[end - 1]))
        {
            end--;
        }

        if (end > start)
        {
            mnd_buf_clear(&flag);
            ok = mnd_buf_append(&flag, s + start, end - start)
                 && mnd_po_flag_add(entry, mnd_buf_cstr(&flag));
        }

        i++;
    }

    mnd_buf_free(&flag);

    return ok;
}

/* 組み立て中のエントリを捨て、新しいエントリを始められる状態にする */
static void mnd_po_read_reset_entry(mnd_po_reader_t *reader)
{
    mnd_po_entry_free(&reader->entry);
    reader->entry_started = false;
    reader->has_keyword = false;
    reader->has_msgstr = false;
    reader->target = NULL;
}

/* 組み立て中のエントリをカタログの末尾に移す */
static bool mnd_po_read_finish_entry(mnd_po_reader_t *reader)
{
    mnd_po_entry_t *dst = mnd_po_catalog_append(reader->out);

    if (dst == NULL)
    {
        return false;
    }

    *dst = reader->entry;
    mnd_po_entry_init(&reader->entry);
    mnd_po_read_reset_entry(reader);

    return true;
}

/* エントリの最初の行なら開始行を記録する */
static void mnd_po_read_start_entry(mnd_po_reader_t *reader, size_t number)
{
    if (!reader->entry_started)
    {
        reader->entry.line = number;
        reader->entry_started = true;
    }
}

/* buf を value の内容で置き換える */
static bool mnd_po_read_set(mnd_buf_t *buf, const mnd_buf_t *value)
{
    mnd_buf_clear(buf);

    return mnd_buf_append(buf, value->data, value->len);
}

/* 前回の msgctxt・msgid・msgid_plural の行をエントリに付ける */
static bool mnd_po_read_add_previous(mnd_po_reader_t *reader, const mnd_po_line_t *line)
{
    mnd_po_entry_t *entry = &reader->entry;
    mnd_buf_t *field;

    switch (line->keyword)
    {
    case MND_PO_KEYWORD_MSGCTXT:
        entry->has_prev_msgctxt = true;
        field = &entry->prev_msgctxt;
        break;
    case MND_PO_KEYWORD_MSGID:
        entry->has_prev_msgid = true;
        field = &entry->prev_msgid;
        break;
    case MND_PO_KEYWORD_MSGID_PLURAL:
    case MND_PO_KEYWORD_MSGSTR:
    case MND_PO_KEYWORD_MSGSTR_INDEXED:
    default:
        entry->has_prev_msgid_plural = true;
        field = &entry->prev_msgid_plural;
        break;
    }

    reader->target = field;

    return mnd_po_read_set(field, &line->value);
}

/* キーワードの行をエントリに付ける */
static bool mnd_po_read_add_keyword(mnd_po_reader_t *reader, const mnd_po_line_t *line)
{
    mnd_po_entry_t *entry = &reader->entry;
    mnd_buf_t *field;

    if (!reader->has_keyword)
    {
        entry->obsolete = line->obsolete;
        reader->has_keyword = true;
    }

    switch (line->keyword)
    {
    case MND_PO_KEYWORD_MSGCTXT:
        entry->has_msgctxt = true;
        field = &entry->msgctxt;
        break;
    case MND_PO_KEYWORD_MSGID:
        field = &entry->msgid;
        break;
    case MND_PO_KEYWORD_MSGID_PLURAL:
        entry->has_msgid_plural = true;
        field = &entry->msgid_plural;
        break;
    case MND_PO_KEYWORD_MSGSTR:
    case MND_PO_KEYWORD_MSGSTR_INDEXED:
    default:
        if (!mnd_po_strings_push(&entry->msgstr, line->value.data, line->value.len))
        {
            return false;
        }

        reader->has_msgstr = true;
        reader->target = &entry->msgstr.items[entry->msgstr.count - 1];

        return true;
    }

    reader->target = field;

    return mnd_po_read_set(field, &line->value);
}

/*
 * 分類した行をエントリに付ける。msgstr の後ろに空行、コメント、前回の文字列、msgctxt、msgid が
 * 現れたら、エントリを完成させてカタログに加える。メモリ不足なら false
 */
static bool mnd_po_read_assemble(mnd_po_reader_t *reader, const mnd_po_line_t *line)
{
    bool starts_entry;

    switch (line->kind)
    {
    case MND_PO_LINE_BLANK:
        return !reader->has_msgstr || mnd_po_read_finish_entry(reader);
    case MND_PO_LINE_IGNORED:
        return true;
    case MND_PO_LINE_STRING:
        return (reader->target == NULL)
               || mnd_buf_append(reader->target, line->value.data, line->value.len);
    case MND_PO_LINE_KEYWORD:
        starts_entry = line->previous || (line->keyword == MND_PO_KEYWORD_MSGCTXT)
                       || (line->keyword == MND_PO_KEYWORD_MSGID);
        break;
    case MND_PO_LINE_TRANSLATOR:
    case MND_PO_LINE_EXTRACTED:
    case MND_PO_LINE_REFERENCE:
    case MND_PO_LINE_FLAGS:
    default:
        starts_entry = true;
        break;
    }

    if (starts_entry && reader->has_msgstr && !mnd_po_read_finish_entry(reader))
    {
        return false;
    }

    mnd_po_read_start_entry(reader, line->number);
    switch (line->kind)
    {
    case MND_PO_LINE_TRANSLATOR:
        reader->target = NULL;

        return mnd_po_strings_push(&reader->entry.translator_comments, line->text.data,
                                   line->text.len);
    case MND_PO_LINE_EXTRACTED:
        reader->target = NULL;

        return mnd_po_strings_push(&reader->entry.extracted_comments, line->text.data,
                                   line->text.len);
    case MND_PO_LINE_REFERENCE:
        reader->target = NULL;

        return mnd_po_read_refs(line->text.data, line->text.len, &reader->entry);
    case MND_PO_LINE_FLAGS:
        reader->target = NULL;

        return mnd_po_read_flags(line->text.data, line->text.len, &reader->entry);
    case MND_PO_LINE_KEYWORD:
        if (line->previous)
        {
            return mnd_po_read_add_previous(reader, line);
        }

        return mnd_po_read_add_keyword(reader, line);
    case MND_PO_LINE_BLANK:
    case MND_PO_LINE_IGNORED:
    case MND_PO_LINE_STRING:
    default:
        break;
    }

    return true;
}

/* 前処理を終えた内容 data[0..len) を行に分けて解析する。メモリ不足なら false */
static bool mnd_po_read_lines(mnd_po_reader_t *reader, const char *data, size_t len)
{
    mnd_po_line_t line;
    size_t pos = 0;
    size_t number = 1;
    bool ok = true;

    mnd_buf_init(&line.value);
    while (ok && (pos < len))
    {
        const char *newline = memchr(data + pos, '\n', len - pos);
        size_t line_end = (newline != NULL) ? (size_t)(newline - data) : len;
        size_t text_len = line_end - pos;
        mnd_po_lex_status_t status;

        if ((text_len > 0) && (data[line_end - 1] == '\r'))
        {
            text_len--;
        }

        line.number = number;
        status = mnd_po_read_lex(reader, data + pos, text_len, &line);
        if (status == MND_PO_LEX_OK)
        {
            ok = mnd_po_read_assemble(reader, &line);
        }
        else if (status == MND_PO_LEX_NO_MEMORY)
        {
            ok = false;
        }

        pos = line_end + 1;
        number++;
    }

    /* 内容の終わりでエントリが完成する。msgstr のないコメントだけの並びは捨てる */
    if (ok && reader->has_msgstr)
    {
        ok = mnd_po_read_finish_entry(reader);
    }

    mnd_buf_free(&line.value);

    return ok;
}

bool mnd_po_read(const char *data, size_t len, const char *path,
                 mnd_po_catalog_t *out, mnd_diag_list_t *diags)
{
    mnd_po_reader_t reader;
    size_t invalid_offset;

    reader.path = path;
    reader.diags = diags;
    reader.out = out;
    reader.failed = false;
    mnd_po_entry_init(&reader.entry);
    reader.entry_started = false;
    reader.has_keyword = false;
    reader.has_msgstr = false;
    reader.target = NULL;

    /* 前処理: BOM を飛ばし、内容全体の UTF-8 を検査する（不正なら解析しない） */
    if (mnd_po_read_starts_with(data, len, "\xEF\xBB\xBF"))
    {
        data += 3;
        len -= 3;
    }

    if (!mnd_utf8_validate(data, len, &invalid_offset))
    {
        size_t line = 0;

        (void)mnd_source_line_of(data, len, invalid_offset, &line);
        mnd_po_read_error(&reader, line, "invalid UTF-8 sequence");

        return false;
    }

    if (!mnd_po_read_lines(&reader, data, len))
    {
        mnd_po_read_no_memory(&reader);
    }

    mnd_po_read_reset_entry(&reader);
    if (reader.failed)
    {
        mnd_po_catalog_free(out);
        mnd_po_catalog_init(out);

        return false;
    }

    return true;
}

