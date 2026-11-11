# Research & Design Decisions

## Summary
- **Feature**: `clopts-standalone`
- **Discovery Scope**: Extension（既存の引数パーサの移設・改名・依存の除去・パッケージ化。挙動は変えない）
- **Key Findings**:
  - foundation の診断は、引数パーサの中では「重大度は常にエラー、パスなし、行番号なし」でしか使われていない（`mnd_diag_add` 34 か所すべて）。メッセージの列だけを持つ型で置き換えられる。
  - ヘルプなどの整形 API が生成する文字列は NUL を含まない。`mnd_buf_t` への追記をやめ、確保した C 文字列を返す形にできる。
  - FetchContent は同じ名前の 2 回目の宣言と取得を無視する。Monde が先に Naos を取得していれば、CLOpts 側は同じ記述のままで二重取得にならない。

## Research Log

### foundation への依存の実態
- **Context**: 依存を除去するために、何をどの範囲で使っているかを確かめた。
- **Sources Consulted**: `src/cli_args/*.c`、`src/cli_args/*.h`、`src/foundation/diag.[ch]`、`buf.[ch]`、`wide.[ch]`
- **Findings**:
  - `diag`: 公開 API 3 つ（`mnd_args_parse`、`mnd_args_argv_get`、`mnd_args_argv_from_wide`）と内部の `mnd_args_decl_check` が `mnd_diag_list_t *` を取る。追加はすべて `MND_SEVERITY_ERROR, NULL, 0`。メモリ不足で追加できないと `dropped` を立てる。
  - `buf`: `append_str`、`append_byte`、`cstr`、`init`、`free` だけを使う。公開 API では `args_help.h` の 4 関数が `mnd_buf_t *out` に追記する。内部では宣言の検証と解析でメッセージの組み立てに使う。
  - `wide`: `mnd_wide_to_utf8` を `args_argv.c` の 1 か所で使う。失敗の理由（文字列）で診断の文言を分けている。
- **Implications**: エラーはメッセージの列、バッファは内部専用の小さな実装、UTF-16 変換は `clopts_argv.c` 内の静的関数で足りる。

### 利用側の範囲
- **Context**: 改名と橋渡しの影響範囲を確かめた。
- **Sources Consulted**: `src/cli/`、`src/monde/main.c`、`tests/cli/`、`README.md`、`docs/`、`.github/`、`cmake/`、`tools/`
- **Findings**:
  - 製品コード: `src/cli/` の 12 ファイルと `src/monde/main.c`。診断を受け取るのは `cli_run.c`（解析）と `main.c`（引数列の取得）の 2 か所だけ。整形 API を使うのは `cli_run.c` と `cli_report.c`。
  - テスト: `tests/cli/` の `io_test.c`、`run_test.c`、`report_test.c` が `mnd_args_*` を使う。
  - `tests/cli_args/` の `decl_test.c`、`help_test.c`、`values_test.c` は内部ヘッダ `args_internal.h` を取り込む。
  - README、`docs/`、ワークフロー、`cmake/`、`tools/` には `cli_args` への言及がない。steering は `structure.md` と `tech.md` が言及する。
- **Implications**: 橋渡しは 2 か所の呼び出し点で使う 1 つの関数で足りる。利用者向けドキュメントの修正はほぼ不要で、steering の更新が中心になる。

### Naos の共有
- **Context**: Monde と一緒に構成したときに Naos を二重に取得・定義しないこと（要件 5.8、7.9）。
- **Sources Consulted**: `cmake/MondeNaos.cmake`、CMake の FetchContent の仕様（同名の最初の宣言が優先され、取得済みの依存に対する `FetchContent_MakeAvailable` は何もしない）
- **Findings**: Naos は `v0.4.0` を `GIT_TAG` で固定し、`FETCHCONTENT_SOURCE_DIR_NAOS` でローカルのソースに差し替えられる。`naos_add_executable` は CMake の関数で、定義されるとプロジェクト全体から見える。
- **Implications**: CLOpts は `naos_add_executable` がすでに定義されていれば何もせず、なければ自分で取得する。Monde は CLOpts を取り込む前に Naos を取得する。

### 変更履歴の保持
- **Context**: 移したファイルの履歴をたどれること（要件 1.5）。
- **Findings**: Git は改名を内容の類似度で検出する。移動と大きな書き換えを同じコミットに入れると、改名として検出されないことがある。
- **Implications**: 「移動だけ（include の行とビルド定義の最小限の修正を含む）」のコミットを先に作り、API の変更と改名は後続のコミットに分ける。

### CMake のパッケージ化
- **Context**: `find_package(CLOpts)` への対応（要件 6）。
- **Sources Consulted**: CMake の `CMakePackageConfigHelpers`、`GNUInstallDirs`、`install(EXPORT)` の仕様
- **Findings**: `configure_package_config_file` と `write_basic_package_version_file` で、再配置できる構成ファイルとバージョン照合のファイルを作れる。`SameMinorVersion` は CMake 3.11 以上で使える。静的ライブラリの PUBLIC なリンク先（`shell32`）は、エクスポートしたターゲットの `INTERFACE_LINK_LIBRARIES` に残る。
- **Implications**: 追加の依存なしに CMake 標準の機能だけで実現できる。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| 自己完結のライブラリ + 利用側の橋渡し | CLOpts が自前のエラー型を持ち、Monde が診断に変換する | 境界が明確。切り離し後も変更が不要 | Monde 側に変換が 1 つ増える | 採用 |
| foundation の一部を同梱 | diag / buf / wide を複製する | 変更量が少ない | シンボルが衝突する。二重管理 | discovery で却下 |
| コールバックで通知 | エラーを関数ポインタで渡す | 型を持たずに済む | API とテストの書き換えが大きい | discovery で却下 |

## Design Decisions

### Decision: エラーはメッセージの列で返す
- **Context**: 公開 API から `mnd_diag_list_t` を取り除く（要件 3.3〜3.5）。
- **Alternatives Considered**:
  1. 重大度・パス・行番号を持つ診断型を CLOpts にも作る
  2. メッセージ文字列の列だけを持つ型にする
- **Selected Approach**: 2。`clopts_errors_t`（メッセージの配列、件数、`dropped`）。
- **Rationale**: 引数パーサは重大度・パス・行番号を使っていない。使わない欄は持たない。
- **Trade-offs**: 将来、警告を返したくなったら型の拡張が必要になる。現在の要件にはない。
- **Follow-up**: 橋渡し後の診断が移す前と同じ文言・順序になることを、Monde の既存テストで確かめる。

### Decision: 整形 API は確保した文字列を返す
- **Context**: 公開 API から `mnd_buf_t` を取り除く（要件 3.6、3.7）。
- **Alternatives Considered**:
  1. CLOpts 独自のバッファ型を公開する
  2. 確保した NUL 終端の文字列を返し、専用の解放関数を用意する
  3. 呼び出し側が渡す固定長の領域に書く
- **Selected Approach**: 2。`char *clopts_format_*(...)` と `clopts_text_free`。
- **Rationale**: 出力は NUL を含まない文字列で、利用側は 1 回書き出すだけである。公開する型が増えない。
- **Trade-offs**: 「既存のバッファの末尾に追記する」使い方はできなくなる。利用側の 2 か所は追記を必要としていない。
- **Follow-up**: 解放を `free` に直接任せず `clopts_text_free` を通す。将来、共有ライブラリにしても API を変えずに済む。

### Decision: 警告のエラー化は選択式にする
- **Context**: Monde と同じ警告水準でビルドする（要件 2.7）一方、取り込んだ側のビルドを壊さない（要件 5.5、5.6）。
- **Selected Approach**: 警告の指定は `clopts` と CLOpts のテストに PRIVATE で付ける。エラー化は `CLOPTS_WARNINGS_AS_ERRORS` で選び、既定は単独構成で ON、取り込み構成で OFF。Monde は ON を指定する。
- **Rationale**: 利用側の未知のコンパイラで、新しい警告がビルドの失敗になるのを避ける。
- **Trade-offs**: オプションが 1 つ増える。

### Decision: 単独性の検査は文字列の走査で行う
- **Context**: CLOpts ディレクトリの外への依存が入り込まないこと（要件 1.2、3.1、4.2、9.2）。
- **Alternatives Considered**:
  1. ライブラリのシンボル表を `nm` / `dumpbin` で調べる
  2. CLOpts ディレクトリのファイルを走査し、禁止する文字列がないことを確かめる
- **Selected Approach**: 2 を CTest のテストにし、加えて CI で「複製した場所での単独構成」を実行する。
- **Rationale**: 全環境で同じ方法が使える。複製での構成が、参照の漏れを確実に検出する。
- **Trade-offs**: 文字列の走査は、コメント内の語も検出する。CLOpts の中では該当の語を使わないことにする。

### Decision: 移行は「移動 → 自己完結 → 改名 → 独立ビルド」の順
- **Context**: 履歴の保持（要件 1.5）と、各段階でビルドとテストが通ること。
- **Selected Approach**: design.md の Migration Strategy のとおり。
- **Rationale**: 移動を内容の変更から分けると、改名の検出が確実になる。各段階の終わりで Monde の全テストを実行でき、不具合の原因を段階に絞れる。
- **Trade-offs**: 1 段階目だけ、`cli_args/` が一時的に foundation にリンクする。

## Synthesis Outcomes
- **Generalization**: エラーの橋渡しは「解析」と「引数列の取得」で同じ変換なので、1 つの関数にまとめた。4 つの整形関数は同じ戻り値の約束（確保した文字列、失敗は NULL）に揃えた。
- **Build vs. Adopt**: パッケージ化は CMake 標準のモジュールを使う。外部の引数パーサへの置き換えは範囲外（外部依存なしの方針）。
- **Simplification**: 公開するバッファ型、重大度・パス・行番号を持つ診断型、`nm` による検査、CLOpts 専用の CI ワークフローファイルを作らないことにした。共有ライブラリ対応とコンポーネント別のインストールも持たない。

## Risks & Mitigations
- 移動と書き換えが混ざって履歴をたどれなくなる — 移動だけのコミットを分ける。`git log --follow` で確かめる。
- 橋渡しでメッセージや `dropped` の扱いが変わり、`monde` の出力が変わる — `tests/cli/` の既存テストと `tests/docs/` の照合テストで検出する。
- Monde の配布物に CLOpts のファイルが混ざる — 取り込み構成ではインストールの規則を既定で加えない。`tools/release/verify_package.cmake` の検査が通ることを確かめる。
- インストール後の利用の検証が、複数構成ジェネレータ（MSVC）で構成の不一致により失敗する — 検証のスクリプトに構成名を渡し、インストール・構成・ビルドのすべてで同じ構成を使う。
- 取り込み構成で MSVC のランタイムの指定が利用側と食い違う — CLOpts はランタイムを指定せず、利用側の設定に従う。

## References
- CMake Documentation: `FetchContent`、`CMakePackageConfigHelpers`、`GNUInstallDirs`、`PROJECT_IS_TOP_LEVEL`（3.21）
- `.kiro/specs/cli-args/requirements.md`、`design.md` — 引数パーサの挙動の基準
- `.kiro/specs/clopts-standalone/brief.md` — discovery の決定事項
