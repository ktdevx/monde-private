#include "foundation/source.h"

bool mnd_span_is_valid(mnd_span_t span, size_t content_len)
{
    return (span.start <= span.end) && (span.end <= content_len);
}

bool mnd_source_line_of(const char *data, size_t len, size_t offset, size_t *line)
{
    size_t count = 1;
    size_t i;

    if (offset > len)
    {
        return false;
    }

    /*
     * 改行は LF の位置で完結する（CRLF も LF で数える）。単独の CR は数えない。
     * したがって CRLF の CR と LF の間を指す offset は CR と同じ行になる
     */
    for (i = 0; i < offset; ++i)
    {
        if (data[i] == '\n')
        {
            ++count;
        }
    }

    *line = count;

    return true;
}

bool mnd_span_text(const char *data, size_t len, mnd_span_t span, mnd_slice_t *out)
{
    if (!mnd_span_is_valid(span, len))
    {
        return false;
    }

    out->data = data + span.start;
    out->len = span.end - span.start;

    return true;
}
