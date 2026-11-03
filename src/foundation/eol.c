#include "foundation/eol.h"

/*
 * data[i] が改行の最後のバイトなら、その改行の種類を返す。そうでなければ MND_EOL_NONE を返す。
 * CR LF は 1 つの CRLF（LF の位置で完結）、直前が CR でない LF は 1 つの LF、
 * 直後が LF でない単独の CR は改行ではない
 */
static mnd_eol line_break_ending_at(const char *data, size_t i)
{
    if (data[i] != '\n')
    {
        return MND_EOL_NONE;
    }

    if ((i > 0) && (data[i - 1] == '\r'))
    {
        return MND_EOL_CRLF;
    }

    return MND_EOL_LF;
}

mnd_eol_info mnd_eol_detect(const char *data, size_t len)
{
    mnd_eol_info info;
    mnd_eol first = MND_EOL_NONE;
    size_t i;

    info.lf_count = 0;
    info.crlf_count = 0;

    for (i = 0; i < len; ++i)
    {
        mnd_eol kind;
        
        kind = line_break_ending_at(data, i);
        if (kind == MND_EOL_LF)
        {
            ++info.lf_count;
        }
        else if (kind == MND_EOL_CRLF)
        {
            ++info.crlf_count;
        }
        else
        {
            continue;
        }

        if (first == MND_EOL_NONE)
        {
            first = kind;
        }
    }

    info.mixed = (info.lf_count > 0) && (info.crlf_count > 0);
    if (info.lf_count > info.crlf_count)
    {
        info.dominant = MND_EOL_LF;
    }
    else if (info.crlf_count > info.lf_count)
    {
        info.dominant = MND_EOL_CRLF;
    }
    else
    {
        /* 同数（どちらも 0 なら first は MND_EOL_NONE） */
        info.dominant = first;
    }

    return info;
}
