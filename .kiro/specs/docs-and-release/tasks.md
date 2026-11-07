# Implementation Plan

- [x] 1. 基盤: 配布物の定義とクイックスタートの材料
- [x] 1.1 配布物の定義（install の規則と CPack の設定）を 1 か所に置き、手元でアーカイブを作れるようにする
  - 設計の MondePackage のとおり、実行ファイルと LICENSE、README.md だけを install の対象にし、アーカイブ名 `monde-<version>-<os>-<arch>`、最上位ディレクトリ、OS ごとの生成器の既定（Windows は ZIP、それ以外は TGZ）、対象名のキャッシュ変数（未指定時は OS とプロセッサから決め、macOS のアーキテクチャ指定を優先）を定義する。ソースパッケージは作らない
  - ルートのビルド構成には読み込みの 1 行だけを加え、既存のターゲット作成関数やコンパイラオプションは変更しない
  - 完了時: `BUILD_TESTING=OFF` の構成からビルドして `cpack -C Release` を実行すると、`monde-0.1.0-linux-x64.tar.gz` ができ、その一覧が `monde-0.1.0-linux-x64/` の下の実行ファイル（実行権限あり）、`LICENSE`、`README.md` の 3 つだけで、`cpack -G ZIP` でも同じ構成になる。既存の `ctest` の結果は変わらない
  - _Boundary: MondePackage_
  - _Requirements: 7.11, 8.1, 8.2, 8.3, 8.4, 8.5, 8.7_

- [x] 1.2 (P) クイックスタートと検査が使う例の文書と翻訳済みの翻訳ファイルを用意する
  - フロントマター（`title`）、見出し、強調とリンクを含む段落、リスト、コードブロックを 1 つずつ含む 20 行程度の英語の文書を LF・BOM なしで作る
  - ファイル名は `hello.md` と `hello.ja.po` に固定する（README のクイックスタートは `init` の出力名を `hello.ja.po` にし、検査はその出力をこの例の `.po` で置き換える）
  - その文書を `--front-matter-keys title` で `extract` し、`init -l ja` した結果に日本語の訳を入れた翻訳ファイルを作る。すべてのエントリを翻訳済みにし、fuzzy を含めない
  - README と `docs/` 配下の Markdown、`docs/examples/` のファイルを、どの OS でも LF で取り出せるように属性を指定する（既存の `-text` の行は変えない）
  - 完了時: ビルドした `monde` で例の文書を `translate` すると、原文と異なる（訳が入った）Markdown が出力され、統計が `untranslated` 0、`fuzzy` 0、`missing` 0 を示す。`msgfmt` がある環境では `msgfmt --check` も通る
  - _Boundary: examples, .gitattributes_
  - _Requirements: 1.4, 3.3, 9.2_

- [x] 2. ドキュメント: README と利用マニュアルの各章
- [x] 2.1 (P) README を全面的に書き直す
  - 設計の README の「構成（節の順）」の 10 節をこの順に書く。サブコマンドの表、Release の対応表、3 行のビルド手順と `-DBUILD_TESTING=OFF`、初版の対応範囲と次の版、SemVer と 0.x の緩和規則、MIT License と The Monde Authors、サードパーティの表（spec.txt は CC-BY-SA 4.0、Naos v0.4.0 は MIT License、どちらもテスト専用で配布物に含まれない）
  - クイックスタートは 1.2 の例の文書を使う `extract` → `init` → 編集 → `translate` の 4 段階とし、コマンド列のコードブロックの直前にマーカー `<!-- monde-check: quickstart -->` を置く。各行はパイプやリダイレクトを使わず、`-o` で出力先を指定する（`init` の出力は `hello.ja.po`）。編集の段階は `#` で始まるコメント行で「`hello.ja.po` を翻訳する。ここでは `docs/examples/hello.ja.po` の訳を使う」と書き、検査がこの行を無視して例の `.po` で置き換えることを前提にする
  - 目次（`docs/README.md`）と 4 章への相対リンクを置く。配布物に同梱されるため、冒頭に「最新のドキュメントはリポジトリの `docs/` にある」と 1 行で示す
  - 本文は日本語、コマンド名・オプション名・出力の例は英語のまま。`docs/dev/` と `docs/inputs/` へのリンクは置かない
  - 完了時: 設計の 10 節がすべてあり、クイックスタートのコマンド列を 1.2 の例をコピーした作業ディレクトリで上から順に実行すると全行が成功する
  - _Boundary: README_
  - _Depends: 1.2_
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 2.2, 2.3, 2.5, 6.1, 6.5_

- [x] 2.2 (P) 利用マニュアルの目次と「インストール」の章を書く
  - 目次は 4 章の一覧と 1 行の説明、相対リンク
  - インストールの章は設計の install.md の「構成」のとおり: OS・アーキテクチャとアーカイブ名の対応表、`SHA256SUMS` での確認方法、展開と配置（Windows は `monde.exe`）、`monde --version` での確認、ソースからのビルド（CMake 3.21 以上、各 OS のコンパイラ、製品だけのビルド、テストを含むビルドと `ctest`）
  - 本文は日本語、相対リンクはそのファイルからの相対で書く。`docs/dev/` と `docs/inputs/` へのリンクは置かない
  - 完了時: `docs/README.md` と `docs/install.md` があり、目次から 4 章のファイル名に辿れる。インストールの章に 4 対象すべてのアーカイブ名の例がある
  - _Boundary: docs/README.md, install.md_
  - _Requirements: 1.3, 2.1, 2.2, 2.3, 2.4, 2.5, 3.1, 3.2_

- [x] 2.3 (P) 「翻訳フロー」の章を書く
  - 設計の workflow.md の「構成」のとおり: 新規翻訳のフロー（各段階のコマンド、入出力ファイル、統計の意味）、原文更新のフロー（`update` の統計 `matched / fuzzy / added / obsoleted` の読み方、fuzzy と未翻訳の確認）、`update` の上書きと失敗時の保全、未翻訳と fuzzy の扱いと `--use-fuzzy` と翻訳率、フロントマターのキーパス（`extract` と `translate` に同じ指定）、`.po` の編集は PO 対応の外部ツールで行うこと、翻訳済み Markdown の差分レビュー
  - 統計や診断のように実行ごとに変わる値を含む出力は引用せず、書式の説明にとどめる
  - 完了時: `docs/workflow.md` があり、2 つのフローがそれぞれ番号付きの手順になっていて、各手順にコマンドと入出力ファイルが書かれている
  - _Boundary: workflow.md_
  - _Requirements: 2.3, 2.4, 2.5, 3.3, 3.4, 3.5, 3.6, 3.7, 3.8, 3.9_

- [x] 2.4 (P) 「コマンドリファレンス」の章を書く
  - 設計の commands.md の「構成」のとおり: 全体の使い方は `monde --help` の出力をマーカー `<!-- monde-check: help -->` 付きのコードブロックで引用し、`help <command>`、`--version` の 1 行、引数なしの実行を補足する。4 サブコマンドはそれぞれ `<!-- monde-check: help <subcommand> -->` 付きで `--help` を引用し、省略時の既定値、`-` の意味、`init` の言語コードの決め方（環境変数の順、`C` / `POSIX`、決められないときのエラー）、`extract` のヘッダのオプションを補足する
  - 統計の 5 つの固定の書式を表で示し、`-q/--quiet` が抑止するものとしないものを書く。診断の 3 つの形式と終了コード 0 / 1 / 2 の意味、文字コード・改行コード・BOM の扱い、翻訳対象の判定、出力先の上書きとエラー時の保全を書く
  - 引用はビルドした `monde` の出力をそのまま（LF、末尾の改行を含めて）貼り、手で整形しない
  - 完了時: `docs/commands.md` に 5 つのマーカー付きコードブロックがあり、それぞれ対応する `--help` の出力とバイト単位で一致する（`diff` で確認）
  - _Boundary: commands.md_
  - _Requirements: 2.3, 2.4, 2.5, 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7, 4.8, 4.9, 4.10_

- [x] 2.5 (P) 「既知の制限」の章を書く
  - 設計の limitations.md の「構成」のとおり: GFM の記法（表、タスクリスト、取り消し線、自動リンク、脚注）が CommonMark としての解釈のまま翻訳単位になること、YAML フロントマターの非対応（アンカー、エイリアス、タグ、複数ドキュメント）、yaml-front-matter の設計の「受け入れた制限」3 点を利用者の言葉で（ルートがインデント 0 のスカラーの継続とブロックスカラー、フロー形式の継続行のインデント、ダブルクオートのキーの中の U+2028・U+2029）、1 ファイル単位・設定ファイルなし・複数形は読み込みのみ・翻訳率による制御なし、構造を変え得る訳文の警告と対処
  - 完了時: `docs/limitations.md` があり、要件 5.1〜5.7 の各項目に対応する見出しまたは箇条書きがある
  - _Boundary: limitations.md_
  - _Requirements: 2.3, 2.4, 2.5, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7_

- [x] 3. ドキュメントの検査を CTest に登録する
- [x] 3.1 ヘルプの引用の検査を作り、`docs` ラベルのテストとして登録する
  - 設計の docs_check の Batch / Job Contract のとおり `cmake -P` のスクリプトを作る。`help` モードは対象の Markdown からマーカー `monde-check: help [<subcommand>]` の直後のコードブロックを抜き出し、ビルドした `monde` の `--help` の標準出力と CRLF を LF に正規化してから比べる。マーカーが 0 件、マーカーの直後にコードブロックがない、終了コードが 0 でない、内容が異なる、のそれぞれを失敗の一覧に加え、最後にまとめて報告する
  - マーカーが 0 件の失敗は、渡されたファイル全体で数えて判定する。`docs_check.help` の対象は `docs/commands.md` だけにする（README には `help` のマーカーがない）
  - `tests/docs/` のビルド構成で `docs_check.help` を登録し、`tests/` のビルド構成に 1 行加える。`BUILD_TESTING=OFF` では何も登録しない
  - 完了時: `ctest -L docs -R help` が合格し、`docs/commands.md` の引用を 1 文字変えると失敗して、どのマーカーが不一致かが出力に出る
  - _Depends: 2.4_
  - _Requirements: 4.3, 9.1_

- [x] 3.2 クイックスタートのコマンド列の検査を加える
  - `quickstart` モードは例の `hello.md` だけを作業先にコピーし、README のマーカー `monde-check: quickstart` の直後のコードブロックの各行（`#` と空行を除く）を引数に分け、先頭の `monde` をビルドした実行ファイルに置き換えて作業先で実行する。各行の終了コードが 0 でなければ失敗。パイプやリダイレクトを含む行は「検査できない行」として失敗にする
  - 先頭の引数が `init` の行を実行した直後に、その出力 `hello.ja.po` の msgstr がすべて空であることを確かめてから、例の `hello.ja.po` で置き換える（翻訳者の編集の代わり）
  - すべての行の後に `hello.pot`、`hello.ja.po`、`hello.ja.md` が存在し、`hello.ja.md` が `hello.md` と異なることを確かめる
  - 完了時: `ctest -L docs -R quickstart` が合格し、README のコマンド列の 1 行を存在しないオプションに変えると失敗する
  - _Depends: 1.2, 2.1_
  - _Requirements: 1.4, 3.3, 9.2_

- [x] 3.3 リポジトリ内リンクの検査を加える
  - `links` モードは `README.md` と `docs/*.md`（`docs/dev/`、`docs/inputs/` は除く）から `[text](target)` を集め、`http://`、`https://`、`mailto:`、`#` で始まるものを除き、`#` 以降を取り除いて、そのファイルからの相対で存在を確かめる
  - 完了時: `ctest -L docs -R links` が合格し、存在しないファイルへのリンクを 1 つ足すと失敗して、そのファイルとリンク先が出力に出る
  - _Depends: 2.1, 2.2, 2.3, 2.4, 2.5_
  - _Requirements: 1.5, 2.2, 2.4, 9.4_

- [x] 4. リリースの自動化
- [x] 4.1 (P) タグとビルド構成のバージョンの一致を確かめるスクリプトを作る
  - 設計の check_version の契約のとおり、タグが `vX.Y.Z` の形式でなければ失敗し、ルートのビルド構成の `project(monde VERSION ...)` から読んだ番号とタグの番号が一致しなければ `tag vX.Y.Z does not match project VERSION A.B.C` を出して失敗、一致すれば `version X.Y.Z` を出して成功する
  - 完了時: `v0.1.0`（成功）、`v0.2.0`（不一致で失敗）、`0.1.0` と `v0.1`（形式の誤りで失敗）の 4 通りで期待どおりの終了コードとメッセージになる
  - _Boundary: check_version_
  - _Requirements: 6.3, 7.8_

- [x] 4.2 (P) 実行ファイルとアーカイブを検証するスクリプトを作る
  - 設計の verify_package の契約のとおり: `--version` の標準出力が `monde X.Y.Z` の 1 行と一致、Linux は `ldd` で静的、macOS は `otool -L` の依存が libSystem だけ、Windows は確認しない。アーカイブの一覧が最上位ディレクトリの下の実行ファイル、`LICENSE`、`README.md` の 3 つだけで、TGZ では実行ファイルに実行権限がある。実行ファイルの前置きコマンド（macOS x64 の `arch -x86_64`）を受け取れる
  - アーカイブの指定を省略すると `--version` と動的依存の確認だけを行う（ワークフローが `cpack` の前に実行ファイルを確かめるため）
  - 失敗は一覧にして終了コード 1、すべて通れば `package <archive> verified`（アーカイブ省略時は `executable verified`）を出す
  - 完了時: 本環境で `-DCMAKE_EXE_LINKER_FLAGS=-static` でビルドして `cpack` したアーカイブに対して成功し、動的リンクのビルドに対しては `ldd` の確認で失敗し、アーカイブに余分なファイルを足したものに対しては一覧の確認で失敗する
  - _Boundary: verify_package_
  - _Depends: 1.1_
  - _Requirements: 6.3, 7.3, 8.5, 8.6, 8.7_

- [x] 4.3 リリースワークフローを作る
  - 設計の release.yml の契約のとおり: `vX.Y.Z` 形式のタグの push と `workflow_dispatch`（入力 `tag`）で起動。`check` ジョブはタグの存在と 4.1 のスクリプトによる一致の確認。`build` ジョブは `needs: check`、`fail-fast: false` で設計のマトリクスの 4 対象（Windows x64 は MSVC、macOS arm64 と x64 は同じランナーでアーキテクチャとデプロイ対象を指定、Linux x64 は静的リンク）を、`BUILD_TESTING=OFF` と対象名を渡した 3 行の手順でビルドし、4.2 のスクリプトをアーカイブ省略で実行して `--version` を確かめ（macOS x64 は `arch -x86_64` を前置き）、`cpack -C Release`、4.2 のスクリプトでアーカイブを検証し、artifact として保存。`publish` ジョブは `needs: build` で artifact を集め、4 つ揃っていることを確かめ、`SHA256SUMS` を作って自己検証し、Release がなければタグ名を題名に本文なしで作成し、アーカイブと `SHA256SUMS` を `--clobber` で添付する
  - 権限はワークフロー全体で `contents: read`、`publish` だけ `contents: write`。入力の `tag` は環境変数経由で渡し、シェルに直接展開しない。サードパーティの Action は `actions/*` に限り、メジャーバージョンで固定する。全ステップで `shell: bash`
  - 既存の CI のワークフローは変更しない
  - `build` ジョブの linux-x64 の手順（構成、ビルド、`--version` の検証、`cpack`、アーカイブの検証）と `publish` ジョブの `sha256sum` / `sha256sum -c` の手順を、本環境で同じコマンド列として実行して通ることを確かめる
  - 完了時: `.github/workflows/release.yml` が YAML として正しく（ローカルで構文を確認）、設計のマトリクスの 4 行、3 つのジョブと `needs` の連鎖、起動条件がそのとおりに書かれ、linux-x64 と `publish` の手順のローカルでの実行が通る。初回のタグ `v0.1.0` での実行結果は保守者が確認する（4.4 の手順書に記す）
  - _Depends: 4.1, 4.2_
  - _Requirements: 6.2, 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8, 7.9, 7.10, 7.11, 7.12, 8.2, 8.6, 8.8, 8.9_

- [x] 4.4 保守者向けのバージョンとリリースの手順書を書く
  - 設計の dev/release.md の「構成」のとおり: SemVer の規則と 0.x の緩和、バージョンを上げる手順（main の CI の確認、ビルド構成のバージョンの書き換え、README とマニュアルの更新を同じ変更に含める、コミットと取り込み、タグの作成と push、ワークフローの完了の確認と Release の本文の記入）、失敗時の対処（`check` の不一致、ビルドの失敗、再実行と手動起動）、手元での再現（`cpack`、4.2 のスクリプト、複数構成ジェネレータでの `-C Release`）、ドキュメントの更新の約束事（マーカーの形式、`ctest -L docs`）、CLI の仕様が変わったときに更新する箇所
  - 完了時: `docs/dev/release.md` があり、手順が番号付きで書かれ、バージョンの一致の確認と `ctest -L docs` が手順に含まれている
  - _Depends: 4.3_
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 9.3_

- [x] 5. 統合の検証: 全体を通して確かめ、ドキュメントと steering を実装に合わせる
  - 本環境で、テストを含む構成の `ctest`（`docs` ラベルを含む全テスト）が合格し、`BUILD_TESTING=OFF` の静的リンクの構成から `cpack` した TGZ と `cpack -G ZIP` の ZIP の両方が 4.2 のスクリプトを通ることを確かめる
  - README とマニュアルの内容を要件 1〜6 の各項目と突き合わせ、欠けている項目があれば該当の章を直す
  - steering の structure.md に、`tests/docs/` が `src/` に対応しない 2 つ目の例外であること、`docs/` の利用者向けの章と `docs/examples/`、`docs/dev/release.md`、`tools/release/` の役割、`cmake/MondePackage.cmake` を追記する
  - 完了時: `ctest --test-dir build -C Release --output-on-failure` が全件合格し、`cpack` の 2 形式が検証を通り、steering の記述が実際のファイル構成と一致する
  - _Boundary: README, docs の 4 章, steering（統合タスクとして明示的に複数の境界にまたがる）_
  - _Requirements: 7.11, 8.1, 8.2, 9.1, 9.2, 9.4_

- [ ] 6. 改訂（2026-11-07）: 実行ファイルに取り込んだ素材の表示
- [x] 6.1 索引 `THIRD_PARTY_NOTICES` と素材ごとの `licenses/` を作る
  - 設計の「改訂（2026-11-07）」の ThirdPartyNotices のとおり、索引の書式（コメント、空行で区切った塊、7 つのキーの順）で `unicode-ucd` と `whatwg-entities` の 2 つを書く
  - `licenses/unicode-ucd.txt` は生成したファイルに載っている Unicode License V3（`Copyright © 1991-2022 Unicode, Inc.`）と同じ文面、`licenses/whatwg-entities.txt` は `Copyright © WHATWG (Apple, Google, Mozilla, Microsoft)` と BSD 3-Clause の全文。どちらも先頭に名称、版、出典、SPDX 識別子、著作権表示を書く
  - `.gitattributes` に `THIRD_PARTY_NOTICES` と `licenses/*` の `text eol=lf` を加える
  - 完了時: 2 つのファイルと索引があり、ライセンスの本文が出典の原文と一致する（Unicode は生成したファイルの文面と、BSD 3-Clause は SPDX の原文と比べる）
  - _Boundary: THIRD_PARTY_NOTICES, licenses/, .gitattributes_
  - _Requirements: 10.1, 10.2, 10.3, 10.6_

- [x] 6.2 生成スクリプトと生成したファイルに ID の行を加える
  - 3 つの生成スクリプトが出力の先頭のコメントに `THIRD_PARTY_NOTICES: <ID>` の行を出すようにし、`gen_md_entities.py` はあわせて WHATWG の著作権表示と BSD 3-Clause の全文を出すようにする
  - 生成元のデータ（UCD 15.0.0 のデータファイルと `entities.json`）から 3 つのファイルを再生成し、差分がコメントだけであることを確かめる
  - 上流（markdown-parser、po-format）のファイルへの変更であり、表の内容と振る舞いは変えない
  - 完了時: 3 つの生成したファイルに ID の行があり、再生成した結果とバイト単位で一致し、既存の `ctest` が全件合格する
  - _Boundary: tools/gen_md_ucd.py, tools/gen_md_entities.py, tools/gen_po_ucd.py と、その生成物（コメントのみ）_
  - _Depends: 6.1_
  - _Requirements: 10.4_

- [x] 6.3 索引とファイルの対応の検査を加える
  - 設計の docs_check（notices モード）の 6 つの確認を `tests/docs/docs_check.cmake` に加え、`docs_check.notices`（ラベル `docs`）を登録する
  - 完了時: `ctest -L docs -R notices` が合格し、索引の行を消す、`licenses/` に余分なファイルを足す、生成したファイルの ID の行を消す、のそれぞれで失敗する（コピーで確かめる）
  - _Boundary: tests/docs_
  - _Depends: 6.1, 6.2_
  - _Requirements: 10.5_

- [x] 6.4 配布物と検証に素材の表示を加える
  - `MondePackage.cmake` の install に `THIRD_PARTY_NOTICES` と `licenses/` を加える。`verify_package.cmake` に `NOTICES` を加え、期待する一覧を索引から組み立てる。`release.yml` のアーカイブの検証に `-DNOTICES=THIRD_PARTY_NOTICES` を渡す
  - 完了時: 静的リンクのビルドの TGZ と ZIP が新しい検証を通り、`licenses/` のファイルが 1 つ欠けたアーカイブと余分なファイルを含むアーカイブはどちらも失敗する。`release.yml` は actionlint を通る
  - _Boundary: MondePackage, verify_package, release.yml_
  - _Depends: 6.1_
  - _Requirements: 8.3, 8.4, 8.7, 10.1_

- [ ] 6.5 ドキュメントと steering を改める
  - README の「サードパーティ」を、取り込んでいるもの（`THIRD_PARTY_NOTICES` と `licenses/` へのリンク）とテスト専用のものの 2 つの表に分ける。`docs/install.md` の展開した後の構成、`docs/dev/release.md` の同梱物、検証のコマンド（`-DNOTICES`）を改め、`docs/dev/release.md` に素材を追加・更新・削除するときの手順を加える
  - steering の tech.md に取り込む素材の規則（実行時の依存はゼロのまま、許容型のライセンスに限る、同じ変更で登録する）を、structure.md に `THIRD_PARTY_NOTICES` と `licenses/` の役割を書く
  - 完了時: `ctest -L docs` が合格し、要件 1.7、10.7 の記述がある
  - _Boundary: README, docs/install.md, docs/dev/release.md, steering_
  - _Depends: 6.1, 6.3, 6.4_
  - _Requirements: 1.7, 8.3, 10.7_

- [ ] 6.6 改訂の統合の検証
  - 全テストの `ctest` と、静的リンクの TGZ と ZIP の `verify_package.cmake`（`NOTICES` 付き）を通し、アーカイブを展開して `THIRD_PARTY_NOTICES` と `licenses/` の中身が直下のファイルと一致することを確かめる
  - 完了時: 全テストが合格し、2 形式の検証が通る
  - _Boundary: 統合タスク_
  - _Depends: 6.1, 6.2, 6.3, 6.4, 6.5_
  - _Requirements: 8.3, 8.4, 10.1, 10.5_
