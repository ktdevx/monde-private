# 調査と設計判断: cli-args

## Summary
- **Feature**: `cli-args`
- **Discovery Scope**: New Feature（新しいモジュール。既存の foundation と CMake 関数の上に積む）
- **Key Findings**:
  - foundation には UTF-16 から UTF-8 への変換がない（`file_io.c` は UTF-8 から UTF-16 への変換だけを `static` で持つ）。変換は Windows の API との境界で必要になる共通の処理なので、foundation に部品 `wide` を新設して両方向の変換をまとめ、`file_io.c` の既存の変換もそれに寄せる（設計レビューでの指摘を受けて変更）。
  - `monde_add_module` には、システムライブラリをリンクする引数がない。`CommandLineToArgvW` を使うには `shell32` が必要なため、`src/cli_args/CMakeLists.txt` で Windows のときだけ `target_link_libraries(... PUBLIC shell32)` を追加する。
  - 要件 8.2（サブコマンドのヘルプは他の使い方のエラーより優先する）と 7.4（最初の誤りで解析をやめる）を両立させるには、最初の誤りを記録した後もヘルプの要求だけを探し続ける「回復走査」が必要になる。

## Research Log

### 既存の基盤（foundation と CMake）
- **Context**: 依存してよい部品と、モジュールの追加手順を確認する。
- **Sources Consulted**: `src/foundation/{buf,diag,utf8}.h`、`src/foundation/file_io.c`、`cmake/MondeTargets.cmake`、`tests/foundation/`、`.kiro/steering/structure.md`
- **Findings**:
  - `mnd_buf_t`（NUL 終端の可変長バッファ）と `mnd_diag_list_t`（英語のメッセージ、パス、行番号を持つ診断リスト）が使える。診断は `printf` 形式で追加する。
  - モジュールは `monde_add_module(<module> SOURCES ... DEPENDS ...)`、テストは `monde_add_module_tests(<module> SOURCES ...)` で追加する。テストは 1 ファイル 1 スイートとし、`main.c` で `NAOS_MAIN` に並べる。
  - foundation が予約している短縮名は `buf`、`diag`、`severity`、`utf8`、`eol`、`span`、`slice`、`source`、`file` である。
- **Implications**: モジュール名は `cli_args`、短縮名は `args` とする（予約済みの名前と重ならない）。エラーは `mnd_diag_list_t` に追加して返す。

### コマンドライン引数の慣例（POSIX、getopt_long）
- **Context**: 要件確認で、短いオプションは POSIX 相当と決まった。既存の慣例と挙動を合わせる。
- **Sources Consulted**: POSIX.1-2017 XBD 12.2 Utility Syntax Guidelines、glibc `getopt_long` のマニュアル
- **Findings**:
  - Guideline 5: 値を取らない短いオプションは 1 つの `-` の後にまとめて書ける。
  - Guideline 6/7: 値付きの短いオプションは、値を次の引数で渡しても、連結して渡してもよい（getopt は両方を受け付ける）。
  - Guideline 10: `--` は最初のオペランドの前の終端で、それ以降はすべてオペランドとして扱う。
  - Guideline 13: 単独の `-` はオペランドとして扱う（標準入力の慣例）。
  - getopt は、値を取るオプションの次の引数が `-` で始まっていても、それを値として消費する。
  - GNU の `getopt_long` は長いオプションの省略形（一意な前方一致）を受け付けるが、要件 3.6 ではこれを受け付けない。
- **Implications**: 要件 3.2、4.1〜4.7、6.3 は getopt の挙動とそろっている。省略形を受け付けない点と、オプションと位置引数の混在（GNU の並べ替え相当）を、設計で明記する。

### Windows でのコマンドライン引数の取得
- **Context**: 要件 10.2、10.3。Windows の `main` の `argv` は ANSI コードページで渡されるため、非 ASCII の文字が失われることがある。
- **Sources Consulted**: Microsoft Learn: `GetCommandLineW`、`CommandLineToArgvW`、`WideCharToMultiByte`（`WC_ERR_INVALID_CHARS`）、「Parsing C command-line arguments」
- **Findings**:
  - `wmain` を使う方法は、MinGW-w64 では `-municode` が必要で、エントリポイントの形が monde-cli の都合に入り込む。
  - `GetCommandLineW` と `CommandLineToArgvW`（shell32）は、どちらのエントリポイントからでも呼べ、MSVC と MinGW-w64 の両方で使える。
  - `CommandLineToArgvW` の分割規則は、CRT の規則とごく一部で異なる（引用符の中で `""` が続く場合など）。
  - `WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ...)` は、対になっていないサロゲートを含む場合に失敗する。
- **Implications**: `GetCommandLineW` と `CommandLineToArgvW` で取得し、各要素を foundation の `mnd_wide_to_utf8`（内部で `WC_ERR_INVALID_CHARS` を指定）で変換する。変換処理は単独でテストできる関数に分け、Windows のテストで不正なサロゲートを与えて要件 10.3 を検証する。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| 宣言テーブル + 単一パスの走査器（採用） | `static const` の構造体配列で宣言し、1 つの関数が引数列を左から走査する | 宣言とヘルプが 1 か所にまとまる。データだけでテストできる | 走査器の状態遷移が増える | 要件 1 と 9 の「宣言からヘルプを生成する」に合う |
| 実行時の登録 API（`add_option(...)` を順に呼ぶ） | ビルダー関数で宣言を組み立てる | 宣言の誤りを登録時に検出できる | 確保の失敗処理が増え、宣言がコードに散らばる | C99 では静的な初期化子のほうが読みやすい |
| コールバック駆動（getopt 風に 1 つずつ返す） | 呼び出し側がループでオプションを受け取る | 実装が小さい | 必須チェック、既定値、ヘルプの優先を呼び出し側が書くことになる | 要件 5〜8 を部品で満たせない |

## Design Decisions

### Decision: 宣言は呼び出し側が所有する静的なデータとする
- **Context**: 要件 1.1〜1.3、1.7。
- **Alternatives Considered**:
  1. 実行時の登録 API
  2. `static const` の構造体配列を渡す
- **Selected Approach**: 2。パーサは宣言を読むだけで、コピーも変更もしない。宣言は解析結果より長く生存する必要がある。
- **Rationale**: 確保がなく、宣言の全体がソース上で 1 か所に見える。テストでも宣言をリテラルで書ける。
- **Trade-offs**: 宣言の誤りは解析の呼び出し時にしか検出できない。毎回の検査は線形時間（宣言は数十個程度）で、問題にならない。
- **Follow-up**: monde-cli のテストで、本番の宣言が検査を通ることを 1 件確認する（monde-cli 側の作業）。

### Decision: 値は引数列の文字列を指し、リスト値だけをコピーする
- **Context**: 要件 3.3、4.2、5.1、10.1、10.5。
- **Alternatives Considered**:
  1. すべての値をコピーして結果が所有する
  2. 単一の値は `argv` の中（`=` の直後や連結値の先頭を含む）を指し、リスト値の項目だけをコピーする
- **Selected Approach**: 2。`--opt=value` の値も `-ovalue` の値も、引数の文字列の末尾部分なので NUL 終端のまま指せる。リスト値は区切るためにコピーが必要になる。
- **Rationale**: 確保の失敗点を減らし、引数列を書き換えない（10.5）。
- **Trade-offs**: 引数列は結果より長く生存する必要がある。`mnd_args_argv_t` を使う場合も同じ。
- **Follow-up**: ヘッダのコメントで寿命の約束を明記する。

### Decision: ヘルプの優先は回復走査で実現する
- **Context**: 要件 7.4（最初の誤りだけを報告する）と 8.2（`--help` は他の誤りより優先する）。
- **Alternatives Considered**:
  1. 解析の前に、`--` までのすべての引数から `--help`/`-h` を探す
  2. 通常の走査で最初の誤りを記録し、その後は `--` までの完全一致の `--help`/`-h` だけを探す
- **Selected Approach**: 2。誤りの前は通常の規則で走査するため、`-o -h` のように値として消費された `-h` はヘルプにならない。誤りの後は値の対応が信頼できないため、完全一致のトークンだけを見る。
- **Rationale**: 誤りがない引数列では、選択肢 1 よりも正確（値として渡した `-h` を誤ってヘルプにしない）。
- **Trade-offs**: 誤りの後に値として書かれた `-h` はヘルプとみなされる。利用者にとっては無害な挙動である。

### Decision: Windows では `CommandLineToArgvW` を使う
- **Context**: 要件 10.2〜10.4。
- **Alternatives Considered**:
  1. `wmain` / `-municode`
  2. `GetCommandLineW` と `CommandLineToArgvW`
  3. コマンドラインの分割を自前で実装する
- **Selected Approach**: 2。`main` の `argc`/`argv` は Windows では使わない。
- **Rationale**: エントリポイントを変えずに済み、分割の規則を自前で持たない。shell32 は OS 標準の DLL で、外部の実行時ライブラリには当たらない。
- **Trade-offs**: CRT と分割規則がごく一部で異なる。shell32 へのリンクが増える。
- **Follow-up**: CI の MSVC と MinGW-w64 の両方で、非 ASCII の引数を扱う変換テストを実行する。

### Decision: UTF-8 と UTF-16 の相互変換を foundation の `wide` に置く
- **Context**: UTF-8 から UTF-16 への変換は `file_io.c` の `static` 関数として既にある。cli-args は逆向き（UTF-16 から UTF-8）を必要とする。
- **Alternatives Considered**:
  1. cli-args の `args_argv.c` の中で `WideCharToMultiByte` を直接呼ぶ
  2. `file_io.c` に逆向きの変換を追加して公開する
  3. foundation に新しい部品 `wide`（`wide.h` / `wide.c`）を作り、両方向の変換をまとめる
- **Selected Approach**: 3。`file_io.c` の既存の変換は `wide` を使う形に置き換える。
- **Rationale**: 同じ関心事（Windows の API との境界での文字コード変換）を 1 か所にまとめる。2 では、ファイル入出力の部品が文字コード変換の窓口を兼ねてしまう。1 では、変換の規則（不正な入力を置換せずに失敗とする）が 2 か所に分かれる。
- **Trade-offs**: 実装済みの foundation に手を入れる。`file_io` の公開 API と挙動は変えず、既存のテストで回帰がないことを確認する。`wide.c` は Windows でだけビルドに加える（空の翻訳単位は `-Wpedantic` で警告になる）。
- **Follow-up**: steering の予約済み短縮名に `wide` を追記する。

### Decision: 自前実装（既存ライブラリを採用しない）
- **Context**: Build vs. Adopt の検討。
- **Alternatives Considered**: getopt（MSVC にない）、`argparse`（C、MIT）、`cargs`（C、MIT）などのコピーによる取り込み。
- **Selected Approach**: 自前で実装する。
- **Rationale**: steering で、実行時の外部依存をゼロにし、引数パーサも自前で実装すると決まっている。既存のライブラリは、サブコマンド、リスト値、ヘルプの優先、UTF-8 の取得という要件の組み合わせを満たさない。

### Synthesis: 一般化と単純化
- **一般化**: 長いオプション、短いオプション、まとめ書きは、どれも「オプションの宣言を 1 つ特定し、必要なら値を 1 つ取る」処理に帰着する。値の記録（単一は後勝ち、リストは連結、フラグは指定済み）は種類ごとの 1 つの関数にまとめる。
- **一般化**: 「指定がない」「既定値」「明示的な指定」は、オプションごとの値スロット 1 つで表す（`explicit_set` と値の組）。
- **単純化**: グローバルオプションの宣言の仕組みは作らない。サブコマンドより前に書けるのは、予約された `--help`、`-h`、`--version` だけとする（要件 2.3）。
- **単純化**: ヘルプの文面は 1 つの整形関数で生成し、列の幅は宣言から計算する。端末の幅による折り返しはしない（要件 9.6）。
- **単純化**: 解析結果の取得は長い名前で行う。宣言の配列の添字を外部に見せない。

## Risks & Mitigations
- `CommandLineToArgvW` と CRT の分割の差 — monde の利用場面（ファイルパスとキー名）では影響しない。差を README で扱う必要が出たら docs-and-release で記載する。
- 回復走査で、値として書かれた `-h` がヘルプになる — 誤りがある場合に限られ、ヘルプの表示は無害である。テストで挙動を固定する。
- 宣言の誤りは実行時にしか検出できない — 解析の呼び出しごとに検査し、宣言の誤りとして区別して返す（1.6）。monde-cli のテストで本番の宣言を一度通す。

## References
- [POSIX.1-2017 XBD 12.2 Utility Syntax Guidelines](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html) — 短いオプションのまとめ書き、値の連結、`--`、`-`
- [getopt(3) — Linux manual page](https://man7.org/linux/man-pages/man3/getopt.3.html) — `getopt_long` の挙動、並べ替え
- [CommandLineToArgvW (Microsoft Learn)](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-commandlinetoargvw) — Windows での引数の分割
- [WideCharToMultiByte (Microsoft Learn)](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte) — `WC_ERR_INVALID_CHARS`
- [Parsing C command-line arguments (Microsoft Learn)](https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments) — CRT の分割規則

---

# ギャップ分析: サブコマンドのない形式への対応（2026-11-10）

改訂後の `requirements.md`（Requirement 1〜13）と、既存の実装（`src/cli_args/`、`src/cli/`、`tests/cli_args/`、`tests/cli/`、`tests/docs/`）との差分を調べた。要件は未承認の状態で分析している。

## Summary

- 既存の実装は「プログラム → サブコマンド 1 段」を型・解析・結果・ヘルプ・エラー文面のすべてで前提にしている。変更は公開 API 全体に及ぶが、部品の分割（宣言の検査、解析、結果、ヘルプ、argv の取得）はそのまま使える。
- 末端コマンドの中の解析（長短のオプション、リスト値、位置引数、`--`、回復走査）は、ほぼ変更なしで再利用できる。差分は「コマンドの選択」「有効なオプションの解決」「経路をまたぐ結果の保持」「ヘルプの一本化」に集中する。
- **要件どうしの衝突を 3 件見つけた**（後述）。いずれも Requirement 13（`monde` の挙動を変えない）と、Requirement 8・9 の新しい規則との間にある。設計の前に要件で決める必要がある。
- `monde` が引き継ぐオプションを使うと Requirement 13 に反する。monde-cli は今回、共通オプションを各サブコマンドに重複して宣言したままにする必要がある。
- 規模は M（3〜7 日）、リスクは Medium。作業量の多くはテスト（`tests/cli_args/` の 196 件）の宣言の書き換えである。

## Current State

### 資産と構成

| 部品 | ファイル | 行数 | 役割 |
| --- | --- | --- | --- |
| 公開 API | `src/cli_args/args.h` | 116 | 宣言の型、状態、結果、取得関数、`mnd_args_parse` |
| DeclCheck | `args_decl.c` | 334 | 宣言の検査と、サブコマンド・オプションの検索 |
| Parser | `args_parse.c` | 572 | 入口の振り分け、サブコマンド選択後の走査、回復走査、検査 |
| ResultStore | `args_result.c` | 378 | 値の保持と取得 |
| HelpFormatter | `args_help.c` / `.h` | 389 | 全体のヘルプ、サブコマンドのヘルプ、バージョン、案内 |
| argv の取得 | `args_argv.c` / `.h` | 184 | Windows での UTF-8 化 |
| テスト | `tests/cli_args/*.c` | 3,800 | 196 件。架空のプログラム `tool`（`test_decl.c`）を使う |

利用側は monde-cli だけである。

- `src/cli/cli_commands.c`: 宣言のテーブル（位置による初期化子）と、サブコマンド名からハンドラを引く表。
- `src/cli/cli_run.c`: 状態ごとの振り分け。`result->command->name` でハンドラを引く。
- `src/cli/cli_report.c`: 診断の接頭辞 `<program> <command>: ` を取り除き、案内の行を付ける。
- `src/cli/cmd_*.c`、`cli_io.c`: `mnd_args_flag` などで値を取得する（選択されたコマンドだけを見る）。
- `tests/cli/run_test.c`: `program.commands[]` と `option_count` を直接たどって宣言を検証する。
- `tests/docs/docs_check.cmake`: README と `docs/commands.md` に引用した `monde --help`、`monde <sub> --help` の出力をバイト単位で比べる。

### サブコマンド 1 段の前提が埋め込まれている箇所

| 箇所 | 現状 |
| --- | --- |
| `mnd_args_program_t` | `commands[]` だけを持つ。プログラム直下のオプションと位置引数を表せない |
| `mnd_args_command_t` | 子コマンドを持てない。オプションに「引き継ぐ」の属性がない |
| `mnd_args_parse` の入口 | `argv[1]` だけを見て `--help` / `-h` / `--version` / `help` / サブコマンド名に振り分ける。サブコマンドの前のオプションはすべて未知のオプションになる |
| `select_command` | 走査の開始位置が `scan.next = 2` に固定されている |
| `mnd_args_scan_t` | コマンドを 1 つだけ持つ。オプションの検索は `mnd_args_find_long(command, ...)` で、そのコマンドの表だけを見る |
| `mnd_args_result_t` | `values` は `command->option_count` 個。取得関数は選択されたコマンドの表を `long_name` で引く |
| `MND_ARGS_NO_COMMAND` | 引数列が空のときだけ返す。`result->command` は NULL |
| `dispatch_help` | `help` の後は 1 語まで（`argc > 3` は `too many arguments for 'help'`） |
| `report_pending` | 接頭辞が `"%s %s: "`（プログラム名とサブコマンド名）に固定されている |
| `mnd_args_format_help` | `command == NULL` が全体のヘルプを表す。全体用とサブコマンド用で別の関数と別の書式を持つ |
| `mnd_args_format_usage_hint` | `Run '<program> --help'` と `Run '<program> <command> --help'` の 2 通りだけ |
| `mnd_args_decl_check` | プログラム → コマンド → オプションの 2 重ループ。重複の検査は 1 つのコマンドの中だけ |

### そのまま使える部分

- 末端コマンドの中の走査（`scan_long`、`scan_short`、`scan_positional`、`scan_tokens`、`scan_record_value`）。オプションの検索を「有効なオプション」に差し替えれば、規則は変わらない（Requirement 3〜6）。
- 回復走査（`recover_help`）と、誤りを保留してから診断にする仕組み（Requirement 7.4、8.3）。
- リスト値の分割と既定値の適用（`mnd_args_result_append_list`、`mnd_args_result_apply_defaults`）。
- ヘルプの行の整形（`append_option_row`、`append_text_row`、`append_positional_usage`、列の揃え方）。
- `args_argv.c`（Requirement 10）。変更は不要である。
- 規約: 宣言は呼び出し側が持つ `static const` のデータ、診断は foundation の `mnd_diag_list_t`、出力は `mnd_buf_t`。

## Requirement-to-Asset Map

| Requirement | 既存の資産 | ギャップ |
| --- | --- | --- |
| 1 宣言 | `args.h` の型、`args_decl.c` | **Missing**: ルートコマンド、子コマンドの入れ子、引き継ぐ属性。**Missing**: 祖先との名前の重複、子を持つコマンドの位置引数、子の名前 `help` の検査。**Constraint**: 宣言が位置による初期化子で書かれていて、型の変更で利用側とテストの全テーブルが壊れる |
| 2 コマンドの選択 | `mnd_args_parse`、`select_command` | **Missing**: ルートからの逐次の選択、任意の深さ、子を持つコマンドでの `--`、末端のルート。**Missing**: 結果に経路を持たせる手段 |
| 3、4 オプション | `scan_long`、`scan_short` | 規則は充足。**Missing**: 有効なオプション（自分の表 + 祖先の引き継ぐオプション）の解決。子の選択前（子を持つコマンドの走査中）にもオプションを解析する経路がない |
| 5 値 | `args_result.c` | 5.1〜5.6 は充足。**Missing**: 5.7（経路上のどのコマンドのオプションも取得できること） |
| 6 位置引数 | `scan_positional`、`post_check` | 充足。6.6（末端のルートで引数なし）は入口の `argc <= 1` の分岐が妨げる |
| 7 エラー | `report_pending`、`mnd_args_format_usage_hint` | **Missing**: 経路を含む接頭辞と案内。7.1 は経路上のコマンドの必須オプションへ広げる必要がある |
| 8 ヘルプとバージョン | 入口の分岐、`dispatch_help`、`recover_help` | **Missing**: どのコマンドでも効く `help <経路>`、子の選択前の `--help`。**衝突あり**（8.3、8.9。後述） |
| 9 文面 | `format_global_help`、`format_command_help` | **Missing**: 一本化した書式、引き継いだオプションの表示、経路を含む使い方の行。**衝突あり**（9.1。後述） |
| 10 argv | `args_argv.c` | 充足。変更なし |
| 11 テスト | `tests/cli_args/` | **Missing**: サブコマンドなしと 2 段以上の構成のテスト用の宣言。既存の 196 件は宣言の書き換えが必要 |
| 12 引き継ぐオプション | なし | **Missing**: すべて新規 |
| 13 monde-cli の追従 | `src/cli/`、`tests/cli/`、`tests/docs/` | 宣言と取得の書き換え。**Constraint**: ヘルプの文面はバイト単位で `docs/` と比べられている |

## 要件どうしの衝突（設計の前に決めること）

既存の `monde` の挙動を調べた結果、新しい規則と Requirement 13 が両立しない箇所が 3 つある。

### 衝突 1: `--version` の後ろに引数がある場合（8.9 と 13.2）

- **現状**: 入口は `argv[1]` が `--version` かどうかだけを見る。`monde --version extra` はバージョンを表示して終了コード 0 を返す。
- **新しい要件**: 8.9 は「`--version` が他の引数とともに指定されている」場合を使い方のエラーとする。
- **13.2**: 改訂前に受け付けていたコマンドラインは、改訂後も同じ出力と終了コードを返す。
- **選択肢**:
  - (a) 8.9 を緩め、ルートコマンドが選択中の間は `--version` をどの位置でも受け付ける（`--help` と同じ扱い）。末端のルートでも規則が 1 つになる。
  - (b) 13.2 に例外を設け、`monde --version extra` をエラーに変える。

### 衝突 2: 未知のサブコマンドの後ろの `--help`（8.3 と 13.4）

- **現状**: 回復走査はサブコマンドを選択した後にだけ働く。`monde bogus --help` は `unknown command 'bogus'` で終了コード 2 を返す。
- **新しい要件**: 8.3 は未知のサブコマンドのエラーも回復の対象に含める。`monde bogus --help` はルートのヘルプで終了コード 0 になる。
- **13.4**: 改訂前に使い方のエラーだったコマンドラインは、改訂後も使い方のエラーとする。
- **選択肢**:
  - (a) 13.4 に例外を設け、新しい挙動（ヘルプを優先する）を採る。規則は「誤りの種類によらずヘルプが勝つ」の 1 つになる。
  - (b) 8.3 から未知のサブコマンドを除く。現状と同じになるが、誤りの種類で扱いが分かれる。

### 衝突 3: ヘルプの書式の一本化（9.1〜9.4 と 13.3）

- **現状**: 全体のヘルプとサブコマンドのヘルプは別の書式である。全体のヘルプには固有の要素がある。
  - 使い方の行: `Usage: monde <command> [<options>] [<arguments>]`
  - `Commands:` の一覧の末尾に `help` の行（疑似サブコマンド）
  - 末尾の案内: `Run 'monde <command> --help' for help on a command.`
- **新しい要件**: 9.1〜9.4 はコマンドごとのヘルプを 1 つの規則で定める。`help` の行と末尾の案内には触れていない。
- **13.3**: `monde --help` の文面は README と `docs/commands.md` の引用と一致させる（`tests/docs/` が検査する）。
- **選択肢**:
  - (a) 「子を持つコマンドのヘルプ」の書式を、現状の全体のヘルプと同じ出力になるように定める（`help` の行と末尾の案内を、子を持つコマンドの共通の要素として要件に加える）。ドキュメントは変わらない。
  - (b) 書式を新しく定め、README と `docs/commands.md` の引用を同じ変更で直す（13.3 は「引用を実際の出力に合わせる」と読み替える）。

### monde-cli での引き継ぐオプションの扱い（制約）

`--output`、`--quiet`、`--no-wrap` は現在、各サブコマンドに重複して宣言されている（`cli_commands.c` のマクロ）。これをルートの引き継ぐオプションに移すと、次の 2 点で Requirement 13 に反する。

- `monde --quiet extract FILE` が受け付けられるようになる（13.4 に反する）。
- `monde --help` の `Options:` に `--quiet` などが現れ、各サブコマンドのヘルプでのオプションの順序も変わる（13.3 に反する）。`--no-wrap` は `translate` にないので、そもそも一律には引き継げない。

今回の改訂では、monde-cli は重複した宣言を保つことになる。引き継ぐオプションの検証は、テスト用の宣言だけで行う（Requirement 11.1 のとおり）。

## Implementation Approach Options

### Option A: 既存の部品をその場で作り替える

`args.h` の型を新しい形に置き換え、`args_decl.c`、`args_parse.c`、`args_result.c`、`args_help.c` をそれぞれ書き換える。部品の分割とファイル名は保つ。

- **変更するファイル**: `src/cli_args/` の `args_argv.*` を除く全部、`tests/cli_args/` の `argv_test.c` を除く全部、`src/cli/cli_commands.c`、`cli_run.c`、`cli_report.c`、`cli_internal.h`、`tests/cli/run_test.c`、`report_test.c`。
- ✅ 部品の境界が今の要件にも合っている。再利用できる走査と整形のコードをそのまま残せる。
- ✅ ファイルが増えない。利用側は 1 つなので、互換の層がいらない。
- ❌ 型を変えた時点で、モジュールとテストの全体がビルドできなくなる。途中の状態で CI を通せず、1 つの大きな変更になりやすい。
- ❌ `args_parse.c` は 572 行で、コマンドの選択と経路の管理を加えると見通しが悪くなる。

### Option B: 新しいモジュールを並べて作り、切り替えてから古いほうを消す

新しい API を別のファイル群（または別の名前の型と関数）で作り、テストをそろえてから monde-cli を切り替え、最後に古い実装を削除する。

- ✅ 途中のどの時点でもビルドとテストが通る。古いテストを仕様の参照として最後まで使える。
- ✅ 新しい設計を古い構造に引きずられずに決められる。
- ❌ 走査と整形のコードを一時的に 2 つ持つ。同じ短縮名 `args` を使えない期間ができ、名前の付け替えが 2 回要る。
- ❌ 利用側が 1 つしかない内部モジュールに対して、移行の手間が大きい。steering の「1 spec = 1 モジュール」の形からも一時的に外れる。

### Option C: その場で作り替えるが、責務を分けて段階的に進める（ハイブリッド）

Option A と同じくその場で置き換えるが、追加する責務を新しいファイルに分け、タスクをビルドが通る単位で並べる。

- **新しいファイルの候補**: コマンドの選択と経路の管理（例: `args_path.c`）、有効なオプションの解決（`args_decl.c` に残すか分けるかは設計で決める）。
- **段階の例**:
  1. 型と宣言の検査を新しい形にする。既存の 1 段の構成を新しい型で表し、既存のテストの宣言を書き換えて通す（挙動は変えない）。
  2. コマンドの選択を一般化する（末端のルート、任意の深さ、子を持つコマンドでのオプション）。
  3. 結果を経路に対応させ、引き継ぐオプションを加える。
  4. ヘルプと案内を一本化する。
  5. monde-cli を書き換え、`tests/cli/` と `tests/docs/` を通す。
- ✅ 各段階の終わりでテストが通る。レビューの単位が小さくなる。
- ✅ `args_parse.c` の肥大を避けられる。
- ❌ 段階 1 で、型だけ新しく挙動は古い中間の状態を作る必要がある。段階の切り方を設計で丁寧に決めないと、同じテストを 2 回書き換えることになる。

## Effort and Risk

- **Effort: M（3〜7 日）**。新しい技術はなく、既存の規約と部品をそのまま使える。量の中心はテストで、`decl_test.c`（820 行、63 件）と `help_test.c`（期待する文面のバイト比較）の書き換えと、新しい構成（サブコマンドなし、2 段以上、引き継ぐオプション）のテストの追加である。
- **Risk: Medium**。公開 API の全面的な変更だが、利用側は 1 つで、`tests/cli/` と `tests/docs/` が `monde` の挙動とヘルプの文面をバイト単位で守っている。主なリスクは上の 3 つの衝突を決めないまま設計に進むことと、ヘルプの文面の意図しない変化である。

## Recommendations for Design Phase

### 推奨する進め方

- **Option C** を軸に検討する。利用側が 1 つで互換の層がいらないため、Option B の並行期間は割に合わない。Option A は途中で CI を通せない点が弱い。
- 設計の前に、3 つの衝突を要件で決める。分析上は次の組み合わせが、規則が最も少なく `monde` への影響も小さい。
  - 衝突 1 は (a): `--version` を `--help` と同じ扱いにする。
  - 衝突 2 は (a): ヘルプを常に優先し、13.4 に例外を明記する。
  - 衝突 3 は (a): 子を持つコマンドのヘルプが現状の全体のヘルプと同じ出力になるように書式を定める。

### 設計で決めること

- **宣言の型**: プログラムとルートコマンドを 1 つの型にまとめるか、プログラム（名前とバージョン）がルートコマンドを指す形にするか。子コマンドの配列を型の再帰でどう表すか。
- **初期化子の書き方**: 位置による初期化子を続けるか、指示付き初期化子（C99。MSVC は `/std:c11` で使える）に切り替えるか。項目が増えるので、後者のほうが今後の追加に強い。全対応コンパイラでの警告の有無を確認する。
- **結果の保持**: 経路上の各コマンドの値をどう持つか（経路の段ごとの配列か、有効なオプションを平らに並べた 1 つの配列か）。名前での取得は、祖先との重複を禁じた（1.8）ので一意に決まる。
- **経路の表現**: 結果が経路を返す形（コマンドへのポインタの配列か、親へのたどり方か）。宣言は `static const` で親へのポインタを持たないため、結果の側で持つ必要がある。
- **ハンドラの対応付け**: monde-cli は名前でハンドラを引いている。入れ子では名前が一意でなくなるため、コマンドの宣言に利用側の識別子（整数や `void *`）を持たせるかどうか。
- **状態の列挙**: `MND_ARGS_NO_COMMAND` を「子コマンドの未指定」に意味を変えて残すか、名前を変えるか。
- **診断の接頭辞**: 経路を含む接頭辞の形と、`cli_report.c` の `strip_usage_prefix` との取り決め。接頭辞を文字列でやり取りするのをやめ、パーサが接頭辞なしの本文を返す形も検討に値する。
- **宣言の検査**: 任意の深さの走査の方法（再帰か反復か）、深さの上限を設けるか、宣言の誤りのメッセージでコマンドをどう示すか。

### Research Needed

- 指示付き初期化子と入れ子の集成体の初期化が、MSVC（`/std:c11`）、MinGW-w64、Apple Clang、GCC、Clang のすべてで、現在の警告の設定のまま通ること。
- 子を持つコマンドの使い方の行の書式（例: `Usage: tool remote [<options>] <command> ...`）について、既存の CLI（git、cargo、docker など）の慣例。現状の `Usage: monde <command> [<options>] [<arguments>]` と両立する規則にできるか。
- 引き継ぐオプションをヘルプでどう見せるか（同じ `Options:` に並べるか、順序をどうするか）。Requirement 9.5 は「同じ項目で示す」としているので、順序の規則だけを決めればよい。

---

# 設計の調査と判断: サブコマンドのない形式への対応（2026-11-10）

## Summary

- **Feature**: `cli-args`（既存の実装の作り替え）
- **Discovery Scope**: Extension（軽量の調査。既存の実装と利用側の統合点を中心に調べた）
- **参照した指針**: `kiro-spec-design` の `design-principles.md`、`design-discovery-light.md`、`design-synthesis.md`、`design-review-gate.md`。steering の `tech.md`（部品の公開 API の互換性は保証しない、コーディングスタイル）と `structure.md`（命名、モジュールの構成）。追加の steering とドメイン別のスキルは、本機能には該当するものがなかった。
- **Key Findings**:
  - 改訂前の全体のヘルプとサブコマンドのヘルプは、「子を持つコマンド」「末端コマンド」「ルートコマンド」の 3 つの性質で要素を決める 1 つの書式として書ける。`monde` の構成では、改訂前とバイト単位で同じ文面になる。
  - 値を「宣言した段」に記録すれば、引き継ぐオプションの要件（前後どちらで指定しても同じ、繰り返しの規則、必須、既定値）は、特別な処理なしで既存の規則から導ける。
  - 入口の振り分けを走査に統合しても、`monde` で結果が変わるコマンドラインは 5 種類だけで、そのうち状態が変わるのは要件 8.3 による 1 種類だけである。

## 要件の衝突の解決

ギャップ分析で挙げた 3 件は、利用者の指示（`-y` での設計の開始）を受けて、分析で推奨した案を採って要件に反映した。

| 衝突 | 採った案 | 要件の変更 |
|---|---|---|
| 1: `--version` の後ろの引数 | (a) ルートコマンドが選択中の間は、`--version` を位置によらず受け付ける | 8.8、8.9 を書き換え |
| 2: 未知のサブコマンドの後ろの `--help` | (a) ヘルプを常に優先する | 13.4 に例外を明記 |
| 3: ヘルプの書式の一本化 | (a) 子を持つコマンドのヘルプに `help` の行と末尾の案内を含める | 9.2 に追記 |

## Research Log

### 指示付き初期化子の移植性
- **Context**: 宣言の型にメンバー（`inherited`、`commands`、`command_count`）を加える。位置による初期化子では、既存の宣言がすべて `-Wmissing-field-initializers`（`-Wextra`）の警告になり、今後のメンバーの追加でも同じことが起きる。
- **Findings**:
  - GCC と Clang で、入れ子の宣言（オプションの配列、子コマンドの配列、ルートコマンド、プログラム）を指示付き初期化子で書き、`-std=c99 -Wall -Wextra -Wpedantic -Werror -Wmissing-field-initializers` で警告なしにコンパイルできることを、この環境で確認した。指示付き初期化子で省いたメンバーは警告の対象にならない。
  - MSVC は C のソースで指示付き初期化子を受け付ける（Visual Studio 2013 以降）。この環境では確認できないため、CI の Windows のジョブで確認する。
- **Implications**: 宣言テーブルは指示付き初期化子で書く。どのメンバーも 0 が「なし」を表すように型を定める（`inherited` は偽が既定、`commands` は `NULL` が末端）。

### 改訂前の入口の挙動
- **Context**: 入口の振り分けを走査に統合するにあたり、`monde` の挙動が変わる範囲を特定する。
- **Findings**（`src/cli_args/args_parse.c` の `mnd_args_parse`）:
  - `argv[1]` が `--help` / `-h` / `--version` と完全に一致すれば、後続を見ずに確定する。
  - サブコマンドの前の誤りは、回復走査の対象にならない。
  - サブコマンドの前の未知のオプションは、`=` 以降を含むトークン全体を示す。
  - `help` の後は語数を先に検査する（3 語以上は `too many arguments for 'help'`）。
- **Implications**: 統合後に結果が変わる場合を design.md の「改訂前の 1 段の構成との違い」の表にまとめた。`tests/cli/run_test.c` の期待値の修正は、この表に当たるものに限る。

### 子を持つコマンドの使い方の行
- **Context**: ギャップ分析の Research Needed。
- **Findings**: 改訂前の `Usage: monde <command> [<options>] [<arguments>]` は、「コマンド名の後に、そのコマンドのオプションと引数が続く」という形である。子を持つコマンド自身がオプションを持つ場合は、経路の直後に ` [<options>]` を加えれば、git や cargo の慣例（`git [<options>] <command> [<args>]`）とも合う。
- **Implications**: 有効なオプションが 1 つ以上ある場合だけ ` [<options>]` を加える。オプションを持たないルート（`monde`）では、改訂前と同じ行になる。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|---|---|---|---|---|
| A: その場で作り替える | 既存の 4 つの部品を保ち、型と中身を置き換える | 境界が要件に合う。互換の層がいらない | 型の変更でモジュールが一度にビルドできなくなる | **採用**（段階の切り方は C の考えを取り入れた） |
| B: 新しいモジュールを並べる | 新旧を並行させて切り替える | 途中も常にビルドが通る | 利用側が 1 つしかなく、並行期間の手間に見合わない | 不採用 |
| C: 新しいファイルに責務を分ける | コマンドの選択と経路を別ファイルにする | `args_parse.c` の肥大を避ける | 走査の状態を 2 つのファイルで共有することになる | 一部採用（下の「Synthesis」を参照） |

## Design Decisions

### Decision: プログラムがルートコマンドを指す
- **Alternatives**: (1) プログラムとルートコマンドを 1 つの型にまとめる。(2) プログラム（名前とバージョン）がルートコマンドを指す。
- **Selected**: (2)。
- **Rationale**: monde-cli は、宣言を複製してバージョンだけを実行時の値に差し替えている。(2) なら、ルートコマンド以下の木を `static const` のまま保てる。名前とバージョンはプログラムに 1 つだけの属性で、コマンドの型に入れると子コマンドでは使わないメンバーになる。
- **Trade-offs**: ルートコマンドの `name` と `summary` は使わない（`NULL` でよい）。

### Decision: 経路は解析結果が段の配列で持つ
- **Alternatives**: (1) 宣言に親へのポインタを持たせる。(2) 結果が段の配列を持つ。(3) 深さの上限を決めて固定長の配列にする。
- **Selected**: (2)。配列は必要に応じて確保する。
- **Rationale**: 宣言は `static const` で、親へのポインタは循環する初期化子を要求する。(3) は要件 1.3（任意の深さ）に反する。
- **Trade-offs**: コマンドの選択のたびに確保が起きうる。深さは高々数段なので問題にならない。

### Decision: 値は宣言した段に記録する
- **Alternatives**: (1) 有効なオプションを選択中のコマンドごとに平らな配列へ複製する。(2) 宣言した段に 1 つだけ記録する。
- **Selected**: (2)。
- **Rationale**: 引き継ぐオプションを子コマンド名の前後で指定した場合に、同じ場所へ書くことになる。要件 12.3〜12.6 が、既存の繰り返し・必須・既定値の規則からそのまま導ける。(1) は、子コマンドを選択するたびに値を移す処理が要る。
- **Trade-offs**: 名前による取得は経路をさかのぼって探す。祖先との名前の重複を宣言の誤りにした（要件 1.8）ので、結果は一意である。

### Decision: 入口の振り分けをなくし、1 つの走査に統合する
- **Rationale**: 「サブコマンドの前」と「サブコマンドの後」を分ける構造が、サブコマンドのない形式と入れ子を妨げていた。ルートコマンドを最初から選択しておけば、`--help`、`--version`、子コマンドの選択、`help` の語は、すべて走査の表の 1 行になる。回復走査も、誤りの種類を問わず同じ処理で働く（要件 8.3）。
- **Trade-offs**: 1 段の構成で 5 種類のコマンドラインの結果が変わる（design.md の表）。

### Decision: ヘルプと案内の関数は解析結果を受け取る
- **Alternatives**: (1) `(program, command)` の組を受け取る（改訂前）。(2) 経路を表す別の型を公開する。(3) 解析結果を受け取る。
- **Selected**: (3)。
- **Rationale**: 使い方の行と案内には経路が要り、`Options:` には祖先の引き継ぐオプションが要る。どちらも結果の段の列に入っている。ヘルプを生成する場面（`HELP`、`NO_COMMAND`、`USAGE_ERROR`）では、呼び出し側は必ず結果を持っている。
- **Trade-offs**: 解析を経ずに任意のコマンドのヘルプを生成する入口はない。テストは `help <経路>` の解析結果を使う。

### Decision: 診断の接頭辞は残し、経路の文字列を生成する関数を公開する
- **Alternatives**: (1) 接頭辞をやめ、パーサは本文だけを返す。(2) 接頭辞を残す。
- **Selected**: (2)。`mnd_args_format_path` を加える。
- **Rationale**: 要件 7.2 が、メッセージにコマンドの経路を含めることを求めている。monde-cli は接頭辞を取り除いて自分の形式で表示しているが、取り除く文字列を自分で組み立てる代わりに、同じ形を返す関数を使えばよい。
- **Trade-offs**: 接頭辞の形の変更は、monde-cli の再検証を要する（Revalidation Triggers に記載）。

### Decision: 宣言の循環を検出する
- **Rationale**: 子コマンドの配列は任意の深さを許すため、誤った宣言（自分自身を子に持つ）で検査が終わらなくなる。深さの上限を設ける代わりに、検査中の祖先の列と照合して循環を検出する。上限がないので、要件 1.3 を制限しない。

### Decision: 利用側の識別子は持たせない
- **Rationale**: monde-cli は名前でハンドラを引いており、1 段の構成では一意である。入れ子で必要になったら、メンバーを加えればよい（指示付き初期化子なので、既存の宣言は壊れない）。現在の要件にない仕組みは入れない。

### Synthesis: 一般化、既存の採用、単純化
- **一般化**: 「全体のヘルプ」と「サブコマンドのヘルプ」、「サブコマンドの前の解析」と「後の解析」、「共通オプションの重複した宣言」は、いずれも「コマンドの木の上の 1 つの規則」の特別な場合である。型と走査と書式をそれぞれ 1 つにした。
- **既存の採用**: 自前で実装する方針（改訂前の判断。外部の実行時依存を持たない）は変えない。末端コマンドの中の走査、回復走査、リスト値、行の整形は既存のコードを再利用する。
- **単純化**: ギャップ分析の Option C が挙げた「コマンドの選択のための新しいファイル」は作らない。統合後のコマンドの選択は走査の表の 1 行で、入口の専用の処理（約 100 行）がなくなるため、`args_parse.c` の大きさはほぼ変わらない。利用側の識別子、深さの上限、互換の層も入れない。

## Risks & Mitigations
- **ヘルプの文面の意図しない変化** — `help_test.c` の既存の期待値を変えないことと、`tests/docs/` が変更なしで通ることを完了の条件にする。
- **MSVC での指示付き初期化子** — CI の Windows のジョブで確認する。通らない場合は、宣言の書き方だけを位置による初期化子に戻せる（型の設計には影響しない）。
- **段階 1 の間は製品がビルドできない** — 2 つの段階を同じブランチで続けて行い、段階 2 が通るまで main に入れない。
- **`monde` の挙動の変化の見落とし** — 変わる場合を design.md の表に列挙した。`tests/cli/` の期待値の修正を、この表に当たるものに限ることをレビューで確認する。

---

# 判断の変更: ヘルプの優先の範囲（2026-11-10）

上の「要件の衝突の解決」の衝突 2 で採った案 (a)（誤りの種類によらずヘルプを優先する）を取り下げ、回復走査の範囲を**末端コマンドの解析中の誤り**に限った。上の記録のうち、衝突 2 に関する「採った案」と、「`monde` で状態が変わるのは 1 種類」という記述は、この節で置き換える。

## 調査: 一般的なコマンドの挙動

誤りの後ろに `--help` がある場合の挙動を、この環境のコマンドで確かめた。

| コマンドライン | 結果（終了コード） |
|---|---|
| `git bogus --help`、`git --bogus --help` | エラー（1、129） |
| `cargo bogus --help`、`cargo --bogus --help` | エラー（101、1） |
| `npm bogus --help`、`go bogus --help` | エラー（1、2） |
| `ls --bogus --help` | エラー（2） |
| Python argparse: `tool bogus --help` | エラー（2） |
| Python argparse: `tool --bogus --help`、`tool build --bogus --help` | ヘルプ（0） |

主流は「左から読んで最初の誤りで止まる」である。未知のサブコマンドの後ろの `--help` をヘルプにするコマンドは、確かめた範囲にはなかった。`--version` の後ろに引数があっても表示する点（`git`、`ls`、`tar`、`curl`、`npm`、`cargo`、argparse）と、サブコマンドの後の `--version` をエラーにする点（`cargo`、argparse）は、設計どおり一般的である。

## Decision: 回復走査は、末端コマンドが選択中の誤りに限る

- **Alternatives**: (a) 誤りの種類によらず回復する。(b) 末端コマンドが選択中の誤りだけ回復する。(c) 回復走査をやめる。
- **Selected**: (b)。利用者の指示による。
- **Rationale**: 改訂前の「サブコマンドの選択後だけ回復する」を、コマンドの木の上にそのまま一般化した規則である。`monde` の挙動が変わらず（`monde bogus --help` と `monde --bogus --help` はエラーのまま）、要件 13.4 の例外がいらない。入れ子でも、サブコマンド名の打ち間違い（`nest remote bogus --help`）に気付ける。(c) は主流に最も近いが、`monde extract --bogus --help` がヘルプになる既存の挙動を変えてしまう。
- **Trade-offs**: 子を持つコマンドでの未知のオプション（`tool --bogus --help`）は、argparse と違ってエラーになる。`git` と `cargo` とは同じである。
- **変更した箇所**: 要件 8.3 を書き換え、8.10 を加え、13.4 の例外を削った。設計は、回復走査の規則、状態遷移図、「改訂前の 1 段の構成との違い」の表（状態が変わる行を削除。残る 4 行はすべて文面だけの変更）、テストの項目を直した。タスクは 2.4 と 3.1 を直した。

---

# ギャップ分析: `--no-<name>` 形式の否定への対応（2026-11-11）

対象は Requirement 14 と、それに伴う 1.5、11.1 の変更。要件は未承認の状態で分析した。

## Summary

- 変更はすべて CLOpts（`cli_args/`）の中に収まる。Monde 側（`src/cli/`、`docs/`）は変えなくてよい。
- 足りないものは 5 つ: 宣言の印、宣言の検査 2 件、`--no-<name>` の照合、フラグの「有効・無効」の記録、ヘルプの `--[no-]<name>` の表示。
- いちばん大きな差は結果の持ち方である。今のフラグは「指定された」の 1 ビットしか持たず、`clopts_flag()` と `clopts_is_set()` が同じ値を返す。14.5 は 2 つを別々に取れることを求める。
- 既存のファイルを拡張する方法（Option A）で足りる。規模は S、リスクは Low。
- 設計で決めることは、結果の記録の形（公開の構造体にメンバーを足すか）と、公開 API の追加に伴う CLOpts のバージョンの扱いの 2 点。

## Current State

| 領域 | 場所 | 今の挙動 |
|---|---|---|
| 宣言の型 | `cli_args/include/clopts/clopts.h` の `clopts_option_t` | メンバーは 8 個。否定の印はない。利用側（`src/cli/cli_commands.c`、テスト、README の例、`tests/package/consumer.c`、`consumer_cxx.cpp`）はすべて指示付き初期化子で書いている |
| 宣言の検査 | `cli_args/src/clopts_decl.c` | 長い名前の重複を、同じコマンドの中と祖先との間で検査する（1.8）。フラグには `required` と `default_value` を許さない |
| 名前の照合 | `clopts_decl.c` の `clopts_find_long` | 有効なオプションの長い名前との完全一致だけ |
| 長いオプションの走査 | `cli_args/src/clopts_parse.c` の `scan_long` | `help`、`version`、`clopts_find_long` の順に照合し、どれにも一致しなければ未知のオプション。エラーには利用者が書いた形（`--name`）を使う |
| 短いオプションの走査 | `clopts_parse.c` | フラグは `clopts_result_set_flag` を呼ぶ |
| 結果の記録 | `cli_args/src/clopts_result.c`、`clopts_value_t` | フラグは `explicit_set = true` にするだけ。`clopts_flag()` と `clopts_is_set()` はどちらも `explicit_set` を返す |
| ヘルプ | `cli_args/src/clopts_help.c` の `option_left_width` と左列の出力 | 左列は `-c, --name` または `    --name`。幅の計算と出力が別の関数に分かれている |
| テスト | `cli_args/tests/` | ヘルプはバイト単位で比較する。`inherit_test.c` は `values[].explicit_set` を直接読む |

`monde` の `--no-wrap` と `--no-fuzzy-matching` は、`no-wrap`、`no-fuzzy-matching` という名前のフラグである（`src/cli/cli_commands.h`）。

## Requirement-to-Asset Map

| 要件 | 対応する資産 | 状態 |
|---|---|---|
| 1.5、14.1 否定の形を受け付けるかの宣言 | `clopts_option_t` | **Missing**: メンバーの追加が要る |
| 14.2 `--no-<name>` で無効を記録 | `scan_long`、`clopts_find_long` | **Missing**: `no-` を外した再照合と、無効の記録 |
| 14.3 `no-<name>` という名前のオプションを優先 | `scan_long` | 今の照合順のままで満たせる（先に完全一致を見る） |
| 14.4 後に書いた指定の優先 | `clopts_result_set_flag` | **Missing**: 肯定の形が「有効」に戻す処理。短い名前とまとめ書きの経路も同じ関数を通るので、1 か所で足りる |
| 14.5 有効・無効と、明示的な指定の有無を別々に取得 | `clopts_value_t`、`clopts_flag`、`clopts_is_set` | **Missing**: 記録が 1 ビットしかない |
| 14.6 指定がなければ無効・指定なし | `clopts_value_init` | 初期値のままで満たせる |
| 14.7 `--no-<name>=<value>` のエラー | `scan_long`、`CLOPTS_ERR_TAKES_NO_VALUE` | 既存のエラーを使える。利用者が書いた形を示す仕組みもある |
| 14.8 否定できないものへの `--no-<name>` は未知のオプション | `scan_long` | 再照合で一致しなければ今の経路に落ちる |
| 14.9 引き継ぐフラグの否定 | `find_effective` | 有効なオプションの探索をそのまま使えば満たせる |
| 14.10 フラグ以外への否定の宣言 | `clopts_decl.c` のオプションの検査 | **Missing**: 検査の追加 |
| 14.11 `no-<name>` との衝突 | `clopts_decl.c` の重複の検査 | **Missing**: 検査の追加。祖先との比較の枠組みは 1.8 のものを使える |
| 14.12 ヘルプの `--[no-]<name>` | `clopts_help.c` | **Missing**: 幅の計算と出力の 2 か所 |
| 14.13 否定できるフラグがなければ今と同じ | 既存のテスト一式 | **Constraint**: 既存のテストを変えずに通すことが検証になる |
| 11.1 テスト用の宣言で検証 | `cli_args/tests/test_decl.c` | **Missing**: 否定できるフラグを持つテスト用の宣言 |

## 制約と気付いた点

- **結果の構造体は公開されている**: `clopts_value_t` は `clopts.h` にあり、テストが直接読んでいる。メンバーを足すと構造体の大きさが変わる。値の配列は CLOpts が確保するので、利用側のソースは変えずに済む。
- **宣言の型へのメンバーの追加**: 末尾に足せば、指示付き初期化子で書いた既存の宣言はそのまま通る。C++ の利用例（`consumer_cxx.cpp`）は、指示付き初期化子がメンバーの宣言順を要求するので、末尾への追加なら影響しない。
- **`clopts_flag()` の意味**: 否定できないフラグでは「指定された」と「有効」が一致するので、「有効」を返すように変えても 14.13 を保てる。
- **衝突の検査の向き（14.11）**: 否定できるフラグが祖先にあり `no-<name>` が子孫にある場合と、その逆の両方を見る必要がある。今の検査は、各コマンドのオプションを祖先のオプションと比べる形なので、比べる条件を足せば両方の向きを 1 回の走査で見られる。
- **ヘルプの列幅**: `[no-]` の 5 バイト分、左列が広がる。否定できるフラグが最も長い行になると、同じコマンドのほかの行の説明の開始位置も変わる。否定できるフラグを使わないコマンドには影響しない。
- **ヘルプの優先（8.3）**: 回復の走査は `--help` / `-h` との完全一致だけを見るので、`--no-<name>` の追加の影響を受けない。
- **`--` の後**: `--no-<name>` は位置引数になる（6.3）。追加の処理は要らない。
- **要件の記述の古さ**: 要件の Adjacent expectations と 7.5 は「project-foundation の診断として返す」と書いているが、clopts-standalone の後は `clopts_errors_t` で返している。今回の改訂とは別の話で、挙動には関係しない。

## Implementation Approach Options

### Option A: 既存のファイルを拡張する

宣言の型にメンバーを 1 つ足し、`clopts_decl.c`、`clopts_parse.c`、`clopts_result.c`、`clopts_help.c` のそれぞれに処理を足す。

- **結果の記録の形**は 2 通りある。
  - **A1**: `clopts_value_t` に、フラグが有効かどうかのメンバーを足す。意味が明確だが、公開の構造体の大きさが変わる。
  - **A2**: 構造体は変えず、フラグでは使っていない `value` を「有効」の印に流用する。構造体は変わらないが、メンバーの意味が種類によって変わり、読み手に分かりにくい。
- ✅ 変更が小さく、今の責務の分け方（宣言の検査、走査、結果、ヘルプ）にそのまま乗る
- ✅ 新しいファイルが要らず、`CMakeLists.txt` と単独性の検査を変えずに済む
- ❌ 否定に関する処理が 4 つのファイルに散らばる

### Option B: 否定の処理を新しいファイルにまとめる

`clopts_negate.c` のようなファイルに、`no-` の照合、衝突の検査、ヘルプの表記をまとめ、既存のファイルから呼ぶ。

- ✅ 否定に関する規則を 1 か所で読める
- ❌ 各処理は数行から十数行で、既存のファイルの内部の型（走査の状態、検査の枠組み、バッファ）に依存する。切り出すと内部ヘッダの宣言が増え、かえって見通しが悪くなる
- ❌ 結果の記録とヘルプの幅の計算は、結局既存の関数の中を変える必要がある

### Option C: 否定の形を隠しオプションとして展開する

解析の前に、否定できるフラグごとに `no-<name>` という内部のオプションを作り、既存の照合と重複の検査に乗せる。

- ✅ 照合と重複の検査を今のコードのまま使える
- ❌ 宣言は `static const` で、今は確保せずに参照している。展開のために宣言の写しを確保する必要があり、メモリ不足の経路が増える
- ❌ 後に書いた指定の優先のために、2 つのオプションを結び付ける処理が結局要る。ヘルプでも隠す処理が要る

## Effort and Risk

- **Effort: S（1〜3 日）**。既存の型と関数の拡張で、新しい依存も外部の調査もない。量の多くはテストである（走査、引き継ぎ、宣言の誤り、ヘルプ）。
- **Risk: Low**。変更は `cli_args/` の中に閉じ、否定できるフラグを使わない宣言の挙動は既存のテストで守られる。公開の型の変更だけが利用側に波及しうる。

## Recommendations for Design Phase

- **有力な進め方**: Option A。結果の記録は A1（メンバーの追加）のほうが読みやすい。CLOpts はまだ 0.1.0 で、利用側は Monde だけなので、構造体の変更の影響は小さい。
- **設計で決めること**:
  - 結果の記録の形（A1 か A2 か）と、`clopts_flag()` と `clopts_is_set()` の意味の定義。
  - 宣言の型に足すメンバーの名前と位置（末尾）。
  - 宣言の誤りのメッセージの文面（14.10、14.11）。既存の `option '--%s' ... in command '%s'` の形に揃える。
  - 公開 API の追加に伴い、`project(CLOpts VERSION ...)` を上げるかどうか。
  - `cli_args/README.md` の機能の一覧と例に、否定できるフラグを加える範囲。
- **Research Needed**:
  - ヘルプの左列で、否定できるフラグが値付きオプションより長くなる場合の列幅を、テスト用の宣言で確かめる。
  - `consumer_cxx.cpp`（C++ からの利用の検証）が、メンバーの追加の後もそのまま通ることを確かめる。
  - 要件が決めていない端の場合の扱いを確かめる: 否定できるフラグの名前が `no-` で始まる場合（`--no-no-<name>`）と、`--no-` だけの引数。

---

# 設計の調査と判断: `--no-<name>` 形式の否定への対応（2026-11-11）

discovery の種類は light（既存の部品の拡張）。外部の調査は行っていない。設計の原則、統合の観点、レビューゲートは kiro-spec-design の rules に従った。出発点は、上のギャップ分析である。

## Summary

- ギャップ分析の Option A（既存のファイルの拡張）を採った。新しいファイルはテストの 1 つだけである。
- 結果の記録は A1（`clopts_value_t` に `enabled` を足す）とした。
- 照合の順序は「完全一致、次に否定の形」とした。`no-wrap` のような既存のフラグを変えないためである。
- CLOpts のバージョンを 0.2.0 に上げる。

## Design Decisions

### Decision: フラグの有効・無効を `clopts_value_t.enabled` に持つ
- **Alternatives**: (a) メンバーを足す。(b) フラグでは使っていない `value` を印に流用する。(c) 取得関数を増やして構造体は隠す。
- **Selected**: (a)。
- **Rationale**: 意味が名前から読める。値の配列は CLOpts が確保するので、利用側のソースは変わらない。CLOpts は 0.1.0 で、利用側は Monde だけである。
- **Trade-offs**: 公開の構造体の大きさが変わる。バージョンを上げて表す。(c) は構造体を非公開にする変更が要り、今回の範囲を超える。

### Decision: `clopts_flag` は「有効」を、`clopts_is_set` は「明示的な指定」を返す
- **Rationale**: 改訂前は 2 つが同じ値を返していた。否定できないフラグでは今後も一致するので、既存の利用側の結果は変わらない（14.13）。取得関数を増やさずに 14.5 を満たせる。

### Decision: 否定できるフラグは、既定で有効と宣言できる（設計の見直しで追加）
- **Context**: 最初の設計では、フラグは指定がなければ必ず無効だった。否定の主な用途は既定で有効の機能（折り返しなど）なので、利用側が `clopts_is_set(r, "x") ? clopts_flag(r, "x") : true` を読む箇所ごとに書くことになり、ヘルプにも既定が出なかった。利用者の指示で、要件 14.14〜14.17 を加えた。
- **Alternatives**: (a) 真偽のメンバー `default_enabled` を足す。(b) 既存の `default_value` に `"true"` などを書かせる。(c) 宣言には持たず、利用側の判定に任せる（最初の設計）。
- **Selected**: (a)。
- **Rationale**: 0 が「既定で無効」なので、既存の宣言に影響しない。既定が宣言にあるので、ヘルプに自動で出せる。(b) は、値を文字列のまま返す CLOpts に、文字列を真偽に解釈する規則を持ち込む。
- **Trade-offs**: 宣言の型のメンバーが 1 つ増える。否定できないフラグには許さない（無効にする手段がなく、常に有効になるだけのため）。ヘルプの注記は既定で有効のときだけ出し、既定で無効のフラグの文面は変えない。

### Decision: 照合は完全一致を先に、否定の形を後にする
- **Rationale**: 14.3 の要件である。今の `scan_long` の流れの最後に 1 段足すだけで済み、否定できるフラグを使わない宣言では、この段が必ず失敗して改訂前と同じ経路に落ちる。
- **Trade-offs**: 否定できる `x` と `no-x` を同時に宣言すると、否定の形に届かなくなる。これは宣言の誤りにして防ぐ（14.11）。

### Decision: 要件が決めていない端の場合は、特別な扱いをしない
- `--no-` だけの引数、`--no-help`、`--no-version` は、手順 3 で一致しないので `unknown option` になる。
- 名前が `no-` で始まる否定できるフラグ（`no-x`）は妥当とし、`--no-no-x` で無効にできる。禁止する規則を足すより、規則を増やさないほうを選んだ。

### Decision: 衝突の検査は、既存の重複の検査に条件を足す
- **Rationale**: 比べる相手（同じコマンドの中、祖先）が 1.8 と同じである。別の走査を作ると、同じ組を 2 回たどることになる。
- **Trade-offs**: 祖先の引き継がないオプションとの組も誤りにする。実際には同時に有効にならないが、1.8 の重複の検査と同じ厳しさに揃えた。

## Synthesis

- **Generalization**: 否定の形は「同じフラグへの、値が偽の指定」である。肯定と否定を 1 つの記録の関数（`clopts_result_set_flag(…, enabled)`）にまとめた。5.5（繰り返しを誤りにしない）と 14.4（後の指定の優先）は、この上書きから同時に出てくる。
- **Build vs. Adopt**: 外部のライブラリは使えない（steering の制約）。採る・作るの判断はない。
- **Simplification**: 取得関数、エラーの種類、解析の状態を増やさなかった。否定の処理を新しいファイルに分ける案（Option B）と、隠しオプションに展開する案（Option C）は、内部の宣言と確保が増えるので採らなかった。

## Risks & Mitigations

- **既存の挙動を変えてしまう**: 既存のスイートの期待値を変えずに通すことと、`ctest -L docs` を完了の条件にする。
- **公開の構造体の変更**: メンバーは末尾に足す。C++ の利用例を含むパッケージの検証を通す。
- **ヘルプの列幅**: 否定できるフラグが最も長い行になる場合を、`neg` のヘルプのバイト単位の比較で確かめる。


---

# ギャップ分析: 出現回数、隠しオプション、別名への対応（2026-11-11）

対象は Requirement 15（フラグの出現回数）、Requirement 16（隠しオプション）、Requirement 17（オプションの別名）と、それに伴う 1.5、1.12、11.1 の変更。要件は未承認の状態で分析した。値の選択肢は、要件確認で対応しないことにしたので対象に含めない。

## Summary

- 変更はすべて CLOpts（`cli_args/`）の中に収まる。Monde 側（`src/cli/`、`tests/cli/`、`docs/`）は変えなくてよい。
- 3 つの機能の重さは大きく違う。出現回数は記録の 1 か所と取得の関数 1 つ、隠しオプションはヘルプの 1 つの関数と宣言の検査 1 件で済む。別名は、解析の側はほぼ照合の 1 か所で済むが、宣言の検査（重複と `no-` の衝突）が「長い名前どうし」から「名前の集合どうし」の比較に広がる。
- 別名の要件のうち、エラーでの別名の表示（17.9）、元の名前での必須のエラー（17.10）、別名では結果を取得できないこと（17.8）、`--no-<別名>`（17.5）は、今の仕組みのままで満たせる。
- 既存のファイルを拡張する方法（Option A）で足りる。規模は S、リスクは Low。
- 設計で決めることは、別名の宣言の形、出現回数の記録の形と取得の関数の名前、すべての有効なオプションが隠しオプションのときの使い方の行、CLOpts のバージョンの扱いの 4 点。

## Current State

| 領域 | 場所 | 今の挙動 |
|---|---|---|
| 宣言の型 | `cli_args/include/clopts/clopts.h` の `clopts_option_t` | メンバーは 10 個（末尾の 2 つは `negatable`、`default_enabled`）。隠しの印も別名もない。利用側はすべて指示付き初期化子で書いている |
| フラグの記録 | `cli_args/src/clopts_result.c` の `clopts_result_set_flag` | `explicit_set = true` と `enabled`（肯定の形なら真、否定の形なら偽）を書くだけで、回数は持たない。長いオプション、否定の形、短いオプションとまとめ書きの 3 つの経路が、すべてこの関数を通る |
| 既定の適用 | `clopts_result.c` の `clopts_result_apply_defaults` | 明示的に指定されていない既定で有効のフラグの `enabled` を真にする |
| 結果の取得 | `clopts_result.c` の `clopts_lookup` | 経路上の各段のオプションを、`long_name` との完全一致だけで探す。見つからなければ `false` / `NULL` / `0` |
| 名前の照合 | `cli_args/src/clopts_decl.c` の `match_long`、`clopts_find_long`、`clopts_find_negated` | 有効なオプションの `long_name` との完全一致。否定の形は、`no-` を外した残りを `clopts_find_long` に渡して探す |
| 宣言の検査 | `clopts_decl.c` の `check_long_name`、`check_negation_conflict`、`find_ancestor_long`、`find_ancestor_negation` | 長い名前の形（空、先頭の `-`、`=` を含む）、予約（`help`、`version`）、同じコマンドの前のオプションとの重複、祖先のオプションとの重複、否定できるフラグと `no-<name>` の衝突を検査する。比較はすべて `long_name` の 1 対 1 |
| エラーの名前 | `cli_args/src/clopts_parse.c` の `scan_error`、`report_pending` | 値の欠落、値を取らない、空の項目、未知のオプションは、利用者が書いたトークンの名前の部分をそのまま示す。必須の欠落は宣言の `long_name` を示す |
| ヘルプのオプションの一覧 | `cli_args/src/clopts_help.c` の `append_options_section` | 有効なオプションを `clopts_effective_count` / `clopts_effective_at` で列挙し、列幅の計算と行の出力の 2 つのループを回す。左列は `long_name` だけから作る |
| 使い方の行 | `clopts_help.c` の `append_usage_line` | 個々のオプションは出さない。末端コマンドでは常に ` [<options>]` を出し、子を持つコマンドでは有効なオプションが 1 つ以上あるときだけ経路の直後に ` [<options>]` を出す |
| テスト | `cli_args/tests/` | 機能ごとに 1 ファイル（`negate_test.c` など）で、テスト用の宣言は `test_decl.c` にある。ヘルプはバイト単位で比較する。`negate_test.c` と `inherit_test.c` は `values[].enabled`、`values[].explicit_set` を直接読む |
| バージョン | `cli_args/CMakeLists.txt` | `project(CLOpts VERSION 0.2.0 ...)`。照合は `SameMinorVersion` で、README の例は `find_package(CLOpts 0.2 ...)` |

## Requirement-to-Asset Map

### Requirement 15: フラグの出現回数

| 要件 | 対応する資産 | 状態 |
|---|---|---|
| 15.1 出現回数の取得 | `clopts.h`、`clopts_lookup` | **Missing**: 取得の関数。探し方は `clopts_lookup` をそのまま使える |
| 15.2 肯定の形を 1 回と数える | `clopts_result_set_flag`、`clopts_value_t` | **Missing**: 回数の記録。3 つの経路が同じ関数を通るので、足す場所は 1 か所 |
| 15.3 否定の形で 0 に戻す | `clopts_result_set_flag`（`enabled == false` の呼び出し） | **Missing**: 同じ関数の中で 0 に戻す |
| 15.4 指定がなければ 0、既定で有効なら 1 | `clopts_result_apply_defaults` | **Missing**: 既定で有効のフラグを有効にする箇所で 1 にする |
| 15.5 子コマンドの名前の前後を合算 | 値は宣言した段に 1 つだけ記録する（既存の設計判断） | 記録が 1 か所なので、追加の処理なしで満たせる |
| 15.6 回数が 1 以上であることと有効であることの一致 | `enabled` と回数 | **Constraint**: 2 つを別々に持つなら、書き込む場所を `clopts_result_set_flag` と `clopts_result_apply_defaults` に限る必要がある |
| 15.7 フラグ以外と宣言のない名前は 0 | `clopts_lookup`、`calloc` による初期化 | フラグ以外では回数を書かないので、初期値の 0 のままで満たせる |

### Requirement 16: 隠しオプション

| 要件 | 対応する資産 | 状態 |
|---|---|---|
| 1.5、16.1 ヘルプに出さないかの宣言 | `clopts_option_t` | **Missing**: メンバーの追加 |
| 16.2 一覧に出さない（引き継ぐオプションを含む） | `append_options_section` の行のループ | **Missing**: 隠しオプションを飛ばす。引き継いだオプションも同じループで列挙しているので、1 か所で足りる |
| 16.3 列幅は隠しオプションを除いて決める | `append_options_section` の幅のループ | **Missing**: 幅のループでも飛ばす |
| 16.4、16.5 解析とエラーは同じ | `clopts_parse.c`、`clopts_result.c` | ヘルプ以外は印を見ないので、変更なしで満たせる |
| 16.6 必須かつ隠しは宣言の誤り | `clopts_decl.c` の `check_option` | **Missing**: 検査の追加 |
| 16.7 `--help`、`--version` は常に出す | `append_options_section` の末尾の固定の行 | 宣言されたオプションではないので、変更なしで満たせる |
| （要件が決めていない点） | `append_usage_line` | **Unknown**: 子を持つコマンドで、有効なオプションがすべて隠しオプションのとき、経路の直後の ` [<options>]` を出すか |

### Requirement 17: オプションの別名

| 要件 | 対応する資産 | 状態 |
|---|---|---|
| 1.5、17.1 別名の宣言 | `clopts_option_t` | **Missing**: メンバーの追加。宣言の形は設計で決める |
| 17.2 `--<alias>`、`--<alias>=<value>` | `match_long` | **Missing**: 別名との照合。`scan_long` は `clopts_find_long` の結果だけを見るので、照合を広げれば走査は変えずに済む |
| 17.3 元の名前と別名の混在は繰り返し | `clopts_result_set_flag`、`clopts_result_set_value`、`clopts_result_append_list` | 同じ `clopts_ref_t` に記録するので、変更なしで満たせる |
| 17.4 引き継ぎの範囲 | `find_effective` | 照合の関数だけを差し替える形なので、変更なしで満たせる |
| 17.5 `--no-<alias>` | `clopts_find_negated` | `clopts_find_long` を呼んでいるので、照合を広げれば満たせる。完全一致を先に見る順序（14.3）も今のまま |
| 17.6 別名による指定で必須を満たす | 必須の検査は `explicit_set` を見る | 変更なしで満たせる |
| 17.7 元の名前で取得 | `clopts_lookup` | 変更なしで満たせる |
| 17.8 別名では取得できない | `clopts_lookup` は `long_name` だけを見る | 変更なしで満たせる（`clopts_lookup` を別名に広げないこと） |
| 17.9 エラーは利用者が書いた別名で示す | `scan_error`、`report_pending` | トークンの名前の部分を示しているので、変更なしで満たせる |
| 17.10 必須の欠落は元の名前 | `CLOPTS_ERR_MISSING_REQUIRED` | 宣言の `long_name` を示しているので、変更なしで満たせる |
| 17.11 ヘルプに出さない | `append_option_left`、`option_left_width` | `long_name` だけから作るので、変更なしで満たせる |
| 17.12 別名の重複 | `check_long_name`、`find_ancestor_long` | **Missing**: 比較の対象を、長い名前と別名を合わせた名前の集合に広げる。同じオプションの中（元の名前とほかの別名）の重複も見る |
| 17.13 予約と形の誤り | `check_long_name` の先頭の 2 つの検査 | **Missing**: 同じ検査を別名にも当てる（検査の共通化） |
| 17.14 別名と `no-` の衝突 | `negation_conflict`、`check_negation_conflict`、`find_ancestor_negation` | **Missing**: 否定できるフラグの側の名前（元の名前と別名）と、相手の側の名前（長い名前と別名）の組み合わせを見る |
| 1.12 使わない宣言は今と同じ | 既存のテスト一式 | **Constraint**: 既存のテストを変えずに通すことが検証になる |
| 11.1 テスト用の宣言で検証 | `cli_args/tests/test_decl.c` | **Missing**: 隠しオプションと別名を持つテスト用の宣言 |

## 制約と気付いた点

- **宣言の型へのメンバーの追加**: 末尾に足せば、指示付き初期化子で書いた既存の宣言はそのまま通る（否定できるフラグのときと同じ）。別名は文字列の配列へのポインタになるので、`clopts_option_t` に初めてポインタの配列のメンバーが入る。
- **結果の構造体は公開されている**: `clopts_value_t` にメンバーを足すと大きさが変わる。値の配列は CLOpts が確保するので、利用側のソースは変えずに済む。`enabled` をテストが直接読んでいるので、`enabled` を回数に置き換えると既存のテストの書き換えが要る。
- **回数の型**: 引数の個数は `int` なので、回数が `size_t` の範囲を超えることはない。
- **別名の検査が今回の中心**: 今の重複の検査は「このオプションの長い名前」を「前のオプションと祖先のオプションの長い名前」と比べる形である。別名が入ると、比べる両側が複数の名前になる。「オプションの n 番目の名前（0 番目が長い名前、1 番目以降が別名）」を取り出す小さな関数を用意して、既存の比較をその上のループにすると、重複（1.8、17.12）と衝突（14.11、17.14）の両方を同じ形で書ける。
- **同じオプションの中の衝突**: 否定できるフラグ `color` に別名 `no-color` を付けた宣言は、17.14 の「否定できるフラグ `<name>` に対して、別名が `no-<name>` のオプションがある」に当たる。今の衝突の検査は別のオプションとの組だけを見るので、自分自身との組を見る処理が要る。
- **照合の順序**: `--no-<x>` は、長い名前または別名が `no-<x>` のオプションを先に探し、なければ否定の形として探す（14.3、17.5）。今の `scan_long` の順序がそのまま使える。
- **宣言の誤りの文面**: 別名の重複と衝突のメッセージが新しく要る。既存の `duplicate option '--%s' in command '%s'` などの形に揃えられる。
- **ヘルプの優先（8.3）と `--` の後（6.3）**: どちらも今回の変更の影響を受けない。
- **Monde への影響**: `src/cli/cli_commands.c` の宣言は新しいメンバーを使わないので、解析結果、ヘルプ、エラーは変わらない。`docs/` に引用したヘルプの照合（`ctest -L docs`）がそのまま通ることが確認になる。
- **要件の記述の古さ**: 7.5 と Adjacent expectations の「project-foundation の診断として返す」は、clopts-standalone の後の実装（`clopts_errors_t`）と合っていない。前回の分析でも挙げた点で、今回の改訂とは別の話である。

## Implementation Approach Options

### Option A: 既存のファイルを拡張する

宣言の型にメンバーを足し、`clopts_result.c`（回数の記録と取得）、`clopts_help.c`（隠しオプションの除外）、`clopts_decl.c`（別名の照合と検査、必須かつ隠しの検査）に処理を足す。`clopts_parse.c` はほぼ変えずに済む。

- **回数の記録の形**は 2 通りある。
  - **A1**: `clopts_value_t` に回数のメンバーを足し、`enabled` も残す。既存のテストと利用側は変えずに済むが、2 つのメンバーを常に一致させる必要がある（15.6）。
  - **A2**: `enabled` を回数に置き換え、`clopts_flag()` は回数が 1 以上かどうかを返す。一致が構造で保証されるが、公開の構造体のメンバーがなくなり、`negate_test.c` の書き換えが要る。
- **別名の宣言の形**は 3 通りある。
  - **B1**: 文字列の配列へのポインタと個数（`options` と `option_count` と同じ形）。既存の宣言の流儀に揃うが、メンバーが 2 つ増え、別名ごとに配列の定義が要る。
  - **B2**: `NULL` で終わる文字列の配列へのポインタ。メンバーは 1 つで済むが、CLOpts のほかの宣言にはない流儀になる。
  - **B3**: カンマ区切りの 1 つの文字列（`"out,outfile"`）。宣言がいちばん短く、配列の定義が要らない。リスト値の既定値と同じ書き方だが、解析のたびに区切る処理が要り、空の項目の検査も要る。
- ✅ 変更が小さく、今の責務の分け方（宣言の検査、走査、結果、ヘルプ）にそのまま乗る
- ✅ 新しいソースファイルが要らず、ビルド定義と単独性の検査を変えずに済む（テストのファイルは増える）
- ❌ `clopts_decl.c`（827 行）がさらに大きくなる

### Option B: 名前の照合と検査を新しいファイルに切り出す

長い名前と別名の列挙、照合、重複と衝突の検査を `clopts_names.c` のようなファイルにまとめ、`clopts_decl.c` から呼ぶ。出現回数と隠しオプションは Option A と同じにする。

- ✅ 名前に関する規則（重複、予約、`no-` の衝突、別名）を 1 か所で読める
- ✅ `clopts_decl.c` の肥大を抑えられる
- ❌ 検査は `decl_frame_t`（祖先の列）と `decl_check_t` に依存しており、切り出すにはこれらを内部ヘッダへ出す必要がある
- ❌ 既存の検査の移動が伴うので、差分が大きくなり、1.12（使わない宣言は今と同じ）の確認の範囲が広がる

### Option C: 別名を、ヘルプに出さない別のオプションとして扱う

別名ごとに、元のオプションと同じ記録先を指す隠しオプションを内部で作る（隠しオプションの仕組みに乗せる）。

- ✅ 照合と重複の検査を今のコードのまま使える
- ❌ 宣言は `static const` で、今は確保せずに参照している。展開のために宣言の写しを確保する必要があり、メモリ不足の経路が増える
- ❌ 2 つのオプションを同じ記録先に結び付ける仕組みが新しく要り、Option A の照合の拡張より大きくなる

## Effort and Risk

- **Effort: S（1〜3 日）**。既存の型と関数の拡張で、新しい依存も外部の調査もない。実装の量は別名の検査が最も多く、全体ではテストが大半を占める（回数、ヘルプ、別名の解析、宣言の誤り）。
- **Risk: Low**。変更は `cli_args/` の中に閉じ、新しい機能を使わない宣言の挙動は既存のテストで守られる。注意が要るのは、別名の重複と衝突の検査で組み合わせを取りこぼすことで、テスト用の宣言で向きごとに確かめれば抑えられる。

## Recommendations for Design Phase

- **有力な進め方**: Option A。3 つの機能は互いに独立しているので、設計とタスクでは機能ごとに分けられる。別名の検査は、名前を番号で取り出す関数を先に入れると、既存の比較を大きく動かさずに広げられる。
- **設計で決めること**:
  - 回数の記録の形（A1 か A2 か）と、取得の関数の名前。
  - 別名の宣言の形（B1、B2、B3 のどれか）と、メンバーの名前と位置（末尾）。
  - 隠しの印のメンバーの名前。
  - 子を持つコマンドで、有効なオプションがすべて隠しオプションのときの使い方の行（` [<options>]` を出すかどうか）。
  - 宣言の誤りのメッセージの文面（16.6、17.12〜17.14）。
  - 公開 API の追加に伴い、`project(CLOpts VERSION ...)` を上げるかどうか（上げる場合は、README の `find_package(CLOpts 0.2 ...)` の例と、インストール後の利用の検証も合わせる）。
  - `cli_args/README.md` に加える説明の範囲。
- **Research Needed**:
  - 別名のメンバーを足した後も、C++ からの利用の検証（`consumer_cxx.cpp`）がそのまま通ることを確かめる。
  - 要件が決めていない端の場合の扱いを確かめる: 別名が `no-` で始まる否定できるフラグ（`--no-no-<x>`）と、否定できるフラグ自身の別名が `no-<name>` の宣言。
  - 隠しオプションだけを持つコマンドのヘルプ（`Options:` の段落に `--help` の行だけが残る）の文面を、テスト用の宣言で確かめる。

---

# 設計の調査と判断: 出現回数、隠しオプション、別名への対応（2026-11-11）

discovery の種類は light（既存の部品の拡張）。外部の調査は行っていない。設計の原則、統合の観点、レビューゲートは kiro-spec-design の rules に従った。出発点は、上のギャップ分析である。

## Summary

- ギャップ分析の Option A（既存のファイルの拡張）を採った。新しいファイルはテストの 3 つだけで、`clopts_parse.c` と内部ヘッダは変えない。
- 出現回数は A1（`clopts_value_t` に `count` を足し、`enabled` を残す）とした。
- 別名の宣言は B3（カンマ区切りの 1 つの文字列）とした。
- 子を持つコマンドの使い方の行は、隠しオプションではない有効なオプションがあるときだけ ` [<options>]` を出す。
- CLOpts のバージョンを 0.3.0 に上げる。

## Design Decisions

### Decision: 出現回数を `clopts_value_t.count` に持ち、`enabled` を残す
- **Alternatives**: (a) `count` を足し、`enabled` も残す。(b) `enabled` を `count` に置き換え、`clopts_flag` は `count > 0` を返す。
- **Selected**: (a)。
- **Rationale**: (b) は公開の構造体からメンバーを消すので、`enabled` を直接読む既存のテスト（`negate_test.c`）と利用側の書き換えが要る。(a) は追加だけで済み、1.12 を既存のテストをそのまま通すことで確かめられる。
- **Trade-offs**: 2 つのメンバーを常に一致させる必要がある（15.6）。書き込みを ResultStore の 3 つの関数（記録、既定の適用、解放）に限り、テストで一致を確かめる。

### Decision: 回数を数えるための宣言は設けない
- **Rationale**: 回数は取得の関数を呼ぶ側が使うかどうかを決めればよく、宣言の印があっても解析の挙動は変わらない。フラグの繰り返しは改訂前からエラーではない（5.5）。
- **Trade-offs**: ヘルプに「重ねて指定できる」ことは出ない。必要なら、利用側が説明に書く。

### Decision: 別名はカンマ区切りの 1 つの文字列で宣言する
- **Alternatives**: (a) 文字列の配列へのポインタと個数。(b) `NULL` で終わる文字列の配列へのポインタ。(c) カンマ区切りの 1 つの文字列。
- **Selected**: (c)。
- **Rationale**: メンバーが 1 つで済み、別名ごとの配列の定義が要らない。`.aliases = "out,outfile"` と 1 行で書け、C と C++ で同じに書ける。リスト値の既定値（`default_value = "a,b"`）と同じ書き方である。照合は、文字列の中の範囲（先頭と長さ）を比べるだけで、確保が要らない。
- **Trade-offs**: 別名はカンマを含められない。長い名前はカンマを含められるままなので、名前の規則が 1 つ食い違う。空の項目（`"a,,b"`、空文字列）は宣言の誤りにする。(a) は既存の `options` / `option_count` の流儀に揃うが、宣言が 3 行に増える。(b) はファイルスコープの複合リテラルに頼る書き方になりやすく、C++ では書けない。

### Decision: 別名の検査は、名前を番号で取り出す関数の上に、既存の比較を広げる
- **Context**: 重複（1.8）と衝突（14.11）の検査は、長い名前の 1 対 1 の比較だった。別名が入ると、両側が複数の名前になる。
- **Selected**: 「オプションの n 番目の名前（0 番目が長い名前、1 番目以降が別名）」を返す関数を `clopts_decl.c` の中に置き、照合と検査の両方がそれを使う。
- **Rationale**: 比べる相手（同じコマンドの前のオプション、祖先）は改訂前と同じで、変わるのは「名前 1 つ」が「名前の列」になる点だけである。照合（`match_long`）も同じ関数を使うので、検査が見る名前と、解析が一致させる名前がずれない。
- **Trade-offs**: 名前が NUL で終わらなくなるので、比較とメッセージを長さ付きに直す必要がある。別名を含まない宣言のメッセージが 1 バイトも変わらないことを、既存の decl のスイートで確かめる。

### Decision: 別名の重複と衝突には、既存の文面を使う
- **Rationale**: 利用側の開発者に必要な情報は「どの名前が、どのコマンドで重なったか」で、長い名前か別名かの区別は宣言を見れば分かる。文面を増やさずに済み、別名を含まない宣言のメッセージも変わらない。
- **Trade-offs**: メッセージの `option '--<n>'` の `<n>` が別名のこともある。別名の形と予約の誤りだけは、どのオプションの別名かを示す新しい文面にした（重複と違い、名前だけからは宣言の場所を探しにくいため）。

### Decision: 否定できるフラグ自身の別名が `no-<name>` の宣言は誤りにする
- **Context**: ギャップ分析で見つけた端の場合。17.14 は「否定できるフラグ `<name>` に対して、別名が `no-<name>` のオプションがある」ことを誤りとしており、自分自身も含む。
- **Rationale**: 照合は完全一致を先に見るので、`--no-color` は別名として `color` を**有効**にしてしまい、否定の形に届かない。利用者の意図と逆の結果になるので、宣言の時点で止める。
- **Trade-offs**: 衝突の検査で、同じオプションの名前どうしの組も見る必要がある。

### Decision: 別名にも否定の形を設ける（追加の処理なし）
- **Rationale**: 要件 17.5 である。`clopts_find_negated` は `no-` を外した残りを `clopts_find_long` に渡しているので、照合を別名に広げるだけで `--no-<別名>` が働く。別名だけを否定の対象から外すほうが、条件が増える。

### Decision: 隠しオプションは、使い方の行の ` [<options>]` の判定からも除く
- **Context**: 最初の要件は一覧に出さないことだけを決めていた（設計のレビューの後、この判断を要件 16.8 として加えた）。子を持つコマンドは、有効なオプションがあるときだけ経路の直後に ` [<options>]` を出す。
- **Alternatives**: (a) 隠しオプションも数える（改訂前の判定のまま）。(b) 隠しオプションを除いて数える。
- **Selected**: (b)。
- **Rationale**: 隠しオプションしか持たないコマンドで ` [<options>]` が出ると、一覧には何もないのに、指定できるオプションがあることだけが伝わる。(b) なら、オプションを宣言しないコマンドのヘルプと同じになる。
- **Trade-offs**: 数える処理が `clopts_help.c` に 1 つ増える。`clopts_effective_count` の意味は変えない（走査は隠しオプションも有効なオプションとして扱う）。

### Decision: 必須かつ隠しの検査は、既存のフラグの検査と独立に報告する
- **Rationale**: 1 つの宣言に誤りが複数あれば、それぞれ報告する方針（改訂前のまま）に合わせた。フラグに `required` と `hidden` を付けると 2 件になる。

### Decision: CLOpts のバージョンを 0.3.0 に上げる
- **Rationale**: 公開の型のメンバーと公開の関数が増える。前の改訂（否定できるフラグ）でも、同じ理由で 0.1.0 から 0.2.0 に上げた。
- **Trade-offs**: 照合が `SameMinorVersion` なので、`find_package(CLOpts 0.2)` と書いた利用側は 0.3.0 を見つけない。利用側は Monde だけで、`add_subdirectory` で取り込んでいるので影響はない。README の例を直す。

## Synthesis

- **Generalization**: 重複の検査、衝突の検査、解析の照合は、どれも「オプションが持つ名前の列」に対する操作である。名前を番号で取り出す関数を 1 つ置き、3 つがそれを使う形にした。実装するのは今回の要件が求める範囲（長い名前と別名）だけで、短い名前の別名などは入れない。
- **Build vs. Adopt**: 外部のライブラリは使えない（steering の制約）。採る・作るの判断はない。
- **Simplification**: `clopts_parse.c`、内部ヘッダ、エラーの種類、解析の状態を変えなかった。別名を内部の隠しオプションに展開する案（Option C）と、名前の処理を新しいファイルに分ける案（Option B）は、確保の経路と内部の宣言が増えるので採らなかった。別名の重複と衝突のために新しい文面を作ることもやめた。

## Risks & Mitigations

- **既存の挙動を変えてしまう**: 既存のスイートの期待値を変えずに通すことと、`ctest -L docs` を完了の条件にする。とくに `clopts_decl.c` の比較を長さ付きに直す箇所は、既存の decl のスイートがメッセージをバイト単位で見ているので、そこで守られる。
- **別名の検査の取りこぼし**: 組み合わせ（前のオプションの長い名前・別名、自分の前の名前、祖先の長い名前・別名、衝突の両方の向き、自分自身との衝突）を、decl のスイートで 1 つずつ確かめる。
- **`count` と `enabled` の食い違い**: 書き込む関数を限り、count のスイートで `clopts_flag` と `clopts_flag_count` を同じ引数列で照らし合わせる。
- **公開の構造体の変更**: メンバーは末尾に足す。C++ の利用例を含むパッケージの検証を通す。
- **ヘルプの列幅**: 隠しオプションが最も長い行になる場合を、`ext run` のヘルプのバイト単位の比較で確かめる。
