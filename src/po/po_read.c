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

/* エントリの組み立ての状態（設計の「エントリの組み立て（状態機械）」の状態図） */
typedef enum mnd_po_read_state
{
    MND_PO_READ_STATE_COMMENTS,   /* コメントと前回の文字列だけ（またはまだ何もない） */
    MND_PO_READ_STATE_CONTEXT,    /* msgctxt の後 */
    MND_PO_READ_STATE_ID,         /* msgid の後 */
    MND_PO_READ_STATE_PLURAL,     /* msgid_plural の後 */
    MND_PO_READ_STATE_SINGLE,     /* msgstr の後（エントリは完成できる） */
    MND_PO_READ_STATE_PLURAL_STR  /* msgstr[n] の後（エントリは完成できる） */
} mnd_po_read_state_t;

/* 状態機械が受け付けない遷移の種類 */
typedef enum mnd_po_read_fault
{
    MND_PO_READ_FAULT_MISSING_MSGID,
    MND_PO_READ_FAULT_MISSING_MSGSTR,
    MND_PO_READ_FAULT_FORM_MISMATCH,  /* 単数形と複数形の不一致 */
    MND_PO_READ_FAULT_INDEX_ORDER,    /* msgstr[n] の添字の順序 */
    MND_PO_READ_FAULT_MIXED           /* 廃止の行と通常の行の混在 */
} mnd_po_read_fault_t;

typedef struct mnd_po_reader
{
    const char *path;
    mnd_diag_list_t *diags;
    mnd_po_catalog_t *out;
    size_t errors;               /* 報告したエラーの数 */
    bool recovering;             /* エラーの後、再開できる行を探している */
    mnd_po_read_state_t state;
    mnd_po_entry_t entry;        /* 組み立て中のエントリ */
    bool entry_started;          /* entry にコメントかキーワードがある */
    bool has_active_previous;    /* entry に #| の行がある */
    bool has_obsolete_previous;  /* entry に #~| の行がある */
    mnd_buf_t *target;           /* 続きの文字列を連結する先。なければ NULL */
    bool target_previous;        /* target は前回の文字列（#| または #~|） */
    bool target_obsolete;        /* target の行は #~ 付き */
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
    reader->errors++;
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
    else if (lead < 0xE0)
    {
        return 2;
    }
    else if (lead < 0xF0)
    {
        return 3;
    }
    else
    {
        return 4;
    }
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
        reader->errors++;

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
        reader->errors++;

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
        reader->errors++;

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
    else if (mnd_po_read_starts_with(s, len, "#~"))
    {
        line->obsolete = true;

        return mnd_po_read_lex_marked(reader, s + 2, len - 2, line);
    }
    else if (mnd_po_read_starts_with(s, len, "#|"))
    {
        line->previous = true;

        return mnd_po_read_lex_marked(reader, s + 2, len - 2, line);
    }
    else if (mnd_po_read_starts_with(s, len, "#."))
    {
        size_t skip = ((len > 2) && (s[2] == ' ')) ? 3 : 2;

        return mnd_po_read_lex_comment(reader, s + skip, len - skip, MND_PO_LINE_EXTRACTED,
                                       line);
    }
    else if (mnd_po_read_starts_with(s, len, "#:"))
    {
        return mnd_po_read_lex_comment(reader, s + 2, len - 2, MND_PO_LINE_REFERENCE, line);
    }
    else if (mnd_po_read_starts_with(s, len, "#,"))
    {
        return mnd_po_read_lex_comment(reader, s + 2, len - 2, MND_PO_LINE_FLAGS, line);
    }
    else
    {
        /* "#" の後ろが空白なら 1 つ除く。そのほかの文字なら "#" の後ろの全部 */
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
    reader->state = MND_PO_READ_STATE_COMMENTS;
    reader->entry_started = false;
    reader->has_active_previous = false;
    reader->has_obsolete_previous = false;
    reader->target = NULL;
    reader->target_previous = false;
    reader->target_obsolete = false;
}

/* エントリの msgstr があり、エントリを完成できる状態か */
static bool mnd_po_read_is_complete(mnd_po_read_state_t state)
{
    return (state == MND_PO_READ_STATE_SINGLE) || (state == MND_PO_READ_STATE_PLURAL_STR);
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

/*
 * 状態機械が受け付けない遷移。エラーを報告し、組み立て中のエントリを捨てる。
 * expected は MND_PO_READ_FAULT_INDEX_ORDER のときに期待した添字
 */
static void mnd_po_read_invalid(mnd_po_reader_t *reader, size_t line, mnd_po_read_fault_t fault,
                                size_t expected)
{
    switch (fault)
    {
    case MND_PO_READ_FAULT_MISSING_MSGID:
        mnd_po_read_error(reader, line, "missing msgid");
        break;
    case MND_PO_READ_FAULT_MISSING_MSGSTR:
        mnd_po_read_error(reader, line, "missing msgstr");
        break;
    case MND_PO_READ_FAULT_FORM_MISMATCH:
        mnd_po_read_error(reader, line, "msgstr form does not match msgid_plural");
        break;
    case MND_PO_READ_FAULT_INDEX_ORDER:
        mnd_diag_add(reader->diags, MND_SEVERITY_ERROR, reader->path, line,
                     "msgstr index out of order: expected [%zu]", expected);
        reader->errors++;
        break;
    case MND_PO_READ_FAULT_MIXED:
    default:
        mnd_po_read_error(reader, line, "obsolete and active lines mixed in one entry");
        break;
    }

    mnd_po_read_reset_entry(reader);
}

/*
 * msgstr のないまま、エントリを続けられない行（空行、コメント、前回の文字列）か内容の終わりが
 * 現れたときの欠落の種類。Context の後なら msgid、Id と Plural の後なら msgstr が欠けている
 */
static mnd_po_read_fault_t mnd_po_read_missing_fault(mnd_po_read_state_t state)
{
    return (state == MND_PO_READ_STATE_CONTEXT) ? MND_PO_READ_FAULT_MISSING_MSGID
                                                : MND_PO_READ_FAULT_MISSING_MSGSTR;
}

/*
 * エントリの外（Comments の状態）にしか現れない行の前処理。完成できるエントリはカタログに加え、
 * msgstr のないエントリの途中なら、その欠落を報告する。行を Comments の状態で受け付けるなら
 * *accept を真にする。メモリ不足なら false
 */
static bool mnd_po_read_leave_entry(mnd_po_reader_t *reader, size_t line, bool *accept)
{
    *accept = false;
    if (mnd_po_read_is_complete(reader->state))
    {
        if (!mnd_po_read_finish_entry(reader))
        {
            return false;
        }
    }
    else if (reader->state != MND_PO_READ_STATE_COMMENTS)
    {
        mnd_po_read_invalid(reader, line, mnd_po_read_missing_fault(reader->state), 0);

        return true;
    }

    *accept = true;

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

/* 続きの文字列の連結先を設定する */
static void mnd_po_read_set_target(mnd_po_reader_t *reader, mnd_buf_t *target,
                                   const mnd_po_line_t *line)
{
    reader->target = target;
    reader->target_previous = line->previous;
    reader->target_obsolete = line->obsolete;
}

/* buf を value の内容で置き換える */
static bool mnd_po_read_set(mnd_buf_t *buf, const mnd_buf_t *value)
{
    mnd_buf_clear(buf);

    return mnd_buf_append(buf, value->data, value->len);
}

/* コメントの行（#、#.、#:、#,）を Comments の状態のエントリに付ける */
static bool mnd_po_read_add_comment(mnd_po_reader_t *reader, const mnd_po_line_t *line)
{
    mnd_po_entry_t *entry = &reader->entry;

    mnd_po_read_start_entry(reader, line->number);
    reader->target = NULL;
    switch (line->kind)
    {
    case MND_PO_LINE_TRANSLATOR:
        return mnd_po_strings_push(&entry->translator_comments, line->text.data, line->text.len);
    case MND_PO_LINE_EXTRACTED:
        return mnd_po_strings_push(&entry->extracted_comments, line->text.data, line->text.len);
    case MND_PO_LINE_REFERENCE:
        return mnd_po_read_refs(line->text.data, line->text.len, entry);
    case MND_PO_LINE_FLAGS:
    default:
        break;
    }

    return mnd_po_read_flags(line->text.data, line->text.len, entry);
}

/* 前回の msgctxt・msgid・msgid_plural の行（#| または #~|）を Comments の状態のエントリに付ける */
static bool mnd_po_read_add_previous(mnd_po_reader_t *reader, const mnd_po_line_t *line)
{
    mnd_po_entry_t *entry = &reader->entry;
    mnd_buf_t *field;

    mnd_po_read_start_entry(reader, line->number);
    if (line->obsolete)
    {
        reader->has_obsolete_previous = true;
    }
    else
    {
        reader->has_active_previous = true;
    }

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

    mnd_po_read_set_target(reader, field, line);

    return mnd_po_read_set(field, &line->value);
}

/*
 * キーワードの行について、現在の状態からの遷移先を決める。受け付けない遷移なら false を返し、
 * *fault（と添字の順序の誤りなら *expected）に種類を入れる
 */
static bool mnd_po_read_keyword_transition(const mnd_po_reader_t *reader,
                                           const mnd_po_line_t *line,
                                           mnd_po_read_state_t *next,
                                           mnd_po_read_fault_t *fault, size_t *expected)
{
    mnd_po_read_state_t state = reader->state;
    bool before_id = (state == MND_PO_READ_STATE_COMMENTS) || (state == MND_PO_READ_STATE_CONTEXT);

    *expected = 0;
    switch (line->keyword)
    {
    case MND_PO_KEYWORD_MSGCTXT:
        *next = MND_PO_READ_STATE_CONTEXT;
        if (state != MND_PO_READ_STATE_COMMENTS)
        {
            *fault = mnd_po_read_missing_fault(state);

            return false;
        }

        return true;
    case MND_PO_KEYWORD_MSGID:
        *next = MND_PO_READ_STATE_ID;
        if (!before_id)
        {
            *fault = MND_PO_READ_FAULT_MISSING_MSGSTR;

            return false;
        }

        return true;
    case MND_PO_KEYWORD_MSGID_PLURAL:
        *next = MND_PO_READ_STATE_PLURAL;
        if (state == MND_PO_READ_STATE_ID)
        {
            return true;
        }

        if (before_id)
        {
            *fault = MND_PO_READ_FAULT_MISSING_MSGID;
        }
        else if (state == MND_PO_READ_STATE_PLURAL)
        {
            *fault = MND_PO_READ_FAULT_MISSING_MSGSTR;
        }
        else
        {
            *fault = MND_PO_READ_FAULT_FORM_MISMATCH;
        }

        return false;
    case MND_PO_KEYWORD_MSGSTR:
        *next = MND_PO_READ_STATE_SINGLE;
        if (state == MND_PO_READ_STATE_ID)
        {
            return true;
        }

        if (before_id || (state == MND_PO_READ_STATE_SINGLE))
        {
            *fault = MND_PO_READ_FAULT_MISSING_MSGID;
        }
        else
        {
            *fault = MND_PO_READ_FAULT_FORM_MISMATCH;
        }

        return false;
    case MND_PO_KEYWORD_MSGSTR_INDEXED:
    default:
        break;
    }

    *next = MND_PO_READ_STATE_PLURAL_STR;
    if (before_id)
    {
        *fault = MND_PO_READ_FAULT_MISSING_MSGID;

        return false;
    }

    if ((state == MND_PO_READ_STATE_ID) || (state == MND_PO_READ_STATE_SINGLE))
    {
        *fault = MND_PO_READ_FAULT_FORM_MISMATCH;

        return false;
    }

    *expected = (state == MND_PO_READ_STATE_PLURAL) ? 0 : reader->entry.msgstr.count;
    if (line->index != *expected)
    {
        *fault = MND_PO_READ_FAULT_INDEX_ORDER;

        return false;
    }

    return true;
}

/* キーワードの行（msgctxt、msgid、msgid_plural、msgstr、msgstr[n]）で状態を進める */
static bool mnd_po_read_step_keyword(mnd_po_reader_t *reader, const mnd_po_line_t *line)
{
    mnd_po_entry_t *entry = &reader->entry;
    mnd_po_read_state_t next = MND_PO_READ_STATE_COMMENTS;
    mnd_po_read_fault_t fault = MND_PO_READ_FAULT_MISSING_MSGID;
    size_t expected = 0;
    mnd_buf_t *field;

    /* 完成できるエントリの後の msgctxt と msgid は、次のエントリを始める */
    if (mnd_po_read_is_complete(reader->state)
        && ((line->keyword == MND_PO_KEYWORD_MSGCTXT) || (line->keyword == MND_PO_KEYWORD_MSGID))
        && !mnd_po_read_finish_entry(reader))
    {
        return false;
    }

    if (reader->state == MND_PO_READ_STATE_COMMENTS)
    {
        /* 最初のキーワードの行が、廃止エントリかどうかと、受け付ける前回の文字列を決める */
        mnd_po_read_start_entry(reader, line->number);
        entry->obsolete = line->obsolete;
        if ((line->obsolete && reader->has_active_previous)
            || (!line->obsolete && reader->has_obsolete_previous))
        {
            mnd_po_read_invalid(reader, line->number, MND_PO_READ_FAULT_MIXED, 0);

            return true;
        }
    }
    else if (line->obsolete != entry->obsolete)
    {
        mnd_po_read_invalid(reader, line->number, MND_PO_READ_FAULT_MIXED, 0);

        return true;
    }

    if (!mnd_po_read_keyword_transition(reader, line, &next, &fault, &expected))
    {
        mnd_po_read_invalid(reader, line->number, fault, expected);

        return true;
    }

    reader->state = next;
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

        mnd_po_read_set_target(reader, &entry->msgstr.items[entry->msgstr.count - 1], line);

        return true;
    }

    mnd_po_read_set_target(reader, field, line);

    return mnd_po_read_set(field, &line->value);
}

/*
 * 続きの文字列の行を、直前のキーワードまたは前回の文字列に連結する。
 * #| 付きの文字列は前回の文字列に、そのほかの文字列はキーワードの文字列にだけ連結でき、
 * #~ の有無も連結先と一致しなければならない
 */
static bool mnd_po_read_step_string(mnd_po_reader_t *reader, const mnd_po_line_t *line)
{
    if (line->previous)
    {
        bool accept;

        if (!mnd_po_read_leave_entry(reader, line->number, &accept))
        {
            return false;
        }

        if (!accept)
        {
            return true;
        }
    }
    else if (reader->state == MND_PO_READ_STATE_COMMENTS)
    {
        mnd_po_read_invalid(reader, line->number, MND_PO_READ_FAULT_MISSING_MSGID, 0);

        return true;
    }

    if ((reader->target == NULL) || (reader->target_previous != line->previous))
    {
        mnd_po_read_invalid(reader, line->number, MND_PO_READ_FAULT_MISSING_MSGID, 0);

        return true;
    }

    if (reader->target_obsolete != line->obsolete)
    {
        mnd_po_read_invalid(reader, line->number, MND_PO_READ_FAULT_MIXED, 0);

        return true;
    }

    return mnd_buf_append(reader->target, line->value.data, line->value.len);
}

/* 分類した 1 行で状態機械を進める。メモリ不足なら false */
static bool mnd_po_read_step(mnd_po_reader_t *reader, const mnd_po_line_t *line)
{
    bool accept;

    switch (line->kind)
    {
    case MND_PO_LINE_IGNORED:
        return true;
    case MND_PO_LINE_BLANK:
        /* コメントだけの並びの後の空行は、エントリの区切りにしない */
        return mnd_po_read_leave_entry(reader, line->number, &accept);
    case MND_PO_LINE_STRING:
        return mnd_po_read_step_string(reader, line);
    case MND_PO_LINE_KEYWORD:
        if (!line->previous)
        {
            return mnd_po_read_step_keyword(reader, line);
        }

        break;
    case MND_PO_LINE_TRANSLATOR:
    case MND_PO_LINE_EXTRACTED:
    case MND_PO_LINE_REFERENCE:
    case MND_PO_LINE_FLAGS:
    default:
        break;
    }

    /* コメントの行と前回の文字列の行は、Comments の状態でだけ受け付ける */
    if (!mnd_po_read_leave_entry(reader, line->number, &accept))
    {
        return false;
    }

    if (!accept)
    {
        return true;
    }

    if (line->kind == MND_PO_LINE_KEYWORD)
    {
        return mnd_po_read_add_previous(reader, line);
    }

    return mnd_po_read_add_comment(reader, line);
}

/*
 * 内容の終わりでエントリを完成させる。コメントだけの並びは捨て、msgstr のないエントリの途中なら
 * 最後の行 last_line で欠落を報告する。メモリ不足なら false
 */
static bool mnd_po_read_finish(mnd_po_reader_t *reader, size_t last_line)
{
    if (mnd_po_read_is_complete(reader->state))
    {
        return mnd_po_read_finish_entry(reader);
    }

    if (reader->state != MND_PO_READ_STATE_COMMENTS)
    {
        mnd_po_read_invalid(reader, last_line, mnd_po_read_missing_fault(reader->state), 0);
    }

    return true;
}

/* s[0..len) の先頭の語（空白とタブまで）が msgctxt か msgid なら true */
static bool mnd_po_read_starts_entry_keyword(const char *s, size_t len)
{
    size_t word_len = 0;

    while ((word_len < len) && !mnd_po_read_is_blank(s[word_len]))
    {
        word_len++;
    }

    return ((word_len == 7) && (memcmp(s, "msgctxt", 7) == 0))
           || ((word_len == 5) && (memcmp(s, "msgid", 5) == 0));
}

/* s[0..len) の先頭の空白とタブを除いた位置を返す */
static size_t mnd_po_read_skip_blanks(const char *s, size_t len)
{
    size_t i = 0;

    while ((i < len) && mnd_po_read_is_blank(s[i]))
    {
        i++;
    }

    return i;
}

/*
 * エラーからの回復で、解析を再開できる行（空行、コメントの行、msgctxt、msgid）か。
 * 行を字句解析せずに先頭だけで判定する（読み飛ばす行のエラーを報告しないため）。
 * 前回の文字列の行（#|、#~|）はコメントの行として扱い、残りが空の #~、#|、#~| は再開の行としない
 */
static bool mnd_po_read_is_resync(const char *s, size_t len)
{
    size_t i = mnd_po_read_skip_blanks(s, len);

    s += i;
    len -= i;
    if (len == 0)
    {
        return true;
    }

    if (s[0] != '#')
    {
        return mnd_po_read_starts_entry_keyword(s, len);
    }

    if (mnd_po_read_starts_with(s, len, "#~|") || mnd_po_read_starts_with(s, len, "#|"))
    {
        i = (s[1] == '~') ? 3 : 2;

        return mnd_po_read_skip_blanks(s + i, len - i) < (len - i);
    }

    if (mnd_po_read_starts_with(s, len, "#~"))
    {
        i = 2 + mnd_po_read_skip_blanks(s + 2, len - 2);

        return mnd_po_read_starts_entry_keyword(s + i, len - i);
    }

    return true;
}

/*
 * 前処理を終えた内容 data[0..len) を行に分けて解析する。エラーを報告したら組み立て中のエントリを
 * 捨て、再開できる行まで読み飛ばして（その間のエラーは報告しない）、最後まで解析を続ける。
 * 誤りを見つけた行そのものが再開できる行で、字句の誤りがなければ、その行から再開する。
 * メモリ不足なら false
 */
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
        size_t errors = reader->errors;

        if ((text_len > 0) && (data[line_end - 1] == '\r'))
        {
            text_len--;
        }

        if (!reader->recovering || mnd_po_read_is_resync(data + pos, text_len))
        {
            mnd_po_lex_status_t status;

            reader->recovering = false;
            line.number = number;
            status = mnd_po_read_lex(reader, data + pos, text_len, &line);
            if (status == MND_PO_LEX_OK)
            {
                ok = mnd_po_read_step(reader, &line);

                /*
                 * 状態機械の誤りを見つけた行が次のエントリの始まりになる行なら、前のエントリだけを
                 * 捨て、同じ行から新しいエントリを始める（次のエントリの誤りも報告するため）。
                 * Comments の状態からもう一度誤りになる場合は、1 回だけで回復に移る
                 */
                if (ok && (reader->errors > errors) && mnd_po_read_is_resync(data + pos, text_len))
                {
                    mnd_po_read_reset_entry(reader);
                    errors = reader->errors;
                    ok = mnd_po_read_step(reader, &line);
                }
            }
            else if (status == MND_PO_LEX_NO_MEMORY)
            {
                ok = false;
            }

            if (reader->errors > errors)
            {
                mnd_po_read_reset_entry(reader);
                reader->recovering = true;
            }
        }

        pos = line_end + 1;
        number++;
    }

    if (ok)
    {
        ok = mnd_po_read_finish(reader, number - 1);
    }

    mnd_buf_free(&line.value);

    return ok;
}

/* 重複（4.7）: 廃止エントリでないエントリの msgctxt と msgid の組の重複をすべて報告する */
static void mnd_po_read_check_duplicates(mnd_po_reader_t *reader)
{
    const mnd_po_catalog_t *catalog = reader->out;
    size_t from = 0;
    size_t first;
    size_t second;

    while (mnd_po_find_duplicate(catalog, from, &first, &second))
    {
        mnd_diag_add(reader->diags, MND_SEVERITY_ERROR, reader->path,
                     catalog->entries[second].line,
                     "duplicate message definition (first defined at line %zu)",
                     catalog->entries[first].line);
        reader->errors++;
        from = second + 1;
    }
}

/* a[0..len) と NUL 終端の b が、ASCII の大文字と小文字を区別せずに等しいか */
static bool mnd_po_read_equals_nocase(const char *a, size_t len, const char *b)
{
    size_t i;

    if (strlen(b) != len)
    {
        return false;
    }

    for (i = 0; i < len; i++)
    {
        char x = a[i];
        char y = b[i];

        if ((x >= 'a') && (x <= 'z'))
        {
            x = (char)(x - ('a' - 'A'));
        }

        if ((y >= 'a') && (y <= 'z'))
        {
            y = (char)(y - ('a' - 'A'));
        }

        if (x != y)
        {
            return false;
        }
    }

    return true;
}

/*
 * charset（4.9）: ヘッダの Content-Type の charset= の値（';' か行末まで、前後の空白を除く）が
 * UTF-8 でも CHARSET（未設定）でもなければ報告する。大文字と小文字は区別しない
 */
static void mnd_po_read_check_charset(mnd_po_reader_t *reader)
{
    static const char key[] = "charset=";
    const size_t key_len = sizeof(key) - 1;
    char shown[MND_PO_READ_SHOW_MAX + 1];
    const mnd_po_entry_t *header;
    mnd_slice_t value;
    size_t index;
    size_t i;
    size_t start;
    size_t end;

    if (!mnd_po_catalog_header(reader->out, &index))
    {
        return;
    }

    header = &reader->out->entries[index];
    if (!mnd_po_header_get(header, "Content-Type", &value))
    {
        return;
    }

    for (i = 0; (i + key_len) <= value.len; i++)
    {
        if (mnd_po_read_equals_nocase(value.data + i, key_len, key))
        {
            break;
        }
    }

    if ((i + key_len) > value.len)
    {
        return;
    }

    start = i + key_len;
    end = start;
    while ((end < value.len) && (value.data[end] != ';'))
    {
        end++;
    }

    while ((start < end) && mnd_po_read_is_blank(value.data[start]))
    {
        start++;
    }

    while ((end > start) && mnd_po_read_is_blank(value.data[end - 1]))
    {
        end--;
    }

    if (mnd_po_read_equals_nocase(value.data + start, end - start, "UTF-8")
        || mnd_po_read_equals_nocase(value.data + start, end - start, "CHARSET"))
    {
        return;
    }

    mnd_po_read_show(value.data + start, end - start, shown);
    mnd_diag_add(reader->diags, MND_SEVERITY_ERROR, reader->path, header->line,
                 "unsupported charset '%s'; only UTF-8 is supported", shown);
    reader->errors++;
}

/* 改行の不一致を警告する。plural が偽なら msgstr の添字を表示しない */
static void mnd_po_read_warn_newline(mnd_po_reader_t *reader, const mnd_po_entry_t *entry,
                                     bool plural, size_t i, const char *where)
{
    if (plural)
    {
        mnd_diag_add(reader->diags, MND_SEVERITY_WARNING, reader->path, entry->line,
                     "msgid and msgstr[%zu] do not both %s with '\\n'", i, where);
    }
    else
    {
        mnd_diag_add(reader->diags, MND_SEVERITY_WARNING, reader->path, entry->line,
                     "msgid and msgstr do not both %s with '\\n'", where);
    }
}

/*
 * 改行の不一致（4.10）: 翻訳済みで、ヘッダでも廃止エントリでもない各エントリについて、
 * msgid と各 msgstr の先頭と末尾の '\n' の有無が異なれば警告する
 */
static void mnd_po_read_check_newlines(mnd_po_reader_t *reader)
{
    const mnd_po_catalog_t *catalog = reader->out;
    size_t header = SIZE_MAX;
    size_t e;

    if (!mnd_po_catalog_header(catalog, &header))
    {
        header = SIZE_MAX;
    }

    for (e = 0; e < catalog->count; e++)
    {
        const mnd_po_entry_t *entry = &catalog->entries[e];
        const mnd_buf_t *msgid = &entry->msgid;
        bool id_begins;
        bool id_ends;
        size_t i;

        if ((e == header) || entry->obsolete
            || (mnd_po_entry_state(entry) != MND_PO_STATE_TRANSLATED))
        {
            continue;
        }

        id_begins = (msgid->len > 0) && (msgid->data[0] == '\n');
        id_ends = (msgid->len > 0) && (msgid->data[msgid->len - 1] == '\n');
        for (i = 0; i < entry->msgstr.count; i++)
        {
            const mnd_buf_t *msgstr = &entry->msgstr.items[i];
            bool str_begins = (msgstr->len > 0) && (msgstr->data[0] == '\n');
            bool str_ends = (msgstr->len > 0) && (msgstr->data[msgstr->len - 1] == '\n');

            if (id_begins != str_begins)
            {
                mnd_po_read_warn_newline(reader, entry, entry->has_msgid_plural, i, "begin");
            }

            if (id_ends != str_ends)
            {
                mnd_po_read_warn_newline(reader, entry, entry->has_msgid_plural, i, "end");
            }
        }
    }
}

bool mnd_po_read(const char *data, size_t len, const char *path,
                 mnd_po_catalog_t *out, mnd_diag_list_t *diags)
{
    mnd_po_reader_t reader;
    size_t invalid_offset;

    reader.path = path;
    reader.diags = diags;
    reader.out = out;
    reader.errors = 0;
    reader.recovering = false;
    mnd_po_entry_init(&reader.entry);
    mnd_po_read_reset_entry(&reader);

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

    /* 読み込み後の検査は、構文エラーがない場合にだけ行う */
    if (reader.errors == 0)
    {
        mnd_po_read_check_duplicates(&reader);
        mnd_po_read_check_charset(&reader);
        mnd_po_read_check_newlines(&reader);
    }

    if (reader.errors > 0)
    {
        mnd_po_catalog_free(out);
        mnd_po_catalog_init(out);

        return false;
    }

    return true;
}

