include_guard(GLOBAL)

# CLOpts のライブラリとテストに共通のコンパイラオプションを適用する。
# すべて PRIVATE とターゲットのプロパティで指定し、リンクする利用側のターゲットには伝えない。
# CMAKE_C_FLAGS などの全体に効く変数は変更しない（取り込んだ側のビルドに影響させないため）。
# 警告をエラーにするのは CLOPTS_WARNINGS_AS_ERRORS が ON のときだけ。-Werror=vla は常に付ける。
function(clopts_apply_compile_options target)
  if(MSVC)
    # MSVC は C99 モードを持たないため /std:c11 を使い、C99 の範囲のみを使用する
    set_target_properties(${target} PROPERTIES
      C_STANDARD 11
      C_STANDARD_REQUIRED ON
      C_EXTENSIONS OFF
    )
    target_compile_options(${target} PRIVATE /W4 /utf-8 /Zc:preprocessor)
    if(CLOPTS_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
  else()
    set_target_properties(${target} PROPERTIES
      C_STANDARD 99
      C_STANDARD_REQUIRED ON
      C_EXTENSIONS OFF
    )
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror=vla)
    if(CLOPTS_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
    if(MINGW)
      target_compile_definitions(${target} PRIVATE __USE_MINGW_ANSI_STDIO=1)
    endif()
  endif()
endfunction()
