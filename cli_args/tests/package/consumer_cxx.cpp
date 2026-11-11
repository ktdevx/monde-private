/*
 * 公開ヘッダを C++ から取り込めること、C のシンボルにリンクできることを確かめる。
 * package_test.cmake が引数を渡して実行し、終了コード 0 を期待する（引数は使わない）
 */
#include "clopts/argv.h"
#include "clopts/clopts.h"
#include "clopts/errors.h"
#include "clopts/help.h"

#include <cstdio>
#include <cstring>

int main()
{
    static const clopts_command_t root_command = {};
    clopts_program_t program = {};
    program.name = "consumer_cxx";
    program.version = "1.0.0";
    program.root = &root_command;

    clopts_errors_t errors = CLOPTS_ERRORS_INIT;
    clopts_result_t result = CLOPTS_RESULT_INIT;
    char arg0[] = "consumer_cxx";
    char arg1[] = "--version";
    char *argv[] = { arg0, arg1, nullptr };

    bool ok = (clopts_parse(&program, 2, argv, &result, &errors) == CLOPTS_VERSION);
    char *version = ok ? clopts_format_version(&program) : nullptr;
    ok = (version != nullptr) && (std::strcmp(version, "consumer_cxx 1.0.0\n") == 0);
    if (version != nullptr)
    {
        std::fputs(version, stdout);
    }
    clopts_text_free(version);
    clopts_result_free(&result);
    clopts_errors_free(&errors);
    return ok ? 0 : 1;
}
