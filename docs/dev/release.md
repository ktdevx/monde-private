# バージョンとリリースの手順（保守者向け）

この文書は Monde の保守者向けです。バージョン番号の規則の運用、リリースの手順、失敗したときの対処、手元での再現の方法、ドキュメントの更新の約束事、実行ファイルに取り込む素材の表示の手順をまとめます。利用者向けのマニュアル（`docs/` 直下の章）からはリンクしません。

関係するファイルは次のとおりです。

| ファイル | 役割 |
| --- | --- |
| [`CMakeLists.txt`](../../CMakeLists.txt) | バージョン番号を書く唯一の場所（`project(monde VERSION X.Y.Z ...)`）。`monde --version` の表示はここから決まる |
| [`.github/workflows/release.yml`](../../.github/workflows/release.yml) | リリースのワークフロー（`check`、`build`、`publish` の 3 ジョブ） |
| [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) | main への push と pull request で動く CI（5 環境のビルドとテスト） |
| [`tools/release/check_version.cmake`](../../tools/release/check_version.cmake) | タグとビルド構成のバージョンの一致を確かめる |
| [`tools/release/verify_package.cmake`](../../tools/release/verify_package.cmake) | 実行ファイルと配布アーカイブを確かめる |
| [`cmake/MondePackage.cmake`](../../cmake/MondePackage.cmake) | 配布物の定義（install の規則と CPack の設定、`MONDE_RELEASE_TARGET`） |
| [`THIRD_PARTY_NOTICES`](../../THIRD_PARTY_NOTICES) | 実行ファイルに取り込んだ第三者の素材の索引。配布物に同梱し、`verify_package.cmake` が期待する同梱物をここから組み立てる |
| [`licenses/`](../../licenses/) | 素材ごとの著作権表示とライセンスの全文（`licenses/<ID>.txt`）。ディレクトリごと配布物に同梱する |
| [`tests/docs/`](../../tests/docs/) | ドキュメントと素材の表示の検査（`ctest -L docs`） |

## バージョン番号の規則

バージョン番号はセマンティックバージョニングに従い、`X.Y.Z` の 3 つの数で表します。README の「バージョン」の節と同じ規則です。

- **X**: CLI の互換性を壊す変更で上げます（サブコマンドやオプションの削除や改名、位置引数の個数や順序の変更、統計の書式や終了コードの意味の変更など）。
- **Y**: 互換性を保つ機能の追加で上げます（新しいサブコマンドやオプションの追加など）。
- **Z**: 修正だけのときに上げます。

X が 0 の間（`0.Y.Z`）は規則を緩めます。

- CLI の互換性を壊す変更でも、X ではなく Y を上げることがあります。
- 修正だけのときは、これまでどおり Z を上げます。

上げた桁より下の桁は 0 に戻します（例: `0.1.3` の次の機能追加は `0.2.0`）。

リリースのタグは `vX.Y.Z`（`v` とバージョン番号）です。初版のタグは `v0.1.0` です。タグから `v` を除いた番号は、`CMakeLists.txt` の `project(monde VERSION X.Y.Z)` と一致しなければなりません。一致しないタグではリリースのワークフローが `check` ジョブで止まります。

## リリースの手順

`X.Y.Z` は今回のバージョン番号に置き換えてください。

1. **main の CI が緑であることを確かめる。** GitHub の Actions で、main の最新のコミットに対する CI（`ci.yml`）の 5 つのジョブがすべて成功していることを確かめます。
2. **バージョン番号を書き換える。** リリース用のブランチで、`CMakeLists.txt` の `project(monde VERSION X.Y.Z LANGUAGES C)` の番号を今回の番号に書き換えます。バージョン番号を書く場所はここだけです。
3. **README とマニュアルの変更を同じ PR に含める。** 今回のリリースに CLI の仕様の変更があれば、対応する README と `docs/` の記述の更新を同じ PR に含めます（直す場所は「[CLI の仕様が変わったときに直す場所](#cli-の仕様が変わったときに直す場所)」を参照）。PR を出す前に、手元で次を実行して、タグとバージョンの一致とドキュメントの検査が通ることを確かめます。

   ```sh
   cmake -DTAG=vX.Y.Z -DSOURCE_DIR=. -P tools/release/check_version.cmake
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ctest --test-dir build -C Release -L docs --output-on-failure
   ```

   `check_version.cmake` は、一致すれば `version X.Y.Z` の 1 行を出して終了コード 0 で終わります。一致しなければ `tag vX.Y.Z does not match project VERSION A.B.C` を出して失敗します。`ctest -L docs` はテストを有効にした構成（`BUILD_TESTING` が `ON`、既定）のビルドディレクトリで実行します。

   `ctest -L docs` が実行するのは README のクイックスタートのコマンド列だけです。翻訳フローの章（`docs/workflow.md`）のコマンド列は、リリースの前に手で実行して確かめます。リポジトリの外の空のディレクトリで、`<repo>` をリポジトリのパスに読み替えて次を順に実行し、すべての行が終了コード 0 で終わり、統計の書式が `docs/workflow.md` の説明と一致することを確かめます。

   ```sh
   cp <repo>/docs/examples/hello.md .
   # 新規翻訳
   monde extract --front-matter-keys title -o hello.pot hello.md
   monde init -l ja -o hello.ja.po hello.pot
   cp <repo>/docs/examples/hello.ja.po .
   monde translate --front-matter-keys title -o hello.ja.md hello.md hello.ja.po
   # 原文更新（hello.md の末尾に段落を 1 つ書き足してから実行する）
   monde extract --front-matter-keys title -o hello.pot hello.md
   monde update -o hello.ja.po hello.pot hello.ja.po
   monde translate --front-matter-keys title -o hello.ja.md hello.md hello.ja.po
   monde translate --use-fuzzy --front-matter-keys title -o hello.ja.md hello.md hello.ja.po
   ```

   `monde` はビルドした実行ファイル（`build/src/monde/monde`、複数構成のジェネレータでは `build/src/monde/Release/monde`）に読み替えます。`docs/workflow.md` のコマンドを変えたときは、この一覧も同じ変更で直します。

4. **コミットして main に取り込む。** PR の CI が緑になったら main にマージします。マージ後の main の CI も緑であることを確かめます。
5. **マージした main のコミットにタグを付けて push する。** 手元の main をマージ後のコミットに合わせ、もう一度バージョンの一致を確かめてからタグを付けます。

   ```sh
   git switch main
   git pull --ff-only origin main
   cmake -DTAG=vX.Y.Z -DSOURCE_DIR=. -P tools/release/check_version.cmake
   git tag vX.Y.Z
   git push origin vX.Y.Z
   ```

   タグの push で `release.yml` が起動します。ブランチの push ではリリースのワークフローは起動しません。

6. **ワークフローの完了を待ち、Release の本文を書く。** Actions で Release ワークフローの `check`、4 つの `build (<対象>)`、`publish` がすべて成功したことを確かめます。`publish` は、タグ名を題名とし本文が空の Release を作り、次の 5 つのファイルを添付します。

   - `monde-X.Y.Z-windows-x64.zip`
   - `monde-X.Y.Z-macos-arm64.tar.gz`
   - `monde-X.Y.Z-macos-x64.tar.gz`
   - `monde-X.Y.Z-linux-x64.tar.gz`
   - `SHA256SUMS`

   添付を確かめたら、Release の本文（変更点の要約。CLI の互換性を壊す変更があればその旨）を GitHub の Release の編集画面で書きます。`gh` を使う場合は `gh release edit vX.Y.Z --notes-file <本文のファイル>` です。ワークフローを再実行しても本文は変わりません。

初版の `v0.1.0` では、上の確認に加えて、4 対象のビルドと検証が成功し、Release が作られ、5 つのファイルが添付されたことを確かめます。続けて同じタグで手動起動（「[再実行と手動起動](#再実行と手動起動)」）を 1 回行い、添付が置き換わり本文が変わらないことも確かめます。

## リリースのワークフローの動き

- **起動**: `vX.Y.Z` の形式のタグの push と、手動起動（`workflow_dispatch`。入力 `tag` にリリースするタグを渡す）だけで動きます。
- **`check`**: タグの形式（`vX.Y.Z`）、タグの存在、`check_version.cmake` によるバージョンの一致を確かめます。ここで失敗すると、ビルドも Release の作成も行いません。
- **`build`**: `windows-x64`、`macos-arm64`、`macos-x64`、`linux-x64` の 4 対象を、タグのコミットからビルドします。各対象で `verify_package.cmake` を実行ファイルだけに対して 1 回、`cpack -C Release` の後にアーカイブに対して 1 回実行します。`fail-fast: false` なので、1 つの対象が失敗しても他の対象は続き、失敗した対象はジョブ名 `build (<対象>)` で分かります。
- **`publish`**: 4 対象がすべて成功したときだけ動きます。4 つのアーカイブが揃っていることを確かめて `SHA256SUMS` を作り、Release がなければ作り（題名はタグ名、本文は空）、あれば題名と本文に触れずに、5 つのファイルを `gh release upload --clobber` で添付します。同名の添付は置き換わります。
- **同時実行**: 同じタグの実行（タグの push と手動起動など）が重なったときは、取り消さずに順に動かします（`concurrency` のグループ `release-<タグ>`）。

## 失敗したときの対処

### `check` の失敗（タグとバージョンの不一致）

`check` で止まったときは、Release も添付も作られていません。ログの `tag vX.Y.Z does not match project VERSION A.B.C` などで原因を確かめます。

- **タグの番号が誤っていた**（`CMakeLists.txt` の番号が正しい）場合: 誤ったタグを手元と GitHub の両方から消し、正しい番号のタグを付け直します。

  ```sh
  git tag -d vX.Y.Z
  git push origin :refs/tags/vX.Y.Z
  git tag vA.B.C
  git push origin vA.B.C
  ```

- **`CMakeLists.txt` の番号を書き換え忘れた**（タグの番号が正しい）場合: まず誤ったタグを上と同じく `git tag -d vX.Y.Z` と `git push origin :refs/tags/vX.Y.Z` で消します。次に、手順の 2〜4 で番号を直す PR を main に取り込み、取り込んだ後の main のコミットに手順 5 で同じタグを付け直します。

タグを消してよいのは、そのタグの Release がまだ作られていない（`publish` が一度も成功していない）ときだけです。Release を公開した後のタグは消したり付け替えたりしません。公開済みのバージョンに誤りがあれば、修正して次の番号（Z を上げる）でリリースし直します。

### ビルドの失敗

`build (<対象>)` のどれかが失敗すると、`publish` は動かず、Release は作られません（すでに Release がある再実行では、添付は前のまま変わりません）。失敗したジョブのログで、Configure、Build、Verify executable、Package、Verify package のどの段階で失敗したかを確かめます。

- **ランナーやネットワークの一時的な失敗**の場合: Actions の実行画面で「Re-run failed jobs」を選びます。再実行は同じタグのコミットをビルドします。
- **`release.yml` のステップ自体（ワークフローに直接書いたコマンド）の誤り**の場合: 修正を PR で main に取り込み、`gh workflow run release.yml --ref main -f tag=vX.Y.Z` で同じタグを手動起動します（「[再実行と手動起動](#再実行と手動起動)」）。手動起動で使うのは main の `release.yml` の定義です。
- **ソース、`CMakeLists.txt`、`cmake/MondePackage.cmake`、`tools/release/*.cmake` の誤り**の場合: `check` と `build` はタグのコミットを checkout するため、ビルドするソースも、構成と配布物の定義も、検証のスクリプト（`check_version.cmake`、`verify_package.cmake`）も、タグのコミットのものが使われます。修正を main に取り込んで同じタグを手動起動しても、修正は使われません。そのタグの Release がまだ作られていなければ、「`check` の失敗」と同じ方法でタグを消し、修正後の main のコミットに付け直して push します。Release がすでにある場合は、次の番号でリリースします。

失敗を手元で調べるときは「[手元での再現](#手元での再現)」の手順を使います。

### 再実行と手動起動

同じタグでリリースのワークフローをもう一度動かすには、Actions の Release ワークフローの「Run workflow」で `tag` に `vX.Y.Z` を入れて起動するか、`gh` で次を実行します。

```sh
gh workflow run release.yml --ref main -f tag=vX.Y.Z
```

- ワークフローの定義（`release.yml`）は `--ref` に指定したブランチ（ここでは main）のものを使います。一方、ソース、`CMakeLists.txt`、`cmake/MondePackage.cmake`、`tools/release/*.cmake` はタグのコミットのものを使います。手動起動で反映できるのは `release.yml` の修正だけです。
- `check` から順にすべてのジョブが動きます。タグが存在しない、または形式が `vX.Y.Z` でないときは `check` で止まります。
- Release がすでにあれば、題名と本文はそのままで、5 つの添付だけが `--clobber` で同名のファイルに置き換わります。
- タグの push の実行と重なったときは、順に動きます。

## 手元での再現

ワークフローの `build` ジョブの段階を手元で再現します。次の例は Linux x64 で、ビルドディレクトリを `build-release` とします（`build-*` は Git の管理外です）。リポジトリのルートで実行します。

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_EXE_LINKER_FLAGS=-static
cmake --build build-release --config Release
cmake -DMONDE=build-release/src/monde/monde -DVERSION=X.Y.Z -DTARGET=linux-x64 -P tools/release/verify_package.cmake
(cd build-release && cpack -C Release)
cmake -DMONDE=build-release/src/monde/monde -DVERSION=X.Y.Z -DTARGET=linux-x64 -DARCHIVE=build-release/monde-X.Y.Z-linux-x64.tar.gz -DNOTICES=THIRD_PARTY_NOTICES -P tools/release/verify_package.cmake
```

- **構成**: 対象名 `<os>-<arch>` は `cmake/MondePackage.cmake` が `MONDE_RELEASE_TARGET` の既定として決めます。ワークフローは `-DMONDE_RELEASE_TARGET=<対象>` で明示します。アーカイブの名前は `monde-<バージョン>-<MONDE_RELEASE_TARGET>` です。
- **Linux の静的リンク**: Linux では `-DCMAKE_EXE_LINKER_FLAGS=-static` を付けます。付けないと、`verify_package.cmake` が「dynamically linked」として失敗します（意図どおりの検査です）。
- **macOS**: ワークフローと同じく `-DCMAKE_OSX_ARCHITECTURES=arm64`（または `x86_64`）と `-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0` を付け、`TARGET` を `macos-arm64`（または `macos-x64`）にします。Apple シリコンで x64 の実行ファイルを確かめるときは `-DRUNNER=arch;-x86_64` を付けて Rosetta 2 で起動します（シェルでは `"-DRUNNER=arch;-x86_64"` のように引用します）。
- **Windows**: Visual Studio などの複数構成のジェネレータでは、実行ファイルは `build-release/src/monde/Release/monde.exe` にでき、`TARGET` は `windows-x64`、アーカイブは `.zip` です。
- **`cpack -C Release`**: 複数構成のジェネレータでは `-C Release` がないと構成が決まらず、パッケージを作れません。単一構成のジェネレータでも付けて差し支えないので、常に付けます。Windows では、Chocolatey の `cpack`（別のプログラム）が PATH 上で先に見つかることがあるため、`cmake` と同じディレクトリの `cpack` を使います。
- **`verify_package.cmake`**: `ARCHIVE` を省略すると、`--version` の出力（`monde X.Y.Z` の 1 行）と動的依存だけを確かめ、`executable verified` を出します。`ARCHIVE` を指定するときは、`-DNOTICES=THIRD_PARTY_NOTICES`（リポジトリの `THIRD_PARTY_NOTICES` のパス）も必須です。アーカイブの名前、中身、実行権限も確かめ、`package <ARCHIVE> verified` を出します。中身は、`monde-X.Y.Z-<対象>/` の下の実行ファイル、`LICENSE`、`README.md`、`THIRD_PARTY_NOTICES` と、`NOTICES` の各素材の `License-File`（`licenses/<ID>.txt`）だけでなければならず、過不足があれば失敗します。アーカイブの `THIRD_PARTY_NOTICES` が `NOTICES` とバイト単位で一致することも確かめます。失敗はすべて一覧にして終了コード 1 で終わります。中身を目で確かめるときは `tar tzvf build-release/monde-X.Y.Z-linux-x64.tar.gz` を使います。
- **`check_version.cmake`**: `cmake -DTAG=vX.Y.Z -DSOURCE_DIR=. -P tools/release/check_version.cmake` で、タグを付ける前に一致を確かめられます（手順の 3 と 5）。

## ドキュメントの更新の約束事

### 検査とマーカー

ドキュメントの記述は `ctest -L docs` で、ビルドした `monde` と照合します。テストを有効にした構成のビルドディレクトリで実行します（CI の `ctest` にも含まれます）。

```sh
cmake --build build --config Release
ctest --test-dir build -C Release -L docs --output-on-failure
```

検査は 4 つです。どれも 1 つの失敗で止まらず、失敗の一覧をまとめて出します。

| テスト | 対象 | 確かめること |
| --- | --- | --- |
| `docs_check.help` | `docs/commands.md` | マーカー `<!-- monde-check: help -->` と `<!-- monde-check: help <サブコマンド> -->` の直後のコードブロックが、`monde --help` と `monde <サブコマンド> --help` の標準出力と一致する |
| `docs_check.quickstart` | `README.md` | マーカー `<!-- monde-check: quickstart -->` の直後のコマンド列が、`docs/examples/` の例で順に成功する |
| `docs_check.links` | `README.md` と `docs/*.md` | 相対リンクの先のファイルかディレクトリが存在する |
| `docs_check.notices` | `THIRD_PARTY_NOTICES`、`licenses/`、`src/` | 索引の書式（各素材が `ID`、`Name`、`Version`、`Source`、`License`、`License-File`、`Used-In` をこの順に持つ）、`License-File` と `Used-In` のファイルの存在、`Used-In` の各ファイルの ID の行、`licenses/` に索引にないファイルがないこと、`src/` の `.c` と `.h` の ID の行が索引にあること（「[取り込む素材を追加・更新・削除するとき](#取り込む素材を追加更新削除するとき)」） |

マーカーの書き方の約束は次のとおりです。

- マーカーの次の行（空行を挟まない）を、`` ``` `` で始まるコードブロックの開始にします。
- `help` のマーカーは `docs/commands.md` に置き、全体の使い方（サブコマンドなし）と 4 つのサブコマンドの分があります。
- `quickstart` のマーカーは `README.md` にちょうど 1 つだけ置きます。コマンド列の各行は `monde` で始め、パイプ、リダイレクト、`;`、`&` を使いません（検査で実行できないため失敗になります）。`#` で始まる行と空行は実行しません。
- `docs_check.links` は `docs/` 直下の `.md` だけを調べ、`docs/dev/` と `docs/inputs/` は調べません。この文書のリンクは手で確かめます。

### ヘルプの引用の更新

`docs/commands.md` の `help` のマーカーの直後のコードブロックは、`--help` の標準出力をバイト単位でそのまま写したものでなければなりません（比べる前に CRLF を LF にそろえる以外の正規化はしません）。行の折り返し、空白、空行も含めて一致させます。手で書き直さず、ビルドした `monde` の出力から写します。

```sh
./build/src/monde/monde --help
./build/src/monde/monde extract --help
./build/src/monde/monde init --help
./build/src/monde/monde update --help
./build/src/monde/monde translate --help
```

出力をそれぞれ対応するマーカーの直後のコードブロックの中身と置き換え、`ctest -L docs` で `docs_check.help` が通ることを確かめます。引用の後の補足（オプションの説明など）は、引用と矛盾しないように合わせて直します。

### CLI の仕様が変わったときに直す場所

monde の CLI の仕様（サブコマンド、オプション、位置引数、統計の書式、終了コード、診断の文面）を変えるときは、次の記述を同じ PR で更新します。リリースの PR にまとめて後回しにしません。

- `docs/commands.md`: `help` のマーカーの直後のヘルプの引用（上の手順で写し直す）と、各サブコマンドの補足、「統計の表示」の書式の表、「診断の表示と終了コード」、出力先のファイルの扱い。
- `README.md`: サブコマンドの一覧の表と、クイックスタートのコマンド列とその説明。
- `docs/workflow.md`: 各段階のコマンドの例と、統計の行の書式と読み方の説明。
- `docs/limitations.md`: 対応範囲が変わったときの既知の制限の記述。
- `docs/examples/`: クイックスタートで使う `hello.md` と `hello.ja.po`（抽出する文字列が変わったときは `hello.ja.po` の訳を合わせる）。

更新した後は `ctest -L docs` を実行し、4 つの検査がすべて通ることを確かめます。互換性を壊す変更であれば、次のリリースで上げる桁を「[バージョン番号の規則](#バージョン番号の規則)」に従って決めます。

## 取り込む素材を追加・更新・削除するとき

`monde` の実行ファイルに第三者の素材（生成したデータの表、取り込んだソース、静的にリンクするライブラリ）を取り込むときは、その表示を同じ変更（同じ PR）で `THIRD_PARTY_NOTICES` と `licenses/` に加えます。配布物にはこの 2 つが入り、利用者はそこで素材とライセンスを確かめます。テスト専用で配布物に含まれないもの（`tests/data/commonmark/spec.txt`、Naos）は載せません。

### 追加する

1. **ライセンスを確かめる。** 取り込めるのは、許容型のライセンス（MIT、BSD、`Unicode-3.0` など）の素材だけです。コピーレフトのライセンス（GPL、CC-BY-SA など。`spec.txt` は CC-BY-SA 4.0）の素材は、テスト専用にとどめ、実行ファイルに取り込みません。実行時の外部依存（共有ライブラリなど）はゼロのままにします。素材は静的に取り込み、実行時に別のファイルやライブラリを必要としないようにします。
2. **索引に塊を加える。** リポジトリの直下の `THIRD_PARTY_NOTICES` に、空行で区切って次の 7 行の塊を加えます。キーはこの順にすべて書きます。`#` で始まる行はコメントです。

   ```text
   ID: <ID>
   Name: <素材の名称>
   Version: <版>
   Source: <出典の URL>
   License: <SPDX のライセンス識別子>
   License-File: licenses/<ID>.txt
   Used-In: <取り込んだ先のファイル>, <取り込んだ先のファイル>
   ```

   - `ID` は小文字の英数字と `-`（`^[a-z0-9][a-z0-9-]*$`）で、索引の中で一意にします。
   - `Used-In` はリポジトリのルートからのパスを `, `（カンマと空白 1 つ）で区切って並べます。

3. **ライセンスのファイルを加える。** `licenses/<ID>.txt` を作り、先頭に `Name:`、`Version:`、`Source:`、`SPDX-License-Identifier:`、`Copyright:` の行、空行の後にライセンスの全文を原文のまま置きます（UTF-8、LF）。既存の `licenses/unicode-ucd.txt` が例です。ライセンスの適用に説明が要るとき（標準そのものと、ソースコードに取り込んだ部分でライセンスが異なるなど）は、全文の前に説明を書きます（`licenses/whatwg-entities.txt` が例です）。`licenses/` には索引の `License-File` に載せたファイルだけを置きます（`cmake/MondePackage.cmake` はディレクトリごと配布物に入れます）。
4. **取り込んだ先のファイルに ID の行を書く。** `Used-In` の各ファイルの先頭のコメントに、次の形の行を 1 行置きます。

   ```c
   /*
    * THIRD_PARTY_NOTICES: <ID>
    */
   ```

   検査が受け付けるのは、` * ` などのコメントの記号の後に `THIRD_PARTY_NOTICES: <ID>` が続き、ID の後に何もない（行末の空白を除く）行だけです。1 行のコメント `/* THIRD_PARTY_NOTICES: <ID> */` は、ID の後に ` */` が続くため書式の誤りとして失敗します。説明を添えるときは次の行に書きます。生成したファイルでは、生成スクリプトがこの行を出力するようにします（`tools/gen_md_ucd.py`、`tools/gen_po_ucd.py`、`tools/gen_md_entities.py` の `NOTICES_ID` が例です）。手で書き足すと、再生成で消えます。
5. **検査を実行する。** テストを有効にした構成のビルドディレクトリで `ctest -L docs` を実行し、`docs_check.notices` を含む 4 つの検査が通ることを確かめます。

   ```sh
   cmake --build build --config Release
   ctest --test-dir build -C Release -L docs --output-on-failure
   ```

6. **配布物を確かめる。** 「[手元での再現](#手元での再現)」の手順でアーカイブを作り、`-DNOTICES=THIRD_PARTY_NOTICES` を付けた `verify_package.cmake` で、新しい `licenses/<ID>.txt` がアーカイブに入っていることを確かめます。`cmake/MondePackage.cmake` と `verify_package.cmake` は直しません（同梱物は索引から決まります）。

### 更新する

素材の版を上げたとき（取り込んだ先のファイルを作り直したとき）は、同じ変更で索引の `Version`、`Source`（URL に版を含むとき）と、`licenses/<ID>.txt` の先頭の行を直します。ライセンスの文面や著作権表示の年が変わっていれば、全文も新しい版の原文に置き換えます。取り込んだ先のファイルが増減したときは `Used-In` を合わせます。最後に追加のときの手順 5 と 6 を実行します。

例として、Unicode Character Database を 15.0.0 から新しい版に上げるときは次のようにします。

1. `tools/gen_md_ucd.py` と `tools/gen_po_ucd.py` の `UNICODE_VERSION` と、版を含む説明を新しい版に合わせ、新しい版のデータファイルから `src/markdown/md_unicode_tables.c` と `src/po/po_ucd_tables.c` を生成し直します（再生成のコマンドは各ファイルの先頭にあります）。生成したファイルに ID の行 ` * THIRD_PARTY_NOTICES: unicode-ucd` が残っていることを確かめます。
2. `THIRD_PARTY_NOTICES` の `unicode-ucd` の塊の `Version` と `Source`（`https://www.unicode.org/Public/<版>/ucd/`）を直します。
3. `licenses/unicode-ucd.txt` の `Version:`、`Source:`、`Copyright:` の行と、新しい版に付いている `license.txt` の全文に置き換えます。
4. README の「サードパーティ」の表の版を直し、`ctest`（すべてのテスト）と配布物の確認を実行します。

### 削除する

素材を取り込まなくなったときは、同じ変更で次の 3 つをまとめて消します。どれかが残ると `docs_check.notices` が失敗します。

- `THIRD_PARTY_NOTICES` のその素材の塊
- `licenses/<ID>.txt`
- 取り込んでいたファイルの ID の行（ファイルごと消すときはファイル）

README の「サードパーティ」の表からもその行を消します。
