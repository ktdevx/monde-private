#ifndef MONDE_MARKDOWN_MD_HTML_H
#define MONDE_MARKDOWN_MD_HTML_H

/* Html: HTML の字句と HTML ブロックの開始・終了条件の内部の宣言（md_html.c） */

#include <stdbool.h>
#include <stddef.h>

/*
 * 共通の約束:
 * - すべての関数は純粋で、data[pos..end) だけを読む。end より後ろは読まない。NUL では止めず、
 *   NUL はふつうの（空白でない）文字として扱う（Security Considerations）。
 * - 定義は CommonMark spec 0.31.2 の「Raw HTML」と「HTML blocks」のとおり。
 *
 * 生の HTML の字句（インライン、5.1）:
 * - pos は `<` の位置。字句であればその長さ（バイト数）を、そうでなければ 0 を返す。
 * - data[pos..end) は連続したバイト列として読む。タグの中の空白（spaces, tabs, and up to one
 *   line ending）には行末記号（LF、CRLF、CR）を 1 つまで含められ、コメント・処理命令・宣言・CDATA
 *   の中身には行末記号を含められる。
 * - 入れ子のコンテナ（引用、リスト項目）の中のリーフでは、2 行目以降の行の先頭にプレフィックス
 *   （`> ` など）がある。この部品はプレフィックスを知らない。呼び出し側（InlineParser）は、
 *   プレフィックスを除いた論理的な文字列の上でこの関数を呼び、得た長さを原文の範囲に戻すか、
 *   リーフが 1 行だけの場合など原文が連続している範囲で呼ぶ。
 *
 * HTML ブロックの条件（2.6）:
 * - data[pos..end) は 1 行。pos はインデント（3 桁以下。確認は呼び出し側）を除いた最初の空白でない
 *   文字、end はその物理的な行の line_end（行末記号の直前）。
 */

/* 開始タグ（open tag）。属性、`/>` を含む */
size_t mnd_md_html_open_tag(const char *data, size_t pos, size_t end);

/* 終了タグ（closing tag）。`</` タグ名、空白、`>`。属性は持てない */
size_t mnd_md_html_closing_tag(const char *data, size_t pos, size_t end);

/* コメント: `<!-->`、`<!--->`、または `<!--` と `-->` を含まない文字列と `-->` */
size_t mnd_md_html_comment(const char *data, size_t pos, size_t end);

/* 処理命令: `<?` と `?>` を含まない文字列と `?>` */
size_t mnd_md_html_processing_instruction(const char *data, size_t pos, size_t end);

/* 宣言: `<!` と ASCII の英字と `>` を含まない文字列と `>` */
size_t mnd_md_html_declaration(const char *data, size_t pos, size_t end);

/* CDATA: `<![CDATA[` と `]]>` を含まない文字列と `]]>` */
size_t mnd_md_html_cdata(const char *data, size_t pos, size_t end);

/* HTML タグ（上の 6 種類のどれか）。生の HTML のインラインの認識に使う */
size_t mnd_md_html_tag(const char *data, size_t pos, size_t end);

/* mnd_md_html_block_start の結果 */
typedef struct mnd_md_html_start
{
    int type;                   /* 開始条件の種類 1〜7（mnd_md_html_block_t.type と同じ）。0 は開始しない */
    bool interrupts_paragraph;  /* 段落を中断できる（種類 1〜6 で真、7 で偽） */
} mnd_md_html_start_t;

/*
 * HTML ブロックの開始条件を 1 から順に判定し、最初に満たした種類を out に入れる。
 * 開始するなら真。開始しなければ偽で、out の type は 0、interrupts_paragraph は偽。
 * 開いている段落があるとき、interrupts_paragraph が偽なら（種類 7）、呼び出し側は開始しない
 */
bool mnd_md_html_block_start(const char *data, size_t pos, size_t end, mnd_md_html_start_t *out);

/*
 * 種類 type（1〜5）の終了条件: 行 data[pos..end) が、種類 1 は `</pre>`、`</script>`、`</style>`、
 * `</textarea>` のどれか（大文字小文字を区別しない）、2 は `-->`、3 は `?>`、4 は `>`、5 は `]]>` を
 * 含むなら真。開始の行にも使う（開始の行が終了条件も満たせば、ブロックはその 1 行だけ）。
 * 呼び出し側は、開始の行では pos に開始の位置を、続く行ではコンテナのプレフィックスの直後を渡す。
 * 種類 6 と 7 の終了条件は「次の行が空行」であり、呼び出し側（BlockParser）が判定する。
 * この関数は種類 6、7、その他の値では常に偽を返す
 */
bool mnd_md_html_block_end(const char *data, size_t pos, size_t end, int type);

#endif
