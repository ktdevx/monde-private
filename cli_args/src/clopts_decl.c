#include "clopts_internal.h"
#include "clopts_buf.h"

/* DeclCheck: 宣言の妥当性の検査と、子コマンド・有効なオプションの検索と列挙 */

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define DECL_PREFIX "invalid argument declaration: "

/* メッセージ上の名前。NULL は空文字列として示す */
static const char *shown(const char *name)
{
    if (name == NULL)
    {
        return "";
    }

    return name;
}

static bool is_empty(const char *s)
{
    return (s == NULL) || (s[0] == '\0');
}

static bool is_ascii_alnum(char c)
{
    return ((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'));
}

static bool names_equal(const char *a, const char *b)
{
    return (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
}

/* name が name_len バイトの列 bytes とバイト単位で完全に一致するなら true */
static bool name_matches(const char *name, const char *bytes, size_t name_len)
{
    if (name == NULL)
    {
        return false;
    }

    return (strlen(name) == name_len) && (memcmp(name, bytes, name_len) == 0);
}

/* option が否定できるフラグなら true */
static bool is_negatable_flag(const clopts_option_t *option)
{
    return (option->kind == CLOPTS_FLAG) && option->negatable;
}

/* 名前の 1 つ。NUL で終わるとは限らない（別名はカンマ区切りの文字列の一部を指す） */
typedef struct clopts_name
{
    const char *bytes;
    size_t len;
} clopts_name_t;

/*
 * option の n 番目の名前を *name に入れて true。0 番目は long_name、1 番目以降は aliases を
 * カンマで区切った項目（空の項目も 1 つの名前として返す）。n が名前の数以上なら false。
 * long_name が NULL なら、0 番目は長さ 0 の名前とする。確保は行わない
 */
static bool option_name_at(const clopts_option_t *option, size_t n, clopts_name_t *name)
{
    const char *item;
    const char *end;
    size_t i;

    if (n == 0)
    {
        name->bytes = shown(option->long_name);
        name->len = strlen(name->bytes);
        return true;
    }

    if (option->aliases == NULL)
    {
        return false;
    }

    item = option->aliases;
    for (i = 1; i < n; ++i)
    {
        item = strchr(item, ',');
        if (item == NULL)
        {
            return false;
        }

        ++item;
    }

    end = strchr(item, ',');
    name->bytes = item;
    name->len = (end != NULL) ? (size_t)(end - item) : strlen(item);
    return true;
}

/* 長さ len を "%.*s" の精度に使える int に収める */
static int name_precision(size_t len)
{
    if (len > (size_t)INT_MAX)
    {
        return INT_MAX;
    }

    return (int)len;
}

/* 名前 name が、NUL で終わる文字列 text とバイト単位で完全に一致するなら true */
static bool name_is(const clopts_name_t *name, const char *text)
{
    return (strlen(text) == name->len) && (memcmp(text, name->bytes, name->len) == 0);
}

/* 2 つの名前がバイト単位で完全に一致するなら true */
static bool names_same(const clopts_name_t *a, const clopts_name_t *b)
{
    return (a->len == b->len) && (memcmp(a->bytes, b->bytes, a->len) == 0);
}

/* name が "no-" の後に base を続けた名前なら true */
static bool is_negated_name(const clopts_name_t *name, const clopts_name_t *base)
{
    return (name->len >= 3) && (name->len - 3 == base->len)
           && (memcmp(name->bytes, "no-", 3) == 0)
           && (memcmp(name->bytes + 3, base->bytes, base->len) == 0);
}

/* 別名が、長い名前として受け付ける形（空でない、先頭が '-' でない、'=' を含まない）なら true */
static bool alias_has_valid_form(const clopts_name_t *alias)
{
    return (alias->len != 0) && (alias->bytes[0] != '-')
           && (memchr(alias->bytes, '=', alias->len) == NULL);
}

/* 別名が、予約された長い名前（help、version）なら true */
static bool alias_is_reserved(const clopts_name_t *alias)
{
    return name_is(alias, "help") || name_is(alias, "version");
}

/*
 * option の n 番目の名前 name（option_name_at で得たもの）を、重複と衝突の比較に加えるなら true。
 * 長い名前は、NULL でなければ加える（改訂前と同じ）。別名は、形と予約の検査（17.13）を通るものだけを
 * 加える。形か予約の誤りになる別名は、その誤りだけを報告し、重複と衝突の相手にもしない
 */
static bool name_is_compared(const clopts_option_t *option, size_t n, const clopts_name_t *name)
{
    if (n == 0)
    {
        return option->long_name != NULL;
    }

    return alias_has_valid_form(name) && !alias_is_reserved(name);
}

/* option の名前（長い名前か別名）のどれかが name と一致するなら true */
static bool option_has_name(const clopts_option_t *option, const clopts_name_t *name)
{
    clopts_name_t other;
    size_t m;

    for (m = 0; option_name_at(option, m, &other); ++m)
    {
        if (name_is_compared(option, m, &other) && names_same(&other, name))
        {
            return true;
        }
    }

    return false;
}

/*
 * flag が否定できるフラグで、その名前（長い名前か別名）<n> と、other の名前 no-<n> の組があるなら、
 * 最初の組の <n> を *base に入れて true。flag と other が同じオプションでもよい
 * （同じオプションの名前の間の衝突。17.14）
 */
static bool negated_pair(const clopts_option_t *flag, const clopts_option_t *other,
                         clopts_name_t *base)
{
    clopts_name_t name;
    clopts_name_t negated;
    size_t n;
    size_t m;

    if (!is_negatable_flag(flag))
    {
        return false;
    }

    for (n = 0; option_name_at(flag, n, &name); ++n)
    {
        if (!name_is_compared(flag, n, &name))
        {
            continue;
        }

        for (m = 0; option_name_at(other, m, &negated); ++m)
        {
            if (name_is_compared(other, m, &negated) && is_negated_name(&negated, &name))
            {
                *base = name;
                return true;
            }
        }
    }

    return false;
}

/*
 * a と b のどちらかが否定できるフラグで、その名前 <n> に対して、もう一方に名前 no-<n> があるなら、
 * 否定できるフラグの方を返し、<n> を *base に入れる。衝突しなければ NULL
 */
static const clopts_option_t *negation_conflict(const clopts_option_t *a, const clopts_option_t *b,
                                                clopts_name_t *base)
{
    if (negated_pair(a, b, base))
    {
        return a;
    }

    if (negated_pair(b, a, base))
    {
        return b;
    }

    return NULL;
}

/* カンマ区切りのリストに空の項目（空文字列、先頭・末尾のカンマ、",,"）があるなら true */
static bool has_empty_item(const char *list)
{
    const char *p;
    size_t item_len = 0;

    for (p = list; *p != '\0'; ++p)
    {
        if (*p == ',')
        {
            if (item_len == 0)
            {
                return true;
            }

            item_len = 0;
        }
        else
        {
            ++item_len;
        }
    }

    return item_len == 0;
}

/*
 * 検査中のコマンドの木の 1 段。ルートコマンドから現在のコマンドまでの祖先の列を、
 * parent をたどる連結リストとして持つ（祖先との重複と循環の検出に使う）
 */
typedef struct decl_frame decl_frame_t;

struct decl_frame
{
    decl_frame_t *parent;              /* ルートコマンドでは NULL */
    const clopts_command_t *command;
    const char *name;                  /* 経路上の名前。ルートコマンドではプログラム名 */
    clopts_buf_t path;                 /* 経路の文字列。最初の誤りの報告のときに作る */
    bool path_ready;
    bool path_failed;                  /* 経路の文字列を作れなかった（メモリ不足） */
};

typedef struct decl_check
{
    clopts_errors_t *errors;
    bool ok;
} decl_check_t;

static void frame_init(decl_frame_t *frame, decl_frame_t *parent,
                       const clopts_command_t *command, const char *name)
{
    frame->parent = parent;
    frame->command = command;
    frame->name = name;
    frame->path.data = NULL;
    frame->path.len = 0;
    frame->path.cap = 0;
    frame->path_ready = false;
    frame->path_failed = false;
}

static bool append_path(clopts_buf_t *buf, const decl_frame_t *frame)
{
    if (frame->parent != NULL)
    {
        if (!append_path(buf, frame->parent) || !clopts_buf_append_byte(buf, ' '))
        {
            return false;
        }
    }

    return clopts_buf_append_str(buf, shown(frame->name));
}

/*
 * frame の経路の文字列（例: "nest remote"）。各段の名前を空白で区切る。
 * メモリ不足で作れなければ、代わりにコマンド自身の名前を返す
 */
static const char *frame_path(decl_frame_t *frame)
{
    if (!frame->path_ready)
    {
        frame->path_ready = true;
        if (!append_path(&frame->path, frame))
        {
            clopts_buf_free(&frame->path);
            frame->path_failed = true;
        }
    }

    if (frame->path_failed)
    {
        return shown(frame->name);
    }

    return clopts_buf_cstr(&frame->path);
}

/*
 * frame の祖先（frame 自身を除く）で、名前（長い名前か別名）が name のオプションを持つ最も近い段。
 * なければ NULL
 */
static decl_frame_t *find_ancestor_long(const decl_frame_t *frame, const clopts_name_t *name)
{
    decl_frame_t *ancestor;
    size_t i;

    for (ancestor = frame->parent; ancestor != NULL; ancestor = ancestor->parent)
    {
        for (i = 0; i < ancestor->command->option_count; ++i)
        {
            if (option_has_name(&ancestor->command->options[i], name))
            {
                return ancestor;
            }
        }
    }

    return NULL;
}

/*
 * frame の祖先（frame 自身を除く）で、option と否定の形の名前が衝突するオプションを持つ最も近い段。
 * 見つかれば、そのオプションを *found に、衝突した否定できるフラグの名前を *base に入れる。なければ NULL
 */
static decl_frame_t *find_ancestor_negation(const decl_frame_t *frame,
                                            const clopts_option_t *option,
                                            const clopts_option_t **found, clopts_name_t *base)
{
    decl_frame_t *ancestor;
    size_t i;

    for (ancestor = frame->parent; ancestor != NULL; ancestor = ancestor->parent)
    {
        for (i = 0; i < ancestor->command->option_count; ++i)
        {
            if (negation_conflict(option, &ancestor->command->options[i], base) != NULL)
            {
                *found = &ancestor->command->options[i];
                return ancestor;
            }
        }
    }

    return NULL;
}

/*
 * frame のコマンドの index 番目のオプションについて、否定できるフラグの名前 <n>（長い名前か別名）と
 * 名前 no-<n> の衝突を、同じコマンドの前のオプション、同じオプションの名前どうし、祖先のオプションとの
 * 間で検査する（14.11、17.14）。祖先の側のオプションが引き継ぐものかどうかは問わない（1.8 と同じ）。
 * 報告は最初の 1 件だけ
 */
static void check_negation_conflict(decl_check_t *check, decl_frame_t *frame, size_t index)
{
    const clopts_command_t *command = frame->command;
    const clopts_option_t *option = &command->options[index];
    const clopts_option_t *other = NULL;
    clopts_name_t base;
    decl_frame_t *ancestor;
    int len;
    size_t j;

    /* j == index は、同じオプションの名前どうしの組 */
    for (j = 0; j <= index; ++j)
    {
        if (negation_conflict(option, &command->options[j], &base) != NULL)
        {
            len = name_precision(base.len);
            clopts_errors_add(check->errors,
                              DECL_PREFIX "option '--no-%.*s' conflicts with negatable flag '--%.*s' in command '%s'",
                              len, base.bytes, len, base.bytes, frame_path(frame));
            check->ok = false;
            return;
        }
    }

    ancestor = find_ancestor_negation(frame, option, &other, &base);
    if (ancestor == NULL)
    {
        return;
    }

    len = name_precision(base.len);

    /* 否定できるフラグが子孫の側（このコマンド）にあるか、祖先の側にあるかで文面を分ける */
    if (negation_conflict(option, other, &base) == option)
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "negatable flag '--%.*s' in command '%s' conflicts with option '--no-%.*s' of command '%s'",
                          len, base.bytes, frame_path(frame), len, base.bytes,
                          frame_path(ancestor));
    }
    else
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "option '--no-%.*s' in command '%s' conflicts with negatable flag '--%.*s' of command '%s'",
                          len, base.bytes, frame_path(frame), len, base.bytes,
                          frame_path(ancestor));
    }

    check->ok = false;
}

/* frame の祖先（frame 自身を除く）で、短い名前が short_name のオプションを持つ最も近い段。なければ NULL */
static decl_frame_t *find_ancestor_short(const decl_frame_t *frame, char short_name)
{
    decl_frame_t *ancestor;
    size_t i;

    for (ancestor = frame->parent; ancestor != NULL; ancestor = ancestor->parent)
    {
        for (i = 0; i < ancestor->command->option_count; ++i)
        {
            if (ancestor->command->options[i].short_name == short_name)
            {
                return ancestor;
            }
        }
    }

    return NULL;
}

/* frame のコマンドの index 番目の子コマンド名を検査する */
static void check_command_name(decl_check_t *check, decl_frame_t *frame, size_t index)
{
    const clopts_command_t *parent = frame->command;
    const char *name = parent->commands[index].name;
    size_t j;

    if (is_empty(name) || (name[0] == '-'))
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "invalid command name '%s' in command '%s'", shown(name),
                          frame_path(frame));
        check->ok = false;
        return;
    }

    if (strcmp(name, "help") == 0)
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "command name 'help' is reserved in command '%s'",
                          frame_path(frame));
        check->ok = false;
        return;
    }

    for (j = 0; j < index; ++j)
    {
        if (names_equal(parent->commands[j].name, name))
        {
            clopts_errors_add(check->errors,
                              DECL_PREFIX "duplicate command '%s' in command '%s'", name,
                              frame_path(frame));
            check->ok = false;
            return;
        }
    }
}

/*
 * frame のコマンドの index 番目のオプションの名前（長い名前と別名）のうち、同じコマンドの前の
 * オプションの名前か、同じオプションの前の名前と重複する最初の 1 つを *found に入れて true（1.8、17.12）
 */
static bool find_duplicate_in_command(const clopts_command_t *command, size_t index,
                                      clopts_name_t *found)
{
    const clopts_option_t *option = &command->options[index];
    clopts_name_t name;
    clopts_name_t earlier;
    size_t n;
    size_t m;
    size_t j;

    for (n = 0; option_name_at(option, n, &name); ++n)
    {
        if (!name_is_compared(option, n, &name))
        {
            continue;
        }

        for (j = 0; j < index; ++j)
        {
            if (option_has_name(&command->options[j], &name))
            {
                *found = name;
                return true;
            }
        }

        /* 同じオプションの前の名前（元の名前と、前に書いた別名） */
        for (m = 0; m < n; ++m)
        {
            if (option_name_at(option, m, &earlier) && name_is_compared(option, m, &earlier)
                && names_same(&earlier, &name))
            {
                *found = name;
                return true;
            }
        }
    }

    return false;
}

/*
 * frame のコマンドの index 番目のオプションの名前（長い名前と別名）のうち、祖先のオプションの名前と
 * 重複する最初の 1 つを *found に入れ、そのオプションを持つ最も近い祖先の段を返す。なければ NULL
 */
static decl_frame_t *find_duplicate_in_ancestors(const decl_frame_t *frame, size_t index,
                                                 clopts_name_t *found)
{
    const clopts_option_t *option = &frame->command->options[index];
    clopts_name_t name;
    decl_frame_t *ancestor;
    size_t n;

    for (n = 0; option_name_at(option, n, &name); ++n)
    {
        if (!name_is_compared(option, n, &name))
        {
            continue;
        }

        ancestor = find_ancestor_long(frame, &name);
        if (ancestor != NULL)
        {
            *found = name;
            return ancestor;
        }
    }

    return NULL;
}

/*
 * frame のコマンドの index 番目のオプションの長い名前の形と予約を検査し、続けて、長い名前と別名を
 * 合わせた名前の重複と、否定の形との衝突を検査する。重複と衝突の報告は、それぞれ最初の 1 件だけ
 */
static void check_long_name(decl_check_t *check, decl_frame_t *frame, size_t index)
{
    const clopts_command_t *command = frame->command;
    const char *name = command->options[index].long_name;
    clopts_name_t duplicate;
    decl_frame_t *ancestor;

    if (is_empty(name) || (name[0] == '-') || (strchr(name, '=') != NULL))
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "invalid option name '%s' in command '%s'", shown(name),
                          frame_path(frame));
        check->ok = false;
        return;
    }

    if ((strcmp(name, "help") == 0) || (strcmp(name, "version") == 0))
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "option '--%s' is reserved in command '%s'", name,
                          frame_path(frame));
        check->ok = false;
        return;
    }

    if (find_duplicate_in_command(command, index, &duplicate))
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "duplicate option '--%.*s' in command '%s'",
                          name_precision(duplicate.len), duplicate.bytes, frame_path(frame));
        check->ok = false;
        return;
    }

    /* 祖先のオプションとは、引き継ぐかどうかによらず重複できない（1.8） */
    ancestor = find_duplicate_in_ancestors(frame, index, &duplicate);
    if (ancestor != NULL)
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "option '--%.*s' in command '%s' duplicates an option of command '%s'",
                          name_precision(duplicate.len), duplicate.bytes, frame_path(frame),
                          frame_path(ancestor));
        check->ok = false;
    }

    check_negation_conflict(check, frame, index);
}

/*
 * frame のコマンドの index 番目のオプションの別名の形と予約を検査する（17.13）。
 * 誤りは別名ごとに報告する。空の項目（空文字列の aliases、先頭・末尾・連続のカンマ）は空の別名として誤りにする
 */
static void check_aliases(decl_check_t *check, decl_frame_t *frame, size_t index)
{
    const clopts_option_t *option = &frame->command->options[index];
    clopts_name_t alias;
    size_t n;

    for (n = 1; option_name_at(option, n, &alias); ++n)
    {
        int len = name_precision(alias.len);

        /* 長い名前と同じ形の条件: 空、先頭が '-'、'=' を含む */
        if (!alias_has_valid_form(&alias))
        {
            clopts_errors_add(check->errors,
                              DECL_PREFIX "invalid alias '%.*s' of option '--%s' in command '%s'",
                              len, alias.bytes, shown(option->long_name), frame_path(frame));
            check->ok = false;
            continue;
        }

        if (alias_is_reserved(&alias))
        {
            clopts_errors_add(check->errors,
                              DECL_PREFIX "alias '--%.*s' of option '--%s' is reserved in command '%s'",
                              len, alias.bytes, shown(option->long_name), frame_path(frame));
            check->ok = false;
        }
    }
}

/* frame のコマンドの index 番目のオプションの短い名前を検査する */
static void check_short_name(decl_check_t *check, decl_frame_t *frame, size_t index)
{
    const clopts_command_t *command = frame->command;
    const clopts_option_t *option = &command->options[index];
    char short_name = option->short_name;
    decl_frame_t *ancestor;
    size_t j;

    if (short_name == '\0')
    {
        return;
    }

    if (short_name == 'h')
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "option '-h' is reserved in command '%s'", frame_path(frame));
        check->ok = false;
        return;
    }

    if (!is_ascii_alnum(short_name))
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "invalid short option for '--%s' in command '%s'",
                          shown(option->long_name), frame_path(frame));
        check->ok = false;
        return;
    }

    for (j = 0; j < index; ++j)
    {
        if (command->options[j].short_name == short_name)
        {
            clopts_errors_add(check->errors,
                              DECL_PREFIX "duplicate option '-%c' in command '%s'", short_name,
                              frame_path(frame));
            check->ok = false;
            return;
        }
    }

    ancestor = find_ancestor_short(frame, short_name);
    if (ancestor != NULL)
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "option '-%c' in command '%s' duplicates an option of command '%s'",
                          short_name, frame_path(frame), frame_path(ancestor));
        check->ok = false;
    }
}

/* frame のコマンドの index 番目のオプションを検査する */
static void check_option(decl_check_t *check, decl_frame_t *frame, size_t index)
{
    const clopts_option_t *option = &frame->command->options[index];
    const char *name = option->long_name;

    check_long_name(check, frame, index);
    /* 長い名前が形の誤りのときにも、別名の形と予約は検査する（17.13） */
    check_aliases(check, frame, index);
    check_short_name(check, frame, index);

    /* 否定の形はフラグだけに許す（14.10） */
    if (option->negatable && (option->kind != CLOPTS_FLAG))
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "option '--%s' cannot be negatable in command '%s'",
                          shown(name), frame_path(frame));
        check->ok = false;
    }

    /* 既定で有効は否定できるフラグだけに許す（14.16） */
    if (option->default_enabled && !is_negatable_flag(option))
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "option '--%s' must be a negatable flag to be enabled by default in command '%s'",
                          shown(name), frame_path(frame));
        check->ok = false;
    }

    if (option->kind == CLOPTS_FLAG)
    {
        if (option->required || (option->default_value != NULL))
        {
            clopts_errors_add(check->errors,
                              DECL_PREFIX "flag '--%s' cannot be required or have a default in command '%s'",
                              shown(name), frame_path(frame));
            check->ok = false;
        }
    }
    else
    {
        if (option->required && (option->default_value != NULL))
        {
            clopts_errors_add(check->errors,
                              DECL_PREFIX "option '--%s' cannot be both required and defaulted in command '%s'",
                              shown(name), frame_path(frame));
            check->ok = false;
        }

        if ((option->kind == CLOPTS_LIST) && (option->default_value != NULL)
            && has_empty_item(option->default_value))
        {
            clopts_errors_add(check->errors,
                              DECL_PREFIX "default of '--%s' has an empty item in command '%s'",
                              shown(name), frame_path(frame));
            check->ok = false;
        }
    }

    /* 必須のオプションは隠せない（16.6）。必須のフラグでは、上の誤りと合わせて 2 件になる */
    if (option->required && option->hidden)
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "option '--%s' cannot be both required and hidden in command '%s'",
                          shown(name), frame_path(frame));
        check->ok = false;
    }
}

static void check_positionals(decl_check_t *check, decl_frame_t *frame)
{
    const clopts_command_t *command = frame->command;
    bool names_match;

    /* 子を持つコマンドは位置引数を持たない（1.6）。範囲の規則は末端コマンドだけに適用する */
    if (command->command_count > 0)
    {
        if ((command->positional_count != 0) || (command->min_positionals != 0)
            || (command->max_positionals != 0))
        {
            clopts_errors_add(check->errors,
                              DECL_PREFIX "command '%s' cannot have both subcommands and positional arguments",
                              frame_path(frame));
            check->ok = false;
        }

        return;
    }

    if (command->min_positionals > command->max_positionals)
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "invalid positional range in command '%s'", frame_path(frame));
        check->ok = false;
    }

    if (command->max_positionals == CLOPTS_UNLIMITED)
    {
        names_match = (command->positional_count >= 1);
    }
    else
    {
        names_match = (command->positional_count == command->max_positionals);
    }

    if (!names_match)
    {
        clopts_errors_add(check->errors,
                          DECL_PREFIX "positional names do not match the range in command '%s'",
                          frame_path(frame));
        check->ok = false;
    }
}

/*
 * frame のコマンドを検査し、子コマンドを深さ優先でたどる（深さに上限はない）。
 * 祖先と同じ宣言の子コマンドは循環として報告し、その先をたどらない
 */
static void check_tree(decl_check_t *check, decl_frame_t *frame)
{
    const clopts_command_t *command = frame->command;
    size_t i;

    for (i = 0; i < command->option_count; ++i)
    {
        check_option(check, frame, i);
    }

    check_positionals(check, frame);

    for (i = 0; i < command->command_count; ++i)
    {
        const clopts_command_t *child = &command->commands[i];
        decl_frame_t *ancestor;
        decl_frame_t child_frame;

        check_command_name(check, frame, i);

        for (ancestor = frame; ancestor != NULL; ancestor = ancestor->parent)
        {
            if (ancestor->command == child)
            {
                break;
            }
        }

        if (ancestor != NULL)
        {
            clopts_errors_add(check->errors,
                              DECL_PREFIX "command '%s' contains itself", frame_path(ancestor));
            check->ok = false;
            continue;
        }

        frame_init(&child_frame, frame, child, child->name);
        check_tree(check, &child_frame);
        clopts_buf_free(&child_frame.path);
    }
}

bool clopts_decl_check(const clopts_program_t *program, clopts_errors_t *errors)
{
    decl_check_t check;
    decl_frame_t root;

    check.errors = errors;
    check.ok = true;

    if (is_empty(program->name) || is_empty(program->version))
    {
        clopts_errors_add(errors,
                          DECL_PREFIX "program name and version are required");
        check.ok = false;
    }

    if (program->root == NULL)
    {
        clopts_errors_add(errors, DECL_PREFIX "root command is required");
        return false;
    }

    /* ルートコマンドの経路上の名前はプログラム名。子を持たないルートコマンドも妥当な宣言である（1.4） */
    frame_init(&root, NULL, program->root, program->name);
    check_tree(&check, &root);
    clopts_buf_free(&root.path);

    return check.ok;
}

const clopts_command_t *clopts_find_child(const clopts_command_t *parent,
                                          const char *name, size_t name_len)
{
    size_t i;

    for (i = 0; i < parent->command_count; ++i)
    {
        if (name_matches(parent->commands[i].name, name, name_len))
        {
            return &parent->commands[i];
        }
    }

    return NULL;
}

/*
 * 有効なオプションのうち、順序で skip 個を飛ばした次の 1 つを探す。match が NULL でなければ、
 * match が true を返す最初の 1 つを探す（skip は 0 で呼ぶ）。
 * 順序は、最後の段のすべてのオプション、続いて親から祖先へ向かって引き継ぐオプション（12.1、12.2）
 */
typedef bool (*clopts_option_match_t)(const clopts_option_t *option, const void *key);

static bool find_effective(const clopts_result_t *result, size_t skip,
                           clopts_option_match_t match, const void *key, clopts_ref_t *ref)
{
    size_t level;

    if (result->level_count == 0)
    {
        return false;
    }

    level = result->level_count;
    while (level > 0)
    {
        const clopts_command_t *command;
        bool last;
        size_t i;

        --level;
        command = result->levels[level].command;
        last = (level + 1 == result->level_count);

        for (i = 0; i < command->option_count; ++i)
        {
            const clopts_option_t *option = &command->options[i];

            if (!last && !option->inherited)
            {
                continue;
            }

            if ((match != NULL) && !match(option, key))
            {
                continue;
            }

            if (skip > 0)
            {
                --skip;
                continue;
            }

            ref->level = level;
            ref->index = i;
            return true;
        }
    }

    return false;
}

/* 長い名前の照合に使う、name_len バイトの名前 */
typedef struct clopts_name_key
{
    const char *name;
    size_t len;
} clopts_name_key_t;

/* 長い名前と別名のどれかが、キーの名前とバイト単位で完全に一致するなら true（17.2、17.6） */
static bool match_long(const clopts_option_t *option, const void *key)
{
    const clopts_name_key_t *wanted = (const clopts_name_key_t *)key;
    clopts_name_t name;
    size_t n;

    for (n = 0; option_name_at(option, n, &name); ++n)
    {
        /* 長い名前のないオプション（妥当でない宣言）は、空の名前にも一致させない */
        if ((n == 0) && (option->long_name == NULL))
        {
            continue;
        }

        if ((name.len == wanted->len) && (memcmp(name.bytes, wanted->name, name.len) == 0))
        {
            return true;
        }
    }

    return false;
}

static bool match_short(const clopts_option_t *option, const void *key)
{
    return option->short_name == *(const char *)key;
}

bool clopts_find_long(const clopts_result_t *result, const char *name, size_t name_len,
                      clopts_ref_t *ref)
{
    clopts_name_key_t key;

    key.name = name;
    key.len = name_len;

    return find_effective(result, 0, match_long, &key, ref);
}

bool clopts_find_negated(const clopts_result_t *result, const char *name, size_t name_len,
                         clopts_ref_t *ref)
{
    clopts_ref_t found;

    if ((name_len <= 3) || (memcmp(name, "no-", 3) != 0))
    {
        return false;
    }

    /* 引き継ぎの範囲は肯定の形と同じ（14.9） */
    if (!clopts_find_long(result, name + 3, name_len - 3, &found))
    {
        return false;
    }

    if (!is_negatable_flag(clopts_option_at(result, found)))
    {
        return false;
    }

    *ref = found;
    return true;
}

bool clopts_find_short(const clopts_result_t *result, char short_name, clopts_ref_t *ref)
{
    if (short_name == '\0')
    {
        return false;
    }

    return find_effective(result, 0, match_short, &short_name, ref);
}

size_t clopts_effective_count(const clopts_result_t *result)
{
    size_t count = 0;
    size_t level;
    size_t i;

    if (result->level_count == 0)
    {
        return 0;
    }

    for (level = 0; level < result->level_count; ++level)
    {
        const clopts_command_t *command = result->levels[level].command;

        if (level + 1 == result->level_count)
        {
            count += command->option_count;
            continue;
        }

        for (i = 0; i < command->option_count; ++i)
        {
            if (command->options[i].inherited)
            {
                ++count;
            }
        }
    }

    return count;
}

clopts_ref_t clopts_effective_at(const clopts_result_t *result, size_t i)
{
    clopts_ref_t ref;

    ref.level = 0;
    ref.index = 0;
    (void)find_effective(result, i, NULL, NULL, &ref);

    return ref;
}

const clopts_option_t *clopts_option_at(const clopts_result_t *result, clopts_ref_t ref)
{
    return &result->levels[ref.level].command->options[ref.index];
}

const char *clopts_value_name(const clopts_option_t *option)
{
    if (option->value_name == NULL)
    {
        return "VALUE";
    }

    return option->value_name;
}

const char *clopts_positional_name(const clopts_command_t *command, size_t index)
{
    if (command->positional_count == 0)
    {
        return NULL;
    }

    if (index >= command->positional_count)
    {
        index = command->positional_count - 1;
    }

    return command->positionals[index].name;
}
