# Yet another MySQL normalizer plugin for Groonga

このノーマライザープラグインは、``NormalizerMySQLUnicodeCI``ノーマライザーの正規化ルールをカスタマイズしています。
ノーマライザープラグインの登録には、``register``コマンドを利用します。

```
register normalizers/yamysql
```

## Normalizer

### ``NormalizerYaMySQLUnicodeCI``

原則、``NormalizerMySQLUnicodeCI``の正規化ルールで文字列が正規化されます。

``yamysql_register``コマンドを使うことにより、以下のルールを任意に組み合わせたノーマライザーを追加することができます。

### ``ExpectKata``

* カタカナをひらがなに正規化せず、カタカナのままにする。

### ``ExepectKanaWithVoicedSoudMark``

* 濁音カタカナひらがなを正規化せず、濁音付のままにする。

### ``Machine``

* Linuxでは文字化けが生じる以下の機種依存文字を正規化する。

    > DOUBLE_VERTICAL_LINE       0x02016  
    > WAVE_DASH       0x0301c

### ``Rt``

* 改行コード(CR,LF)を半角スペースに正規化する

    > CARRIAGE_RETURN      0x0000D  
    > LINE_FEED      0x0000A

### ``Hyphen``

* ハイフン、ダッシュ、長音記号等を長音記号(0x030fc)に正規化する

    > Hyphen-minus     0x0002d  
    > Hyphen     0x02010  
    > Non-breaking Hyphen     0x02011  
    > Figure Dash     0x02012  
    > En Dash     0x02013  
    > Em Dash     0x02014  
    > Horizontal Bar     0x02015  
    > Hyphen Bullet     0x02043  
    > Minus Sign     0x02212  
    > Katakana-Hiragana Prolonged Sound Mark     0x030fc  
    > Halfwidth Katakana-hiragana Prolonged Sound Mark     0x0FF70

以下、全て未実装

### ``ExceptKanaCI``

* ひらがなカタカナ小文字は大文字に正規化しない。

### ``VariantKanji``

* 常用漢字、数字漢字、人名用漢字の異体字を正規化する。

https://github.com/cjkvi/cjkvi-variants/blob/master/joyo-variants.txt  
https://github.com/cjkvi/cjkvi-variants/blob/master/numeric-variants.txt  
https://github.com/cjkvi/cjkvi-variants/blob/master/jinmei-variants.txt  

### ``VariantKatakana``

* 以下のカタカナの異体字を正規化する。

ヂ/ジ ヅ/ズ  
ヴァ/バ ヴィ/ビ ヴェ/ベ ヴォ/ボ  
ヴ/ブ  
ヱ/エ  
ヰ/イ

### ``VariantHiragana``

* 下のひらがなの異体字を正規化する。

ぢ/じ づ/ず  
ゑ/え  
ゐ/い

### ``PreFilterHtml``

* HTMLタグを除去する。

### ``PreFilterHtml``

* HTMLタグを除去する。

### ``PreFilterSymbol``

* 記号を除去する。

### ``PreFilterMecabPartofspeech``

* Mecabで分かち書きをし、所定の品詞を除去する。

### ``PreFilterMecabStopwords``

* Mecabで分かち書きをし、所定の単語を除去する。

### ``PreFilterMecabProlong``

* Mecabで分かち書きをし、4文字以上のカタカナの末尾の長音記号を削除する。

PreFilterは、ノーマライズ前に文字列を除去することによりトークナイズ時にもスニペット時にも有効になります。  
トークナイザーでフィルターすると検索対象の文字列とスニペット対象の文字列が異なりますが、ノーマライザーの中でchecksをつけながら文字列を除去することにより、適正なスニペットを行うことができます(未検証)。

## Install

### Source install

Build this normalizer.

    % sh autogen.sh
    % ./configure
    % make
    % sudo make install

## Dependencies

* Groonga >= 4.0.5

Install ``groonga-devel`` in CentOS/Fedora. Install ``libgroonga-dev`` in Debian/Ubutu.

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
    >   --normalizer NormalizerYaMySQLUnicodeCI \
    > column_create Terms diaries_body COLUMN_INDEX|WITH_POSITION Diaries body

Mroonga:

    mysql> use db;
    mysql> select mroonga_command("register normalizers/yamysql");
    mysql> CREATE TABLE `Diaries` (
        -> id INT NOT NULL,
        -> body TEXT NOT NULL,
        -> PRIMARY KEY (id) USING HASH,
        -> FULLTEXT INDEX (body) COMMENT 'parser "TokenBigram", normalizer "NormalizerYaMySQLUnicodeCI'
        -> ) ENGINE=mroonga DEFAULT CHARSET=utf8;

Rroonga:

    irb --simple-prompt -rubygems -rgroonga
    >> Groonga::Context.default_options = {:encoding => :utf8}   
    >> Groonga::Database.create(:path => "/tmp/db")
    >> Groonga::Plugin.register(:path => "/usr/lib/groonga/plugins/normalizers/yamysql.so")
    >> Groonga::Schema.create_table("Diaries",
    ?>                              :type => :hash,
    ?>                              :key_type => :integer32) do |table|
    ?>   table.text("body")
    >> end
    >> Groonga::Schema.create_table("Terms",
    ?>                              :type => :patricia_trie,
    ?>                              :normalizer => :NormalizerYaMySQLUnicodeCI,
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
