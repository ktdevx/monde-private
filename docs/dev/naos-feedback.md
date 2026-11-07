# Naos への改善要望メモ

Monde の開発で Naos（テストフレームワーク）を使っていて気づいた「こんな機能があったらいいのに」を記録する。Issue への登録は開発者（人）が定期的に抜き出して行う。

> **AI エージェントへ:** このファイルへの追記だけを行うこと。Naos のリポジトリに Issue・コメント・PR を作成するなど、外部への投稿は依頼されても開発者の明示的な指示がない限り行わない。状態を「登録済み」に書き換えるのも開発者が行う。

## 記録のルール

- 1 件 1 見出し。新しいものを下に追加する。
- 状態は「未登録」で書き始める。開発者が Issue にしたら「登録済み（Issue の URL）」、見送ったら「見送り（理由）」に書き換える。
- 対象の Naos のバージョンを書く（現在は v0.4.0）。
- 重要度は Monde の開発への影響で決める。記録した時点の見立てで書き、状況が変われば更新する（開発者が書き換えてもよい）。
  - 高: 回避策がない、または開発・CI が止まる
  - 中: 回避策はあるが、手間・副作用（表示や検査の質が落ちるなど）がある
  - 低: あると便利。今は困っていない
- 実際に起きたこと（ログ、コード）と、Monde 側で取った回避策を必ず書く。推測だけのものは「確度」に明記する。
- Monde の中で解決すべき問題（Monde のコードや CMake の誤り）はここに書かない。

## テンプレート

```markdown
### <要望の要約>
- 状態: 未登録
- 重要度: 高 / 中 / 低（理由を一言）
- 対象: Naos v0.4.0
- 確度: 再現済み / コードを読んで確認 / 推測
- 背景: <何をしようとして、何が起きたか。ログやファイルの場所>
- 要望: <Naos にどうなってほしいか>
- 現在の回避策: <Monde 側でどうしたか>
```

## 要望一覧

### NAOS_REQUIRE_NOT_NULL / NAOS_CHECK_NOT_NULL が GCC の -Wmaybe-uninitialized を誘発する
- 状態: 登録済み（https://github.com/vabelorg/naos/issues/39）
- 重要度: 中（回避策はあるが、失敗時の表示がポインタ専用の形式でなくなる。GCC の -Werror 環境では知らないとビルドが止まる）
- 対象: Naos v0.4.0
- 確度: 再現済み（GitHub Actions の gcc ジョブ、ubuntu-latest）
- 背景: `malloc` した直後の（中身をまだ書いていない）ポインタを `NAOS_REQUIRE_NOT_NULL(p)` で確認すると、GCC が `'p' may be used uninitialized [-Werror=maybe-uninitialized]` を出す。マクロがポインタを `naos_check_not_null(const void *ptr, ...)` に渡すため、GCC が「未初期化の領域を const ポインタで読み取り関数に渡している」とみなす。`-Werror` 環境ではビルドが止まる。Clang、MSVC では出ない。該当箇所: `tests/foundation/diag_test.c` の `add_long_message_is_not_truncated`。
- 要望: NULL 判定をマクロ側で `(ptr) != NULL` として評価し、関数には判定結果と表示用の値だけを渡すなど、未初期化領域へのポインタを渡しても警告が出ない形にしてほしい。
- 現在の回避策: `NAOS_REQUIRE_TRUE(p != NULL)` に置き換えた（失敗時の表示はポインタ専用の形式ではなくなる）。
- 対応: Naos 側で修正済み（https://github.com/vabelorg/naos/pull/44 、2026-11-06 マージ）。Monde が固定している v0.4.0 には入っていないため、修正を含む版に上げるまで回避策を残す。

### 失敗位置の出力形式をコンパイラに関係なく揃えられるようにしてほしい
- 状態: 登録済み（https://github.com/vabelorg/naos/issues/41）
- 重要度: 低（ログの検索で両方の形式を許容すれば済む）
- 対象: Naos v0.4.0
- 確度: 再現済み（ローカルの MSVC と GitHub Actions の 5 環境）
- 背景: 失敗位置が MSVC では `file(line): error:`、それ以外では `file:line: error:` になる（`src/human.c` の `put_location` が `_MSC_VER` で切り替えている）。IDE で位置へ飛べるようにする意図は理解できるが、複数環境の CI ログを同じ正規表現で集計・検索するときに形式が揃わない。
- 要望: 形式を選べるオプション（コマンドライン引数や環境変数など）がほしい。既定は現状のままでよい。
- 現在の回避策: Monde の設計書と確認手順で、両方の形式を許容すると明記した。

### naos_add_executable の PROPERTIES にセミコロンを含む値を渡せない
- 状態: 登録済み（https://github.com/vabelorg/naos/issues/42）
- 重要度: 低（Monde ではまだ必要になっていない）
- 対象: Naos v0.4.0
- 確度: コードを読んで確認（Monde ではまだ必要になっていない）
- 背景: `PROPERTIES` は `<name> <value>` の組を CMake のリストとして受け取り、要素数が奇数だとエラーにする（`cmake/Naos.cmake` の引数検査）。`ENVIRONMENT "A=1;B=2"` のように値自体がリストになるプロパティを渡すと、リストが展開されて組がずれ、エラーになるか別のプロパティとして解釈される。
- 要望: リストの値を渡す方法（エスケープの規約を文書化する、プロパティごとに別の引数で渡せるようにする、など）を用意してほしい。
- 現在の回避策: なし。Naos のテストはビルド後に検出・登録されるため、構成時に `set_tests_properties` を呼んで代わりにできるかは未確認。

### 補助関数の失敗を呼び出し元の位置で表示する手段がほしい
- 状態: 登録済み（https://github.com/vabelorg/naos/issues/43）
- 重要度: 低（trace で呼び出し元の位置を補えるため、今は困っていない）
- 対象: Naos v0.4.0
- 確度: 再現済み（`tests/po_merge/test_util.c` の補助関数で確認）
- 背景: テスト用の補助関数（PO を読み込む、書き出して期待する文字列と比べる、など）の中で `NAOS_CHECK_*` / `NAOS_REQUIRE_*` を使うと、失敗の位置は補助関数の中（`test_util.c:66` など）と表示され、どのテストのどの呼び出しで失敗したかがわからない。マクロが呼ぶ記録用の関数（`naos_check_str` など）は file と line を受け取るが、`naos/control.h` に「直接呼ぶことを意図していない」とあるため、呼び出し元の位置を渡して使うことは避けた。
- 要望: 位置を引数で受け取る形のアサーション（例: `NAOS_CHECK_STR_EQ_AT(file, line, a, b)`）か、補助関数であることを宣言して失敗の位置を呼び出し元にする仕組みを、公開の API として用意してほしい。
- 現在の回避策: `test_util.h` に、補助関数の呼び出しを `NAOS_TRACE_PUSH` / `NAOS_TRACE_POP` で囲むマクロ（`TEST_READ_PO` など）を用意した。失敗の表示は補助関数の中の位置になるが、続く trace の行に呼び出し元のファイルと行が出る。

### NAOS_REQUIRE_NOT_NULL に関数ポインタを渡せない
- 状態: 登録済み（https://github.com/vabelorg/naos/issues/40）
- 重要度: 低（`NAOS_REQUIRE_TRUE(p != NULL)` で代わりにできる）
- 対象: Naos v0.4.0
- 確度: 再現済み（`tests/markdown/model_test.c`、GCC の `-Wpedantic`）
- 背景: `NAOS_REQUIRE_NOT_NULL` はポインタを `const void *` に変換して記録用の関数に渡すため、関数ポインタ（確保関数の `alloc` など）を渡すと、ISO C が禁じる関数ポインタからオブジェクトポインタへの変換として `-Wpedantic` の警告になり、`-Werror` 環境ではビルドが止まる。
- 要望: NULL 判定をマクロ側で `(ptr) != NULL` として評価し、関数ポインタでも使えるようにしてほしい。
- 現在の回避策: `NAOS_REQUIRE_TRUE(p != NULL)` に置き換えた。
- 対応: Naos 側で修正済み（https://github.com/vabelorg/naos/pull/44 、2026-11-06 マージ）。Monde が固定している v0.4.0 には入っていないため、修正を含む版に上げるまで回避策を残す。

### 検査の失敗を一時的に捕まえて、補助関数の失敗の報告そのものを検査したい
- 状態: 未登録
- 重要度: 低（失敗を報告しない下位の関数に分けることで検査できており、今は困っていない）
- 対象: Naos v0.4.0
- 確度: コードを読んで確認（`naos/control.h`、`naos/suite.h`）
- 背景: tu のテスト用の補助関数 `test_check_bytes`（`tests/tu/test_util.c`）は、2 つのバイト列が異なれば最初に異なる位置と前後を note に出してテストを失敗させる。この「異なるときに正しく失敗し、正しい位置を報告する」ことを front_matter スイートのテストで確かめたかったが、Naos には検査の失敗を記録せずに捕まえる（または「このブロックの中で失敗が 1 回起きること」を期待する）手段がない。`NAOS_ADD_TEST_XFAIL` はテスト全体を対象にした既知の不具合の印であり、補助関数の失敗の経路を検査する用途には合わない。
- 要望: ブロックの中で起きた `NAOS_CHECK_*` の失敗を記録せずに数え、メッセージ（note を含む）を取り出せる仕組み（例: `NAOS_EXPECT_FAILURE_BEGIN()` / `NAOS_EXPECT_FAILURE_END(&count)`）がほしい。
- 現在の回避策: 失敗させない下位の関数（`test_find_difference`、`test_describe_difference`）に分け、位置と説明の文字列をテストで比べた。失敗させる側（`test_check_bytes`）は説明を note に出して `NAOS_CHECK_FALSE` するだけの薄い関数にし、失敗時の表示は一時的に失敗する呼び出しを加えて手で確認した。
