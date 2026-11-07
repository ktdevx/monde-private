# Research & Design Decisions: translation-units

## Summary
- **Feature**: `translation-units`
- **Discovery Scope**: Extension（既存の 3 部品 markdown-parser、yaml-front-matter、po-format の上に Monde 固有の処理を載せる。統合中心の軽量な調査）
- **Key Findings**:
  - 3 つの上流部品はすべて実装済みで、公開ヘッダは各 design.md と一致する。translation-units が必要とする情報（行ごとの `prefix` / `content`、インラインの範囲、スカラーの範囲と論理値、整形関数、エントリの型）はすべてそろっており、上流の変更は不要である。
  - markdown-parser は「どのコンテナがどのバイトを使ったか」の内訳を返さない。2 行目以降のプレフィックスは、原文の行があればその `prefix` をそのまま再利用でき、足りない行だけを `content_indent` と引用の有無から組み立てればよい。
  - yaml-front-matter の整形は「論理値が同じになる」ことだけを保証し、バイト単位の同一性は保証しない（折り返しスカラー、複数行のプレーンスカラー、`\u` エスケープなど）。したがって「msgstr == msgid でバイト単位に同じ」（5.2）は、訳文が msgid と同じなら置換しないという短絡で満たす。Markdown の書き戻し経路はそれ自体でバイト単位の同一性を満たすように設計し、テストで短絡なしの経路も検証する。
  - po-format の `mnd_po_catalog_find` は線形探索で、カタログの文字列は NUL を含まない UTF-8 であることを前提にする。本部品は訳文の UTF-8 と NUL を検査してから使う。

## Research Log

### markdown-parser の境界（行・範囲・インライン）
- **Context**: 要件 2.1〜2.5、4.8、5.2〜5.4 を満たすために、パーサが返す範囲の正確な意味を確かめる。
- **Sources Consulted**: `src/markdown/md.h`、`md_parse.h`、`md_block.c`（`leaf_add_line`、`start_list_item`、`line_advance_to_nonspace`）、`.kiro/specs/markdown-parser/design.md` の「範囲の規則」、`research.md` の行の内容範囲の決定。
- **Findings**:
  - リーフ（段落・見出し）は `lines[]` を持ち、各行は `prefix = [行頭, content.start)`、`content = [content.start, line_end)`（行末記号を含まない）、`eol_len`、`pad`、`lazy`、`line` を持つ。`prefix.end == content.start` で、行の全バイトは `prefix + content + eol` で尽きる。
  - 段落と setext 見出しの行の内容は、行頭の空白を除いた位置から始まり、末尾の空白を含む。ATX 見出しは 1 行で、`content == heading.content`（開きと閉じの `#` と前後の空白を除く）。
  - `pad` は、タブの一部だけがプレフィックスとして使われた場合に内容の前に補う仮想の空白の数で、インデントコードブロックのための仕組みである。段落と見出しは `line_advance_to_nonspace` で残りの空白（タブの仮想の空白を含む）をすべて消費してから開くため、`pad == 0` である。
  - `item.content_indent` は、そのリスト項目を囲むコンテナの内容の桁を 0 とした、項目の内容の開始の桁（`indent + 記号の幅 + 記号の後ろの空白`）。引用は `>` とその後ろの空白 1 つを使う。
  - 遅延継続行（`lazy`）のプレフィックスは行頭の空白だけで、`>` を含まない。
  - `mnd_md_logical_text(doc, leaf, span, out)` は、行のプレフィックスを飛ばし、`pad` の数の空白を補い、行の境目を `"\n"` にして論理的な文字列を返す。段落・見出しでは `pad == 0` なので、結果は「各行の内容を `\n` で連結したもの」に等しい。
  - 行番号は 1 起点で、LF と CRLF を 1 つの改行と数え、単独の CR は数えない（`mnd_source_line_of` と同じ）。CommonMark は単独の CR も行末として扱うため、1 つの行番号を複数の CommonMark の行が共有することがある。
  - 方言の種類のノード（`kind >= MND_MD_KIND_DIALECT`）は木に現れ得る。CommonMark 方言では現れない。
  - パーサの境界外として明記されているもの: どのノードを翻訳単位にするか、msgid の作り方、URL のプレースホルダ化、プレフィックスの復元、出力のエスケープ。
- **Implications**:
  - 翻訳セグメントの範囲は `[lines[0].content.start, 最後の行の content.end から末尾の空白を除いた位置)`。msgid はこの範囲の `mnd_md_logical_text`。
  - 書き戻しでは、訳文の k 行目（k ≥ 1）の前に、原文の k 行目があれば「その行の直前の行末記号のバイト列 + `lines[k].prefix` のバイト列」をそのまま出力する。これにより、遅延継続行・CRLF と LF の混在・タブを含むプレフィックスのすべてで、msgstr == msgid のときにバイト単位で同一になる。
  - 原文より多い行には、祖先のコンテナから組み立てたプレフィックスと、呼び出し側の改行コードを使う。

### yaml-front-matter の境界（検出・照合・整形）
- **Context**: 要件 1.8〜1.12、2.6、2.7、4.10、5.2、7.2、7.3。
- **Sources Consulted**: `src/yaml/yaml.h`、`yaml_front_matter.h`、`yaml_parse.h`、`yaml_path.h`、`yaml_format.h`、`.kiro/specs/yaml-front-matter/design.md` の「利用側」「System Flows」「改訂」、`brief.md` の制約。
- **Findings**:
  - `mnd_yaml_front_matter_detect` はメモリを確保せず、`present`、`span`、`content`、`body_start` を返す。`present == false` なら `body_start == 0`。結果に行番号はなく、`mnd_source_line_of` で求める。
  - `mnd_yaml_parse(data, len, content, ...)` は文書全体と内容の範囲を受け取り、オフセットと行番号は文書全体の先頭を基準にする。失敗時は 1 つのエラーの診断と `*out == NULL`。
  - `mnd_yaml_path_find` は原文の順のノードの列を返し、スカラー以外も含む。該当なしは `count == 0` で成功。不正なキーパス（空、空のセグメント）はエラー。結果は文書の確保関数で確保するため、`mnd_yaml_matches_free` を `mnd_yaml_doc_free` より前に呼ぶ。
  - スカラーの `value` は論理値（UTF-8、改行は LF、NUL 終端）。ブロックスカラーの clip は末尾に `\n` を 1 つ、keep は複数残す。先頭の空行がある `|` の値は `\n` で始まる。
  - `mnd_yaml_format_value` の事後条件は「置き換えて解析し直すと論理値が一致する」ことで、バイト単位の同一性ではない。元のスタイルを維持できないときはダブルクオートに切り替える。空の範囲への出力は区切り（`" "` または `": "`）から始まる。ブロックスカラーの末尾の改行の数が chomp と合わないときもダブルクオートになる。
  - `mnd_yaml_doc_eol(doc)` はフロントマターの内容の支配的な改行コードで、複数行の整形に使われる。
- **Implications**:
  - フロントマターの msgid は、論理値から先頭と末尾の `\n` を取り除いたもの。取り除いた数を記録し、書き戻しの前に同じ数の `\n` を戻してから整形に渡す。これで chomp の判定が元のままになり、`msgfmt --check` の「msgid と msgstr の一方だけが改行で始まる・終わる」の警告も避けられる（2.7、2.8）。
  - 5.2 は、フロントマターについては整形関数では保証できないため、Rebuilder の短絡（訳文が msgid と同じなら置換しない）で満たす。
  - 検出と解析は translation-units が順に呼ぶ。キーパスの指定がないときは解析を呼ばず、`body_start` だけを markdown-parser に渡す（1.9）。

### po-format のエントリの型とカタログの検索
- **Context**: 要件 3.x、4.3〜4.7、6.x、7.4。
- **Sources Consulted**: `src/po/po.h`、`.kiro/specs/po-format/design.md`（不変条件、状態の判定、ライターの重複の検査）、`.kiro/specs/po-merge/requirements.md`（1.2、2.2、3.6）。
- **Findings**:
  - `mnd_po_entry_t` は `extracted_comments`（1 行 1 要素）、`refs`（ファイル名と行番号の組の列）、`flags`、`msgid`、`msgstr`（単数形は 1 個）、`obsolete`、`line`（読み込み元の行番号）を持つ。文字列はすべて NUL を含まない UTF-8 で、エスケープはライターが行う。
  - ヘッダは「msgctxt のない空の msgid の、最初の廃止エントリでないエントリ」。空の msgid はヘッダと衝突する。
  - `mnd_po_catalog_find(cat, NULL, msgid, &index)` は msgctxt のないエントリだけを、廃止エントリを除いて線形に探す。`mnd_po_entry_state` は、fuzzy フラグがあり最初の msgstr が空でなければ FUZZY、フラグがなくすべての msgstr が空でなければ TRANSLATED、それ以外は UNTRANSLATED。
  - ライターは、廃止エントリでないエントリに同じ（msgctxt, msgid）があれば何も書かずに失敗する。po-merge は、テンプレートの並び順を更新後の `.po` の順にし、`#.` と `#:` を常にテンプレートから取る。
- **Implications**:
  - 抽出は msgid で重複を統合してから `mnd_po_catalog_append` する（3.2）。並びは最初の出現の順（3.3）。
  - 再構成の検索は `mnd_po_catalog_find` をそのまま使う。性能目標はないが、セグメントの数 × エントリの数の線形探索になる。一般的な文書（数百セグメント × 数千エントリ）では問題にならない。将来必要になれば、msgid をキーにした索引を Rebuilder の内部に足せる（公開 API は変わらない）。
  - 訳文の UTF-8 と NUL の検査は `mnd_utf8_validate` と長さの比較で行い、エラーには呼び出し側が渡した翻訳ファイルのパスと `entry->line` を添える（7.4）。

### テスト資産の再利用
- **Context**: 要件 5.3（CommonMark spec の全ケース）と 3.9（`msgfmt --check`）。
- **Sources Consulted**: `tests/markdown/spec_reader.h`、`tests/markdown/CMakeLists.txt`、`tests/po_merge/CMakeLists.txt`、`cmake/MondeTargets.cmake`。
- **Findings**:
  - `tests/markdown/spec_reader.c` は spec.txt から 652 例を取り出す。テスト実行ファイル専用で、製品ライブラリには含めない。`MND_MD_SPEC_FILE` の定義でパスを渡す。
  - `monde_add_module_tests` は `SOURCES` に任意のパスを受け付け、`DEFINITIONS` でコンパイル定義を渡せる。include ディレクトリは `src/` だけが公開される。
  - po-merge は `find_program(msgmerge)` で外部ツールを探し、ある環境でだけ CTest に登録する。
- **Implications**:
  - `tests/tu/` は `${PROJECT_SOURCE_DIR}/tests/markdown/spec_reader.c` を SOURCES に加え、`#include "../markdown/spec_reader.h"` で使う（複製しない）。
  - `msgfmt` のある環境でだけ、抽出結果を書き出して `msgfmt --check` に通すテストを登録する（po-merge と同じ方式）。

### 外部の実装の慣例（po4a、md2po）
- **Context**: URL とインライン記法を msgid に含める方針の妥当性の確認（要件確認で決定済み）。
- **Sources Consulted**: po4a の Markdown モジュール（Locale::Po4a::Text の markdown オプション）と mdpo の既知の挙動（訓練知識。設計を左右しないため再検証はしない）。
- **Findings**: どちらもインライン記法と URL を msgid にそのまま含め、ブロック単位で 1 エントリにする。リンク参照定義は po4a では翻訳単位にしない。
- **Implications**: 本設計の方針は既存ツールの慣例と一致し、翻訳者の学習コストが低い。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| 共通のセグメント列 + 抽出器 + 再構成器（採用） | 文書を 1 回走査して「翻訳セグメント」の列を作り、抽出と再構成はその列の上だけで動く | 抽出と差し戻しの規則が 1 か所（Scanner）に集まり、食い違いが構造的に起きない。セグメントの列を公開するので統計や将来の msgctxt に使える | 走査の結果を保持するため、文書とセグメントのライフサイクル管理が必要 | brief の Approach に一致 |
| 抽出と再構成を独立に実装 | それぞれが構文木を直接歩く | 部品が小さい | 判定と正規化の規則が 2 か所に分かれ、5.x のラウンドトリップが壊れやすい | 不採用 |
| 構文木から Markdown を再生成 | 訳文を木に入れて書き出す | 構造の保護が容易 | Q4-1 のバイト単位の保持を満たせない | steering で却下済み |

## Design Decisions

### Decision: モジュール名を `tu`、短縮名を `tu` にする
- **Context**: steering は「モジュール名は spec 名の短い形」とし、外部に見える名前に `mnd_<短縮名>_` を付ける。`translation_units` は長く、`unit` はテストの用語と紛らわしい。
- **Alternatives Considered**:
  1. `translation_units` / `mnd_tu_`
  2. `units` / `mnd_unit_`
  3. `tu` / `mnd_tu_`
- **Selected Approach**: 3。ディレクトリ `src/tu/`、`tests/tu/`、ターゲット `monde::tu`、接頭辞 `mnd_tu_`、ヘッダガード `MONDE_TU_*`。
- **Rationale**: 「tu」は翻訳業界で translation unit の標準的な略（TMX の `<tu>` 要素）で、短く、既存の短縮名と衝突しない。
- **Trade-offs**: 初見では意味が取りにくい。ヘッダの先頭のコメントで説明する。
- **Follow-up**: steering の structure.md に予約済みの短縮名 `tu` を追記する。

### Decision: 走査（Scanner）を公開し、抽出と再構成はセグメントの列の上で動かす
- **Context**: brief の Approach と、要件 3.8（出現位置と種類を参照できる）、4.12（判定と正規化の同一性）。
- **Alternatives Considered**:
  1. `mnd_tu_extract` と `mnd_tu_translate` だけを公開し、走査は内部に隠す
  2. 走査の結果（`mnd_tu_doc_t`）を公開し、抽出と再構成はそれを受け取る
- **Selected Approach**: 2。`mnd_tu_scan` が文書を解析してセグメントの列を作り、`mnd_tu_extract` と `mnd_tu_translate` はそれを入力にする。
- **Rationale**: 判定と正規化の経路が 1 本になり、4.12 が構造で保証される。monde-cli は走査の結果からセグメント数を知ることができ、将来の msgctxt の付与もセグメントの属性の追加で済む。
- **Trade-offs**: monde-cli の呼び出しが 1 段増える（scan → extract / translate → doc_free）。
- **Follow-up**: `mnd_tu_doc_t` は原文のバッファを指し続けるため、呼び出し側がバッファを保持する責任を API のコメントに明記する。

### Decision: 原文の行があるかぎり、その行の行末記号とプレフィックスをそのまま再利用する
- **Context**: 要件 4.8、5.2、5.4。パーサは「どのコンテナがどのバイトを使ったか」の内訳を返さない。
- **Alternatives Considered**:
  1. すべての継続行のプレフィックスを祖先のコンテナから組み立てる
  2. 原文の k 行目があればその `prefix` と直前の行末記号を再利用し、足りない行だけ組み立てる
- **Selected Approach**: 2。
- **Rationale**: 遅延継続行（`>` がない）、CRLF と LF の混在、タブを含むインデントでも、msgstr == msgid のときにバイト単位で同一になる。組み立てが必要なのは訳文の行数が原文より多いときだけで、そのときは意味の等しいプレフィックスで十分である。
- **Trade-offs**: 原文の k 行目が遅延継続行なら、訳文の k 行目も `>` なしで出る。CommonMark の解釈は同じ段落の継続になるので問題ない。
- **Follow-up**: 組み立てるプレフィックスは「最も内側の祖先の規則の連結」とし、段落の直前の行のプレフィックスが遅延継続行でなければそれを優先して再利用する（見た目の一貫性のため）。

### Decision: 訳文が msgid と同じなら置換しない（短絡）
- **Context**: 要件 5.2。yaml-front-matter の整形は論理値の一致しか保証しない。
- **Alternatives Considered**:
  1. yaml-front-matter にバイト保持の整形モードを追加する（上流の再検証が必要）
  2. 訳文が msgid と等しいセグメントは原文のバイト列をそのまま出力する
- **Selected Approach**: 2。Markdown とフロントマターの両方に適用する。
- **Rationale**: 上流を変えずに 5.2 を満たせる。訳文が原文と同じなら差分が出ないのは利用者の期待にも合う。
- **Trade-offs**: Markdown の書き戻し経路は 5.2 だけでは検証されなくなる。これを補うため、テストでは短絡を無効にした内部関数（`mnd_tu_splice_markdown`）を spec.txt の全例に対して直接呼び、バイト単位の同一性を確かめる。
- **Follow-up**: 統計では、短絡したセグメントもカタログの状態（翻訳済みなど）で数える（短絡は出力の最適化で、状態には影響しない）。

### Decision: フロントマターの msgid から先頭と末尾の改行を取り除く
- **Context**: 要件 2.7、2.8。ブロックスカラーの clip / keep と、先頭の空行。
- **Selected Approach**: 論理値の先頭と末尾の `\n` の数を数えて取り除き、セグメントに記録する。書き戻しでは、訳文の前後に同じ数の `\n` を付けてから `mnd_yaml_format_value` に渡す。取り除いた結果が空か空白だけなら、セグメントにしない（1.12）。
- **Rationale**: 翻訳者が改行の有無を気にせずに済み、`msgfmt --check` の警告を避け、chomp の判定が元のままになるので元のスタイルが維持される。
- **Trade-offs**: 訳文に翻訳者が末尾の改行を加えると、整形でダブルクオートに切り替わる可能性がある（整形の規則どおり）。

### Decision: 構造の検査は警告のみで、固定の検査項目の表で定義する
- **Context**: 要件 4.11。要件確認で「警告して、そのまま出力」と決定。
- **Selected Approach**: 訳文を行に分け、空行、ブロックを開始し得る行頭（ATX 見出し、引用、リスト記号、フェンス、区切り線、setext の下線）、ATX 見出しへの改行を検査する。検出したら 1 セグメントにつき 1 つの警告を返し、出力は変えない。
- **Rationale**: 過検出は許容する（警告だけで出力は変わらない）。検査項目を表で固定することで、テストと利用マニュアルの記述が一致する。
- **Trade-offs**: HTML ブロックの開始やインデントコードブロック（4 桁の空白）は検査しない。前者は過検出が多すぎ、後者は段落の継続行では起きない。

### Decision: 統計は再構成の出力と同時に、出現ごとに 4 状態 + 合計で返す
- **Context**: 要件 6.x。要件確認で「出現ごと」と決定。
- **Selected Approach**: `mnd_tu_stats_t { total, translated, fuzzy, untranslated, missing }`。fuzzy は採用の有無にかかわらず fuzzy。
- **Rationale**: 率の定義（fuzzy を分子に入れるか等）は monde-cli に委ねる。

## Synthesis Outcomes
- **一般化**: 「原文の範囲 + 論理テキスト + 書き戻し方法」という 1 つのセグメントの型で、Markdown の段落・見出しとフロントマターの値の両方を表す。書き戻し方法の違いは `origin`（Markdown / フロントマター）で分岐する 2 つの Splicer にとどめる。
- **Build vs. Adopt**: 構文解析・整形・PO の表現はすべて既存の部品を採用し、本部品は判定・正規化・置換の規則だけを持つ。外部ライブラリは追加しない（steering の制約）。
- **簡素化**: 方言インターフェース、msgctxt、プレースホルダなどの将来の拡張のための抽象は入れない。セグメントの構造体に `kind_label` と `line` を持たせておくだけで、msgctxt の付与は属性の追加で済む。カタログの索引（ハッシュ）も、性能目標がないため入れない。

## Risks & Mitigations
- 書き戻しの経路のバイト同一性が崩れる細部（タブ、単独の CR、末尾の空白）— spec.txt の全例に対して短絡なしの経路を直接テストし、CRLF 変換版と末尾改行なし版も加える。
- `mnd_po_catalog_find` の線形探索による大きな文書での遅さ — 性能目標はない。問題になったら Rebuilder の内部に msgid の索引を足す（公開 API は不変）。
- 構造の検査の過検出・見逃し — 警告のみで出力に影響しない。検査項目の表を利用マニュアルに転記する。
- 方言の種類のノード（次の版の GFM）— 子を走査するだけで、種類の名前は `#.` に出さない。GFM の表のセルを翻訳単位にするときに本 spec を再検証する。
- spec.txt の例に含まれる単独の CR の行末 — `#:` の行番号は `mnd_source_line_of` と同じ数え方で、1 つの行番号を複数の CommonMark の行が共有することを許容する（行番号は参照用であり、置換には使わない）。

## References
- `.kiro/specs/markdown-parser/design.md` — 範囲の規則、Text の契約
- `.kiro/specs/yaml-front-matter/design.md` — 利用側の流れ、整形の規則、改訂
- `.kiro/specs/po-format/design.md` — エントリの不変条件、状態の判定、ライターの重複の検査
- `.kiro/specs/po-merge/requirements.md` — テンプレートの順序と `#.` / `#:` の取り扱い
- `docs/inputs/qa.md` — Q2-1〜Q2-4、Q3-1、Q4-1〜Q4-3、Q9-1
- GNU gettext manual, "PO Files" / `msgfmt --check` — 改行で始まる・終わる msgid と msgstr の不一致の検査
