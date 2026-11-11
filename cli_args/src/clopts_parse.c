#include "clopts/clopts.h"
#include "clopts_internal.h"
#include "foundation/buf.h"

/* Parser: 引数列の走査と状態の決定 */

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 保留した使い方の誤りの種類（「メッセージ」の節の表の場面） */
typedef enum mnd_args_error_kind
{
    MND_ARGS_ERR_UNKNOWN_OPTION,     /* unknown option '<arg>' */
    MND_ARGS_ERR_REQUIRES_VALUE,     /* option '<arg>' requires a value */
    MND_ARGS_ERR_TAKES_NO_VALUE,     /* option '<arg>' does not take a value */
    MND_ARGS_ERR_EMPTY_ITEM,         /* option '<arg>' has an empty item in '<value>' */
    MND_ARGS_ERR_MISSING_ARGUMENT,   /* missing argument <NAME> */
    MND_ARGS_ERR_UNEXPECTED,         /* unexpected argument '<token>' */
    MND_ARGS_ERR_MISSING_REQUIRED,   /* missing required option '--<name>' */
    MND_ARGS_ERR_UNKNOWN_COMMAND     /* unknown command '<token>' */
} mnd_args_error_kind_t;

/*
 * 保留した使い方の誤り。回復走査でヘルプの要求が見つかれば捨て、見つからなければ診断にする（7.4、8.3）。
 * arg は arg_len バイトの名前で、短いオプションの "-c" のように引数列の中に連続して現れない形は
 * arg_buf にコピーして指す
 */
typedef struct mnd_args_pending
{
    mnd_args_error_kind_t kind;
    const char *arg;
    size_t arg_len;
    const char *value;  /* MND_ARGS_ERR_EMPTY_ITEM の値。ほかは NULL */
    char arg_buf[3];
} mnd_args_pending_t;

/*
 * 走査の状態。ルートコマンドを選択した状態から、トークンを左から順に見る。
 * 選択中のコマンドは結果の最後の段から得る
 */
typedef struct mnd_args_scan
{
    const mnd_args_program_t *program;
    int argc;
    char *const *argv;
    int next;                           /* 次に見るトークンの添字 */
    bool terminated;                    /* "--" を見た */
    mnd_args_result_t *result;
    mnd_diag_list_t *diags;
    mnd_args_pending_t pending;         /* 走査が MND_ARGS_USAGE_ERROR を返したときの最初の誤り */
} mnd_args_scan_t;

static mnd_args_status_t finish(mnd_args_result_t *result, mnd_args_status_t status)
{
    result->status = status;
    return status;
}

static mnd_args_status_t out_of_memory(mnd_args_result_t *result, mnd_diag_list_t *diags)
{
    mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "out of memory while parsing arguments");
    return finish(result, MND_ARGS_OUT_OF_MEMORY);
}

/*
 * 診断に示す名前の長さ。"%.*s" の精度は int なので、それを超える長さは int の最大値に切り詰める
 * （引数の長さがそこまで達することは実際にはない）
 */
static int name_precision(size_t len)
{
    if (len > (size_t)INT_MAX)
    {
        return INT_MAX;
    }

    return (int)len;
}

/*
 * 最初の誤りを保留する。診断はまだ追加しない。
 * 戻り値は常に MND_ARGS_USAGE_ERROR で、走査を終えるために呼び出し元へそのまま返す
 */
static mnd_args_status_t scan_error(mnd_args_scan_t *scan, mnd_args_error_kind_t kind,
                                    const char *arg, size_t arg_len, const char *value)
{
    mnd_args_pending_t *pending = &scan->pending;

    pending->kind = kind;
    pending->value = value;
    pending->arg_len = arg_len;
    if (arg_len < sizeof pending->arg_buf)
    {
        memcpy(pending->arg_buf, arg, arg_len);
        pending->arg_buf[arg_len] = '\0';
        pending->arg = pending->arg_buf;
    }
    else
    {
        pending->arg = arg;
    }

    return MND_ARGS_USAGE_ERROR;
}

/* 経路の文字列（プログラム名に、段 1 以降のコマンド名を空白 1 つで区切って続けたもの）を out に追加する */
static bool append_path(const mnd_args_scan_t *scan, mnd_buf_t *out)
{
    const mnd_args_result_t *result = scan->result;
    size_t level;

    if (!mnd_buf_append_str(out, scan->program->name))
    {
        return false;
    }

    for (level = 1; level < result->level_count; level++)
    {
        if (!mnd_buf_append_byte(out, ' ')
            || !mnd_buf_append_str(out, result->levels[level].command->name))
        {
            return false;
        }
    }

    return true;
}

/* 保留した誤りを、誤りを保留した時点の経路の接頭辞を付けた診断として 1 件追加する（7.2） */
static mnd_args_status_t report_pending(mnd_args_scan_t *scan)
{
    const mnd_args_pending_t *pending = &scan->pending;
    mnd_buf_t path = MND_BUF_INIT;
    const char *prefix;
    int len = name_precision(pending->arg_len);

    if (!append_path(scan, &path))
    {
        mnd_buf_free(&path);
        return out_of_memory(scan->result, scan->diags);
    }

    prefix = mnd_buf_cstr(&path);

    switch (pending->kind)
    {
    case MND_ARGS_ERR_UNKNOWN_OPTION:
        mnd_diag_add(scan->diags, MND_SEVERITY_ERROR, NULL, 0, "%s: unknown option '%.*s'",
                     prefix, len, pending->arg);
        break;
    case MND_ARGS_ERR_REQUIRES_VALUE:
        mnd_diag_add(scan->diags, MND_SEVERITY_ERROR, NULL, 0,
                     "%s: option '%.*s' requires a value", prefix, len, pending->arg);
        break;
    case MND_ARGS_ERR_TAKES_NO_VALUE:
        mnd_diag_add(scan->diags, MND_SEVERITY_ERROR, NULL, 0,
                     "%s: option '%.*s' does not take a value", prefix, len, pending->arg);
        break;
    case MND_ARGS_ERR_EMPTY_ITEM:
        mnd_diag_add(scan->diags, MND_SEVERITY_ERROR, NULL, 0,
                     "%s: option '%.*s' has an empty item in '%s'", prefix, len, pending->arg,
                     pending->value);
        break;
    case MND_ARGS_ERR_MISSING_ARGUMENT:
        mnd_diag_add(scan->diags, MND_SEVERITY_ERROR, NULL, 0, "%s: missing argument <%.*s>",
                     prefix, len, pending->arg);
        break;
    case MND_ARGS_ERR_UNEXPECTED:
        mnd_diag_add(scan->diags, MND_SEVERITY_ERROR, NULL, 0, "%s: unexpected argument '%.*s'",
                     prefix, len, pending->arg);
        break;
    case MND_ARGS_ERR_MISSING_REQUIRED:
        mnd_diag_add(scan->diags, MND_SEVERITY_ERROR, NULL, 0,
                     "%s: missing required option '--%.*s'", prefix, len, pending->arg);
        break;
    case MND_ARGS_ERR_UNKNOWN_COMMAND:
        mnd_diag_add(scan->diags, MND_SEVERITY_ERROR, NULL, 0, "%s: unknown command '%.*s'",
                     prefix, len, pending->arg);
        break;
    }

    mnd_buf_free(&path);

    return finish(scan->result, MND_ARGS_USAGE_ERROR);
}

/*
 * 1 つのオプションの値を記録する（5.1〜5.4）。arg と arg_len は利用者が書いたオプションの形
 * （"--name" または "-c"）で、空の項目のメッセージに使う
 */
static mnd_args_status_t scan_record_value(mnd_args_scan_t *scan, mnd_args_ref_t ref,
                                           const char *arg, size_t arg_len, const char *value)
{
    mnd_args_status_t status;

    if (mnd_args_option_at(scan->result, ref)->kind == MND_ARGS_VALUE)
    {
        mnd_args_result_set_value(scan->result, ref, value);
        return MND_ARGS_OK;
    }

    status = mnd_args_result_append_list(scan->result, ref, value, true);
    if (status == MND_ARGS_OUT_OF_MEMORY)
    {
        return out_of_memory(scan->result, scan->diags);
    }

    if (status != MND_ARGS_OK)
    {
        return scan_error(scan, MND_ARGS_ERR_EMPTY_ITEM, arg, arg_len, value);
    }

    return MND_ARGS_OK;
}

/* 選択中のコマンドがルートコマンドなら true */
static bool root_is_selected(const mnd_args_scan_t *scan)
{
    return scan->result->level_count == 1;
}

/* 選択中のコマンドが子を持つなら true */
static bool has_children(const mnd_args_scan_t *scan)
{
    return mnd_args_command(scan->result)->command_count > 0;
}

/*
 * "--" で始まるトークンを処理する（3.1〜3.7、8.1、8.8、8.9）。
 * 戻り値が MND_ARGS_OK なら走査を続け、それ以外なら走査を終える
 */
static mnd_args_status_t scan_long(mnd_args_scan_t *scan, const char *token)
{
    const char *name = token + 2;
    const char *equals = strchr(name, '=');
    size_t name_len = (equals != NULL) ? (size_t)(equals - name) : strlen(name);
    size_t arg_len = name_len + 2;
    mnd_args_ref_t ref;

    if ((name_len == 4) && (memcmp(name, "help", 4) == 0))
    {
        if (equals != NULL)
        {
            return scan_error(scan, MND_ARGS_ERR_TAKES_NO_VALUE, token, arg_len, NULL);
        }

        return finish(scan->result, MND_ARGS_HELP);
    }

    /* --version はルートコマンドが選択中の間だけバージョンの要求で、それ以外は未知のオプション */
    if ((name_len == 7) && (memcmp(name, "version", 7) == 0) && root_is_selected(scan))
    {
        if (equals != NULL)
        {
            return scan_error(scan, MND_ARGS_ERR_TAKES_NO_VALUE, token, arg_len, NULL);
        }

        return finish(scan->result, MND_ARGS_VERSION);
    }

    if (!mnd_args_find_long(scan->result, name, name_len, &ref))
    {
        return scan_error(scan, MND_ARGS_ERR_UNKNOWN_OPTION, token, arg_len, NULL);
    }

    if (mnd_args_option_at(scan->result, ref)->kind == MND_ARGS_FLAG)
    {
        if (equals != NULL)
        {
            return scan_error(scan, MND_ARGS_ERR_TAKES_NO_VALUE, token, arg_len, NULL);
        }

        mnd_args_result_set_flag(scan->result, ref);

        return MND_ARGS_OK;
    }

    if (equals != NULL)
    {
        return scan_record_value(scan, ref, token, arg_len, equals + 1);
    }

    /* 次のトークンは "-" で始まる場合も含めて値とする（3.2） */
    if (scan->next >= scan->argc)
    {
        return scan_error(scan, MND_ARGS_ERR_REQUIRES_VALUE, token, arg_len, NULL);
    }

    scan->next++;

    return scan_record_value(scan, ref, token, arg_len, scan->argv[scan->next - 1]);
}

/*
 * "-" に 1 文字以上が続くトークンを、2 文字目から 1 文字ずつ処理する（4.1〜4.6、8.1）。
 * 戻り値が MND_ARGS_OK なら走査を続け、それ以外なら走査を終える
 */
static mnd_args_status_t scan_short(mnd_args_scan_t *scan, const char *token)
{
    const char *p;

    for (p = token + 1; *p != '\0'; p++)
    {
        unsigned char c = (unsigned char)*p;
        char arg[3];
        mnd_args_ref_t ref;

        if (c == 'h')
        {
            return finish(scan->result, MND_ARGS_HELP);
        }

        /* ASCII 以外のバイトは文字の境界が分からないので、トークン全体で示す */
        if (c > 0x7F)
        {
            return scan_error(scan, MND_ARGS_ERR_UNKNOWN_OPTION, token, strlen(token), NULL);
        }

        arg[0] = '-';
        arg[1] = (char)c;
        arg[2] = '\0';

        if (!mnd_args_find_short(scan->result, (char)c, &ref))
        {
            return scan_error(scan, MND_ARGS_ERR_UNKNOWN_OPTION, arg, 2, NULL);
        }

        if (mnd_args_option_at(scan->result, ref)->kind == MND_ARGS_FLAG)
        {
            mnd_args_result_set_flag(scan->result, ref);
            continue;
        }

        /* 値付きオプションは残りを値とし、なければ次のトークンを値とする（4.2、4.4、4.1） */
        if (p[1] != '\0')
        {
            return scan_record_value(scan, ref, arg, 2, p + 1);
        }

        if (scan->next >= scan->argc)
        {
            return scan_error(scan, MND_ARGS_ERR_REQUIRES_VALUE, arg, 2, NULL);
        }

        scan->next++;

        return scan_record_value(scan, ref, arg, 2, scan->argv[scan->next - 1]);
    }

    return MND_ARGS_OK;
}

/* 位置引数を記録する。上限に達した後のトークンは余分な位置引数とする（6.1、6.5） */
static mnd_args_status_t scan_positional(mnd_args_scan_t *scan, const char *token)
{
    if (scan->result->positional_count >= mnd_args_command(scan->result)->max_positionals)
    {
        return scan_error(scan, MND_ARGS_ERR_UNEXPECTED, token, strlen(token), NULL);
    }

    if (!mnd_args_result_add_positional(scan->result, token))
    {
        return out_of_memory(scan->result, scan->diags);
    }

    return MND_ARGS_OK;
}

/*
 * 子を持つコマンドの走査中に見た "help" の語を処理する（8.4〜8.6）。
 * 後続のトークンを、選択中のコマンドの子コマンド名として順にたどり、たどり着いたコマンドの
 * ヘルプの要求とする。トークンはすべて名前として扱い、オプションとして解釈しない。
 * 名前が一致しないか、末端コマンドに達した後にトークンが残っていれば、段を "help" を見た
 * 時点まで戻してから誤りを保留する（接頭辞は "help" を書いたコマンドの経路になる）
 */
static mnd_args_status_t scan_help_word(mnd_args_scan_t *scan)
{
    size_t level_count = scan->result->level_count;

    while (scan->next < scan->argc)
    {
        const char *name = scan->argv[scan->next];
        const mnd_args_command_t *command;
        mnd_args_error_kind_t kind;

        scan->next++;

        if (has_children(scan))
        {
            command = mnd_args_find_child(mnd_args_command(scan->result), name, strlen(name));
            if (command != NULL)
            {
                if (!mnd_args_result_push(scan->result, command))
                {
                    return out_of_memory(scan->result, scan->diags);
                }

                continue;
            }

            kind = MND_ARGS_ERR_UNKNOWN_COMMAND;
        }
        else
        {
            kind = MND_ARGS_ERR_UNEXPECTED;
        }

        mnd_args_result_truncate(scan->result, level_count);

        return scan_error(scan, kind, name, strlen(name), NULL);
    }

    return finish(scan->result, MND_ARGS_HELP);
}

/*
 * オプションでない引数を処理する（2.2、2.3、2.6、6.1、8.7）。末端コマンドでは位置引数とし、
 * 子を持つコマンドでは "help" の語か子コマンド名として解釈する
 */
static mnd_args_status_t scan_operand(mnd_args_scan_t *scan, const char *token)
{
    const mnd_args_command_t *command;

    if (!has_children(scan))
    {
        return scan_positional(scan, token);
    }

    if (strcmp(token, "help") == 0)
    {
        return scan_help_word(scan);
    }

    command = mnd_args_find_child(mnd_args_command(scan->result), token, strlen(token));
    if (command == NULL)
    {
        return scan_error(scan, MND_ARGS_ERR_UNKNOWN_COMMAND, token, strlen(token), NULL);
    }

    if (!mnd_args_result_push(scan->result, command))
    {
        return out_of_memory(scan->result, scan->diags);
    }

    return MND_ARGS_OK;
}

/*
 * "--" を処理する（2.4、2.5、6.3）。末端コマンドでは終端とし、以降のトークンをすべて位置引数にする。
 * 子を持つコマンドでは、次のトークンがあれば余分な引数とし、なければ走査を終える
 */
static mnd_args_status_t scan_terminator(mnd_args_scan_t *scan)
{
    const char *token;

    if (!has_children(scan))
    {
        scan->terminated = true;
        return MND_ARGS_OK;
    }

    if (scan->next >= scan->argc)
    {
        return MND_ARGS_OK;
    }

    token = scan->argv[scan->next];
    scan->next++;

    return scan_error(scan, MND_ARGS_ERR_UNEXPECTED, token, strlen(token), NULL);
}

/*
 * 選択中のコマンドの宣言で、トークンを末尾まで左から順に走査する（2.1）。
 * "--" は scan_terminator、"--" で始まるほかのトークンは長いオプション、
 * "-" に 1 文字以上が続くトークンは短いオプションとして処理する。
 * "-" 単独と "-" で始まらないトークンはオプションでない引数とする（4.7）。
 * 戻り値が MND_ARGS_OK なら末尾に到達した。MND_ARGS_USAGE_ERROR なら最初の誤りを保留して止まった
 */
static mnd_args_status_t scan_tokens(mnd_args_scan_t *scan)
{
    mnd_args_status_t status;

    while (scan->next < scan->argc)
    {
        const char *token = scan->argv[scan->next];

        scan->next++;

        if (scan->terminated)
        {
            status = scan_positional(scan, token);
        }
        else if ((token[0] == '-') && (token[1] == '-') && (token[2] == '\0'))
        {
            status = scan_terminator(scan);
        }
        else if ((token[0] == '-') && (token[1] == '-'))
        {
            status = scan_long(scan, token);
        }
        else if ((token[0] == '-') && (token[1] != '\0'))
        {
            status = scan_short(scan, token);
        }
        else
        {
            status = scan_operand(scan, token);
        }

        if (status != MND_ARGS_OK)
        {
            return status;
        }
    }

    return MND_ARGS_OK;
}

/*
 * 回復走査（7.4、8.3）。最初の誤りの原因となったトークン（消費した値を含む）の次から、
 * "--" より前に残っているトークンのうち、完全に "--help" または "-h" と一致するものを探す
 */
static bool recover_help(const mnd_args_scan_t *scan)
{
    int i;

    if (scan->terminated)
    {
        return false;
    }

    for (i = scan->next; i < scan->argc; i++)
    {
        const char *token = scan->argv[i];

        if (strcmp(token, "--") == 0)
        {
            return false;
        }

        if ((strcmp(token, "--help") == 0) || (strcmp(token, "-h") == 0))
        {
            return true;
        }
    }

    return false;
}

/*
 * 誤りなしで走査を終えた後の検査（6.4、7.1）。位置引数の不足、必須のオプションの欠落の順に、
 * 最初の 1 つを保留して MND_ARGS_USAGE_ERROR を返す。必須のオプションは、経路上のすべての段を
 * 段 0 から順に、各段では宣言の順に見る
 */
static mnd_args_status_t post_check(mnd_args_scan_t *scan)
{
    const mnd_args_result_t *result = scan->result;
    const mnd_args_command_t *command = mnd_args_command(result);
    size_t count = result->positional_count;
    size_t level;
    size_t i;

    if (count < command->min_positionals)
    {
        const char *name = mnd_args_positional_name(command, count);

        return scan_error(scan, MND_ARGS_ERR_MISSING_ARGUMENT, name, strlen(name), NULL);
    }

    for (level = 0; level < result->level_count; level++)
    {
        const mnd_args_level_t *entry = &result->levels[level];

        for (i = 0; i < entry->command->option_count; i++)
        {
            const mnd_args_option_t *option = &entry->command->options[i];

            if (option->required && !entry->values[i].explicit_set)
            {
                return scan_error(scan, MND_ARGS_ERR_MISSING_REQUIRED, option->long_name,
                                  strlen(option->long_name), NULL);
            }
        }
    }

    return MND_ARGS_OK;
}

/*
 * トークンを走査し、状態を決める（「走査の後の検査と優先順位」の節）。
 * 使い方の誤りの診断は、ヘルプの要求がないと決まってから 1 件だけ追加する。
 * 回復走査は、誤りを検出した時点で選択中のコマンドが末端コマンド（末端コマンドのルートを含む）の
 * 場合だけ行う。子を持つコマンドが選択中の誤りは、後ろに --help があっても報告する（8.3、8.10）
 */
static mnd_args_status_t scan_command(mnd_args_scan_t *scan)
{
    mnd_args_status_t status;

    status = scan_tokens(scan);
    if (status == MND_ARGS_USAGE_ERROR)
    {
        if (!has_children(scan) && recover_help(scan))
        {
            return finish(scan->result, MND_ARGS_HELP);
        }

        return report_pending(scan);
    }

    if (status != MND_ARGS_OK)
    {
        return status;
    }

    /* 子を持つコマンドで引数列が終わった（2.4） */
    if (has_children(scan))
    {
        return finish(scan->result, MND_ARGS_NO_COMMAND);
    }

    status = post_check(scan);
    if (status != MND_ARGS_OK)
    {
        return report_pending(scan);
    }

    status = mnd_args_result_apply_defaults(scan->result);
    if (status == MND_ARGS_OUT_OF_MEMORY)
    {
        return out_of_memory(scan->result, scan->diags);
    }

    return finish(scan->result, status);
}

mnd_args_status_t mnd_args_parse(const mnd_args_program_t *program, int argc, char *const *argv,
                                 mnd_args_result_t *result, mnd_diag_list_t *diags)
{
    mnd_args_scan_t scan;

    mnd_args_result_free(result);

    if (!mnd_args_decl_check(program, diags))
    {
        return finish(result, MND_ARGS_DECL_ERROR);
    }

    /* ルートコマンドを段 0 として選択する */
    result->program = program;
    if (!mnd_args_result_push(result, program->root))
    {
        return out_of_memory(result, diags);
    }

    scan.program = program;
    scan.argc = argc;
    scan.argv = argv;
    scan.next = 1;
    scan.terminated = false;
    scan.result = result;
    scan.diags = diags;
    memset(&scan.pending, 0, sizeof scan.pending);

    return scan_command(&scan);
}
