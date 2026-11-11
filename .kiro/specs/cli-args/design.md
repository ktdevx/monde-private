# 技術設計書: cli-args

## Overview

**Purpose**: cli-args は、コマンドとオプションを `static const` の宣言テーブルで定義し、その宣言に基づいてコマンドライン引数を解析する汎用の部品である。同じ宣言から英語のヘルプ、バージョン、使い方の案内を生成し、使い方のエラーを foundation の診断として返す。Windows では、非 ASCII を含む引数を UTF-8 で取得する機能も提供する。

**Users**: cli-args を使う開発者（現在は monde-cli の開発者）が、コマンドの体系をこの部品で宣言し、解析結果を受け取って各処理に振り分ける。本部品は Monde 固有の知識を持たないため、テストでは架空のプログラムの宣言だけで挙動を検証する。

**Impact**: 実装済みの cli-args（プログラムの直下にサブコマンドが 1 段だけ並ぶ構成）を、**ルートコマンドを起点とする再帰的なコマンドの木**に作り替える。サブコマンドのない形式は「子コマンドを持たないルートコマンド」として、同じ規則で扱う。公開 API（宣言の型、結果の型、ヘルプの関数）は互換性を保たずに置き換える。利用側の monde-cli は新しい API に書き換えるが、`monde` が受け付けるコマンドラインとヘルプの文面は変えない。引数の取得（`args_argv.*`）と foundation の `wide` は変更しない。

**改訂（2026-11-11）**: 宣言で選んだフラグが `--no-<name>` 形式の否定を受け付けるようにする（要件 14）。この改訂の設計は、末尾の「[改訂（2026-11-11）: 否定できるフラグ](#改訂2026-11-11-否定できるフラグ)」の節にまとめる。それより前の節は 2026-11-10 の改訂の時点の名前（`src/cli_args/`、`mnd_args_*`）で書いてあり、現在の名前（`cli_args/`、`clopts_*`）へは clopts-standalone の設計書の「名前の対応」で読み替える。

### Goals
- 1 つの宣言の形で、サブコマンドなし・1 段・入れ子の 3 つの構成を表し、同じ解析規則で扱う。
- 親コマンドのオプションを、宣言ごとに子孫へ引き継げる。
- 解析の結果を「成功」「ヘルプ」「バージョン」「子コマンドの未指定」「使い方のエラー」「宣言の誤り」「メモリ不足」で区別し、選択されたコマンドの経路とともに返す。
- コマンドごとのヘルプを 1 つの書式の規則で生成する。1 段の構成では、改訂前と同じ文面になる。
- `monde` の利用者から見た挙動を保つ。変わるのは、一部の使い方のエラーの文面だけである。

### Non-Goals
- Monde のコマンドとオプションの定義の変更、`monde` のコマンド体系の変更（monde-cli）
- 診断とヘルプの表示先・表示形式の最終決定と、終了コードの決定（monde-cli）
- 子コマンドを持つコマンドの位置引数、既定の子コマンド、サブコマンドの別名、同時に指定できないオプションの組の検査
- 値の型変換と妥当性の検査、長いオプションの省略形
- すべてのフラグへの一律の `--no-<name>` の適用、短い名前による否定、`--<name>=false` の形式、否定できないフラグを既定で有効にすること、値付きオプションの指定の取り消し
- 端末の幅による折り返し、未知のオプション・サブコマンドに対する候補の提示
- 環境変数や設定ファイルからの読み込み
- 改訂前の API との互換の層

## Boundary Commitments

### This Spec Owns
- 宣言の型（`mnd_args_program_t`、`mnd_args_command_t`、`mnd_args_option_t`、`mnd_args_positional_t`）と、その妥当性の規則
- 引数列の解析規則（本書「解析規則」の節）と、解析結果の型 `mnd_args_result_t`、`mnd_args_level_t`、状態 `mnd_args_status_t`
- 予約名: 子コマンド名 `help`、オプションの長い名前 `help` と `version`、短い名前 `h`
- 使い方のエラーと宣言の誤りの英語のメッセージ（本書「メッセージ」の節）
- コマンドのヘルプ、バージョン、使い方の案内、コマンドの経路の文面の書式
- Windows でのコマンドライン引数の取得と UTF-8 への変換（`mnd_args_argv_t`）。今回の改訂では変更しない
- foundation の部品 `wide`（改訂前の本 spec が追加した）。今回の改訂では変更しない
- 新しい API に合わせた monde-cli の書き換え（`src/cli/` の宣言、解析結果の受け取り、ヘルプと案内の生成の呼び出し）。挙動は変えない

### Out of Boundary
- `monde` 固有の宣言の内容（サブコマンド名、オプション名、説明文、バージョン文字列）と、各サブコマンドの処理
- 診断とヘルプを `stdout` / `stderr` に書く処理、終了コード（monde-cli）
- `monde` の共通オプション（`--output`、`--quiet`、`--no-wrap`）を引き継ぐオプションへ移すこと。`monde --quiet extract` が通るようになり、ヘルプの文面も変わるため、要件 13 に反する。今回は各サブコマンドへの重複した宣言を保つ
- README と利用マニュアルの改訂（文面が変わらないので不要。`tests/docs/` が一致を検査する）
- オプション値の解釈（パスとしての検査、数値への変換、キー名の検査など）
- foundation の既存の公開 API の変更

### Allowed Dependencies
- 製品コード（`src/cli_args/`）: C 標準ライブラリ、`monde::foundation` の `buf.h`、`diag.h`、`wide.h`（Windows のみ）。Windows では Win32 API の `GetCommandLineW`、`CommandLineToArgvW`（shell32）、`LocalFree` のみ。
- テストコード（`tests/cli_args/`）: 上記に加えて Naos。
- モジュール間の依存は `foundation ← cli_args ← monde-cli` の一方向。cli_args は他の汎用パーサ（po、markdown、yaml）に依存しない。
- モジュール内の依存の方向は「Architecture」の節に従う。`args_help.c` と `args_argv.c` は `args_parse.c` / `args_result.c` の関数に依存しない。
- monde-cli（`src/cli/`）は、cli_args の公開ヘッダ（`args.h`、`args_help.h`、`args_argv.h`）だけを include する。`args_internal.h` を include しない。

### Revalidation Triggers
- 宣言の型のメンバーの追加・削除・意味の変更 → monde-cli の宣言テーブル
- 解析規則（コマンドの選択、有効なオプション、優先順位、`--` の扱い）の変更 → monde-cli のテストと利用マニュアル
- 予約名の追加・変更 → monde-cli の宣言
- `mnd_args_status_t` の値の追加・意味の変更 → monde-cli の終了コードの決定
- ヘルプ・メッセージ・案内の文面の書式の変更 → monde-cli のテストの期待値、README と `docs/commands.md` の引用（`tests/docs/`）
- 診断の接頭辞（コマンドの経路）の形の変更 → monde-cli の `cli_report.c`（接頭辞を取り除く）
- 値の寿命の約束（引数列と宣言を指すこと）の変更 → monde-cli の引数列の扱い
- `monde` が引き継ぐオプションを使い始める場合 → monde-cli の要件、README と利用マニュアル

## Architecture

### Existing Architecture Analysis

改訂前の実装は、宣言の検査（DeclCheck）、走査（Parser）、結果の保持（ResultStore）、文面の生成（HelpFormatter）、引数の取得（ArgvSource）をファイル単位で分けている。この分割は改訂後の要件にも合うため、そのまま保つ。変更は次の 4 点に集中する（詳細は `research.md` のギャップ分析）。

- **宣言の型**: プログラムがサブコマンドの配列だけを持つ形を、ルートコマンドを指す形に変える。コマンドは子コマンドの配列を持てる。
- **コマンドの選択**: 入口で `argv[1]` を振り分ける専用の処理をなくし、ルートコマンドから始まる 1 つの走査に統合する。
- **結果の保持**: 選択された 1 つのコマンドの値だけを持つ形を、経路の段ごとに値を持つ形に変える。
- **ヘルプ**: 全体用とサブコマンド用の 2 つの書式を、コマンドの種類（子を持つか、末端か、ルートか）で要素が決まる 1 つの書式にする。

末端コマンドの中の走査（長短のオプション、値の記録、位置引数、`--`、回復走査）と、ヘルプの行の整形は、オプションの検索を「有効なオプション」の検索に差し替えるだけで再利用する。

### Architecture Pattern & Boundary Map

宣言の木を入力とする単一パスの走査器である。

```mermaid
graph TB
    subgraph monde_cli[monde-cli 下流]
        Run[cli_run と cli_report]
        Decl[monde の宣言テーブル]
    end
    subgraph cli_args[cli_args]
        Argv[ArgvSource args_argv.c]
        Parser[Parser args_parse.c]
        Check[DeclCheck args_decl.c]
        Result[ResultStore args_result.c]
        Help[HelpFormatter args_help.c]
        Types[宣言と結果の型 args.h]
    end
    subgraph foundation[foundation]
        Buf[buf]
        Diag[diag]
        Wide[wide Windows のみ]
    end
    Run --> Argv
    Run --> Parser
    Run --> Help
    Decl --> Types
    Parser --> Check
    Parser --> Result
    Result --> Check
    Help --> Check
    Check --> Types
    Parser --> Diag
    Check --> Diag
    Argv --> Diag
    Argv --> Wide
    Help --> Buf
```

**Architecture Integration**:
- 採用したパターン: 再帰的な宣言テーブル + 単一パスの走査器。コマンドの選択は走査の中の 1 つの分岐であり、サブコマンドの有無で入口を分けない。
- 依存の方向（違反はレビューで誤りとして扱う）: `args.h`（型） ← `args_decl.c`（検査と検索） ← `args_result.c`（結果の保持） ← `args_parse.c`（走査）。`args_help.c` は `args.h` と `args_decl.c` の検索関数だけに依存する。`args_argv.c` は `args_argv.h` と foundation の `diag`、`wide` だけに依存する。
- 経路の持ち方: 宣言は `static const` で親へのポインタを持たない。経路は解析結果（`mnd_args_result_t` の `levels`）が持ち、DeclCheck の検索と HelpFormatter は結果を読んで経路を知る。
- 既存のパターンの踏襲: `monde_add_module`、接頭辞 `mnd_`、`_t` 付きの typedef、エラーは `mnd_diag_list_t` で返す、`stdout` / `stderr` に書かない、`exit` しない。
- steering との整合: 汎用部品として Monde を知らない（1.11）。部品の公開 API の互換性は保証しない方針（tech.md）に沿って、API を置き換える。

### Technology Stack

| Layer | Choice / Version | Role in Feature | Notes |
|-------|------------------|-----------------|-------|
| 言語 | C99（MSVC は `/std:c11`） | 実装 | VLA を使わない。宣言テーブルは指示付き初期化子（C99）で書く |
| 依存モジュール | `monde::foundation` | `mnd_buf_t`（文面の出力先）、`mnd_diag_list_t`（エラー） | 変更なし |
| テスト | Naos v0.4.0（CTest） | 単体テスト | ラベル `cli_args`、`cli`、`docs` |

新しい依存はない。

## File Structure Plan

### Directory Structure
```
src/cli_args/
├── CMakeLists.txt     # 変更なし
├── args.h             # 変更: 宣言の型（ルートコマンド、子コマンド、引き継ぐ属性）、結果の型（経路の段）、取得関数
├── args_help.h        # 変更: 解析結果を受け取るヘルプ・案内・経路の生成関数
├── args_internal.h    # 変更: 子コマンドと有効なオプションの検索、段を扱う結果の書き込み関数
├── args_decl.c        # 変更: 宣言の木の検査（再帰）、子コマンドの検索、有効なオプションの検索と列挙
├── args_result.c      # 変更: 経路の段ごとの値の保持、名前による取得、既定値の適用
├── args_parse.c       # 変更: ルートからの単一の走査、コマンドの選択、help の語、回復走査、事後の検査
├── args_help.c        # 変更: 一本化したコマンドのヘルプ、案内、経路
├── args_argv.h        # 変更なし
└── args_argv.c        # 変更なし

tests/cli_args/
├── CMakeLists.txt     # 変更: 新しいスイートのソースを加える
├── main.c             # 変更: 新しいスイートを登録する
├── test_decl.h        # 変更: 3 つの架空のプログラム（tool、single、nest）の宣言
├── test_decl.c        # 変更: 同上の定義（指示付き初期化子）
├── decl_test.c        # 変更: 新しい宣言の規則（1.6〜1.9）を加え、既存の検査を新しい型に合わせる
├── dispatch_test.c    # 変更: 1 段の構成でのコマンドの選択、help、--help、--version、子コマンドの未指定
├── long_test.c        # 変更: 新しい型と取得関数に合わせる（規則は同じ）
├── short_test.c       # 変更: 同上
├── values_test.c      # 変更: 同上
├── positional_test.c  # 変更: 同上
├── errors_test.c      # 変更: 経路を含む接頭辞、回復走査の範囲（8.3、8.10）
├── help_test.c        # 変更: 結果を受け取る関数に合わせる。1 段の構成の文面は改訂前と同じ
├── single_test.c      # 新規: スイート single。サブコマンドのない構成（2.6、6.6、8.2、8.7、8.8、9.3、9.4）
├── nested_test.c      # 新規: スイート nested。2 段以上の構成（2.2〜2.5、2.8、7.2、7.3、8.4〜8.6、9.2）
├── inherit_test.c     # 新規: スイート inherit。引き継ぐオプション（12.1〜12.6、3.6、5.7、9.5）
└── argv_test.c        # 変更なし
```

### Modified Files（monde-cli）
- `src/cli/cli_commands.c` — 宣言テーブルを新しい型と指示付き初期化子に書き換える。ルートコマンド（説明と 4 つの子コマンド）を加える。オプションと位置引数の内容、順序は変えない。
- `src/cli/cli_run.c` — 状態ごとの振り分けを新しい API に合わせる（`mnd_args_command`、結果を受け取る `mnd_args_format_help`）。
- `src/cli/cli_report.c`、`cli_report.h` — `mnd_cli_report_usage_error` が解析結果を受け取り、`mnd_args_format_path` で得た経路を接頭辞として取り除く。
- `tests/cli/run_test.c` — 宣言の検証をルートコマンドの子コマンドをたどる形にする。8.6 などで文面が変わる期待値を直す。
- `tests/cli/report_test.c` — `mnd_cli_report_usage_error` の新しい引数に合わせる。
- `src/cli/cmd_*.c`、`cli_io.c` — 変更なし（`mnd_args_flag` などの取得関数の形は変わらない）。

### 変更しないファイル
- `README.md`、`docs/*.md` — ヘルプの文面が変わらないため。`tests/docs/` が変更なしで通ることを完了の条件とする（13.3）。
- `src/foundation/`、`src/monde/main.c`。

## 規約（本モジュール）

- モジュール名 `cli_args`、ターゲット `monde_cli_args`（別名 `monde::cli_args`）、テスト `cli_args_tests`、ラベル `cli_args`。短縮名は `args`。
- すべての公開関数は、明記したものを除いてポインタ引数に `NULL` を受け付けない。グローバルな状態を持たない。
- 文字列は UTF-8 の NUL 終端のバイト列として扱い、名前の照合はバイト単位の完全一致（大文字と小文字を区別）とする（2.7、3.7、10.1）。
- **宣言テーブルは指示付き初期化子で書く**（製品コードとテストの両方）。書かなかったメンバーは 0（`NULL`、`false`、`'\0'`）になるため、どのメンバーも 0 が「なし」を表すように型を定める。今後メンバーを加えても、既存の宣言は変えずに済む。GCC と Clang で、現在の警告の設定（`-Wall -Wextra -Wpedantic -Werror`、C99）のまま、入れ子の宣言が警告なしで通ることを確認済みである。

## 用語と記法

- **段**: 経路上の 1 つのコマンド。段 0 がルートコマンド、最後の段が選択中のコマンドである。
- **経路の文字列**: プログラム名に、段 1 以降のコマンド名を空白 1 つで区切って続けたもの（例: `tool`、`tool remote add`）。以下 `<path>` と書く。
- **子を持つコマンド**: `command_count > 0` のコマンド。**末端コマンド**: `command_count == 0` のコマンド。
- **有効なオプション**: 選択中のコマンドが宣言したオプションと、祖先が `inherited` を真として宣言したオプション。

## 解析規則

`argv[0]` はプログラムの起動名として読み飛ばし、メッセージとヘルプには宣言の `program->name` を使う。以下で「トークン」は `argv[1]` 以降の各要素を指す。

### 走査（2.x〜6.x、8.x、12.x）

宣言を検査した後、ルートコマンドを段 0 として選択し、トークンを左から 1 つずつ見る（2.1）。入口での特別な振り分けはない。`--` を見るまでは次の表に従う。「選択中」は最後の段のコマンドを指す。

| トークンの形 | 扱い |
|---|---|
| `--` | 末端コマンド: 終端とし、以降のトークンはすべて位置引数にする（6.3）。子を持つコマンド: 次のトークンがあれば、それを示す `unexpected argument` とする（2.5）。なければ走査を終える |
| `--help` | 選択中のコマンドのヘルプの要求（8.1、8.2）。走査を終えて `MND_ARGS_HELP` を返す |
| `--help=<x>` | 使い方のエラー `does not take a value`（3.5） |
| `--version` | 選択中のコマンドがルートコマンドなら、バージョンの要求（8.8）。走査を終えて `MND_ARGS_VERSION` を返す。そうでなければ `unknown option`（8.9） |
| `--version=<x>` | ルートコマンドなら `does not take a value`。そうでなければ `unknown option` |
| `--<name>` | `<name>` を有効なオプションの長い名前から検索する。フラグなら指定済みにする（3.1）。値・リスト値なら次のトークンを、`-` で始まる場合や子コマンド名と一致する場合も含めて値として消費する（3.2）。次のトークンがなければ `requires a value`（3.4） |
| `--<name>=<value>` | `<name>` は最初の `=` までとする。値・リスト値なら `=` の後ろ全体を値にする（3.3）。フラグなら `does not take a value`（3.5） |
| `-<c1><c2>...` | 2 文字目から 1 文字ずつ処理する（下記） |
| `-` 単独、または `-` で始まらない | オプションでない引数（4.7）。下の「オプションでない引数」に従う |

長い名前が有効なオプションに見つからない場合は `unknown option` とし、トークン（`=` 以降を除く）を示す（3.6）。前方一致、まだ選択されていない子コマンドのオプション、祖先の引き継がないオプションは、いずれも見つからない扱いになる。

短いオプションの各文字 `c` の処理（4.1〜4.6）:
1. `c == 'h'` なら選択中のコマンドのヘルプの要求とする（8.1）。
2. `c` が ASCII 以外のバイトなら、トークン全体を示す `unknown option` とする。
3. `c` を有効なオプションの短い名前から検索し、見つからなければ `-<c>` を示す `unknown option` とする（4.5）。
4. フラグなら指定済みにして次の文字へ進む（4.3）。
5. 値・リスト値なら、`c` より後ろに残りがあればそれを値とし（4.2、4.4）、なければ次のトークンを値として消費する（4.1、4.4）。次のトークンもなければ `-<c>` を示す `requires a value` とする（4.6）。このトークンの処理を終える。

オプションでない引数:

| 選択中のコマンド | トークン | 扱い |
|---|---|---|
| 末端コマンド | すべて（`help` を含む） | 位置引数として記録する（6.1、8.7）。個数が `max_positionals` を超えたら、そのトークンを示す `unexpected argument`（6.5） |
| 子を持つコマンド | `help` | 次節の「`help` の語」に従う（8.4〜8.6） |
| 子を持つコマンド | 子コマンド名と一致 | その子コマンドを新しい段として選択し、次のトークンから走査を続ける（2.2） |
| 子を持つコマンド | 上記以外 | そのトークンを示す `unknown command`（2.3） |

ルートコマンドが末端コマンドなら、表の 1 行目だけが当てはまる。どのトークンもサブコマンド名として解釈しない（2.6）。

値の記録（5.x、12.x）: 値は、そのオプションを**宣言した段**に記録する。引き継ぐオプションは、子コマンド名の前で指定しても後で指定しても同じ場所に記録されるため、解析結果は同じになり（12.4）、前後の両方で指定すれば同じオプションの繰り返しとして扱われる（12.3）。
- フラグ: 指定済みにする。繰り返してもエラーにしない（5.5）。
- 単一の値: 最後に指定した値で置き換える（5.4）。
- リスト値: 値を `,` で区切り、各項目を空白を含めてそのまま末尾に追加する（5.1、5.3）。空の項目（値が空、先頭・末尾の `,`、`,,`）があれば `empty item` とする（5.2）。

### `help` の語（8.4〜8.7）

子を持つコマンドの走査中に、オプションでない引数 `help` を見たら、その時点の段の数を記録して次のように進める。

1. 後続のトークンを順に、選択中のコマンドの子コマンド名として検索する。見つかれば、その子コマンドを新しい段として選択する（8.5）。
2. トークンが尽きたら、選択中のコマンドのヘルプの要求として `MND_ARGS_HELP` を返す（8.4、8.5）。
3. 選択中のコマンドが子を持ち、トークンがどの子コマンド名とも一致しなければ、`unknown command` とする。選択中のコマンドが末端コマンドで、まだトークンがあれば、`unexpected argument` とする（8.6）。どちらの場合も、段を `help` を見た時点まで戻してから誤りを保留する。接頭辞と案内は `help` を書いたコマンドの経路になる。

この間、トークンはすべて名前として扱う（`-` で始まるトークンをオプションとして解釈しない）。

### 走査の後の検査と優先順位（2.4、6.4、7.1、7.4、8.3、8.10）

1. 走査中に最初の誤りを検出したら、その診断を保留する（7.4）。誤りを検出した時点で選択中のコマンドが**末端コマンド**なら、回復走査に移る。回復走査では、誤りの原因となったトークン（消費した値を含む）の次から、`--` より前に残っているトークンのうち、完全に `--help` または `-h` と一致するものだけを探す。見つかれば保留した誤りを捨てて、その末端コマンドの `MND_ARGS_HELP` を返す（8.3）。見つからなければ保留した誤りだけを報告して `MND_ARGS_USAGE_ERROR` を返す。選択中のコマンドが**子を持つコマンド**なら、回復走査を行わずに保留した誤りを報告する（8.10）。未知のサブコマンド、子コマンドの選択前の未知のオプション、`help` の語の誤り（段を戻した後は子を持つコマンドが選択中になる）がこれに当たる。改訂前の「サブコマンドの選択後だけ回復する」という範囲を、そのまま一般化した規則である。
2. 走査が誤りなしで終わり、選択中のコマンドが子を持つなら、`MND_ARGS_NO_COMMAND` を返す（2.4）。必須のオプションの検査と既定値の適用は行わない。
3. 選択中のコマンドが末端コマンドなら、位置引数の個数が `min_positionals` 未満かを検査する。不足していれば、最初に不足している位置引数の名前を示す `missing argument` とする（6.4）。
4. 次に、経路上のすべての段のオプションを、段 0 から順に、各段では宣言の順に見て、必須で明示的に指定されていない最初の 1 つを示す `missing required option` とする（7.1）。引き継ぐオプションは宣言した段に記録されるので、経路上のどこで指定されても指定済みになる（12.5）。
5. 最後に、経路上のすべての段で、明示的に指定されていないオプションに既定値を適用する（5.6、12.6）。

手順 3 と 4 の誤りも保留として扱うが、この時点で残りのトークンはないため、回復走査は何も見つけない。報告する使い方のエラーは常に 1 件である（7.4）。

```mermaid
stateDiagram-v2
    [*] --> Scan: ルートコマンドを選択
    Scan --> Scan: オプションを記録
    Scan --> Scan: 子コマンドを選択
    Scan --> Scan: 位置引数を記録
    Scan --> Done_Help: --help または -h
    Scan --> Done_Version: ルートで --version
    Scan --> HelpWord: 子を持つコマンドで help
    HelpWord --> HelpWord: 子コマンドをたどる
    HelpWord --> Done_Help: 末尾に到達
    HelpWord --> Done_Error: 名前の誤り
    Scan --> Rest: 末端コマンドで --
    Rest --> Rest: 位置引数を記録
    Scan --> Recover: 末端コマンドでの最初の誤り
    Scan --> Done_Error: 子を持つコマンドでの誤り
    Recover --> Done_Help: 完全一致の --help または -h
    Recover --> Done_Error: -- または末尾に到達
    Scan --> Done_NoCommand: 子を持つコマンドで末尾に到達
    Scan --> PostCheck: 末端コマンドで末尾に到達
    Rest --> PostCheck: 末尾に到達
    PostCheck --> Done_Error: 位置引数の不足または必須の欠落
    PostCheck --> Done_Ok: 既定値を適用
```

### 改訂前の 1 段の構成との違い

1 段の構成（`monde`）で結果が変わるのは次の場合だけである。`-h` で始まるまとめ書きの行と `monde --` の行を除き、いずれも使い方のエラーのままで、文面だけが変わる。ほかは、状態・選択されるコマンド・値・メッセージが改訂前と一致する。

| コマンドライン | 改訂前 | 改訂後 | 根拠 |
|---|---|---|---|
| `monde help a b`（`a` が未知） | `too many arguments for 'help'` | `unknown command 'a'` | 8.6（エラーのまま。文面だけが変わる） |
| `monde help extract x` | `too many arguments for 'help'` | `unexpected argument 'x'` | 8.6（同上） |
| `monde --foo=bar` | `unknown option '--foo=bar'` | `unknown option '--foo'` | 3.6（同上） |
| `monde --version=1` | `unknown option '--version=1'` | `option '--version' does not take a value` | 3.5（同上） |
| `monde --help=1` | `unknown option '--help=1'` | `option '--help' does not take a value` | 3.5（同上） |
| `monde -xy` | `unknown option '-xy'` | `unknown option '-x'` | 4.5（同上。子コマンドの選択前にも短いオプションを 1 文字ずつ処理する） |
| `monde -vh` | `unknown option '-vh'` | `unknown option '-v'` | 4.5（同上） |
| `monde -- extract` | `unknown option '--'` | `unexpected argument 'extract'` | 2.5（同上） |
| `monde help extract --help` | `too many arguments for 'help'` | `unexpected argument '--help'` | 8.6（同上） |
| `monde --` | `unknown option '--'`（使い方のエラー） | 子コマンドの未指定（ルートのヘルプを標準エラー出力へ、終了コード 2） | 2.5（終了コードは同じ） |
| `monde -hx`、`monde -hh`、`monde -h1`、`monde -h=1` | `unknown option '<トークン>'`（使い方のエラー、終了コード 2） | ルートのヘルプ（標準出力、終了コード 0） | 4.3、4.5、8.1、13.4 の例外（短いオプションを左から処理し、`h` でヘルプの要求になる。`monde extract -hx` と同じ扱い） |

`monde --version extra` と `monde --help extra` は、改訂前と同じくバージョンとヘルプを返す（8.1、8.8）。`monde bogus --help` と `monde --bogus --help` は、改訂前と同じく使い方のエラーになる（8.10）。

## メッセージ

診断はすべて `MND_SEVERITY_ERROR`、`path == NULL`、`line == 0` で追加する。使い方のエラーのメッセージは、誤りを検出した時点の経路の文字列を使って `<path>: ` で始める（7.2）。ルートコマンドが選択中なら `tool: `、子コマンドの選択後は `tool build: `、`tool remote add: ` のようになる。`<arg>` は利用者が書いた形（`--name` または `-c`、または問題のトークン）である。

| 場面 | メッセージ（`<prefix>` は `<path>: `） | 要件 |
|---|---|---|
| 未知のサブコマンド | `<prefix>unknown command '<token>'` | 2.3、8.6 |
| 未知のオプション | `<prefix>unknown option '<arg>'` | 3.6、4.5、8.9 |
| 値の欠落 | `<prefix>option '<arg>' requires a value` | 3.4、4.6 |
| フラグへの値 | `<prefix>option '--<name>' does not take a value` | 3.5 |
| リストの空の項目 | `<prefix>option '<arg>' has an empty item in '<value>'` | 5.2 |
| 位置引数の不足 | `<prefix>missing argument <NAME>` | 6.4 |
| 余分な引数 | `<prefix>unexpected argument '<token>'` | 2.5、6.5、8.6 |
| 必須のオプションの欠落 | `<prefix>missing required option '--<name>'` | 7.1 |
| 宣言の誤り | `invalid argument declaration: <理由>`（理由は DeclCheck の表） | 1.6〜1.10 |
| メモリ不足 | `out of memory while parsing arguments` | — |

詳しい使い方を確認する方法（7.3）は、メッセージには含めず、HelpFormatter の `mnd_args_format_usage_hint` で別に生成する。引数の取得（ArgvSource）のメッセージは変更しない。

## Requirements Traceability

| Requirement | Summary | Components | Interfaces | Flows |
|-------------|---------|------------|------------|-------|
| 1.1, 1.2, 1.3, 1.4, 1.5 | 宣言の内容（ルート、子コマンド、引き継ぐ属性） | 型（args.h） | `mnd_args_program_t`、`mnd_args_command_t`、`mnd_args_option_t` | — |
| 1.6, 1.7, 1.8, 1.9, 1.10 | 宣言の誤り | DeclCheck、Parser | `mnd_args_decl_check`、`MND_ARGS_DECL_ERROR` | 解析の最初 |
| 1.11 | Monde を知らない | 全体 | — | コードレビュー、テストの宣言 |
| 2.1, 2.2, 2.3, 2.5, 2.6, 2.7 | コマンドの選択 | Parser、DeclCheck | `mnd_args_parse`、`mnd_args_find_child` | 走査 |
| 2.4 | 子コマンドの未指定 | Parser、HelpFormatter | `MND_ARGS_NO_COMMAND`、`mnd_args_format_help` | 走査の後の検査 |
| 2.8 | 経路の特定 | ResultStore | `mnd_args_command`、`mnd_args_depth`、`mnd_args_command_at` | — |
| 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7 | 長いオプション | Parser、DeclCheck、ResultStore | `mnd_args_find_long` | 走査 |
| 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7 | 短いオプション | Parser、DeclCheck、ResultStore | `mnd_args_find_short` | 走査 |
| 5.1, 5.2, 5.3, 5.4, 5.5, 5.6 | リスト値、繰り返し、既定値 | ResultStore、Parser | `mnd_args_is_set`、`mnd_args_list_*` | 走査、事後の検査 |
| 5.7 | 経路上のどのオプションも取得できる | ResultStore | `mnd_args_flag`、`mnd_args_value` ほか | — |
| 6.1, 6.2, 6.3, 6.4, 6.5, 6.6 | 位置引数と終端 | Parser、ResultStore | `mnd_args_positional_*` | 走査、事後の検査 |
| 7.1, 7.2, 7.4 | 必須、メッセージ、最初の誤り | Parser | 診断 | 回復走査、事後の検査 |
| 7.3 | ヘルプの要求方法 | HelpFormatter | `mnd_args_format_usage_hint` | — |
| 7.5, 7.6 | 診断で返す、状態の区別 | Parser | `mnd_args_status_t` | — |
| 8.1, 8.2, 8.3, 8.10 | `--help` と回復走査 | Parser | `MND_ARGS_HELP` | 走査、回復走査 |
| 8.4, 8.5, 8.6, 8.7 | `help` の語 | Parser | `MND_ARGS_HELP` | `help` の語 |
| 8.8, 8.9 | バージョン | Parser、HelpFormatter | `MND_ARGS_VERSION`、`mnd_args_format_version` | 走査 |
| 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 9.8, 9.9 | 文面の生成 | HelpFormatter、DeclCheck | `mnd_args_format_help`、`mnd_args_effective_*` | — |
| 10.1, 10.5 | バイト列のまま、引数列を変えない | Parser、ResultStore | 値は引数列を指す | — |
| 10.2, 10.3, 10.4 | Windows での取得 | ArgvSource（変更なし） | `mnd_args_argv_get` | — |
| 11.1, 11.2, 11.3, 11.4 | 単体テスト | CliArgsTests | `cli_args_tests` | — |
| 12.1, 12.2 | 引き継ぐ・引き継がないオプションの有効範囲 | DeclCheck | `mnd_args_find_long`、`mnd_args_find_short` | 走査 |
| 12.3, 12.4, 12.5, 12.6 | 前後での指定、必須、既定値 | ResultStore、Parser | 宣言した段への記録 | 走査、事後の検査 |
| 13.1, 13.2, 13.3, 13.4 | monde-cli の追従 | MondeCliAdapter | `src/cli/` の宣言と振り分け | Migration Strategy |
| 14.1 | 否定の形を受け付けるかの宣言（1.5 の追加分を含む） | 型（clopts.h） | `clopts_option_t.negatable` | — |
| 14.2, 14.3, 14.7, 14.8 | `--no-<name>` の照合とエラー | Parser、DeclCheck | `clopts_find_negated`、`scan_long` | 改訂（2026-11-11）の「解析規則」 |
| 14.4, 14.5, 14.6 | 後に書いた指定の優先、有効・無効と明示的な指定の取得 | ResultStore | `clopts_value_t.enabled`、`clopts_flag`、`clopts_is_set` | 同上 |
| 14.9 | 引き継ぐフラグの否定 | DeclCheck、ResultStore | `clopts_find_negated`、宣言した段への記録 | 同上 |
| 14.10, 14.11 | 否定に関する宣言の誤り | DeclCheck | `clopts_decl_check` | 解析の最初 |
| 14.12 | ヘルプの `--[no-]<name>` | HelpFormatter | `clopts_format_help` | — |
| 14.13 | 否定できるフラグがなければ改訂前と同じ | 全体 | 既存のテスト一式 | 改訂（2026-11-11）の「Testing Strategy」 |
| 14.14, 14.16 | 既定で有効の宣言と、その誤り | 型（clopts.h）、DeclCheck | `clopts_option_t.default_enabled`、`clopts_decl_check` | 解析の最初 |
| 14.15 | 指定がなければ既定の状態を返す | ResultStore | `clopts_result_apply_defaults`、`clopts_flag` | 走査の後の検査 |
| 14.17 | ヘルプに既定で有効を示す | HelpFormatter | `clopts_format_help` | — |
| 15.1〜15.7 | フラグの出現回数 | 型（clopts.h）、ResultStore | `clopts_value_t.count`、`clopts_flag_count` | 改訂（2026-11-11）: 出現回数、隠しオプション、別名の「Requirements Traceability」 |
| 16.1〜16.8 | 隠しオプション | 型（clopts.h）、DeclCheck、HelpFormatter | `clopts_option_t.hidden` | 同上 |
| 17.1〜17.14 | オプションの別名 | 型（clopts.h）、DeclCheck | `clopts_option_t.aliases`、`clopts_find_long` | 同上 |

## Components and Interfaces

| Component | Domain/Layer | Intent | Req Coverage | Key Dependencies (P0/P1) | Contracts |
|-----------|--------------|--------|--------------|--------------------------|-----------|
| 型（args.h） | 宣言 | 宣言・状態・結果の型 | 1.1〜1.5 | — | State |
| DeclCheck | 宣言 | 宣言の木の検査、子コマンドと有効なオプションの検索 | 1.6〜1.10, 2.7, 3.7, 12.1, 12.2 | Diag (P0) | Service |
| ResultStore | 解析 | 経路の段ごとの結果の保持と取得 | 2.8, 5.1, 5.3〜5.7, 6.1, 10.1, 10.5, 12.3〜12.6 | DeclCheck (P0) | Service, State |
| Parser | 解析 | 引数列の走査と状態の決定 | 2.1〜2.6, 3.x, 4.x, 5.2, 6.x, 7.1, 7.2, 7.4〜7.6, 8.1〜8.10 | DeclCheck (P0)、ResultStore (P0)、Diag (P0) | Service |
| HelpFormatter | 文面 | コマンドのヘルプ、バージョン、案内、経路の生成 | 2.4, 7.3, 8.8, 9.1〜9.9 | DeclCheck (P1)、Buf (P0) | Service |
| ArgvSource | 入力 | 引数列の UTF-8 での取得（変更なし） | 10.2〜10.4 | Diag (P0)、Wide (P0) | Service |
| CliArgsTests | テスト | 単体テスト | 11.1〜11.4 | Naos (P0) | — |
| MondeCliAdapter | monde-cli | `src/cli/` を新しい API に合わせる | 13.1〜13.4 | cli_args の公開 API (P0) | — |

### 宣言

#### 型（`src/cli_args/args.h`）

| Field | Detail |
|-------|--------|
| Intent | 呼び出し側が `static const` で書く宣言の型と、解析の状態・結果の型 |
| Requirements | 1.1, 1.2, 1.3, 1.4, 1.5 |

**Contracts**: State [x]

```c
typedef enum mnd_args_kind
{
    MND_ARGS_FLAG,   /* 値を取らない */
    MND_ARGS_VALUE,  /* 単一の値。繰り返すと後勝ち */
    MND_ARGS_LIST    /* カンマ区切りのリスト値。繰り返すと連結 */
} mnd_args_kind_t;

typedef struct mnd_args_option
{
    const char *long_name;      /* 必須。先頭の "--" を含まない（例: "output"） */
    char short_name;            /* 1 文字の ASCII 英数字。ないなら '\0' */
    mnd_args_kind_t kind;
    const char *value_name;     /* ヘルプ上の値の表示名（例: "FILE"）。NULL なら "VALUE"。フラグでは使わない */
    const char *help;           /* 説明（英語、1 行）。NULL 可 */
    bool required;              /* フラグでは false でなければならない */
    const char *default_value;  /* 既定値。NULL ならなし。リスト値はカンマ区切りで書く。フラグでは NULL */
    bool inherited;             /* true なら、宣言したコマンドの子孫でも有効（1.5） */
} mnd_args_option_t;

typedef struct mnd_args_positional
{
    const char *name;  /* ヘルプとメッセージ上の名前（例: "INPUT"）。山括弧を含まない */
    const char *help;  /* 説明。NULL 可 */
} mnd_args_positional_t;

#define MND_ARGS_UNLIMITED ((size_t)-1)

typedef struct mnd_args_command mnd_args_command_t;

struct mnd_args_command
{
    const char *name;                         /* 子コマンド名。ルートコマンドでは使わない（NULL 可） */
    const char *summary;                      /* 親のヘルプの一覧に出す 1 行の説明。ルートコマンドでは使わない */
    const char *description;                  /* このコマンドのヘルプの説明。NULL なら summary を使う */
    const mnd_args_option_t *options;         /* option_count == 0 なら NULL 可 */
    size_t option_count;
    const mnd_args_positional_t *positionals; /* 末端コマンドだけが持てる。positional_count == 0 なら NULL 可 */
    size_t positional_count;
    size_t min_positionals;
    size_t max_positionals;                   /* MND_ARGS_UNLIMITED なら上限なし */
    const mnd_args_command_t *commands;       /* 子コマンド。command_count == 0 なら末端コマンド（NULL 可） */
    size_t command_count;
};

typedef struct mnd_args_program
{
    const char *name;                /* プログラム名。ルートコマンドの名前として使う */
    const char *version;             /* バージョン文字列 */
    const mnd_args_command_t *root;  /* ルートコマンド。必須 */
} mnd_args_program_t;

typedef enum mnd_args_status
{
    MND_ARGS_OK,             /* 解析に成功。末端コマンドが選択されている */
    MND_ARGS_HELP,           /* ヘルプの要求。対象は結果の経路の最後のコマンド */
    MND_ARGS_VERSION,        /* バージョンの要求 */
    MND_ARGS_NO_COMMAND,     /* 子を持つコマンドで子コマンドが指定されなかった。対象は経路の最後のコマンド */
    MND_ARGS_USAGE_ERROR,    /* 利用者の入力の誤り。診断を 1 件追加済み */
    MND_ARGS_DECL_ERROR,     /* 宣言の誤り（プログラムの不具合）。診断を 1 件以上追加済み */
    MND_ARGS_OUT_OF_MEMORY   /* メモリ不足。診断を追加済み（記録できなければ dropped） */
} mnd_args_status_t;
```

- 3 つの構成の表し方: サブコマンドなしは `root->command_count == 0`（1.4）。1 段は、ルートコマンドの子コマンドがすべて末端コマンド。入れ子は、子コマンドがさらに `commands` を持つ（1.3）。
- 位置引数の名前の対応は改訂前と同じである。`i` 番目の位置引数の名前は `positionals[i]`、上限なしで `i >= positional_count` なら最後の要素の名前を使う。
- 利用側の識別子（ハンドラの対応付けのための整数など）は持たせない。利用側は `mnd_args_command` が返すポインタか名前で振り分ける。必要になれば、メンバーの追加で対応できる（指示付き初期化子のため、既存の宣言は壊れない）。

### DeclCheck（`src/cli_args/args_decl.c`、内部ヘッダ `args_internal.h`）

| Field | Detail |
|-------|--------|
| Intent | 宣言の木の妥当性の検査と、子コマンド・有効なオプションの検索と列挙 |
| Requirements | 1.6, 1.7, 1.8, 1.9, 1.10, 2.7, 3.7, 12.1, 12.2 |

**Responsibilities & Constraints**
- 検査は解析のたびに、引数列を見る前に行う。誤りはすべて診断に追加する（1 件で止めない）。
- ルートコマンドから木を深さ優先でたどる。たどる間、ルートから現在のコマンドまでの祖先の列を持ち、祖先との名前の重複（1.8）と循環の検出に使う。深さに上限を設けない（1.3）。
- 循環（あるコマンドの子孫に、そのコマンド自身と同じ宣言が現れる）は宣言の誤りとして報告し、その先をたどらない。検査が終わらなくなるのを防ぐ。
- 検索はバイト単位の完全一致で行う（2.7、3.7）。
- 有効なオプションの検索と列挙は、解析結果の段の列（`result->levels`）を読む。最後の段のオプションを宣言の順に、続いて親から祖先へ向かって、`inherited` が真のオプションを宣言の順に見る（12.1、12.2）。祖先との重複は宣言の誤りなので、名前は一意に決まる。

宣言の規則と、違反したときの理由（メッセージの `<理由>`）。`<path>` は検査中のコマンドの経路の文字列である。

| 規則 | 理由の文面 |
|---|---|
| `program->name`、`program->version` が `NULL` でも空でもない | `program name and version are required` |
| `program->root` が `NULL` でない | `root command is required` |
| 子を持つコマンドは位置引数を持たない（`positional_count`、`min_positionals`、`max_positionals` がすべて 0）（1.6） | `command '<path>' cannot have both subcommands and positional arguments` |
| 子コマンド名は `NULL` でも空でもなく、`-` で始まらない | `invalid command name '<name>' in command '<path>'` |
| 子コマンド名が `help` でない（1.7） | `command name 'help' is reserved in command '<path>'` |
| 同じ親の子コマンド名が重複しない（1.7） | `duplicate command '<name>' in command '<path>'` |
| コマンドが自分自身を子孫に持たない | `command '<path>' contains itself` |
| 長い名前は `NULL` でも空でもなく、`-` で始まらず、`=` を含まない | `invalid option name '<name>' in command '<path>'` |
| 長い名前が `help`、`version` でない、短い名前が `h` でない（1.9） | `option '--help' is reserved in command '<path>'` / `option '--version' is reserved in command '<path>'` / `option '-h' is reserved in command '<path>'` |
| 短い名前は `'\0'` または ASCII 英数字 | `invalid short option for '--<name>' in command '<path>'` |
| 同じコマンド内で長い名前・短い名前が重複しない（1.8） | `duplicate option '--<name>' in command '<path>'` / `duplicate option '-<c>' in command '<path>'` |
| 祖先のオプション（引き継ぐかどうかによらない）と長い名前・短い名前が重複しない（1.8） | `option '--<name>' in command '<path>' duplicates an option of command '<ancestor path>'` / `option '-<c>' in command '<path>' duplicates an option of command '<ancestor path>'` |
| フラグは `required == false` かつ `default_value == NULL` | `flag '--<name>' cannot be required or have a default in command '<path>'` |
| `required` と `default_value` を同時に持たない | `option '--<name>' cannot be both required and defaulted in command '<path>'` |
| リスト値の既定値に空の項目がない | `default of '--<name>' has an empty item in command '<path>'` |
| 末端コマンドで `min_positionals <= max_positionals` | `invalid positional range in command '<path>'` |
| 末端コマンドで、上限ありなら `positional_count == max_positionals`、上限なしなら `positional_count >= 1` | `positional names do not match the range in command '<path>'` |

**Contracts**: Service [x]

```c
/* args_internal.h（モジュール内部と tests/cli_args/ のみ） */

/* 経路上の 1 つのオプションの位置。level は段の添字、index はその段のコマンドの options の添字 */
typedef struct mnd_args_ref
{
    size_t level;
    size_t index;
} mnd_args_ref_t;

/* 宣言が妥当なら true。誤りごとに診断を追加して false を返す */
bool mnd_args_decl_check(const mnd_args_program_t *program, mnd_diag_list_t *diags);

/* parent の子コマンドを名前で探す。見つからなければ NULL */
const mnd_args_command_t *mnd_args_find_child(const mnd_args_command_t *parent,
                                              const char *name, size_t name_len);

/* 有効なオプションを名前で探す。見つかれば *ref に位置を入れて true（12.1、12.2） */
bool mnd_args_find_long(const mnd_args_result_t *result, const char *name, size_t name_len,
                        mnd_args_ref_t *ref);
bool mnd_args_find_short(const mnd_args_result_t *result, char short_name, mnd_args_ref_t *ref);

/* 有効なオプションの列挙。順序は「最後の段のオプション、親の引き継ぐオプション、…、ルートの引き継ぐオプション」 */
size_t mnd_args_effective_count(const mnd_args_result_t *result);
mnd_args_ref_t mnd_args_effective_at(const mnd_args_result_t *result, size_t i); /* i < count */

/* ref が指すオプションの宣言 */
const mnd_args_option_t *mnd_args_option_at(const mnd_args_result_t *result, mnd_args_ref_t ref);

/* option->value_name が NULL なら "VALUE" */
const char *mnd_args_value_name(const mnd_args_option_t *option);

/* min(index, positional_count - 1) 番目の名前 */
const char *mnd_args_positional_name(const mnd_args_command_t *command, size_t index);
```

### 解析

#### ResultStore（`src/cli_args/args_result.c`）

| Field | Detail |
|-------|--------|
| Intent | 解析結果（状態、経路、段ごとのオプションの値、位置引数）を保持し、長い名前で取り出せるようにする |
| Requirements | 2.8, 5.1, 5.3, 5.4, 5.5, 5.6, 5.7, 6.1, 10.1, 10.5, 12.3, 12.4, 12.5, 12.6 |

**Responsibilities & Constraints**
- 経路を段の配列で持つ。各段は、コマンドへのポインタと、そのコマンドの `options` と同じ添字の値の配列を持つ。値は宣言した段に記録する（12.3〜12.5）。
- 名前による取得は、最後の段から段 0 へ向かって、各段のコマンドが宣言したすべてのオプション（引き継ぐかどうかによらない）を探す（5.7）。経路上で名前は一意である（1.8）。
- 単一の値と位置引数は、引数列または宣言の文字列を指す（コピーしない）。引数列と宣言（`program` を含む）は結果より長く生存しなければならない。リスト値の項目だけを確保してコピーする。
- 引数列を書き換えない（10.5）。
- 既定値の適用（5.6、12.6）: 経路上のすべての段で、明示的に指定されていないオプションに `default_value` を設定する。`explicit_set` は `false` のままにする。

**Contracts**: Service [x] / State [x]

```c
/* args.h */
typedef struct mnd_args_value
{
    bool explicit_set;   /* 利用者が明示的に指定した */
    const char *value;   /* 単一の値: 引数列か既定値を指す。なければ NULL */
    char **items;        /* リスト値: 確保した項目の配列 */
    size_t item_count;
    size_t item_cap;
} mnd_args_value_t;

/* 経路上の 1 つの段 */
typedef struct mnd_args_level
{
    const mnd_args_command_t *command;
    mnd_args_value_t *values;   /* command->option_count 個。0 個なら NULL */
} mnd_args_level_t;

typedef struct mnd_args_result
{
    mnd_args_status_t status;
    const mnd_args_program_t *program;  /* 解析に使った宣言。解析前は NULL */
    mnd_args_level_t *levels;           /* levels[0] がルートコマンド、最後が選択中のコマンド */
    size_t level_count;
    size_t level_cap;
    const char **positionals;           /* 引数列を指す */
    size_t positional_count;
    size_t positional_cap;
} mnd_args_result_t;

#define MND_ARGS_RESULT_INIT { MND_ARGS_NO_COMMAND, NULL, NULL, 0, 0, NULL, 0, 0 }

void mnd_args_result_init(mnd_args_result_t *result);
/* 確保した領域を解放して初期状態に戻す。初期状態・解放済みに対しても安全 */
void mnd_args_result_free(mnd_args_result_t *result);

/* 経路（2.8）。level_count == 0（DECL_ERROR、OUT_OF_MEMORY、解析前）なら NULL / 0 */
const mnd_args_command_t *mnd_args_command(const mnd_args_result_t *result);   /* 選択されたコマンド（最後の段） */
size_t mnd_args_depth(const mnd_args_result_t *result);                        /* 段の数。ルートだけなら 1 */
const mnd_args_command_t *mnd_args_command_at(const mnd_args_result_t *result, size_t level); /* level < depth */

/* 以下、long_name が経路上のどのコマンドでも宣言されていない場合は false / NULL / 0 を返す（5.7） */
bool mnd_args_is_set(const mnd_args_result_t *result, const char *long_name);   /* 明示的な指定（5.6） */
bool mnd_args_flag(const mnd_args_result_t *result, const char *long_name);     /* フラグが指定された */
const char *mnd_args_value(const mnd_args_result_t *result, const char *long_name); /* 単一の値または既定値 */
size_t mnd_args_list_count(const mnd_args_result_t *result, const char *long_name);
const char *mnd_args_list_at(const mnd_args_result_t *result, const char *long_name, size_t index);
size_t mnd_args_positional_count(const mnd_args_result_t *result);
const char *mnd_args_positional_at(const mnd_args_result_t *result, size_t index); /* index < count */
```

```c
/* args_internal.h（Parser だけが使う） */
/* command を新しい段として加え、値の配列を確保する。失敗はメモリ不足 */
bool mnd_args_result_push(mnd_args_result_t *result, const mnd_args_command_t *command);
/* 段を level_count 個まで減らし、取り除いた段の値を解放する（help の語の誤りで使う） */
void mnd_args_result_truncate(mnd_args_result_t *result, size_t level_count);
void mnd_args_result_set_flag(mnd_args_result_t *result, mnd_args_ref_t ref);
void mnd_args_result_set_value(mnd_args_result_t *result, mnd_args_ref_t ref, const char *value);
/* value を ',' で区切って追加する。空の項目があれば何も追加せず MND_ARGS_USAGE_ERROR、確保の失敗は MND_ARGS_OUT_OF_MEMORY */
mnd_args_status_t mnd_args_result_append_list(mnd_args_result_t *result, mnd_args_ref_t ref,
                                              const char *value, bool explicit_set);
bool mnd_args_result_add_positional(mnd_args_result_t *result, const char *arg);
/* 経路上のすべての段で、明示的に指定されていないオプションに既定値を適用する */
mnd_args_status_t mnd_args_result_apply_defaults(mnd_args_result_t *result);
```

- 改訂前の `result->command` と `result->values` は廃止する。利用側は `mnd_args_command` を使う。
- 取得関数の名前と引数は改訂前と同じである。monde-cli のハンドラ（`cmd_*.c`）は変更なしで動く。

#### Parser（`src/cli_args/args_parse.c`）

| Field | Detail |
|-------|--------|
| Intent | 宣言に基づいて引数列を走査し、状態を決めて結果と診断を返す |
| Requirements | 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 3.1〜3.7, 4.1〜4.7, 5.2, 6.1〜6.6, 7.1, 7.2, 7.4, 7.5, 7.6, 8.1〜8.10 |

**Responsibilities & Constraints**
- 「解析規則」と「メッセージ」の節のとおりに動作する。
- 走査の状態は、解析結果（段の列）と、次に見るトークンの位置、終端を見たかどうか、保留した誤りからなる。選択中のコマンドは結果の最後の段から得る（走査の状態に別に持たない）。
- 診断の接頭辞は、誤りを保留した時点の段の列から作る（7.2）。
- `stdout` / `stderr` に書かず、`exit` / `abort` しない（7.5）。
- 状態 `MND_ARGS_USAGE_ERROR` のとき、追加する診断は 1 件だけである（7.4）。

**Contracts**: Service [x]

```c
/* args.h */
/*
 * argv[0] を読み飛ばし、argv[1]〜argv[argc - 1] を解析する。
 * result は初期化済みであること。以前の内容は解放してから書き込む。
 * 戻り値は result->status と同じ。
 */
mnd_args_status_t mnd_args_parse(const mnd_args_program_t *program, int argc, char *const *argv,
                                 mnd_args_result_t *result, mnd_diag_list_t *diags);
```
- 事前条件: `argc >= 0`、`argv[0]`〜`argv[argc - 1]` が NUL 終端の文字列（`argc == 0` のとき `argv` の要素は読まない）。
- 事後条件（「経路」は `result->levels`）:
  - `MND_ARGS_OK`: 経路の最後が末端コマンド。経路上のすべてのオプションに既定値が適用済み。位置引数の個数が `[min, max]` の範囲内。
  - `MND_ARGS_HELP`: 経路の最後がヘルプの対象のコマンド。診断なし。
  - `MND_ARGS_VERSION`: 経路はルートコマンドだけ。診断なし。
  - `MND_ARGS_NO_COMMAND`: 経路の最後が、子コマンドを指定されなかった子を持つコマンド。診断なし。
  - `MND_ARGS_USAGE_ERROR`: 診断 1 件。経路は誤りを検出した時点のもの（案内の生成に使う）。
  - `MND_ARGS_DECL_ERROR`: 引数列を読まない。診断 1 件以上。経路は空。
  - `MND_ARGS_OUT_OF_MEMORY`: 経路の内容は不定だが、`mnd_args_result_free` で解放できる。
- `MND_ARGS_DECL_ERROR` と `MND_ARGS_OUT_OF_MEMORY` 以外では `result->program == program` で、経路は 1 段以上ある。HelpFormatter の関数に渡せる。

呼び出し側の典型的な使い方:

```mermaid
sequenceDiagram
    participant Run as 呼び出し側
    participant Parser
    participant Help as HelpFormatter
    Run->>Parser: mnd_args_parse(program, argc, argv)
    Parser-->>Run: status, result, diags
    alt HELP または NO_COMMAND
        Run->>Help: mnd_args_format_help(result)
    else VERSION
        Run->>Help: mnd_args_format_version(program)
    else USAGE_ERROR
        Run->>Help: mnd_args_format_usage_hint(result)
    else OK
        Run->>Run: mnd_args_command(result) に応じて処理を振り分け
    end
```

**Implementation Notes**
- 回復走査は、最初の誤りの原因となったトークンの次から始める（値の欠落のように誤りが末尾で起きた場合は、残りがない）。
- `--help` / `-h` / `--version` を値として消費した場合（例: `-o -h`）は、ヘルプやバージョンにならない（3.2、4.1、8.1、8.8）。
- 改訂前の入口の振り分け（`argv[1]` の比較、`dispatch_help`、`select_command`）は削除し、走査に統合する。

### 文面

#### HelpFormatter（`src/cli_args/args_help.c`、`args_help.h`）

| Field | Detail |
|-------|--------|
| Intent | 解析結果の経路が指すコマンドのヘルプ、バージョン、使い方の案内、経路の文字列を決定的に生成する |
| Requirements | 2.4, 7.3, 8.8, 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 9.8, 9.9 |

**Responsibilities & Constraints**
- 宣言の検査は行わない。`mnd_args_parse` が `MND_ARGS_DECL_ERROR`、`MND_ARGS_OUT_OF_MEMORY` 以外を返した結果を前提とする。
- 結果の型（`args.h`）のメンバーを読むだけで、`args_result.c` と `args_parse.c` の関数は呼ばない。
- 出力は `out` の末尾に追加する。改行は LF のみ。端末の幅や環境に依存しない（9.9）。
- 失敗はメモリ不足だけで、`false` を返す。

**Contracts**: Service [x]

```c
/* args_help.h */
/* 経路の最後のコマンドのヘルプ */
bool mnd_args_format_help(const mnd_args_result_t *result, mnd_buf_t *out);
/* "<program> <version>\n" */
bool mnd_args_format_version(const mnd_args_program_t *program, mnd_buf_t *out);
/* "Run '<path> --help' for usage.\n" */
bool mnd_args_format_usage_hint(const mnd_args_result_t *result, mnd_buf_t *out);
/* 経路の文字列 "<path>"（改行なし）。診断の接頭辞 "<path>: " と同じ形 */
bool mnd_args_format_path(const mnd_args_result_t *result, mnd_buf_t *out);
```

コマンドのヘルプの書式（9.1〜9.5）。`{...}` は宣言の値、`{pad}` は列を揃える空白である。説明が `NULL` または空なら、その段落と直後の空行を出さない。

子を持つコマンド:

```
Usage: {path}{ [<options>]} <command> [<options>] [<arguments>]

{command.description または command.summary}

Commands:
  {child.name}{pad}  {child.summary}
  help{pad}  Show help for a command

Options:
  {有効なオプションの左列}{pad}  {option.help}{注記}
  -h, --help{pad}  Show this help
      --version{pad}  Show version information

Run '{path} <command> --help' for help on a command.
```

末端コマンド:

```
Usage: {path} [<options>] {positional usage}

{command.description または command.summary}

Arguments:
  <{positional.name}>{pad}  {positional.help}

Options:
  {有効なオプションの左列}{pad}  {option.help}{注記}
  -h, --help{pad}  Show this help
      --version{pad}  Show version information
```

- 使い方の行: 子を持つコマンドでは、有効なオプションが 1 つ以上あるときだけ、経路の直後に ` [<options>]` を出す。末端コマンドでは常に ` [<options>]` を出す。
- `--version` の行は、ルートコマンドのヘルプにだけ出す（9.4）。
- `Arguments:` の段落は、位置引数がなければ出さない（9.3）。
- `Options:` の行の順序は、`mnd_args_effective_at` の順（そのコマンドのオプション、親の引き継ぐオプション、…、ルートの引き継ぐオプション）、`-h, --help`、`--version` とする。引き継いだオプションは、そのコマンドのオプションと同じ段落に同じ形で並べる（9.5）。
- 位置引数の使い方の行: `i < min_positionals` なら `<NAME>`、それ以外は `[<NAME>]`。上限なしの場合、最後の名前を `<NAME>...`（最後の添字が `min_positionals` 未満）または `[<NAME>...]` とする。要素は空白 1 つで区切る。
- オプションの左列: 短い名前があれば `-c, --name`、なければ `    --name`（短い名前の分の空白 4 つ）。値は ` <VALUE>`、リスト値は ` <VALUE>[,<VALUE>...]`（9.7）。
- 注記（9.6）: 必須なら ` (required)`、既定値があれば ` (default: {default_value})`。`help` が `NULL` の場合は、注記の先頭の空白を出さない。
- 列の揃え（9.8）: 段落ごとに左列の最大のバイト数を求め、各行で左列の後ろに空白を補って、説明の開始位置を `2 + 最大幅 + 2` バイト目に揃える。説明が空の行では、末尾に空白を出さない。

**改訂前の文面との一致**: 1 段の構成で、ルートコマンドがオプションを持たず、子コマンドが引き継いだオプションを持たない場合（`monde` の構成）、ルートコマンドのヘルプは改訂前の全体のヘルプと、子コマンドのヘルプは改訂前のサブコマンドのヘルプと、バイト単位で一致する。案内も `Run 'monde --help' for usage.`、`Run 'monde extract --help' for usage.` で一致する。`help_test.c` の既存の期待値と `tests/docs/` がこれを固定する（13.3）。

### 入力

#### ArgvSource（`src/cli_args/args_argv.c`、`args_argv.h`）

変更しない（10.2、10.3、10.4）。契約は改訂前のとおり、`mnd_args_argv_get`、`mnd_args_argv_free`、Windows での `mnd_args_argv_from_wide` である。

### monde-cli

#### MondeCliAdapter（`src/cli/cli_commands.c`、`cli_run.c`、`cli_report.c`、`cli_report.h`）

| Field | Detail |
|-------|--------|
| Intent | `monde` の宣言と、解析結果の受け取りを新しい API に合わせる。利用者から見た挙動は変えない |
| Requirements | 13.1, 13.2, 13.3, 13.4 |

**Responsibilities & Constraints**
- 宣言（13.1）: ルートコマンドは、説明（改訂前のプログラムの説明）と 4 つの子コマンドを持ち、オプションを持たない。子コマンドのオプションと位置引数は、内容と順序を改訂前のまま保つ。`inherited` は使わない。
- 振り分け（13.2）: `MND_ARGS_OK` では `mnd_args_command(&result)->name` でハンドラを引く。`MND_ARGS_HELP` は標準出力へ、`MND_ARGS_NO_COMMAND` は標準エラー出力へ `mnd_args_format_help(&result)` の文面を書く。終了コードは改訂前と同じである。
- 使い方のエラー（13.4）: `mnd_cli_report_usage_error` は解析結果を受け取る。`mnd_args_format_path` で得た経路に `": "` を続けた接頭辞を診断の本文から取り除き、`mnd_args_format_usage_hint` の案内を続ける。出力の形は改訂前と同じである。

```c
/* cli_report.h */
void mnd_cli_report_usage_error(FILE *err, const mnd_args_result_t *result, const mnd_diag_list_t *diags);
```

**Implementation Notes**
- `mnd_cli_program` は、改訂前と同じく宣言を複製して `version` だけを実行時の値に差し替える。ルートコマンドは `static const` のままで、`program.root` がそれを指す。
- 利用者から見て変わるのは、「改訂前の 1 段の構成との違い」の表の場合だけである。`tests/cli/run_test.c` の期待値のうち、この表に当たるものを直す。

## Data Models

### Domain Model

```mermaid
graph LR
    Program --> RootCommand
    RootCommand --> Option
    RootCommand --> ChildCommand
    ChildCommand --> Option
    ChildCommand --> Positional
    ChildCommand --> ChildCommand
    Result --> Level
    Level --> RootCommand
    Level --> ChildCommand
    Level --> Value
```

- **宣言**（不変、呼び出し側が所有）: プログラムが 1 つのルートコマンドを指す。コマンドは、オプションの配列と、子コマンドの配列または位置引数の配列を持つ。
- **結果**（解析ごとに作る、呼び出し側が所有して `mnd_args_result_free` で解放）: 段の配列が経路を表す。段は宣言のコマンドを指し、そのコマンドのオプションと同じ個数の値を持つ。
- **不変条件**:
  - `MND_ARGS_DECL_ERROR`、`MND_ARGS_OUT_OF_MEMORY`、解析前を除き、`levels[0].command == program->root`。
  - `levels[i + 1].command` は `levels[i].command` の子コマンドの 1 つである。
  - 位置引数は、経路の最後が末端コマンドのときだけ 1 個以上になる。

## Error Handling

### Error Strategy
- 利用者の入力の誤り: `MND_ARGS_USAGE_ERROR` と診断 1 件。表示、案内の併記、終了コードは呼び出し側が決める。
- 宣言の誤り（プログラムの不具合）: `MND_ARGS_DECL_ERROR` と、誤りごとの診断。利用者の入力の誤りと状態で区別する（1.10）。
- メモリ不足: `MND_ARGS_OUT_OF_MEMORY`。結果は解放可能な状態を保つ。HelpFormatter は `false` を返す。
- どの関数も `stdout` / `stderr` に書かず、プロセスを終了しない（7.5）。`assert` による契約違反の検出はデバッグビルドのみとする。

### Error Categories and Responses
- **使い方の誤り**: メッセージの表に従う。メッセージには問題の引数と、誤りを検出した時点の経路を含める（7.2）。
- **宣言の誤り**: DeclCheck の表に従う。メッセージにはコマンドの経路を含める。
- **子コマンドの未指定**: 誤りではなく独立した状態（`MND_ARGS_NO_COMMAND`）として返す（2.4、7.6）。エラーとして扱うかどうかは呼び出し側が決める（monde-cli は使い方を標準エラー出力に書き、終了コード 2 を返す）。

## Testing Strategy

すべて Naos で書き、`cli_args_tests` に 1 ファイル 1 スイートでまとめる（11.3）。宣言は `tests/cli_args/test_decl.c` の架空のプログラムだけを使い、Monde の宣言を使わない（11.1）。各テストは、解析の後に引数列が変わっていないことを確認できるよう、引数列を配列のリテラルで持つ。

テスト用の宣言（11.2）:

- **`tool`**（バージョン `1.2.3`、1 段）: 改訂前と同じ内容を、ルートコマンドの子コマンドとして宣言する。
  - `build`: `--output/-o <FILE>`（値、必須）、`--keys/-k <KEY>`（リスト、既定値 `a,b`）、`--level <N>`（値、既定値 `1`）、`--verbose/-v`（フラグ）、`--quiet/-q`（フラグ）。位置引数 `SOURCE`（1 個ちょうど）。
  - `copy`: オプションなし。位置引数 `SRC`、`DEST`（2 個ちょうど）。
  - `cat`: オプションなし。位置引数 `FILE`（0 個以上、上限なし）。
- **`single`**（バージョン `2.0`、サブコマンドなし）: ルートコマンドが末端コマンド。`--output/-o <FILE>`（値）、`--verbose/-v`（フラグ）、`--keys/-k <KEY>`（リスト）。位置引数 `FILE`（0 個以上、上限なし）。
- **`nest`**（バージョン `3.0`、入れ子）:
  - ルート: `--verbose/-v`（フラグ、引き継ぐ）、`--config/-c <FILE>`（値、引き継ぐ、既定値 `nest.cfg`）、`--dry-run`（フラグ、引き継がない）。
  - `remote`（子を持つ）: `--token/-t <TOKEN>`（値、引き継ぐ、必須）、`--tags <TAG>`（リスト、引き継ぐ）。
    - `remote add`: `--force/-f`（フラグ）。位置引数 `NAME`、`URL`（2 個ちょうど）。
    - `remote remove`: オプションなし。位置引数 `NAME`（1 個ちょうど）。
  - `status`: オプションなし。位置引数なし。

### Unit Tests
- **decl**: 改訂前の検査（名前の重複、予約名、必須のフラグ、リストの既定値、位置引数の範囲）を、経路を含む新しい文面で確認する。加えて、ルートコマンドが `NULL`、子を持つコマンドの位置引数（1.6）、同じ親の子コマンド名の重複と `help` という子コマンド名（1.7）、祖先のオプションとの長い名前・短い名前の重複（引き継ぐ場合と引き継がない場合の両方）（1.8）、`--version` という長い名前（1.9）、自分自身を子に持つコマンドで、それぞれ `MND_ARGS_DECL_ERROR` と名前を含む診断が返ること。2 段目のコマンドの誤りで、診断が `nest remote` のような経路を示すこと。誤りを 2 つ含む宣言で診断が 2 件になること（1.10）。
- **dispatch**（`tool`）: `tool build x -o y` で `build` が選択され、`mnd_args_depth` が 2 であること（2.2、2.8）。`tool bulid` が `tool: unknown command 'bulid'`、`tool --verbose build` が `tool: unknown option '--verbose'`、`tool Build` が未知のサブコマンドになること（2.3、2.7、3.6）。`tool` が `MND_ARGS_NO_COMMAND` で経路がルートだけであること（2.4）。`tool -- build` が `unexpected argument 'build'`、`tool --` が `MND_ARGS_NO_COMMAND`（2.5）。`tool --help`、`tool -h`、`tool --help build`、`tool help`、`tool help copy`、`tool help nope`、`tool help copy x`、`tool --version`、`tool --version x`、`tool build --version` の状態と経路（8.1、8.2、8.4〜8.6、8.8、8.9）。
- **long / short / values / positional**（`tool`）: 改訂前のテストを、新しい型と取得関数に合わせて保つ（3.1〜3.7、4.1〜4.7、5.1〜5.6、6.1〜6.5）。
- **errors**（`tool`）: `tool build x` が `missing required option '--output'`（7.1）。各メッセージが `tool build: ` で始まること（7.2）。`tool build --bad --worse x` の診断が 1 件であること（7.4）。`tool build --bad -h`、`tool build x --help` が `MND_ARGS_HELP` になること（8.3）。`tool bogus --help`、`tool --bogus -h`、`tool help nope --help` が使い方のエラーのままで、診断が 1 件であること（8.10）。`tool build -o -h x` が `-h` を値として解析すること。各状態が区別されること（7.6）。
- **single**（`single`）: `single` が `MND_ARGS_OK` で位置引数 0 個、`mnd_args_depth` が 1 であること（2.6、6.6）。`single a -v b` と `single -v a b` の結果が同じであること（6.2）。`single help` と `single build` が位置引数になること（2.6、8.7）。`single --help` がルートコマンドのヘルプ、`single --version` と `single --version x` がバージョンであること（8.2、8.8）。`single --bad --help` がルートコマンドのヘルプになること（末端コマンドのルートでは回復走査が働く）（8.3）。`single --bad` が `single: unknown option '--bad'`、案内が `Run 'single --help' for usage.` であること（7.2、7.3）。ヘルプの文面（使い方の行、`Arguments:`、`Options:` に `--version` がある）をバイト単位で比較すること（9.1、9.3、9.4）。
- **nested**（`nest`）: `nest remote add n u -t x` で経路が `nest`、`remote`、`add` の 3 段になること（2.2、2.8）。`nest remote` が `MND_ARGS_NO_COMMAND` で経路の最後が `remote` であること、そのヘルプの文面（使い方の行に ` [<options>]`、`Commands:` に `add`、`remove`、`help`、末尾の案内が `Run 'nest remote <command> --help' ...`）（2.4、9.2）。`nest remote bogus` が `nest remote: unknown command 'bogus'` で、`nest remote bogus --help` も同じエラーになること（8.10）。`nest remote add --bad --help` が `add` のヘルプになること（8.3）。`nest remote bogus` の案内が `Run 'nest remote --help' for usage.`（2.3、7.2、7.3）。`nest remote add n` が `nest remote add: missing argument <URL>`（6.4、7.2）。`nest help remote add`、`nest remote help add`、`nest remote add --help` が同じコマンドのヘルプになること（8.1、8.4、8.5）。`nest help remote bogus` が `nest: unknown command 'bogus'`、`nest help status x` が `nest: unexpected argument 'x'`（8.6）。`nest remote --version` が未知のオプションになること（8.9）。`nest status` が `MND_ARGS_OK`（必須の `--token` は経路にないので検査されない）（7.1）。
- **inherit**（`nest`）: `nest -v remote add n u -t x`、`nest remote -v add n u -t x`、`nest remote add -v n u -t x` で `mnd_args_flag(result, "verbose")` が真になり、ほかの値も同じであること（12.1、12.4）。`nest remote --dry-run add ...` が `nest remote: unknown option '--dry-run'`、`nest --dry-run remote add n u -t x` が成功して `mnd_args_flag(result, "dry-run")` が真であること（12.2、3.6、5.7）。`nest -c a remote add -c b n u -t x` で `config` が `b`、`nest remote --tags a add --tags b,c n u -t x` で `tags` が `a`、`b`、`c` になること（12.3）。`--token` を `remote` の直後と `add` の後のどちらで指定しても成功し、指定しなければ `nest remote add: missing required option '--token'` になること（12.5、7.1）。`config` を指定しないと `nest.cfg` が返り、`mnd_args_is_set` が偽であること（12.6）。`nest status --tags a` が未知のオプションになること（`remote` の子孫ではない）（12.1）。`nest remote add --help` のヘルプで、`Options:` が `--force`、`--token`、`--tags`、`--verbose`、`--config`、`-h, --help` の順に並ぶこと（9.5）。
- **help**（`tool`）: ルートコマンドと `build`・`copy`・`cat` のヘルプ、バージョン、案内、`mnd_args_format_path` を、期待する文面のリテラルとバイト単位で比較する（7.3、8.8、9.1〜9.9、11.4）。ルートコマンドと 3 つの子コマンドのヘルプの期待値は、改訂前の `help_test.c` の文面から変えない。同じ結果から 2 回生成した文面が一致すること（9.9）。
- **argv**: 変更なし（10.2〜10.4）。

### Integration Tests（monde-cli）
- `tests/cli/run_test.c`: 宣言の検証（4 つの子コマンド、オプション、位置引数）が新しい型で通ること（13.1）。既存のコマンドラインのテストが、「改訂前の 1 段の構成との違い」の表に当たるものを除いて、期待値を変えずに通ること（13.2、13.4）。`monde bogus --help` が改訂前と同じく使い方のエラー（終了コード 2）になることを加える（8.10、13.4）。
- `tests/cli/report_test.c`: 使い方のエラーの出力（接頭辞の除去と案内の行）が、改訂前と同じバイト列になること（13.4）。
- `tests/docs/`: 変更なしで通ること。README と `docs/commands.md` に引用した `monde --help`、`monde <sub> --help` の出力と一致する（13.3）。
- `tests/cli/` のほかのスイート（extract、init、update、translate、roundtrip）が変更なしで通ること（13.2）。

### 全体にかかわる確認
- 引数列を変えない（10.5）: 各スイートで、解析の前後で引数の文字列の内容が一致することを確認する。
- Monde を知らない（1.11）: `src/cli_args/` に `monde`、`extract` などの Monde 固有の語が現れないことをコードレビューで確認する。
- 値のバイト列の保持（10.1）: 非 ASCII の UTF-8（`"\xE6\x97\xA5"`）を値と位置引数に渡し、同じバイト列が返ること。
- 対応環境のすべて（MSVC、MinGW-w64、Apple Clang、GCC、Clang）で、指示付き初期化子の宣言が警告なしでビルドできること（CI）。

## Security Considerations
- 書式文字列を外部入力から作らない。利用者の引数とコマンド名は常に `%s` の引数として `mnd_diag_add` に渡す。
- 段の数、リスト値の項目数と長さ、位置引数の個数の計算は、確保の前にオーバーフローを検査する。
- 宣言の循環を DeclCheck が検出するため、誤った宣言で検査や解析が終わらなくなることはない。

## Migration Strategy

`cli_args` は 1 つの静的ライブラリなので、型（`args.h`）を変えると、モジュール内の 4 つの `.c` とテストが同時にビルドできなくなる。互換の層は作らず、次の 2 段階で進める。段階 1 の間は、`monde_cli` と `monde` のターゲットはビルドできない。

```mermaid
flowchart LR
    P1[段階 1 cli_args の作り替え] --> C1{cli_args_tests が通る}
    C1 --> P2[段階 2 monde-cli の追従]
    P2 --> C2{全ターゲットと全テストが通る}
```

- **段階 1（cli_args）**: 型、DeclCheck、ResultStore、Parser、HelpFormatter と `tests/cli_args/` を新しい形にする。確認は、ターゲット `cli_args_tests` だけをビルドして `ctest -L cli_args` で行う。モジュール内では、依存の方向（型 → DeclCheck → ResultStore → Parser、HelpFormatter）の順に進める。既存の 1 段の構成（`tool`）のテストを先に通し、その後でサブコマンドなし（`single`）、入れ子（`nest`）、引き継ぐオプションのテストを加える。
- **段階 2（monde-cli）**: `src/cli/` と `tests/cli/` を書き換える。確認は、全ターゲットのビルドと `ctest` の全体（`cli`、`docs` を含む）で行う。`tests/docs/` が変更なしで通ることが、ヘルプの文面が変わっていないことの証拠になる。
- **戻し方**: 2 つの段階は同じブランチの連続した変更とし、段階 2 が通るまで main に入れない。途中で中止する場合は、ブランチを破棄すればよい（データの移行はない）。

## 改訂（2026-11-11）: 否定できるフラグ

要件 14 と、それに伴う 1.5、11.1 の変更の設計である。この節は現在の名前（`cli_args/`、`clopts_*` / `CLOPTS_*`）で書く。この節に書いていない規則（走査、優先順位、メッセージの接頭辞、ヘルプの書式）は、前の節のままとする。

### 概要

- **Purpose**: 宣言で選んだフラグを、`--<name>` で有効に、`--no-<name>` で無効にでき、両方が指定されたときは後に書いた方を優先する。否定できるフラグは、既定で有効と宣言できる。
- **Impact**: CLOpts の公開の型にメンバーを 3 つ足す（宣言の `negatable` と `default_enabled`、結果の `enabled`）。否定できるフラグを宣言しない利用側（Monde を含む）は、ソースも挙動も変わらない。
- **分類**: 既存の部品の拡張（Extension）。新しい部品、新しい依存、新しいファイル（テストを除く）はない。

### Boundary Commitments

#### This Spec Owns
- `clopts_option_t` の `negatable` と、その妥当性の規則（フラグだけに許す、`no-<name>` との衝突を誤りにする）
- `clopts_option_t` の `default_enabled` と、その妥当性の規則（否定できるフラグだけに許す）、ヘルプでの既定の表記
- `--no-<name>` の照合の規則と、照合の順序（完全一致を先に見る）
- フラグの結果の持ち方（`clopts_value_t` の `enabled`）と、`clopts_flag` / `clopts_is_set` の意味
- ヘルプでの `--[no-]<name>` の表記
- 追加する宣言の誤りの英語のメッセージ
- CLOpts のバージョン番号（0.1.0 → 0.2.0）と `cli_args/README.md` の該当箇所

#### Out of Boundary
- `monde` のオプションを否定できるフラグに置き換えること。`src/cli/`、`tests/cli/`、`docs/`、ルートの `README.md` は変更しない
- 短い名前による否定、`--<name>=false` の形式、否定できないフラグを既定で有効にすること、値付きオプションとリスト値の取り消し
- すべてのフラグへの一律の適用
- CLOpts のビルド定義、パッケージ定義、CI（clopts-standalone が持つ）。変更するのは `project(CLOpts VERSION ...)` の番号と、テストのソースの一覧だけである

#### Allowed Dependencies
- CLOpts のライブラリは C99 の標準ライブラリだけに依存する（clopts-standalone の設計のまま）。この改訂で依存は増えない。
- `cli_args/` の中から、その外のファイルを参照しない。
- モジュール内の依存の向きは変えない: 型 → DeclCheck → ResultStore → Parser。HelpFormatter は DeclCheck にだけ依存する。

#### Revalidation Triggers
- `negatable`、`default_enabled` の意味や、照合の順序の変更 → 否定できるフラグを宣言している利用側
- `clopts_flag` / `clopts_is_set` の意味の変更 → すべての利用側（Monde の `src/cli/cmd_*.c` を含む）
- ヘルプの `--[no-]<name>` と `(default: on)` の表記の変更 → 否定できるフラグを宣言している利用側のヘルプの期待値
- `monde` が否定できるフラグを使い始める場合 → monde-cli の要件、`docs/commands.md` と README の引用（`tests/docs/`）

### File Structure Plan

```
cli_args/
├── CMakeLists.txt                # 変更: project(CLOpts VERSION 0.2.0)
├── README.md                     # 変更: 機能の一覧に否定できるフラグを加え、宣言と取得の説明を足す
├── include/clopts/clopts.h       # 変更: clopts_option_t に negatable と default_enabled、clopts_value_t に enabled。clopts_flag / clopts_is_set のコメント
├── src/
│   ├── clopts_internal.h         # 変更: clopts_find_negated を加える。clopts_result_set_flag に bool enabled の引数を加える
│   ├── clopts_decl.c             # 変更: 否定に関する宣言の検査（14.10、14.11、14.16）、clopts_find_negated
│   ├── clopts_result.c           # 変更: enabled の記録と解放時の初期化、既定で有効の適用、clopts_flag が enabled を返す
│   ├── clopts_parse.c            # 変更: scan_long に否定の形の照合を加える。フラグの記録に enabled を渡す
│   └── clopts_help.c             # 変更: 左列の幅の計算と出力に "[no-]" を加える。説明に " (default: on)" を加える
└── tests/
    ├── CMakeLists.txt            # 変更: negate_test.c を加える
    ├── main.c                    # 変更: スイート negate を登録する
    ├── test_decl.h               # 変更: 架空のプログラム neg の宣言
    ├── test_decl.c               # 変更: 同上の定義
    ├── negate_test.c             # 新規: スイート negate（14.1〜14.9、14.12、14.15、14.17）
    ├── decl_test.c               # 変更: 否定に関する宣言の誤り（14.10、14.11、14.16）
    └── values_test.c             # 変更: clopts_result_set_flag の呼び出し 3 か所に引数 true を足す（期待値は変えない）
```

変更しないファイル: `cli_args/src/clopts_argv.c`、`clopts_buf.[ch]`、`clopts_errors.c`、`cli_args/cmake/`、`cli_args/tests/` のほかのスイート（14.13 の検証として、期待値を変えずに通す。values_test.c は上の呼び出しの修正だけ）、`src/`、`tests/`、`docs/`、`.github/`。

### 型（`cli_args/include/clopts/clopts.h`）

```c
typedef struct clopts_option
{
    const char *long_name;
    char short_name;
    clopts_kind_t kind;
    const char *value_name;
    const char *help;
    bool required;
    const char *default_value;
    bool inherited;
    bool negatable;             /* true なら "--no-<long_name>" で無効にできる。フラグだけに指定できる（14.1） */
    bool default_enabled;       /* true なら、指定がないときに有効とする。否定できるフラグだけに指定できる（14.14） */
} clopts_option_t;

typedef struct clopts_value
{
    bool explicit_set;   /* 利用者が明示的に指定した（否定の形を含む） */
    const char *value;
    char **items;
    size_t item_count;
    size_t item_cap;
    bool enabled;        /* フラグ: 最後の指定が肯定の形なら true、否定の形なら false。未指定なら宣言の default_enabled。フラグ以外では false */
} clopts_value_t;

bool clopts_is_set(const clopts_result_t *result, const char *long_name);   /* どちらかの形で明示的に指定された（5.6、14.5） */
bool clopts_flag(const clopts_result_t *result, const char *long_name);     /* フラグが有効である。未指定なら既定の状態（14.5、14.15） */
```

- どのメンバーも末尾に足す。0（`false`）が「否定できない」「既定で無効」「無効」を表すので、指示付き初期化子で書いた既存の宣言は変えずに済む。C++ の指示付き初期化子は宣言順を要求するが、末尾への追加は既存の記述に影響しない。
- `clopts_flag` の戻り値を `explicit_set` から `enabled` に変える。否定できないフラグは肯定の形でしか指定できないので、2 つは常に一致し、戻り値は改訂前と変わらない（14.13）。
- 取得関数は増やさない。既定で有効のフラグも、利用側は `clopts_flag(r, "x")` だけで最終的な状態を得る。既定の状態か明示的な指定かを区別したいときだけ `clopts_is_set` を併せて使う。
- 既定で有効の宣言に、既存の `default_value`（文字列）は使わない。CLOpts は値を文字列のまま返す部品で、`"true"` のような文字列を真偽に解釈する規則を持ち込まないためである。フラグに `default_value` を付けると宣言の誤りになる点は、改訂前のままとする。

### 解析規則

「解析規則」の節の走査の表の `--<name>` と `--<name>=<value>` の行で、長い名前の照合を次の順序にする。1 と 2 は改訂前のままである。

1. `help`、`version`（ルートコマンドが選択中のとき）の組み込みの名前。
2. `<name>` 全体を、有効なオプションの長い名前から探す（`clopts_find_long`）。見つかれば改訂前と同じに扱う（14.3）。
3. `<name>` が `no-` で始まり、残りが 1 バイト以上あるとき、残りを有効なオプションの長い名前から探す。見つかったオプションがフラグで `negatable` が真なら、否定の形とする（`clopts_find_negated`、14.2）。
   - `=` が付いていれば、利用者が書いた形（`--no-<name>`）を示す `does not take a value` とする（14.7）。
   - そうでなければ、そのフラグを `explicit_set = true`、`enabled = false` にする。
4. どれにも当たらなければ、トークン（`=` 以降を除く）を示す `unknown option` とする（14.8）。

この順序から、次の扱いが決まる。

| 引数 | 宣言 | 扱い |
|---|---|---|
| `--no-wrap` | `no-wrap` というフラグがある | `no-wrap` の指定（手順 2）。`monde` の既存のフラグはこの経路を通る |
| `--no-color` | `color` が否定できるフラグ | `color` を無効にする（手順 3） |
| `--no-wrap` | `wrap` が否定できないフラグ | `unknown option '--no-wrap'` |
| `--no-name` | `name` が値付きオプションかリスト値 | `unknown option '--no-name'` |
| `--no-force` | `force` が子コマンドのフラグで、まだ選択されていない | `unknown option '--no-force'`（有効なオプションではない） |
| `--no-`、`--no-bogus` | — | `unknown option` |
| `--no-no-x` | `no-x` が否定できるフラグ | `no-x` を無効にする。特別な扱いはしない |
| `--no-help`、`--no-version` | — | `unknown option`（`help` と `version` は宣言できない予約名なので、否定できるフラグにもならない） |

フラグの記録（5.5 と 14.4 の統合）: 肯定の形（`--<name>`、`-<c>`、まとめ書きの中の `<c>`）は `explicit_set = true`、`enabled = true` にする。否定の形は `explicit_set = true`、`enabled = false` にする。どちらも上書きなので、最後に現れた指定が残る。記録する場所は、ほかのオプションと同じく**宣言した段**である。このため、引き継ぐフラグを子コマンド名の前後にまたがって指定しても、引数列の順で最後の指定が残る（14.9）。

ほかの規則への影響はない。

- `--` の後の `--no-<name>` は位置引数になる（6.3）。
- 回復の走査（8.3）は `--help` / `-h` との完全一致だけを見るので、変わらない。末端コマンドで `--no-<x>` が `unknown option` になり、後ろに `--help` があれば、ヘルプを優先する。
- 必須の検査は、フラグを対象にしないので変わらない。
- 既定値の適用（走査が成功した後、経路上のすべての段に対して行う）に、フラグの分を足す。明示的に指定されていないフラグで `default_enabled` が真なら、`enabled = true` にする。`explicit_set` は偽のままにする（14.15）。経路上のすべての段が対象なので、引き継ぐフラグは子孫のコマンドが選択された場合にも既定の状態になる（12.6 と同じ仕組み）。

### メッセージ

使い方のエラーは既存のものを使い、文面を増やさない。

| 場面 | メッセージ | 要件 |
|---|---|---|
| 否定の形への値 | `<prefix>option '--no-<name>' does not take a value` | 14.7 |
| 否定できないものへの否定 | `<prefix>unknown option '--no-<name>'` | 14.8 |

宣言の誤り（`invalid argument declaration: ` に続く理由）を 4 つ加える。`<path>` は経路の文字列である。

| 誤り | 理由 | 要件 |
|---|---|---|
| フラグ以外に `negatable` が真 | `option '--<name>' cannot be negatable in command '<path>'` | 14.10 |
| 否定できるフラグ以外に `default_enabled` が真 | `option '--<name>' must be a negatable flag to be enabled by default in command '<path>'` | 14.16 |
| 同じコマンドに、否定できるフラグ `<name>` と長い名前 `no-<name>` のオプションがある | `option '--no-<name>' conflicts with negatable flag '--<name>' in command '<path>'` | 14.11 |
| コマンドとその祖先の間で、一方に否定できるフラグ `<name>`、他方に長い名前 `no-<name>` のオプションがある | `option '--no-<name>' in command '<path>' conflicts with negatable flag '--<name>' of command '<path>'`（`no-<name>` が子孫の側）、`negatable flag '--<name>' in command '<path>' conflicts with option '--no-<name>' of command '<path>'`（否定できるフラグが子孫の側） | 14.11 |

### Components and Interfaces

| Component | 変更 | Req Coverage |
|---|---|---|
| 型（`clopts.h`） | `negatable`、`default_enabled`、`enabled` を加える | 1.5, 14.1, 14.5, 14.14 |
| DeclCheck（`clopts_decl.c`） | 宣言の検査 3 種と `clopts_find_negated` を加える | 14.2, 14.8〜14.11, 14.16 |
| ResultStore（`clopts_result.c`） | `enabled` を記録し、既定で有効を適用し、`clopts_flag` が返す | 14.4〜14.6, 14.15 |
| Parser（`clopts_parse.c`） | `scan_long` に手順 3 を加える | 14.2, 14.3, 14.7, 14.8 |
| HelpFormatter（`clopts_help.c`） | 左列に `[no-]`、説明に既定の注記を加える | 14.12, 14.17 |
| CLOptsTests（`cli_args/tests/`） | プログラム `neg`、スイート negate、decl の追加 | 11.1, 14.1〜14.17 |

#### DeclCheck

```c
/*
 * name が "no-" で始まり、残り（1 バイト以上）が有効なオプションの長い名前と一致し、
 * そのオプションが negatable のフラグなら、*ref に位置を入れて true（14.2、14.8、14.9）
 */
bool clopts_find_negated(const clopts_result_t *result, const char *name, size_t name_len,
                         clopts_ref_t *ref);
```

- 有効なオプションの探索は `clopts_find_long` と同じ `find_effective` を使う。引き継ぎの範囲（12.1、12.2）がそのまま否定の形にも当てはまる（14.9）。
- 宣言の検査は、既存の「オプションごとの検査」と「長い名前の重複の検査」に条件を足す。
  - オプションごとの検査: `negatable` が真で `kind != CLOPTS_FLAG` なら誤り（14.10）。`default_enabled` が真で、否定できるフラグ（`kind == CLOPTS_FLAG` かつ `negatable`）でなければ誤り（14.16）。フラグ以外に両方を付けた宣言は、2 つの誤りを報告する。
  - 重複の検査: 2 つのオプション A、B を比べる箇所（同じコマンドの中、祖先との間）で、名前の完全一致に加えて「A が否定できるフラグで、B の長い名前が `no-` + A の長い名前」と、その逆を誤りとする（14.11）。祖先との比較は各コマンドが自分の祖先を見る形なので、否定できるフラグが祖先の側にある場合も子孫の側にある場合も、1 回の走査で検出する。祖先の側のオプションが引き継ぐものかどうかは問わない（1.8 と同じ）。
- 1 つの宣言に誤りが複数あれば、それぞれ報告する（既存の方針のまま）。

#### ResultStore

```c
/* フラグを「明示的に指定された」にし、有効か無効かを記録する（5.5、14.2、14.4） */
void clopts_result_set_flag(clopts_result_t *result, clopts_ref_t ref, bool enabled);
```

- `enabled` の初期値は偽にする（14.6）。値の配列は `calloc` で確保するので、確保した時点で偽になる。`clopts_value_free` は `enabled` も偽に戻す。
- `clopts_result_apply_defaults` は、フラグで `explicit_set` が偽かつ `default_enabled` が真なら `enabled = true` にする（14.15）。確保を行わないので、失敗の経路は増えない。
- `clopts_flag` は `enabled` を、`clopts_is_set` は `explicit_set` を返す。オプションが経路上で宣言されていなければ、どちらも `false`（5.7 のまま）。

#### Parser

- `scan_long`: `clopts_find_long` が失敗したら `clopts_find_negated` を試す。成功したら、`=` の有無で `CLOPTS_ERR_TAKES_NO_VALUE` か `clopts_result_set_flag(result, ref, false)` に分ける。
- 肯定の形の 2 か所（`scan_long` のフラグ、短いオプションのフラグ）は `clopts_result_set_flag(result, ref, true)` を呼ぶ。
- エラーの種類は増やさない。

#### HelpFormatter

- 左列は、否定できるフラグだけ `--` の後ろに `[no-]` を挟む: `-c, --[no-]color`、`    --[no-]force`。
- `option_left_width` は、否定できるフラグで 5 バイトを足す。`append_option_left` は同じ条件で `[no-]` を出す。列の揃え方（最大幅 + 2 バイトの空き）は変えない（14.12）。
- 説明の注記に、`default_enabled` が真のフラグで ` (default: on)` を加える。位置と空白の規則は、値付きオプションの ` (default: X)` と同じである（説明がなければ先頭の空白を出さない）。既定で無効のフラグには注記を出さない（14.17）。例: `    --[no-]sync  Flush after each file (default: on)`。
- 否定できるフラグを含まないコマンドのヘルプは、1 バイトも変わらない（14.13）。

### Error Handling

- 使い方のエラーと宣言の誤りは、既存の経路（`clopts_errors_t` への追加と `CLOPTS_USAGE_ERROR` / `CLOPTS_DECL_ERROR`）で返す。新しい状態はない。
- 否定の形の処理は確保を行わないので、メモリ不足の経路は増えない。

### Testing Strategy

テスト用の宣言に、架空のプログラム **`neg`**（バージョン `4.0`）を加える（11.1）。

- ルート（子を持つ）: `--color/-c`（フラグ、否定できる、引き継ぐ）、`--pager`（フラグ、否定できる、引き継ぐ、既定で有効）、`--no-cache`（フラグ、否定できない）、`--wrap`（フラグ、否定できない）、`--name <NAME>`（値）。
- `run`（末端）: `--force/-f`（フラグ、否定できる）、`--sync`（フラグ、否定できる、既定で有効、説明あり）、`--quiet/-q`（フラグ、否定できない）。位置引数 `FILE`（0 個以上、上限なし）。

**negate**（`cli_args/tests/negate_test.c`、プログラム `neg`）:

- 肯定・否定・未指定の 3 つの状態: `neg --color run`、`neg --no-color run`、`neg run` で、`clopts_flag` と `clopts_is_set` がそれぞれ（真・真）（偽・真）（偽・偽）になること（14.2、14.5、14.6）。
- 後に書いた指定の優先: `neg --color --no-color run` が無効、`neg --no-color --color run` と `neg --no-color -c run` が有効、`neg run -fq --no-force` が `force` 無効・`quiet` 有効、`neg run --no-force -qf` が `force` 有効になること。どれもエラーにならないこと（14.4）。
- 引き継ぐフラグ: `neg --color run --no-color` が無効、`neg --no-color run --color` が有効、`neg run --no-color` が無効になること（14.9）。
- 既定で有効: `neg run` で `sync` と `pager` が `clopts_flag` 真・`clopts_is_set` 偽になること。`neg run --no-sync` と `neg --no-pager run`、`neg run --no-pager` が偽・真、`neg run --sync` が真・真、`neg run --no-sync --sync` が真・真になること（14.15、14.4）。
- 完全一致の優先: `neg --no-cache run` で `no-cache` が有効になること（14.3）。
- 値の指定: `neg --no-color=1 run` が `neg: option '--no-color' does not take a value`、`neg run --no-force=` が `neg run: option '--no-force' does not take a value` になること（14.7）。
- 未知のオプション: `neg --no-wrap run`、`neg --no-name run`、`neg --no-force run`（まだ選択されていない子コマンドのフラグ）、`neg --no-bogus run`、`neg --no- run`、`neg --no-colo run`（前方一致）、`neg --No-color run` が、その引数を示す `unknown option` になること（14.8、3.6、3.7）。
- ほかの規則との組み合わせ: `neg run -- --no-force` で `--no-force` が位置引数になり `force` が未指定のままであること（6.3）。`neg run --no-quiet --help` がヘルプの要求になること（8.3）。
- ヘルプ: ルートコマンドと `run` のヘルプを、期待する文面のリテラルとバイト単位で比較する。`-c, --[no-]color`、`    --no-cache`、`-f, --[no-]force` の左列と、説明の開始位置が最大幅に揃うことを含む（14.12、11.4）。`--[no-]sync` と `--[no-]pager` の説明に ` (default: on)` が付き、既定で無効の `--[no-]color` と `--[no-]force` には付かないこと（14.17）。`run` のヘルプに、引き継いだ `--[no-]color` と `--[no-]pager` が現れること（9.5）。

**decl**（`cli_args/tests/decl_test.c` への追加）:

- 値付きオプションとリスト値に `negatable` を付けた宣言が、それぞれ `cannot be negatable` の誤りになること（14.10）。
- 否定できないフラグ、値付きオプション、リスト値に `default_enabled` を付けた宣言が、それぞれ `must be a negatable flag to be enabled by default` の誤りになること。否定できるフラグに `default_value` を付けた宣言が、改訂前と同じ `cannot be required or have a default` の誤りになること（14.16）。
- 同じコマンドに `color`（否定できる）と `no-color` がある宣言、祖先に `color`（否定できる）で子孫に `no-color` がある宣言、その逆の宣言が、上の表の文面の誤りになること。祖先の側が引き継がないオプションでも誤りになること（14.11）。
- `color`（否定できない）と `no-color` が並ぶ宣言、`no-x`（否定できる）だけの宣言が、妥当であること（14.11 の境界）。

**改訂前と同じであること**（14.13）:

- `cli_args/tests/` の既存のスイート（decl、dispatch、long、short、values、positional、errors、help、single、nested、inherit、argv、errors_api）を、期待値を変えずに通す。変えるのは `main.c` の登録、`decl_test.c` への追加、`values_test.c` の `clopts_result_set_flag` の呼び出しへの引数 `true` の追加だけである。
- Monde の全テスト（`ctest` の全体。`cli`、`docs`、`clopts` を含む）を、`src/` と `tests/` を変えずに通す。`ctest -L docs` が通ることが、`monde` のヘルプの文面が変わっていないことの証拠になる。
- CLOpts の単独構成（`cmake -S cli_args -B build-clopts`）でビルドとテストを通す。`clopts_package.find_package` が、バージョン 0.2.0 と C++ の利用例（`consumer_cxx.cpp`）で通ることを含む。

### Migration Strategy

- 互換の層は要らない。公開の型へのメンバーの追加は末尾で、既存の利用側のソースは変えずにビルドできる。
- `clopts_result_set_flag` の引数の追加は内部の関数の変更で、公開の API には影響しない。呼び出し元は `clopts_parse.c` の肯定の形の 2 か所と否定の形の 1 か所（追加）、内部の関数を直接使う `cli_args/tests/values_test.c` の 3 か所である。`values_test.c` には引数 `true` を足すだけで、テストの意味と期待値は変えない。型と ResultStore を先に変え、続けて Parser を変える。DeclCheck と HelpFormatter は、型の変更の後なら互いに独立に進められる。
- CLOpts のバージョンは、公開の型の変更を表すために 0.2.0 に上げる。番号を書く場所は `cli_args/CMakeLists.txt` の `project(CLOpts VERSION ...)` だけで、パッケージの検証はこの値を使う。Monde のバージョンは変えない。

## 改訂（2026-11-11）: 出現回数、隠しオプション、別名

要件 15、16、17 と、それに伴う 1.5、1.12、11.1 の変更の設計である。この節は現在の名前（`cli_args/`、`clopts_*` / `CLOPTS_*`）で書く。この節に書いていない規則（走査、優先順位、メッセージの接頭辞、ヘルプの書式、否定できるフラグの規則）は、前の節のままとする。

### 概要

- **Purpose**: フラグが指定された回数を取得できるようにし、オプションをヘルプに出さない宣言と、長い名前の別名の宣言を加える。
- **Impact**: CLOpts の公開の型にメンバーを 3 つ足す（宣言の `hidden` と `aliases`、結果の `count`）。公開の関数を 1 つ足す（`clopts_flag_count`）。この 3 つを使わない利用側（Monde を含む）は、ソースも挙動も変わらない。
- **分類**: 既存の部品の拡張（Extension）。新しい部品、新しい依存、新しいソースファイル（テストを除く）はない。
- **3 つの機能の関係**: 互いに独立している。共有するのは、宣言の型と、「別名で指定したフラグも 1 回と数える」（15.2、17.3）という 1 点だけで、後者は記録の場所が 1 つであることから追加の処理なしで満たせる。

### Boundary Commitments

#### This Spec Owns
- フラグの出現回数の持ち方（`clopts_value_t` の `count`）、数え方の規則、取得の関数 `clopts_flag_count`
- `clopts_option_t` の `hidden` と、その妥当性の規則（必須のオプションには許さない）、ヘルプのオプションの一覧と使い方の行からの除外
- `clopts_option_t` の `aliases` の書き方（カンマ区切りの 1 つの文字列）と、その妥当性の規則（形、予約、重複、`no-` の衝突）、別名の照合
- 追加する宣言の誤りの英語のメッセージ
- CLOpts のバージョン番号（0.2.0 → 0.3.0）と `cli_args/README.md` の該当箇所

#### Out of Boundary
- `monde` のオプションに隠しオプションや別名を使うこと、`monde` が出現回数を使うこと。`src/cli/`、`tests/cli/`、`docs/`、ルートの `README.md` は変更しない
- 値の選択肢の検査（要件確認で対応しないことにした）、値の型変換、そのほかの値の妥当性検査
- 子コマンドをヘルプに出さない宣言、組み込みのオプション（`--help`、`-h`、`--version`）を隠す宣言
- 短い名前の別名、別名のヘルプでの表示、別名による解析結果の取得、別名が使われたことの通知
- 出現回数の下限・上限の宣言、値付きオプションとリスト値の出現回数
- CLOpts のビルド定義、パッケージ定義、CI（clopts-standalone が持つ）。変更するのは `project(CLOpts VERSION ...)` の番号と、テストのソースの一覧だけである

#### Allowed Dependencies
- CLOpts のライブラリは C99 の標準ライブラリだけに依存する（clopts-standalone の設計のまま）。この改訂で依存は増えない。
- `cli_args/` の中から、その外のファイルを参照しない。
- モジュール内の依存の向きは変えない: 型 → DeclCheck → ResultStore → Parser。HelpFormatter は DeclCheck にだけ依存する。

#### Revalidation Triggers
- 出現回数の数え方（否定の形で 0 に戻す、既定で有効なら 1）の変更 → `clopts_flag_count` を使う利用側
- `count` と `enabled` の関係（`enabled` は `count > 0` と常に一致する）の変更 → `clopts_flag` を使うすべての利用側（Monde の `src/cli/cmd_*.c` を含む）
- `aliases` の書き方（カンマ区切り）や照合の順序の変更 → 別名を宣言している利用側
- 隠しオプションをヘルプのどこに出さないか（一覧、使い方の行）の変更 → 隠しオプションを宣言している利用側のヘルプの期待値
- `monde` が隠しオプション、別名、出現回数を使い始める場合 → monde-cli の要件、`docs/commands.md` と README の引用（`tests/docs/`）

### File Structure Plan

```
cli_args/
├── CMakeLists.txt                # 変更: project(CLOpts VERSION 0.3.0)
├── README.md                     # 変更: 概要に 3 つの機能を加え、節「出現回数」「隠しオプション」「別名」を足す。find_package の例を 0.3 にする
├── include/clopts/clopts.h       # 変更: clopts_option_t に hidden と aliases、clopts_value_t に count、関数 clopts_flag_count
├── src/
│   ├── clopts_decl.c             # 変更: 名前の列挙（長い名前と別名）、別名の照合、別名の形・予約・重複・衝突の検査（17.12〜17.14）、必須かつ隠しの検査（16.6）
│   ├── clopts_result.c           # 変更: count の記録と解放時の初期化、既定で有効の適用で count を 1 にする、clopts_flag_count
│   └── clopts_help.c             # 変更: オプションの一覧の 2 つのループと使い方の行で、隠しオプションを除く
└── tests/
    ├── CMakeLists.txt            # 変更: count_test.c、hidden_test.c、alias_test.c を加える
    ├── main.c                    # 変更: スイート count、hidden、alias を登録する
    ├── test_decl.h               # 変更: 架空のプログラム ext の宣言
    ├── test_decl.c               # 変更: 同上の定義
    ├── count_test.c              # 新規: スイート count（15.1〜15.7）
    ├── hidden_test.c             # 新規: スイート hidden（16.1〜16.5、16.7、16.8）。ファイル内の宣言 solo を含む
    ├── alias_test.c              # 新規: スイート alias（17.1〜17.11）
    └── decl_test.c               # 変更: 隠しオプションと別名に関する宣言の誤り（16.6、17.12〜17.14）
```

変更しないファイル: `cli_args/src/clopts_parse.c`（走査は `clopts_find_long` と `clopts_result_set_flag` の結果だけを使うので、変更が要らない）、`clopts_internal.h`（内部の関数の宣言は増えない）、`clopts_argv.c`、`clopts_buf.[ch]`、`clopts_errors.c`、`cli_args/cmake/`、`cli_args/tests/` のほかのスイート（1.12 の検証として、期待値を変えずに通す）、`src/`、`tests/`、`docs/`、`.github/`。

### 型（`cli_args/include/clopts/clopts.h`）

```c
typedef struct clopts_option
{
    const char *long_name;
    char short_name;
    clopts_kind_t kind;
    const char *value_name;
    const char *help;
    bool required;
    const char *default_value;
    bool inherited;
    bool negatable;
    bool default_enabled;
    bool hidden;                /* true なら、ヘルプに出さない。必須のオプションには指定できない（16.1） */
    const char *aliases;        /* 長い名前の別名。複数はカンマ区切りで書く（例: "out,outfile"）。NULL ならなし（17.1） */
} clopts_option_t;

typedef struct clopts_value
{
    bool explicit_set;
    const char *value;
    char **items;
    size_t item_count;
    size_t item_cap;
    bool enabled;        /* フラグ: count > 0 と常に一致する */
    size_t count;        /* フラグ: 出現回数。最後の否定の形より後ろの肯定の形の数。未指定なら既定で有効のフラグは 1、ほかは 0。フラグ以外では 0（15.2〜15.4） */
} clopts_value_t;

/* フラグの出現回数。フラグではないオプション、経路上で宣言されていない名前、別名には 0 を返す（15.1、15.7、17.8） */
size_t clopts_flag_count(const clopts_result_t *result, const char *long_name);
```

- どのメンバーも末尾に足す。0（`false`、`NULL`）が「ヘルプに出す」「別名なし」「0 回」を表すので、指示付き初期化子で書いた既存の宣言は変えずに済む。C++ の指示付き初期化子は宣言順を要求するが、末尾への追加は既存の記述に影響しない。
- `enabled` は残す。`clopts_flag` は今までどおり `enabled` を返し、`clopts_flag_count` は `count` を返す。2 つを書き込む場所を ResultStore の 3 つの関数（記録、既定の適用、解放）に限ることで、`enabled == (count > 0)` を保つ（15.6）。
- `CLOPTS_RESULT_INIT` は変えない（値の配列は解析のときに CLOpts が確保する）。
- 別名はカンマ区切りの 1 つの文字列で書く。リスト値の既定値（`default_value = "a,b"`）と同じ書き方で、別名ごとの配列の定義が要らず、C と C++ で同じに書ける。このため、別名はカンマを含められない。長い名前はカンマを含められるままとする（改訂前の規則を変えない）。

### 解析規則

#### 出現回数（Requirement 15）

フラグの記録（前の改訂の「フラグの記録」）に、回数を足す。

| 指定 | `explicit_set` | `enabled` | `count` |
|---|---|---|---|
| 肯定の形（`--<name>`、`--<alias>`、`-<c>`、まとめ書きの中の `<c>`） | `true` | `true` | 1 を足す |
| 否定の形（`--no-<name>`、`--no-<alias>`） | `true` | `false` | 0 にする |
| 指定なし、既定で有効のフラグ（走査の後の既定の適用） | `false` のまま | `true` | 1 にする |
| 指定なし、それ以外のフラグ | `false` | `false` | 0 |

- 記録する場所は、ほかのオプションと同じく**宣言した段**の 1 か所である。このため、引き継ぐフラグを子コマンド名の前後にまたがって指定しても、同じ `count` に足される（15.5）。別名による指定も同じ場所に記録するので、`-v --verbose --<alias>` は 3 になる（15.2、17.3）。
- 否定できないフラグは肯定の形でしか指定できないので、`count` は肯定の形の総数になる（15.2）。否定できるフラグは、否定の形で 0 に戻るので、最後の否定の形より後ろの肯定の形の数になる（15.3）。
- 引数の個数は `int` の範囲なので、`count` が `size_t` の範囲を超えることはない。
- 値付きオプションとリスト値では `count` を書かない。確保した時点の 0 のままである（15.7）。
- 既定の適用は、改訂前と同じく、解析が成功したとき（`CLOPTS_OK`）だけ行う。ヘルプやバージョンの要求、子コマンドが指定されなかった場合、使い方のエラーの結果では、既定で有効のフラグも `count` は 0、`enabled` は偽のままである（`clopts_flag` の 14.15 と同じ扱いで、`enabled == (count > 0)` はどの状態でも保たれる）。

例（プログラム `neg`、`color` は否定できるフラグ、`pager` は否定できる既定で有効のフラグ）:

| 引数列 | `clopts_flag_count` | `clopts_flag` |
|---|---|---|
| `neg -c -c run -c` | `color` は 3 | 真 |
| `neg -c -c --no-color run -c` | `color` は 1 | 真 |
| `neg -c --no-color run` | `color` は 0 | 偽 |
| `neg run` | `color` は 0、`pager` は 1 | 偽、真 |
| `neg --pager --pager run` | `pager` は 2 | 真 |
| `neg --no-pager run` | `pager` は 0 | 偽 |

#### 隠しオプション（Requirement 16）

走査、結果、エラーは `hidden` を見ない。隠しオプションは、ほかのオプションと同じに照合され、同じに記録され、指定の誤りは同じ文面のエラーになる（16.4、16.5）。`hidden` を見るのは、宣言の検査（16.6）とヘルプの生成（16.2、16.3）だけである。

#### 別名（Requirement 17）

「解析規則」の節の走査の表の `--<name>` と `--<name>=<value>` の行で、手順 2 の照合の対象を広げる。順序は変えない。

1. `help`、`version`（ルートコマンドが選択中のとき）の組み込みの名前。
2. `<name>` 全体を、有効なオプションの**長い名前と別名**から探す（`clopts_find_long`）。見つかれば、そのオプションの指定として、元の名前の場合と同じに扱う（17.2）。
3. `<name>` が `no-` で始まり、残りが 1 バイト以上あるとき、残りを有効なオプションの**長い名前と別名**から探す。見つかったオプションが否定できるフラグなら、否定の形とする（`clopts_find_negated`、17.5）。
4. どれにも当たらなければ、`unknown option` とする。

この順序から、次の扱いが決まる（`output` は値付きオプションで別名 `out`、`color` は否定できるフラグで別名 `colour`）。

| 引数 | 扱い |
|---|---|
| `--out x`、`--out=x` | `output` に値 `x` を記録する |
| `--out` が引数列の最後 | `option '--out' requires a value`（利用者が書いた別名で示す。17.9） |
| `--colour`、`--no-colour` | `color` を有効、無効にする |
| `--no-colour=1` | `option '--no-colour' does not take a value`（17.9） |
| `--ou`（別名の前方一致）、`--Out` | `unknown option`（3.6、3.7 と同じ） |
| `--no-out` | `unknown option '--no-out'`（`output` は否定できるフラグではない） |

- エラーの名前は、既存の仕組み（走査がトークンの名前の部分を保留する）のままで、利用者が書いた別名になる。必須のオプションの欠落は、宣言の `long_name` を示すので、元の名前になる（17.10）。
- 引き継ぎの範囲は、有効なオプションの探索（`find_effective`）で決まるので、別名も元の名前と同じ範囲で有効になる（17.4）。
- 解析結果の取得（`clopts_lookup`）は `long_name` だけを見る。別名を渡すと、宣言されていない名前と同じ結果になる（17.7、17.8）。

### ヘルプ

- **オプションの一覧**: 有効なオプションのうち、`hidden` が真のものを、列幅の計算と行の出力の両方で飛ばす。引き継いだオプションも同じ列挙を通るので、子孫のコマンドのヘルプからも消える（16.2、16.3）。`-h, --help` の行と、ルートコマンドの `--version` の行は、宣言されたオプションではないので常に出す（16.7）。
- **使い方の行**: 子を持つコマンドの、経路の直後の ` [<options>]` は、「有効なオプションが 1 つ以上あるとき」ではなく「**隠しオプションではない**有効なオプションが 1 つ以上あるとき」に出す。隠しオプションしか持たないコマンドのヘルプを、オプションを 1 つも宣言しないコマンドのヘルプと同じにするためである（16.8）。末端コマンドの ` [<options>]` は、改訂前から常に出しているので変えない。
- **別名**: ヘルプの左列は `long_name` だけから作るので、別名は出ない（17.11）。変更は要らない。
- 隠しオプションを含まないコマンドのヘルプは、1 バイトも変わらない（1.12）。

例（ルートコマンドが `--verbose/-v` と、隠しオプション `--trace-parser` を持つ場合）:

```text
Options:
  -v, --verbose  Print more messages
  -h, --help     Show this help
      --version  Show version information
```

### メッセージ

使い方のエラーは既存のものを使い、文面を増やさない。

宣言の誤り（`invalid argument declaration: ` に続く理由）を 3 つ加える。`<path>` は経路の文字列である。

| 誤り | 理由 | 要件 |
|---|---|---|
| 必須のオプションに `hidden` が真 | `option '--<name>' cannot be both required and hidden in command '<path>'` | 16.6 |
| 別名が長い名前として受け付けない形（空、先頭が `-`、`=` を含む） | `invalid alias '<alias>' of option '--<name>' in command '<path>'` | 17.13 |
| 別名が `help` または `version` | `alias '--<alias>' of option '--<name>' is reserved in command '<path>'` | 17.13 |

別名の重複（17.12）と `no-` の衝突（17.14）は、既存の 5 つの文面をそのまま使い、名前の位置に、重複または衝突した名前（別名のこともある）を入れる。

| 誤り | 理由（既存の文面） | 要件 |
|---|---|---|
| 同じコマンドの中で、名前（長い名前か別名）が重複している。同じオプションの元の名前とほかの別名との重複を含む | `duplicate option '--<n>' in command '<path>'` | 1.8, 17.12 |
| コマンドの名前が、祖先のオプションの名前と重複している | `option '--<n>' in command '<path>' duplicates an option of command '<path>'` | 1.8, 17.12 |
| 同じコマンドの中で、否定できるフラグの名前 `<n>` と、名前 `no-<n>` がある。同じオプションの中の組を含む | `option '--no-<n>' conflicts with negatable flag '--<n>' in command '<path>'` | 14.11, 17.14 |
| コマンドとその祖先の間で、一方に否定できるフラグの名前 `<n>`、他方に名前 `no-<n>` がある | 14.11 の 2 つの文面（どちらが子孫の側かで分ける） | 14.11, 17.14 |

別名を 1 つも含まない宣言では、`<n>` は長い名前だけになり、メッセージは改訂前と 1 バイトも変わらない（1.12）。

### Requirements Traceability

| Requirement | Summary | Components | Interfaces | Flows |
|---|---|---|---|---|
| 1.5 | 隠しの印と別名の宣言（追加分） | 型（clopts.h） | `clopts_option_t.hidden`、`clopts_option_t.aliases` | — |
| 1.12 | 使わない宣言は改訂前と同じ | 全体 | 既存のテスト一式 | この節の「Testing Strategy」 |
| 11.1 | テスト用の宣言で検証 | CLOptsTests | プログラム `ext`、スイート count、hidden、alias | — |
| 15.1, 15.7 | 出現回数の取得、フラグ以外と宣言のない名前は 0 | ResultStore | `clopts_flag_count` | — |
| 15.2, 15.3, 15.5 | 肯定の形を数え、否定の形で 0 に戻す。引き継ぐフラグは合算 | ResultStore | `clopts_result_set_flag`、宣言した段への記録 | この節の「出現回数」 |
| 15.4 | 指定がなければ 0、既定で有効なら 1 | ResultStore | `clopts_result_apply_defaults` | 走査の後の検査 |
| 15.6 | 回数が 1 以上であることと有効であることの一致 | ResultStore | `clopts_value_t.count`、`clopts_value_t.enabled` | — |
| 16.1 | ヘルプに出さないかの宣言 | 型（clopts.h） | `clopts_option_t.hidden` | — |
| 16.2, 16.3, 16.7, 16.8 | 一覧と列幅から除く。組み込みのオプションは出す。使い方の行の判定からも除く | HelpFormatter | `clopts_format_help` | この節の「ヘルプ」 |
| 16.4, 16.5 | 解析とエラーは同じ | Parser、ResultStore | 変更なし（`hidden` を見ない） | — |
| 16.6 | 必須かつ隠しは宣言の誤り | DeclCheck | `clopts_decl_check` | 解析の最初 |
| 17.1 | 別名の宣言 | 型（clopts.h） | `clopts_option_t.aliases` | — |
| 17.2, 17.4, 17.5 | 別名の照合、引き継ぎの範囲、否定の形 | DeclCheck | `clopts_find_long`、`clopts_find_negated` | この節の「別名」 |
| 17.3, 17.6 | 元の名前と別名の混在は繰り返し。別名で必須を満たす | ResultStore | 同じ `clopts_ref_t` への記録 | — |
| 17.7, 17.8 | 元の名前で取得し、別名では取得できない | ResultStore | `clopts_lookup`（変更なし） | — |
| 17.9, 17.10 | エラーは書かれた別名で、必須の欠落は元の名前で示す | Parser | 変更なし（保留した名前、宣言の `long_name`） | — |
| 17.11 | 別名をヘルプに出さない | HelpFormatter | 変更なし（左列は `long_name` から作る） | — |
| 17.12, 17.13, 17.14 | 別名の重複、形と予約、`no-` の衝突 | DeclCheck | `clopts_decl_check` | 解析の最初 |

### Components and Interfaces

| Component | 変更 | Req Coverage |
|---|---|---|
| 型（`clopts.h`） | `hidden`、`aliases`、`count`、`clopts_flag_count` を加える | 1.5, 15.1, 16.1, 17.1 |
| DeclCheck（`clopts_decl.c`） | 名前の列挙、別名の照合、別名の検査、必須かつ隠しの検査 | 16.6, 17.2, 17.4, 17.5, 17.12〜17.14 |
| ResultStore（`clopts_result.c`） | `count` の記録、既定の適用、`clopts_flag_count` | 15.1〜15.7, 17.3 |
| Parser（`clopts_parse.c`） | 変更なし | 16.4, 16.5, 17.9, 17.10 |
| HelpFormatter（`clopts_help.c`） | 一覧と使い方の行で隠しオプションを除く | 16.2, 16.3, 16.7, 16.8, 17.11 |
| CLOptsTests（`cli_args/tests/`） | プログラム `ext`、スイート count、hidden、alias、decl の追加 | 11.1, 15.1〜17.14 |

#### DeclCheck

名前の列挙を、ファイルの中だけで使う関数として加える。公開もせず、内部ヘッダにも出さない。

```c
/* 名前の 1 つ。NUL で終わるとは限らない（別名はカンマ区切りの文字列の一部を指す） */
typedef struct clopts_name
{
    const char *bytes;
    size_t len;
} clopts_name_t;

/*
 * option の n 番目の名前を *name に入れて true。0 番目は long_name、1 番目以降は aliases を
 * カンマで区切った項目（空の項目も 1 つの名前として返す）。n が名前の数以上なら false。
 * long_name が NULL なら、0 番目は長さ 0 の名前とする
 */
static bool option_name_at(const clopts_option_t *option, size_t n, clopts_name_t *name);
```

- **照合**: `match_long` は、キーを `option_name_at` のすべての名前と比べる。`clopts_find_long` と、それを呼ぶ `clopts_find_negated` は、これで別名にも一致する（17.2、17.5）。確保は行わない。
- **検査の一般化**: 既存の重複の検査と衝突の検査は、「このオプションの長い名前」と「相手のオプションの長い名前」を比べている。これを「このオプションの各名前」と「相手のオプションの各名前」の組の比較に広げる。比べる相手は改訂前と同じである。
  - 同じコマンドの、前に宣言されたオプション（すべての名前）
  - **同じオプションの、前の名前**（元の名前と、前に書いた別名）。追加分
  - 祖先のコマンドのオプション（すべての名前。引き継ぐかどうかは問わない。1.8 と同じ）
- **重複**（1.8、17.12）: 2 つの名前がバイト単位で一致すれば誤り。各コマンドが自分の前のオプションと祖先を見る形なので、「前のオプションの別名」と「後のオプションの長い名前」の重複も、後のオプションの検査で見つかる。
- **衝突**（14.11、17.14）: 否定できるフラグの名前 `<n>` と、名前 `no-<n>` の組があれば誤り。組は、別のオプションとの間だけでなく、**同じオプションの名前の間**でも見る（否定できるフラグ `color` に別名 `no-color` を付けた宣言）。
- **別名の形と予約**（17.13）: 別名ごとに、長い名前と同じ条件（空、先頭が `-`、`=` を含む）と、予約（`help`、`version`）を検査する。`aliases` が空文字列の場合と、先頭・末尾・連続のカンマは、空の別名として誤りになる。
- **報告の数**: 1 つのオプションについて、重複は最初の 1 件、衝突は最初の 1 件を報告する（改訂前の `return` の位置のまま）。形と予約の誤りは別名ごとに報告する。長い名前が形の誤りのとき、そのオプションの重複と衝突は検査しない（改訂前のまま）。別名の形と予約の検査は、その場合も行う。
- **必須かつ隠し**（16.6）: オプションごとの検査に、`hidden` と `required` がともに真なら誤り、を足す。フラグに `required` を付けた宣言は、既存の誤り（`cannot be required or have a default`）と合わせて 2 件になる。
- 名前を示すメッセージは、`%.*s` で長さを指定して出す（別名が NUL で終わらないため）。長さを `int` に収める関数は、`clopts_parse.c` の `name_precision` と同じ内容のものを `clopts_decl.c` の中に置く（数行の関数で、共有のために内部ヘッダを変えることはしない）。

#### ResultStore

```c
size_t clopts_flag_count(const clopts_result_t *result, const char *long_name);
```

- `clopts_result_set_flag(result, ref, enabled)`: `enabled` が真なら `count` に 1 を足し、偽なら 0 にする。`explicit_set` と `enabled` の書き込みは改訂前のままである。関数の形は変えないので、呼び出し元（`clopts_parse.c` の 3 か所と `values_test.c`）は変えない。
- `clopts_result_apply_defaults`: 既定で有効のフラグを有効にする箇所で、`count = 1` も書く（15.4）。
- `clopts_value_free`: `count` も 0 に戻す。
- `clopts_flag_count`: `clopts_lookup` で値を探し、見つかれば `count` を、見つからなければ 0 を返す。フラグ以外のオプションの `count` は常に 0 なので、種類の判定は要らない（15.7）。

#### HelpFormatter

- `append_options_section`: 列幅を求めるループと、行を出すループで、`hidden` が真のオプションを飛ばす。
- `append_usage_line`: 子を持つコマンドの条件を、隠しオプションではない有効なオプションの数で判定する。数える処理は `clopts_help.c` の中の関数にする（`clopts_effective_count` の意味は変えない。走査の側は隠しオプションも有効なオプションとして数える必要があるため）。

### Error Handling

- 宣言の誤りは、既存の経路（`clopts_errors_t` への追加と `CLOPTS_DECL_ERROR`）で返す。新しい状態と、新しい使い方のエラーはない。
- 3 つの機能のどれも確保を行わないので、メモリ不足の経路は増えない。

### Testing Strategy

テスト用の宣言に、架空のプログラム **`ext`**（バージョン `5.0`）を加える（11.1）。出現回数は、既存のプログラム `tool`、`nest`、`neg` でも検証する。

- ルート（子を持つ）: `--verbose/-v`（フラグ、引き継ぐ、別名 `chatty`）、`--color`（フラグ、否定できる、引き継ぐ、別名 `colour`）、`--trace`（フラグ、隠し、引き継ぐ）、`--profile <NAME>`（値、隠し、引き継がない）。
- `run`（末端）: `--output/-o <FILE>`（値、別名 `out,outfile`）、`--token <TOKEN>`（値、必須、別名 `key`）、`--tags <TAG>`（リスト、別名 `labels`）、`--jobs/-j <NUMBER-OF-WORKERS>`（値、隠し、既定値 `1`、説明あり。左列は 30 桁で、次に長い `    --tags <TAG>[,<TAG>...]` の 27 桁を上回り、引き継いだオプションを含めてこのコマンドで最も長い）、`--force/-f`（フラグ）。位置引数 `FILE`（0 個以上、上限なし）。

有効なオプションがすべて隠しオプションになるコマンドは、`ext` では作れない（ルートの引き継ぐオプションが子孫でも見えるため）。この場合だけ、`hidden_test.c` の中に小さな宣言 **`solo`**（バージョン `1.0`）を置く。ルート（子を持つ）のオプションは `--dump`（フラグ、隠し）だけで、子は `state`（末端、オプションなし）である。

**count**（`cli_args/tests/count_test.c`）:

- 否定できないフラグ: `tool build -vvv ...` が 3、`tool build -v --verbose -qv ...` で `verbose` が 3・`quiet` が 1、指定なしが 0 になること。`clopts_flag` が回数 1 以上のときだけ真になること（15.2、15.4、15.6）。
- 否定できるフラグ: この節の「出現回数」の例の表の 6 行（15.3、15.4）。
- 引き継ぐフラグ: `nest -v remote -v --token t add -v n u` で `verbose` が 3 になること。`neg -c run -c --no-color -c` が 1 になること（15.5）。
- 別名との合算: `ext -v --chatty run --token t --verbose` で `verbose` が 3 になること（15.2、17.3）。
- 0 を返す場合: 値付きオプション（指定あり、複数回の指定を含む）、リスト値、宣言のない名前、別名（`chatty`）、`NULL` の名前、解析前の結果（15.7、17.8）。
- 状態が成功以外のとき: `neg --help` と `neg run --bogus` の結果で、既定で有効の `pager` が `clopts_flag_count` は 0、`clopts_flag` は偽になること（既定は成功のときだけ適用する）。`neg -c --help` のように、ヘルプの要求より前に指定したフラグは数えられていること。

**hidden**（`cli_args/tests/hidden_test.c`、プログラム `ext`）:

- ヘルプ: `ext` のルートコマンドと `run`、`solo` のルートコマンドのヘルプを、期待する文面のリテラルとバイト単位で比較する（11.4）。確かめる点は次のとおり。
  - ルートと `run` のどちらにも `--trace` が出ないこと（引き継ぐ隠しオプション。16.2）。
  - `run` の一覧に `--jobs` が出ず、説明の開始位置が `--jobs` の行を除いた最大幅に揃うこと（16.3）。
  - `-h, --help` と、ルートの `--version` が出ること（16.7）。
  - `solo` の使い方の行が `Usage: solo <command> [<options>] [<arguments>]` で、経路の直後の ` [<options>]` がないこと（16.8）。`Options:` の段落が `-h, --help` と `--version` の行だけになること。`ext` のルートコマンドでは、経路の直後の ` [<options>]` が出ること。
  - `solo --dump state` が成功し、`dump` が有効になること（隠しオプションしかないコマンドでも解析は同じ）。
- 解析: `ext --trace run --token t --jobs 4` で `trace` が有効、`jobs` が `4` になること。`ext run --token t` で `jobs` が既定値 `1` になること。`ext run --token t -j2` の短い名前も働くこと（16.4）。
- エラー: `ext run --token t --jobs` が `ext run: option '--jobs' requires a value`、`ext --trace=1 run` が `ext: option '--trace' does not take a value`、`ext run --profile x`（引き継がない隠しオプション）が `ext run: unknown option '--profile'` になること（16.5、12.2）。

**alias**（`cli_args/tests/alias_test.c`、プログラム `ext`）:

- 照合: `--out x`、`--out=x`、`--outfile x` が `output` の値になること。`--key t` で必須の `token` が満たされること。`--labels a,b` が `tags` の項目になること（17.2、17.6）。
- 繰り返し: `--output a --out b` が `b`、`--out a -o b` が `b`、`--tags a --labels b,c` が 3 項目になること（17.3）。
- 引き継ぎ: `ext run --token t --chatty` と `ext --chatty run --token t` が同じ結果になること（17.4）。
- 否定の形: `ext --colour run ...` が有効、`ext --no-colour run ...` が無効、`ext --color --no-colour run ...` が無効、`ext --no-color run --colour ...` が有効になること（17.5、14.4）。
- 取得: `clopts_value(r, "out")`、`clopts_is_set(r, "key")`、`clopts_flag(r, "colour")`、`clopts_list_count(r, "labels")` が、宣言のない名前と同じ結果になること（17.8）。元の名前では値が取れること（17.7）。
- エラー: `ext run --token t --out` が `ext run: option '--out' requires a value`、`ext --chatty=1 run` が `ext: option '--chatty' does not take a value`、`ext --no-colour=1 run` が `ext: option '--no-colour' does not take a value`、`ext run --token t --labels a,,b` が `ext run: option '--labels' has an empty item in 'a,,b'` になること（17.9）。
- 必須の欠落: `ext run` が `ext run: missing required option '--token'` になること（17.10）。
- 未知のオプション: `--ou`、`--Out`、`--no-out`、`--no-chatty`（否定できないフラグの別名）が `unknown option` になること（3.6、3.7、14.8）。
- ヘルプ: ルートコマンドと `run` のヘルプに、別名（`chatty`、`colour`、`out`、`outfile`、`key`、`labels`）が現れないこと。hidden のスイートのバイト単位の比較が、これも確かめている（17.11）。

**decl**（`cli_args/tests/decl_test.c` への追加）:

- 値付きオプションに `required` と `hidden` を付けた宣言が、`cannot be both required and hidden` の誤りになること。`hidden` だけの宣言（フラグ、値、リスト値）が妥当であること（16.6、16.1）。
- 別名の形: `aliases` が `""`、`"a,,b"`、`",a"`、`"a,"`、`"-x"`、`"a=b"` の宣言が、それぞれ `invalid alias` の誤りになること。`"help"`、`"x,version"` が `is reserved` の誤りになること（17.13）。
- 別名の重複: 同じコマンドの前のオプションの長い名前と同じ別名、前のオプションの別名と同じ長い名前、2 つのオプションの同じ別名、自分の長い名前と同じ別名（`output` に `"output"`）、同じ別名の 2 回書き（`"out,out"`）が、`duplicate option` の誤りになること。祖先のオプションの長い名前・別名と同じ別名、祖先のオプションの別名と同じ長い名前が、`duplicates an option of command` の誤りになること。祖先の側が引き継がないオプションでも誤りになること（17.12）。
- 別名の衝突: 否定できるフラグ `color`（別名 `colour`）と、長い名前 `no-colour` のオプション、別名 `no-color` のオプション、別名 `no-colour` のオプションが、それぞれ誤りになること。否定できるフラグ `color` 自身の別名が `no-color` の宣言が誤りになること。祖先と子孫の両方の向きで誤りになること（17.14）。
- 境界: 否定できないフラグ `color` と別名 `no-color` を持つ別のオプションが並ぶ宣言、兄弟のコマンドどうしで同じ別名を持つ宣言が、妥当であること。

**改訂前と同じであること**（1.12）:

- `cli_args/tests/` の既存のスイート（decl、dispatch、long、short、values、positional、errors、help、single、nested、inherit、argv、errors_api、negate）を、期待値を変えずに通す。変えるのは `main.c` の登録と `decl_test.c` への追加だけである。
- Monde の全テスト（`ctest` の全体。`cli`、`docs`、`clopts` を含む）を、`src/` と `tests/` を変えずに通す。`ctest -L docs` が通ることが、`monde` のヘルプの文面が変わっていないことの証拠になる。
- CLOpts の単独構成（`cmake -S cli_args -B build-clopts`）でビルドとテストを通す。`clopts_package.find_package` が、バージョン 0.3.0 と C++ の利用例（`consumer_cxx.cpp`）で通ることを含む。

### Migration Strategy

- 互換の層は要らない。公開の型へのメンバーの追加は末尾で、既存の利用側のソースは変えずにビルドできる。内部の関数の形も変えない。
- 進める順序は、型（`clopts.h` の 3 つのメンバーと関数の宣言）を最初に入れ、その後に 3 つの機能を 1 つずつ入れる。出現回数は `clopts_result.c`、隠しオプションは `clopts_help.c` と `clopts_decl.c` の 1 か所、別名は `clopts_decl.c` を変える。隠しオプションと別名はどちらも `clopts_decl.c` とテスト用の宣言（`test_decl.[ch]`）に触れるので、並行にはせず順に進める。
- CLOpts のバージョンは、公開の型と関数の追加を表すために 0.3.0 に上げる。照合が `SameMinorVersion` なので、`find_package(CLOpts 0.2)` と書いた利用側は、0.3.0 を見つけなくなる。README の例を `0.3` に直す。Monde は `add_subdirectory` で取り込んでいるので影響を受けない。Monde のバージョンは変えない。
