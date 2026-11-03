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
