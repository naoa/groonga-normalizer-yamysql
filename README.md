# Yet another MySQL normalizer plugin for Groonga

## Normalizer

* ``NormalizerYaMySQL``
* ``NormalizerYaMySQLKanaCI``
* ``NormalizerYaMySQLRemovePhrase``
* ``NormalizerYaMySQLKanaCIRemovePhrase``

原則、``NormalizerMySQLUnicodeCI``の正規化ルールで文字列が正規化されます。それに加え、以下の機能をカスタマイズしています。もはやMySQL関係ありません。

### ``LowercaseAlpha``

* アルファベットを大文字ではなく、小文字に正規化します。

### ``ExceptKatakana``

* カタカナをひらがなに正規化せず、カタカナのままにします。

### ``ExceptKanaWithVoicedSoundMark``

* 濁音カタカナひらがなを正規化せず、濁音付のままにします。

```
normalize NormalizerYaMySQL "データベース"
[[0,0.0,0.0],{"normalized":"データベース","types":[],"checks":[]}]

normalize NormalizerMySQLUnicodeCI "データベース"
[[0,0.0,0.0],{"normalized":"てーたへーす","types":[],"checks":[]}]
```

### ``KanaCI``

* ひらがなカタカナ小文字を大文字に正規化します。

```
normalize NormalizerYaMySQLKanaCI "フィルム"
[[0,0.0,0.0],{"normalized":"フイルム","types":[],"checks":[]}]
```

```
normalize NormalizerYaMySQL "フィルム"
[[0,0.0,0.0],{"normalized":"フィルム","types":[],"checks":[]}]
```

### ``Machine``

* Linuxでは文字化けが生じる以下の機種依存文字を正規化します。

    > DOUBLE_VERTICAL_LINE       0x02016  
    > WAVE_DASH       0x0301c

### ``Return``

* 改行コード(CR,LF)を除去します。

    > CARRIAGE_RETURN      0x0000d  
    > LINE_FEED      0x0000a

### ``Hyphen``

* ハイフン、ダッシュ、長音記号等を長音記号(0x030fc)に正規化します。マイナスは正規化しません。

    > HYPHEN     0x02010  
    > NON-BREAKING HYPHEN     0x02011  
    > FIGURE DASH     0x02012  
    > EN DASH     0x02013  
    > EM DASH     0x02014  
    > HORIZONTAL BAR     0x02015  
    > HYPHEN BULLET     0x02043  
    > MINUS SIGN     0x02212  
    > KATAKANA-HIRAGANA PROLONGED SOUND MARK     0x030fc  
    > HALFWIDTH KATAKANA-HIRAGANA PROLONGED SOUND MARK     0x0ff70

```
normalize NormalizerYaMySQL "デ―タベ‐ス"
[[0,0.0,0.0],{"normalized":"データベース","types":[],"checks":[]}]
```

### ``VariantKatakana``

* 以下のカタカナの異体字を正規化します。

ヂ/ジ ヅ/ズ  
ヴァ/バ ヴィ/ビ ヴェ/ベ ヴォ/ボ  
ヴ/ブ  
ヱ/エ  
ヰ/イ

```
normalize NormalizerYaMySQL "ヴァイオリン"
[[0,0.0,0.0],{"normalized":"バイオリン","types":[],"checks":[]}]
```

### ``VariantHiragana``

* 以下のひらがなの異体字を正規化します。

ぢ/じ づ/ず  
ゑ/え  
ゐ/い

```
normalize NormalizerYaMySQL "おっづ"
[[0,0.0,0.0],{"normalized":"おっず","types":[],"checks":[]}]
```

### ``RemovePhrase``

** 実験中 **

ノーマライズ前に``RemovePhrase``テーブルのキーと一致する文字列を削除します。``*RemovePhrase``トークナイザーを利用し、``remove_phrases``テーブルがあるときのみ有効になります。
特殊機能として、``<remove_html>``をキーに含めておくと``<``と``>``に囲まれる文字列すべてを除去します。``<remove_symbol>``をキーに含めておくと記号すべてを除去します。
パトリシアトライのLCPサーチを利用しているため、``remove_phrases``テーブルは、``TABLE_PAT_KEY``である必要があります。環境変数``GRN_REMOVE_PHRASE_TABLE_NAME``でテーブル名を変更することができます。

ノーマライザーでのフィルター機能は、トークナイザーでフィルターするのに比べ処理負荷が大きくなりますが、検索時とスニペット時で検索ワードが異なることがなく、適切なスニペットを行うことができます。  

この機能は``TokenMecab``や``TokenDelimit``では正常に動作しない。

### Source install

Build this normalizer.

    % sh autogen.sh
    % ./configure
    % make
    % sudo make install

## Dependencies

* Groonga >= 3.0.3

Install ``groonga-devel`` in CentOS/Fedora.
Install ``libgroonga-dev`` in Debian/Ubuntu.  

See http://groonga.org/docs/install.html

## Usage

Firstly, register `normalizers/yamysql`

Groonga:

    % groonga db
    > register normalizers/yamysql
    > table_create Diaries TABLE_HASH_KEY INT32
    > column_create Diaries body COLUMN_SCALAR TEXT
    > table_create Terms TABLE_PAT_KEY ShortText \
    >   --default_tokenizer TokenBigram \
    >   --normalizer NormalizerYaMySQL \
    > column_create Terms diaries_body COLUMN_INDEX|WITH_POSITION Diaries body

Mroonga:

    mysql> use db;
    mysql> select mroonga_command("register normalizers/yamysql");
    mysql> CREATE TABLE `Diaries` (
        -> id INT NOT NULL,
        -> body TEXT NOT NULL,
        -> PRIMARY KEY (id) USING HASH,
        -> FULLTEXT INDEX (body) COMMENT 'parser "TokenBigram", normalizer "NormalizerYaMySQL'
        -> ) ENGINE=Mroonga DEFAULT CHARSET=utf8;

Rroonga:

    irb --simple-prompt -rubygems -rgroonga
    >> Groonga::Context.default_options = {:encoding => :utf8}   
    >> Groonga::Database.create(:path => "/tmp/db")
    >> Groonga::Plugin.register(:path => "normalizers/yamysql.so")
    >> Groonga::Schema.create_table("Diaries",
    ?>                              :type => :hash,
    ?>                              :key_type => :integer32) do |table|
    ?>   table.text("body")
    >> end
    >> Groonga::Schema.create_table("Terms",
    ?>                              :type => :patricia_trie,
    ?>                              :normalizer => :NormalizerYaMySQL,
    ?>                              :default_tokenizer => "TokenBigram") do |table|
    ?>   table.index("Diaries.body")
    >> end
    
## Author

* Naoya Murakami <naoya@createfield.com>

## License

LGPLv2 only. See COPYING for details.

This is programmed based on the Groonga MySQL normalizer.  
https://github.com/groonga/groonga-normalizer-mysql

This program is the same license as Groonga MySQL normalizer.

The Groonga MySQL normalizer is povided by ``Copyright(C) 2013-2014 Kouhei Sutou <kou@clear-code.com>``

This program include normalization table defined in MySQL source code. So this program is deriverd work of ``MYSQL_SOURCE/strings/ctype-utf8.c`` . This program is the same license as ``MYSQL_SOURCE/strings/ctype-utf8.c`` and it is licensed under LGPLv2 only.
