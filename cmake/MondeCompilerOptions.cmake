include_guard(GLOBAL)

# 全 Monde ターゲットに共通のコンパイラオプションを適用する。
# CMAKE_C_FLAGS などのグローバル変数は変更しない（Naos 本体のビルドに影響させないため）。
function(monde_apply_compile_options target)
  if(MSVC)
    # MSVC は C99 モードを持たないため /std:c11 を使い、C99 の範囲のみを使用する
    set_target_properties(${target} PROPERTIES
      C_STANDARD 11
      C_STANDARD_REQUIRED ON
      C_EXTENSIONS OFF
    )
    target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /Zc:preprocessor)
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
  else()
    set_target_properties(${target} PROPERTIES
      C_STANDARD 99
      C_STANDARD_REQUIRED ON
      C_EXTENSIONS OFF
    )
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror -Werror=vla)
    if(MINGW)
      target_compile_definitions(${target} PRIVATE __USE_MINGW_ANSI_STDIO=1)
    endif()
  endif()
endfunction()
