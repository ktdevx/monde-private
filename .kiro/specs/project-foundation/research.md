# Research & Design Decisions

## Summary
- **Feature**: `project-foundation`
- **Discovery Scope**: New Feature（グリーンフィールド。ソースコード・ビルド構成・CI が存在しない状態からの構築）
- **Key Findings**:
  - Naos v0.4.0 は `naos_add_executable()` で各テストを `<target>.<suite>.<test>` の名前で CTest に 1 件ずつ登録し、ターゲット `Naos::naos` を提供する。別プロジェクトのサブディレクトリとして取り込むと静的ライブラリになり、自己テストやインストール規則は生成しない。
  - コンパイラの警告・フラグはグローバル変数（`CMAKE_C_FLAGS`）ではなくターゲット単位で適用すれば、Naos 本体のビルドに影響させずに Monde のターゲットだけを統一できる。ただし MSVC のランタイム選択（`CMAKE_MSVC_RUNTIME_LIBRARY`）は Naos を含めて全ターゲットで揃える必要がある。
  - 「VLA や警告があるとビルドが失敗する」こと自体を CTest のテスト（失敗が期待されるビルド）で検証できる。誤検知を避けるため、正常にビルドできるべき対照ターゲットを併置する。

## Research Log

### Naos の取り込みと CTest 連携
- **Context**: Requirement 2.1〜2.4（固定バージョンでの取り込み、CTest 実行、新モジュールの登録手順）
- **Sources Consulted**:
  - https://github.com/vabelorg/naos （README）
  - `docs/getting-started/installation.md`、`docs/reference/cmake.md`、`docs/reference/assertions.md`、`docs/guides/writing-tests.md`、`docs/guides/ctest-and-ci.md`、`docs/guides/fixtures.md`（タグ v0.4.0）
  - タグ一覧: v0.4.0（09fde7172c4b63d416c32e98499cb0c0306d3dea）、v0.3.0、v0.2.0、v0.1.0
- **Findings**:
  - 取り込み: `FetchContent_Declare(Naos GIT_REPOSITORY https://github.com/vabelorg/naos.git GIT_TAG v0.4.0)` + `FetchContent_MakeAvailable(Naos)`。公式はタグでの固定を推奨。CMake 3.21 以上が必要。
  - `naos_add_executable(<target> SOURCES ... [LIBRARIES ...] [PREFIX ...] [LABELS ...] [PROPERTIES ...] [DISCOVERY_TIMEOUT ...] [ALLOW_NO_ASSERTIONS])`。`LIBRARIES` は PRIVATE でリンクされ、`Naos::naos` もリンクされる。
  - CTest 名は `<prefix><suite>.<test>`（`PREFIX` の既定は `<target>.`）。各テストは `--naos-filter` で個別プロセスとして実行され、終了コード 77 はスキップ。ビルド前は `<prefix>NOT_BUILT` が失敗として登録される。
  - CTest にテストを登録するにはトップレベルで `enable_testing()` または `include(CTest)` が必要。
  - 複数ファイル: 各ファイルで `NAOS_SUITE(name, NAOS_ADD_TEST(...))` を定義し、`main` を持つファイルで `NAOS_DECLARE_SUITE(name);` と `NAOS_MAIN(NAOS_ADD_SUITE(name), ...)` を書く。スイート名は実行ファイル内で一意。
  - アサーション: `NAOS_CHECK_*` / `NAOS_REQUIRE_*`（`INT_*`、`UINT_*`、`STR_EQ`、`MEM_EQ`、`PTR_EQ`、`NULL`、`NOT_NULL`、`TRUE`、`FALSE`）。`size_t` は `UINT` 系で比較する。NUL を含むバイト列は `MEM_EQ` で比較する。アサーションを 1 つも実行しないテストは失敗になる。
  - セットアップ・ティアダウン: `NAOS_SETUP` / `NAOS_TEARDOWN` / `NAOS_SET_SETUP` / `NAOS_SET_TEARDOWN`。スキップは `NAOS_SKIP`。
  - Naos 自身はトップレベルのときだけ `-Wall -Wextra -Wpedantic -Werror`（MSVC は `/W4 /WX`）でビルドされる。対応環境は Linux（GCC、Clang）、macOS（Clang）、Windows（MSVC、MSYS2 の MinGW-w64 GCC）で、Monde の対象と一致する。
  - CI 例: `cmake --build build --config <cfg>`、`ctest --test-dir build -C <cfg> --output-on-failure`。
- **Implications**:
  - モジュールのテスト登録は `naos_add_executable()` を包む Monde 側の関数 1 つに集約する（2.4）。
  - Naos はテスト有効時（`BUILD_TESTING`）のみ取得し、製品ターゲットからは参照しない（1.8）。
  - `--output-on-failure` により失敗したテストの出力（`file:line: error: ...`）がログに残る（2.3、3.4）。

### コンパイラごとのフラグと VLA の扱い
- **Context**: Requirement 1.4〜1.7
- **Sources Consulted**: steering `tech.md`、roadmap Constraints、CMake ドキュメント（`C_STANDARD`、`CMAKE_MSVC_RUNTIME_LIBRARY`、ポリシー CMP0091）
- **Findings**:
  - GCC / Clang / Apple Clang / MinGW-w64 GCC: `C_STANDARD 99` + `C_EXTENSIONS OFF`（`-std=c99`）、`-Wall -Wextra -Wpedantic -Werror -Werror=vla`。
  - MSVC は C99 モードを持たないため `/std:c11` を使い、コードは C99 の範囲に制限する。`/W4 /WX /utf-8 /Zc:preprocessor` と `_CRT_SECURE_NO_WARNINGS` を指定する。MSVC は VLA を実装しない（`__STDC_NO_VLA__`）ため、VLA は常にコンパイルエラーになる。
  - `/Zc:preprocessor` の有効性は `_MSVC_TRADITIONAL == 0` で、`/utf-8` の有効性は UTF-8 の文字列リテラルのバイト列で確認できる。
  - MinGW-w64 で `%llu` などの書式を警告なしで使うには `__USE_MINGW_ANSI_STDIO=1` を定義する。
  - `CMAKE_MSVC_RUNTIME_LIBRARY` に `MultiThreaded$<$<CONFIG:Debug>:Debug>` を指定すると静的 CRT になり、VC++ 再頒布パッケージへの依存がなくなる。CMake 3.21 を最小バージョンにすると CMP0091 は NEW になる。
- **Implications**: フラグは CMake モジュール 1 か所で定義し、Monde のターゲット作成関数がそれを適用する。

### Windows での非 ASCII パス
- **Context**: Requirement 4.6、5.4
- **Sources Consulted**: steering `tech.md`、Win32 API（`MultiByteToWideChar`、`_wfopen`）の仕様
- **Findings**:
  - `fopen` は ANSI コードページでパスを解釈するため、UTF-8 のパスをそのまま渡すと非 ASCII 文字が壊れる。`MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ...)` で UTF-16 に変換し `_wfopen` を使う。
  - `_wfopen` は MSVC と MinGW-w64 の両方の CRT で使える。
  - `MAX_PATH` を超える長いパスは `\\?\` 接頭辞なしでは扱えない。要件に含まれないため対象外とする。
- **Implications**: パス変換はファイル入出力コンポーネント内部の `#ifdef _WIN32` 部分に閉じ込め、公開 API は常に UTF-8 の `const char *` を受け取る。

### UTF-8 の妥当性検査
- **Context**: Requirement 4.4
- **Sources Consulted**: RFC 3629、Unicode Standard 第 3 章 Table 3-7（Well-Formed UTF-8 Byte Sequences）
- **Findings**: 不正とみなすのは、継続バイトの単独出現、途中で切れたシーケンス、冗長表現（overlong、例: C0 80）、サロゲート（U+D800〜U+DFFF）、U+10FFFF 超、F5〜FF のバイト。U+0000 は妥当な UTF-8 である。
- **Implications**: Table 3-7 に従うバイト単位の検証器を自前で実装し、最初の不正バイトのオフセットを返す。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| 単一モジュール `foundation` に共通部品を集約 | 1 ライブラリターゲット内に buf / diag / utf8 / eol / source / file_io を置く | 依存が単純。下流は `monde::foundation` 1 つにリンクすればよい | モジュール内の部品が増えると肥大化する | steering の例（`src/foundation/`）と一致。採用 |
| 共通部品ごとに別ライブラリ | buf、diag などを別ターゲットにする | 依存を細かく制御できる | ターゲット数が増え、下流のリンク指定が煩雑になる。部品間の依存（file_io → diag → buf）で結局まとめてリンクされる | 不採用 |
| フラグをグローバル（`CMAKE_C_FLAGS`）に設定 | トップレベルで一括設定する | 記述が短い | Naos 本体にも `-Werror` などが及び、Naos の変更で Monde のビルドが壊れる | 不採用。ターゲット単位で適用する |

## Design Decisions

### Decision: シンボルの接頭辞と include の基点
- **Context**: structure.md で「接頭辞の規則」と「include パスの基点」を本 spec で確定するとされている。
- **Alternatives Considered**:
  1. モジュール名をそのまま接頭辞にする（`foundation_buf_append`）— 長い
  2. プロジェクト共通の接頭辞 `mnd_` + 部品の短縮名（`mnd_buf_append`、`mnd_po_...`）
- **Selected Approach**: 2。外部から見える関数・型は `mnd_<短縮名>_`、マクロは `MND_<短縮名>_`。短縮名は各モジュールが自分の設計で決め、既存の短縮名と重複させない。include の基点は `src/` とし、`#include "foundation/buf.h"` の形で書く。
- **Rationale**: 標準ライブラリやテストフレームワーク（`naos_`）と衝突せず、短い。include の基点を 1 つにすると、モジュール間の依存が include 文から読み取れる。
- **Trade-offs**: 短縮名の重複は機械的に防げないため、設計レビューで確認する。
- **Follow-up**: structure.md に規則を反映する。

### Decision: エラーの返し方
- **Context**: Requirement 4.4、4.7、5.5、7.4、8.4、9.1〜9.4
- **Alternatives Considered**:
  1. 関数ごとに独自のエラーコード列挙型を返す
  2. 成否を `bool` で返し、詳細は呼び出し側が渡す診断リストに積む
- **Selected Approach**: 2。メモリ確保と範囲検証のように詳細が自明な失敗は `bool` のみで返す。ファイル入出力のように理由の説明が要る失敗は、診断リストにエラーを追加したうえで `false` を返す。
- **Rationale**: 診断の表示形式と終了コードを CLI 層で一括して決めるという方針（9 の Objective）と一致し、部品ごとのエラー型の乱立を避けられる。
- **Trade-offs**: 呼び出し側が失敗の種類でプログラム的に分岐したい場合、メッセージ文字列しか手がかりがない。現時点の要件にはその必要がないため、必要になった時点で診断にコードを追加する（追加は後方互換）。

### Decision: 行番号の算出は都度走査とし、索引は作らない
- **Context**: Requirement 8.3。性能目標は設けない（tech.md）。
- **Alternatives Considered**:
  1. 先頭からの線形走査（状態を持たない関数）
  2. 行頭オフセットの索引を構築し二分探索する
- **Selected Approach**: 1。
- **Rationale**: メモリ確保が不要で失敗経路が少なく、最小の設計で要件を満たす（Simplification）。パーサは走査中に行番号を自分で追跡できる。
- **Trade-offs**: 1 つの文書で多数の位置の行番号を求めると時間が 2 乗で増える。必要になれば索引の API を後から追加できる（既存 API は変えない）。

### Decision: メモリ確保失敗のテスト方法
- **Context**: Requirement 7.4
- **Alternatives Considered**:
  1. アロケータを差し替えるフック（テスト専用のグローバル状態）
  2. 容量計算のオーバーフロー（`SIZE_MAX` の要求）で確保不能の経路を通す
- **Selected Approach**: 2。容量計算でオーバーフローする要求と `realloc` の失敗は同じ失敗経路（内容を保ったまま `false` を返す）に合流させる。
- **Rationale**: 製品コードにテスト専用のグローバル状態を持ち込まない。
- **Trade-offs**: `realloc` が実際に `NULL` を返す経路はコードレビューで確認する。

### Decision: ビルド失敗の要件をテストで検証する
- **Context**: Requirement 1.5、1.6、1.7
- **Selected Approach**: `tests/toolchain/` に、既定のビルドから除外したプローブターゲット（警告を含むもの、VLA を含むもの、問題のない対照）を置き、CTest から `cmake --build --target <probe>` を実行する。警告・VLA のプローブは `WILL_FAIL`、対照は成功を期待する。`/utf-8` と `/Zc:preprocessor` の有効性は Naos のテストでコンパイル時・実行時に確認する。
- **Rationale**: 要件をドキュメント上の約束ではなく、全 CI 環境で毎回確かめられる検査にする。対照ターゲットにより「別の理由でビルドが失敗している」誤検知を防ぐ。
- **Trade-offs**: テスト中にビルドを呼ぶため直列実行（`RUN_SERIAL`）が必要で、テスト時間が少し延びる。

### Decision: テストデータはソースコード内のバイト列で持つ
- **Context**: Requirement 4〜6、8 のテスト。CRLF やバイト単位の一致を検証する。
- **Selected Approach**: 入力のバイト列は C の文字列リテラル・配列で記述し、テスト実行時にビルドディレクトリ配下の作業ディレクトリへ書き出す。非 ASCII のディレクトリは構成時に CMake で作成する。
- **Rationale**: Git の改行変換（`core.autocrlf`）でフィクスチャファイルの改行が変わる事故を避ける。

## Synthesis Outcomes
- **Generalization**: ファイル読み込みの不正 UTF-8 エラー（4.4）の行番号は、ソース範囲の行番号算出（8.3）を再利用する。改行の数え方（LF と CRLF を 1 つ、単独 CR は数えない）は改行検出（6.5）と行番号（8.3）で同じ規則を共有する。
- **Build vs. Adopt**: テストフレームワークは Naos を採用（指定）。UTF-8 検証、バッファ、診断は外部依存ゼロの制約により自前実装する。CI の MinGW-w64 環境は Naos と同じ MSYS2（`msys2/setup-msys2`）を採用する。
- **Simplification**: アロケータのフック、行番号の索引、エラーコード列挙型、`printf` 形式のバッファ追加関数、警告をエラーにしないビルドオプション、CMake プリセットはいずれも現要件に不要なため作らない。

## Consulted Skills
- `kiro-spec-design`（rules: design-principles、design-discovery-full、design-synthesis、design-review-gate）を適用した。ほかに本タスクに該当するドメインスキル（UI、アクセシビリティ等）はない。

## Risks & Mitigations
- Naos のヘッダが Monde の警告設定（`-Werror` など）で警告を出す — Naos 自身が同等の設定で自己テストしているため可能性は低い。発生した場合は Naos の include ディレクトリを `INTERFACE_SYSTEM_INCLUDE_DIRECTORIES` に設定して警告対象から外す。
- MinGW-w64 の `-std=c99`（`__STRICT_ANSI__`）で `_wfopen` などの宣言が隠れる — 実装時に CI で確認する。隠れる場合は file_io.c の Windows 部分でのみ必要な宣言を有効にする。
- Naos が pre-1.0 のため、タグ更新で API が変わる — タグを固定し、更新は専用の変更として行い全 CI 環境で確認する。
- GitHub Actions のランナーイメージ更新でコンパイラのバージョンが変わり、新しい警告で `-Werror` が失敗する — 失敗は環境ごとに報告されるため、原因の切り分けは容易。警告の修正で対応する。
- ビルドを呼ぶテストの並列実行による競合 — `RUN_SERIAL` を設定する。

## References
- [Naos](https://github.com/vabelorg/naos) — テストフレームワーク（v0.4.0、MIT）
- [RFC 3629](https://www.rfc-editor.org/rfc/rfc3629) — UTF-8 の定義
- Unicode Standard, Chapter 3, Table 3-7 — Well-Formed UTF-8 Byte Sequences
- [CMake: CMAKE_MSVC_RUNTIME_LIBRARY](https://cmake.org/cmake/help/latest/variable/CMAKE_MSVC_RUNTIME_LIBRARY.html)
- [msys2/setup-msys2](https://github.com/msys2/setup-msys2) — GitHub Actions での MSYS2 環境
