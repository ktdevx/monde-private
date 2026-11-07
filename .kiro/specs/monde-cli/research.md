# Research & Analysis: monde-cli

## ギャップ分析（/kiro-validate-gap、2026-11-07）

### 1. 現状の調査

#### 既存の部品と公開 API（monde-cli が呼ぶもの）

| 部品 | ターゲット | monde-cli が使う API | 備考 |
| --- | --- | --- | --- |
| foundation | `monde::foundation` | `mnd_file_read`（BOM 除去・UTF-8 検証）、`mnd_file_write`（`add_bom`）、`mnd_eol_detect`（dominant / mixed）、`mnd_diag_*`、`mnd_buf_*` | 標準入出力の読み書きはない。診断の整形・表示関数もない（表示は CLI の責務と明記） |
| cli-args | `monde::cli_args` | `mnd_args_program_t` / `mnd_args_command_t` / `mnd_args_option_t` の static const 宣言、`mnd_args_parse` → `mnd_args_status_t`（OK / HELP / VERSION / NO_COMMAND / USAGE_ERROR / DECL_ERROR / OUT_OF_MEMORY）、`mnd_args_format_help` / `_version` / `_usage_hint`、`mnd_args_argv_get`（Windows の UTF-8 化） | プログラム共通のオプションは `--help` / `--version` だけ。`-q` や `--no-wrap` はサブコマンドごとに宣言する |
| po-format | `monde::po` | `mnd_po_read(data, len, path, out, diags)`、`mnd_po_write(catalog, {no_wrap, eol}, out, diags)`、`mnd_po_init(catalog, language, revision_date, diags)`、`mnd_po_header_template(out, project_id_version, creation_date)`、`mnd_po_header_set`、`mnd_po_date_format(tm, utc_offset_minutes, out)`、`mnd_po_catalog_header` | 時計は読まない。`Report-Msgid-Bugs-To` は `mnd_po_header_set` で上書きする。`eol` は CRLF 以外を LF にする |
| po-merge | `monde::po_merge` | `mnd_merge_catalogs(tpl, tpl_path, old, old_path, {fuzzy_matching}, out, stats, diags)` → `mnd_merge_stats_t { matched, fuzzy, added, obsoleted }` | 設計は「既存 `.po` の改行コードで書く」ことを CLI に期待 |
| translation-units | `monde::tu` | `mnd_tu_scan(data, len, {path, front_matter_keys, count}, &doc, diags)`、`mnd_tu_extract(doc, ref_file, catalog, diags)`、`mnd_tu_translate(doc, catalog, {use_fuzzy, eol, catalog_path}, out, stats, diags)` → `mnd_tu_stats_t { total, translated, fuzzy, untranslated, missing }` | ヘッダは加えない（CLI が先に `mnd_po_header_template` で加える手順を `tests/tu/extract_pot_main.c` が示している） |

#### ビルドとテストの規約
- `cmake/MondeTargets.cmake`: `monde_add_module` / `monde_add_module_tests` / `monde_add_executable`（MSVC 静的 CRT、MinGW `-static`、Naos のリンクを構成時エラーに）。`src/CMakeLists.txt` にまだ実行ファイルのターゲットはない。
- `project(monde VERSION 0.1.0)` がバージョンの唯一の定義。実行ファイルへはコンパイル定義で渡せる（12.4）。
- テストは `tests/<module>/` に Naos のスイート（`NAOS_MAIN` + `NAOS_DECLARE_SUITE`）。`tests/tu/` は `../markdown/spec_reader.c` を直接ソースに加え、`MND_MD_SPEC_FILE` で spec.txt のパスを受け取っている（monde-cli も同じ方法で再利用できる）。
- 外部ツールを使うテストは、Naos を使わない補助実行ファイル + `cmake -P` スクリプト（`tests/tu/msgfmt_check.cmake`、`tests/po/msgcat_compare.cmake`）で `execute_process` を使う。`find_program(MONDE_MSGFMT msgfmt)` はキャッシュされ、見つからなければ `message(STATUS ...)` して登録しない（11.2〜11.3 と同じ流儀）。
- CI は Linux のジョブで `apt-get install -y gettext` 済み（11.4 の前提は整っている）。
- `tests/tu/roundtrip_test.c` は spec.txt の 652 例 × 3 変種（original / CRLF / 末尾改行なし）× 3 検査を部品レベルで実施済み。monde-cli の 10.1〜10.3 はこれを実行ファイル経由で繰り返すものになる。

#### 規約から決まること
- モジュール名は `cli`（`src/cli/`、`tests/cli/`、ターゲット `monde::cli`、接頭辞 `mnd_cli_`、ヘッダガード `MONDE_CLI_*`）。短縮名 `cli` は未予約。
- 波括弧は Allman、`//` 不可、複合条件の括弧、`static` の内部関数、foundation 経由のファイル入出力。

### 2. 要件の実現性分析（Requirement → 既存資産 → ギャップ）

| 要件 | 既存資産 | ギャップ（Missing / Unknown / Constraint） |
| --- | --- | --- |
| 1 コマンド体系とヘルプ | cli-args が解析・ヘルプ・使い方の案内を提供 | **Missing**: 4 サブコマンドの宣言、ステータスごとの表示先と終了コードの対応。**Constraint**: `MND_ARGS_DECL_ERROR` はプログラムの不具合なので、終了コードの扱い（1 として内部エラー扱い）を設計で決める |
| 2 入出力の指定 | `mnd_file_read` / `mnd_file_write` | **Missing**: 標準入力の読み込み（Windows では `_setmode(_fileno(stdin), _O_BINARY)` が必要）と標準出力への書き込み（同様に `_O_BINARY`）。**Missing**: 同じサブコマンドで `-` が 2 回のチェック。2.7（同じパスへの上書き）は「全入力をメモリに読んでから書く」現在の流れで満たせる。2.8（失敗時に出力を作らない）は「処理が完了してから `mnd_file_write`」で満たせるが、書き込み途中の失敗（ディスク満杯）で既存ファイルが壊れる点は残る → **Unknown**: 一時ファイル + rename を入れるか（Windows は `MoveFileExW(MOVEFILE_REPLACE_EXISTING)`、foundation への追加になる） |
| 3 文字コード・改行・BOM | `mnd_file_read` が BOM を除去し `had_bom` を返す。`mnd_eol_detect` が dominant を返す。`mnd_po_write` と `mnd_tu_translate` が eol を受け取る | **Constraint**: `MND_EOL_NONE` は LF に落ちる（3.5 と一致）。`mnd_po_read` も BOM を読み飛ばすが二重除去は無害。3.8（標準入出力でも変換なし）は上記のバイナリモード設定で満たす |
| 4 extract | `mnd_tu_scan` → `mnd_po_header_template` → `mnd_tu_extract` → `mnd_po_write`（`extract_pot_main.c` と同じ手順） | **Missing**: 現在日時の取得と UTC オフセットの算出（`time` / `localtime` / `gmtime` の差で C99 の範囲で求められる。MSVC の `localtime` 警告は `_CRT_SECURE_NO_WARNINGS` 済み）。`Report-Msgid-Bugs-To` は `mnd_po_header_set` で設定。`--front-matter-keys` は `mnd_args_list_*` の項目をそのまま `front_matter_keys` に渡す（`const char *const *` へ詰め直す） |
| 5 init | `mnd_po_read` → `mnd_po_init` → `mnd_po_write` | **Missing**: 環境変数 `LC_ALL` / `LC_MESSAGES` / `LANG` の参照（`getenv`）と、`.` / `@` 以降の除去、`C` / `POSIX` の無視。**Constraint**: `mnd_po_init` は空・空白・制御文字の言語コードを診断で拒否する（5.7 は CLI 側で事前に検査して使い方のエラー（2）にするか、po の診断を 2 に対応付けるかを設計で決める） |
| 6 update | `mnd_po_read` × 2 → `mnd_merge_catalogs` → `mnd_po_write` | ギャップなし。eol は既存 `.po` の `mnd_eol_detect` から。`--no-fuzzy-matching` → `fuzzy_matching = false` |
| 7 translate | `mnd_file_read` × 2 → `mnd_po_read` → `mnd_tu_scan` → `mnd_tu_translate` | ギャップなし。BOM は `had_bom` を `mnd_file_write` の `add_bom` に渡す（標準出力の場合は CLI が BOM を書く）。7.9（廃止・msgctxt 付きの無視）は tu の仕様で満たされている |
| 8 統計 | `mnd_tu_stats_t`、`mnd_merge_stats_t`、`catalog.count - 1` | **Missing**: 英語の固定書式の文面と翻訳率（小数 1 桁）の計算、`-q` の宣言。**Unknown**: 書式の具体案（設計で決める。例: `extract: 12 messages`、`update: 10 matched, 2 fuzzy, 3 added, 1 obsoleted`、`translate: 8 translated, 1 fuzzy, 2 untranslated, 1 missing (12 total), 66.7% translated`） |
| 9 診断と終了コード | `mnd_diag_t { severity, message, path, line }`、`dropped` | **Missing**: `<path>:<line>: error: <message>` の整形と標準エラー出力への表示、`dropped` 時の追加メッセージ、終了コードの対応表 |
| 10 ラウンドトリップの受け入れテスト | `tests/markdown/spec_reader.c`（`mnd_md_spec_load`）、`tests/tu/roundtrip_test.c` の変種生成 | **Missing**: 実行ファイルを起動して標準入出力と終了コードを確かめる仕組み。Naos にプロセス起動の支援はなく、既存の流儀は `cmake -P` + `execute_process`。**Unknown**: 652 例 × 3 検査をプロセス起動で回す方法（下記 3 の Option を参照） |
| 11 msgfmt --check | `tests/tu/msgfmt_check.cmake` の型、`find_program(MONDE_MSGFMT)`、CI の gettext 導入 | ギャップなし。`extract` / `init` / `update` の出力それぞれに同型のスクリプトを用意する |
| 12 実行ファイル | `monde_add_executable`、`project(... VERSION 0.1.0)` | **Missing**: `src/monde/`（または `src/cli/`）の `main.c` とターゲット `monde`。バージョンは `target_compile_definitions(... MONDE_VERSION="${PROJECT_VERSION}")` で 1 か所から渡せる |

#### 複雑さの性質
- 大半は既存 API の**順次呼び出し（ワークフロー）**で、アルゴリズムはない。
- 新規に書く論理は、標準入出力・日時・環境変数・診断整形・統計整形という**OS 境界の小さな部品**と、**プロセス起動を伴うテスト基盤**に集中する。

### 3. 実装アプローチの選択肢

#### Option A: 実行ファイル 1 ターゲットに集約（`src/monde/main.c` + 数ファイル）
- `monde_add_executable(monde SOURCES main.c cli_*.c LIBRARIES monde::cli_args monde::po_merge monde::tu)` のみ。モジュールライブラリは作らない。
- テストはすべて実行ファイルの起動（`cmake -P` スクリプト、または Naos 不要の C ドライバ）で行う。
- ✅ ファイルが最少。「薄い統合層」という方針に素直。
- ❌ Naos の単体テストで内部関数（環境変数の正規化、翻訳率の整形、診断の整形、`-` の扱い）を直接検証できない。`monde_add_module_tests` は `monde::<module>` へのリンクを前提にしているため、テストの規約から外れる。
- ❌ 652 例のラウンドトリップをプロセス起動だけで行うと、Windows の CI で時間がかかる可能性がある（**Research Needed**: 起動 1 回あたりのコストの実測）。

#### Option B: ライブラリ `monde::cli` + 極薄の `main.c`（推奨候補）
- `src/cli/` に `mnd_cli_run(argc, argv, streams)` 相当の入口と、サブコマンドのハンドラ、診断・統計の整形、標準入出力の扱いを置き、`monde_add_module(cli ... DEPENDS cli_args po_merge tu)` でライブラリにする。`src/monde/main.c`（数十行）は `mnd_args_argv_get` → `mnd_cli_run` → 終了コードだけ。
- 標準入出力・標準エラー出力は `FILE *` またはバッファを差し替えられる構造にし、Naos テストから**プロセスを起動せずに**サブコマンドを実行できるようにする。652 例のラウンドトリップはこのプロセス内の経路で網羅し、実行ファイルの起動は代表的なケース（各サブコマンド 1〜数件、終了コード 0/1/2、`-`、BOM、CRLF）に絞って `cmake -P` で行う。
- ✅ 規約（`src/<module>/` + `tests/<module>/` + `monde::<module>`）に完全に沿う。内部の小部品を Naos で単体テストできる。
- ✅ 10.4（失敗した例の番号を出力）は `roundtrip_test.c` と同じ方法でそのまま実現できる。
- ❌ 10.1 の「ビルドした実行ファイルで」を厳密に読むと、全 652 例をプロセス起動で回す必要があり、プロセス内テストだけでは足りない。→ 設計で「全例はプロセス内 + 実行ファイルでの抜き取り」と解釈するか、Option C のドライバを足すかを決める。
- ❌ 標準出力への書き込みをライブラリに含めるため、foundation の「I/O は境界に寄せる」原則との整合を設計で明示する必要がある（CLI 層自体が境界なので許容範囲）。

#### Option C: Option B + 実行ファイルを起動する専用ドライバ
- Option B に加え、Naos を使わない C のドライバ（`tests/cli/roundtrip_driver.c`、`tu_extract_pot` と同型）を作る。ドライバは spec.txt を読み、例ごとに一時ファイルを書き、`monde` を起動して結果を比較し、失敗した例の番号を標準出力に列挙して終了コードで結果を返す。CTest にはドライバ 1 件（変種ごとに 3 件）として登録する。
- プロセスの起動は `system()` ではなく、POSIX は `fork` / `execv`、Windows は `CreateProcessW` を使う小さな関数が必要（引用符のエスケープを避けるため）。または、ドライバを `cmake -P` で書き、`foreach` + `execute_process` で回す（C コードは不要だが 652 × 3 回の `execute_process` になる）。
- ✅ 10.1〜10.4 を字義どおり満たす。失敗時の報告も自前で制御できる。
- ❌ 新規コード（プロセス起動の OS 依存部分）が増える。**Research Needed**: CI の各環境での所要時間、Windows での一時ファイルと非 ASCII パスの扱い。

### 4. 工数とリスク

| 項目 | 見積もり | 根拠 |
| --- | --- | --- |
| 工数 | **M**（3〜7 日） | 部品の API は揃っており統合は順次呼び出しだが、標準入出力（Windows のバイナリモード）、日時とオフセット、環境変数、診断・統計の整形、プロセス起動を伴う受け入れテストと `msgfmt` テストの CMake 配線、5 環境の CI 通過が必要 |
| リスク | **Medium** | 技術は既知だが、(1) 全例のラウンドトリップをプロセス起動で行う場合の CI 時間、(2) 時刻のオフセットと `getenv` の Windows での挙動、(3) 一時ファイル + rename を入れる場合の OS 差、の 3 点に未確定がある |

### 5. 設計フェーズへの推奨事項

#### 推奨アプローチ
- **Option B を基本に、10.1〜10.3 をどう満たすかで Option C を足す**。判断の軸は「全 652 例をプロセス起動で回しても CI が許容できるか」。設計の冒頭で 1 例あたりの起動コストを 1 環境（Windows が最も遅い）で見積もり、許容できなければ「全例はプロセス内（`mnd_cli_run`）+ 実行ファイルは抜き取り」と要件の読みを設計で明記する。
- モジュール名は `cli`、実行ファイルのターゲットは `monde`。`main.c` は `src/monde/` に置き、`monde_add_executable` で作る（テスト用の部品と同じ `src/cli/` に置くとライブラリに `main` が混ざるため分ける）。

#### 設計で決めるべき主な事項
1. `mnd_cli_run` の入口の形（引数列、標準入出力の差し替え方法、戻り値 = 終了コード）と、サブコマンドのハンドラの共通の流れ（読む → 処理 → 書く → 統計）。
2. 標準入出力の読み書きを foundation に足すか（`mnd_file_read_stream` 相当）、cli に閉じるか。Windows の `_setmode` をどこで呼ぶか。
3. 現在日時と UTC オフセットの算出方法（`localtime` と `gmtime` の差分。DST 境界の扱い）。
4. 言語コードの環境変数からの決定の正規化規則の厳密化（`ja_JP.UTF-8@euro` → `ja_JP`、空文字は未設定、`C.UTF-8` は `C` 扱い）。
5. 診断の整形規則（`path` NULL、`line` 0、`dropped`、警告の順序）と、`MND_ARGS_DECL_ERROR` / `OUT_OF_MEMORY` の終了コード。
6. 統計の文面（固定書式）と翻訳率の丸め（四捨五入か切り捨てか）。
7. 出力の安全性: 「処理完了後に 1 回だけ書く」で止めるか、一時ファイル + rename まで行うか。
8. テストの構成: `tests/cli/` の Naos スイート（プロセス内）、`cmake -P` による実行ファイルの起動テスト、`msgfmt --check` の 3 スクリプト（extract / init / update）、ラウンドトリップのドライバの有無。
9. CI: 現状の 5 環境で `monde` のビルドと新テストが通ること。追加の導入は不要（gettext は Linux に導入済み）。

#### 持ち越す Research 項目
- プロセス起動 1 回あたりのコスト（特に Windows の CI）と、652 × 3 回の許容可能性。
- POSIX / Windows でのプロセス起動の最小実装（`posix_spawn` / `CreateProcessW`）が必要になるか、`cmake -P` で足りるか。
- 一時ファイル + rename を行う場合の Windows の置き換え API と、非 ASCII パスでの動作。
- `getenv` が Windows で返す文字コード（ANSI）と、言語コードとして使う範囲（ASCII のみを想定してよいか）。

---

## 設計ディスカバリー（/kiro-spec-design、2026-11-07）

### Summary
- **Feature**: `monde-cli`
- **Discovery Scope**: Extension（既存部品の統合。light discovery）
- **参照したスキル・規則**: kiro-spec-design の `design-principles.md`、`design-discovery-light.md`、`design-synthesis.md`、`design-review-gate.md`。steering の `product.md`、`tech.md`、`structure.md`。外部ライブラリの追加はないため WebSearch は行っていない。
- **Key Findings**:
  - 統合に必要な公開 API はすべて揃っており（上記ギャップ分析の表）、新規の論理は OS 境界の小部品（標準入出力、日時、環境変数、診断と統計の整形）に限られる。
  - Naos にプロセス起動の支援はなく、外部ツールを使う既存テストは `cmake -P` + `execute_process` で書かれている。この流儀をそのまま実行ファイルの起動テストに使える。
  - Linux でのプロセス起動は約 1 ms/回（`/bin/true` を 1,000 回で 1.0 秒）。652 例 × 2 変種 × 2 コマンド ≈ 2,600 回は Linux で数秒、Windows でも数十秒の見込み。

### Research Log

#### 実行ファイルの起動テストの方式
- **Context**: 10.1〜10.4 は「ビルドした実行ファイルで」全例のラウンドトリップを求める。
- **Sources Consulted**: `tests/tu/msgfmt_check.cmake`、`tests/po/msgcat_compare.cmake`、`tests/tu/roundtrip_test.c`、`tests/markdown/spec_reader.h`、本環境での起動コストの計測。
- **Findings**: CMake の `execute_process` は全環境で同じに動き、引用符のエスケープを気にしなくてよい。例の取り出しは C の `spec_reader` にしかないので、例をファイルに書き出す Naos 不要の補助実行ファイルが必要。
- **Implications**: 「補助実行ファイルで例をファイル群に展開 → `cmake -P` で `monde` を例ごとに起動して比較」を採用する。C でのプロセス起動（`posix_spawn` / `CreateProcessW`）は書かない。

#### 標準入出力の扱い
- **Context**: 2.3〜2.4、3.8。foundation の `file_io` はパスからしか読めず、Windows の標準入出力はテキストモードで開かれる。
- **Findings**: `fread` / `fwrite` によるストリームの読み書きは `mnd_file_read` の内部処理と同じで、`FILE *` を受け取る関数に切り出せる。Windows では `_setmode(_fileno(stdin), _O_BINARY)` を `main` で 1 回呼べば改行の変換を止められる。
- **Implications**: foundation に `mnd_file_read_stream` / `mnd_file_write_stream` を追加し、`mnd_file_read` / `mnd_file_write` をその上に乗せる。バイナリモードの設定は `src/monde/main.c` の責務。

#### 日時と UTC オフセット
- **Context**: 4.2、5.8。`mnd_po_date_format(tm, utc_offset_minutes, out)` は分単位のオフセットを要求し、po は時計を読まない。
- **Findings**: C99 の範囲では `timegm` がない。`localtime` と `gmtime` の結果の差（年月日時分を日数に直して引く）で求められる。MSVC の `localtime` の警告は `_CRT_SECURE_NO_WARNINGS` で抑止済み。
- **Implications**: 時計の取得を関数ポインタで差し替えられるようにして、テストでは固定の時刻を与える。

#### 言語コードの環境変数
- **Context**: 5.3〜5.5。
- **Findings**: `getenv` は C99 で全環境にある。Windows では `LANG` は通常設定されておらず、設定されていれば ANSI 文字列だが、言語コードは ASCII の範囲で扱う。
- **Implications**: `getenv` も関数ポインタで差し替え可能にし、テストでは表から引く。

### Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
| --- | --- | --- | --- | --- |
| A: 実行ファイル 1 ターゲット | `src/monde/` に全ソース | 最少のファイル | Naos から内部関数をテストできず、テストの規約（`monde::<module>`）から外れる | 不採用 |
| B: ライブラリ `monde::cli` + 極薄 main | `src/cli/` にハンドラと小部品、`src/monde/main.c` は数十行 | 規約に沿う。入出力と時計・環境変数を差し替えてプロセス内でテストできる | 10.1 の「実行ファイルで」を別途満たす必要 | **採用** |
| C: B + 実行ファイルの起動テスト | B に加え、`cmake -P` で `monde` を全例について起動 | 10.1〜10.4 を字義どおり満たす | CI 時間（Windows で数十秒の見込み） | **採用**（B と組み合わせる） |

### Design Decisions

#### Decision: 入口 `mnd_cli_run` と差し替え可能な環境
- **Context**: プロセスを起動せずにサブコマンド全体をテストしたい。日時と環境変数は実行環境に依存する。
- **Alternatives Considered**: (1) `main` に直接書く、(2) `FILE *` と関数ポインタをまとめた環境構造体を渡す、(3) 入出力をバッファに抽象化する独自ストリーム型を作る。
- **Selected Approach**: (2)。`mnd_cli_env_t { FILE *in; FILE *out; FILE *err; const char *version; mnd_cli_getenv_fn getenv; mnd_cli_now_fn now; }` を受け取る `int mnd_cli_run(const mnd_cli_env_t *env, int argc, char *const *argv)`。`NULL` の関数ポインタは実物（`getenv`、`time`）を使う。
- **Rationale**: `FILE *` は C99 の範囲で、テストでは作業ディレクトリのファイルを開いて渡せる（`tmpfile` は Windows でルートに作ろうとして失敗することがあるため使わない）。独自ストリーム型は過剰。
- **Trade-offs**: ライブラリが `FILE *` に書く（I/O が境界に寄る原則は CLI 層が境界なので満たす）。
- **Follow-up**: 標準出力への書き込み失敗（パイプ切断など）の検出を `fflush` の戻り値で行う。

#### Decision: 出力は処理完了後に 1 回だけ書き、一時ファイル + rename は行わない
- **Context**: 2.7〜2.8。
- **Alternatives Considered**: (1) 完了後に `mnd_file_write` で 1 回書く、(2) 一時ファイルに書いて rename で置き換える。
- **Selected Approach**: (1)。
- **Rationale**: 全入力をメモリに読み終えてから処理し、成功したときだけ書く流れで要件を満たす。(2) は Windows の置き換え API と非 ASCII パスの扱いを foundation に足す必要があり、要件が求めていない。
- **Trade-offs**: 書き込み途中のディスク満杯などでは出力先が不完全になり得る（既存の `.po` を上書きする場合も同じ）。利用マニュアルで注意を促す（docs-and-release）。

#### Decision: 統計と情報の文面は固定の英語の書式
- **Selected Approach**: 「Components and Interfaces」の Reporter に書式を列挙する。翻訳率は整数演算で 0.1% 単位に四捨五入する（`(translated * 1000 + total / 2) / total`）。
- **Rationale**: 8.8（機械的に取り出せる固定の書式）。浮動小数点の丸めの環境差を避ける。

#### Decision: 言語コードの検査は CLI が先に行い、使い方のエラー（終了コード 2）にする
- **Context**: 5.7。`mnd_po_init` も同じ検査をして診断を返すが、終了コードの区別（1 と 2）は CLI の責務。
- **Selected Approach**: CLI が `--language` の値と環境変数から得た値を検査し、不正なら使い方のエラーにする。`mnd_po_init` の検査は二重の安全装置として残す。

#### Synthesis（一般化・採用・単純化）
- **一般化**: 4 つのハンドラは「入力を読む → 処理 → 改行コードを決める → 出力を書く → 統計を出す」の同じ骨格を持つ。入力の読み込み（`-` を含む）、出力の書き込み、改行コードの決定、診断の表示を共通部品（IO、Reporter）に寄せ、ハンドラは部品の呼び出し順だけを持つ。
- **採用**: 引数解析（cli-args）、PO の読み書きと init（po-format）、マージ（po-merge）、抽出と再構成（translation-units）はすべて既存部品を使い、CLI では再実装しない。プロセス起動は CMake に任せる。
- **単純化**: 独自ストリーム型、一時ファイル + rename、`--werror`、方言オプション、設定ファイルは作らない。`extract` のヘッダ値は `mnd_po_header_template` + `mnd_po_header_set` だけで組む。

### Risks & Mitigations
- 実行ファイルの起動テストが Windows の CI で遅い（見込み数十秒） — 変種ごとに 1 テストに分け、遅ければ `cmake -P` の対象を抜き取りに減らしてプロセス内テストを正とする（設計の Open Questions に記載）。
- UTC オフセットの計算の誤り（DST、負のオフセット、日付境界） — 固定時刻を与える単体テストで境界値を検証する。
- 標準出力への書き込みの失敗の見落とし — `fwrite` と `fflush` の戻り値を検査し、失敗を終了コード 1 にする。

### References
- `src/cli_args/args.h`、`src/po/po.h`、`src/po/po_init.h`、`src/po/po_write.h`、`src/po_merge/merge.h`、`src/tu/tu.h`、`src/foundation/file_io.h` — 統合する公開 API
- `tests/tu/extract_pot_main.c`、`tests/tu/msgfmt_check.cmake` — extract の手順と msgfmt テストの型
- `tests/markdown/spec_reader.h`、`tests/tu/roundtrip_test.c` — spec.txt の読み込みと変種の生成
