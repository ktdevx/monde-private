#include "cli_args/args_argv.h"

#ifdef _WIN32
#include "foundation/wide.h"
#endif

/* ArgvSource: 引数列の UTF-8 での取得（Windows では wide で UTF-8 に変換する） */
