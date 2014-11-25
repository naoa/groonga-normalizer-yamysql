# Yet another MySQL normalizer plugin for Groonga

## Normalizer

* ``NormalizerYaMySQL``
* ``NormalizerYaMySQLKanaCI``

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

### ``VariantKanji``

* 以下の漢字の異体字を正規化します(0x0fffffまでのみ)。

http://kanji-database.sourceforge.net/variants/search.html  
http://kanji-database.sourceforge.net/variants/variants.txt

```
normalize NormalizerYaMySQL "高髙崎﨑亜亞悪惡圧壓囲圍医醫為爲壱壹隠隱栄榮営營衛衞駅驛円圓塩鹽艶艷応應欧歐殴毆桜櫻奥奧横橫穏穩仮假価價画畫会會絵繪壊壞懐懷拡擴殻殼覚覺学學岳嶽楽樂缶罐巻卷陥陷勧勸寛寬関關歓歡観觀気氣帰歸亀龜偽僞戯戲犠犧旧 舊拠據挙擧峡峽挟挾狭狹郷鄕暁曉区區駆驅勲勳薫薰径徑茎莖恵惠渓溪経經蛍螢軽輕継繼鶏鷄芸藝欠缺県縣倹儉剣劍険險圏圈検檢献獻 権權顕顯験驗厳嚴広廣効效恒恆鉱鑛号號国國黒黑砕碎済濟斎齋剤劑雑雜参參桟棧蚕蠶惨慘賛贊残殘糸絲歯齒児兒辞辭湿濕実實写寫釈 釋寿壽収收従從渋澁獣獸縦縱粛肅処處緒緖諸諸叙敍将將祥祥称稱焼燒証證奨奬条條乗乘浄淨剰剩畳疊縄繩壌壤嬢孃譲讓醸釀触觸嘱囑 神神真眞寝寢慎愼尽盡図圖粋粹酔醉穂穗随隨髄髓枢樞数數瀬瀨声聲斉齊静靜窃竊摂攝専專浅淺戦戰践踐銭錢潜潛繊纖禅禪双雙壮壯争 爭荘莊捜搜挿插曽曾装裝総總騒騷増增蔵藏臓臟属屬続續堕墮対對体體帯帶滞滯台臺滝瀧択擇沢澤担擔単單胆膽団團断斷弾彈遅遲痴癡 虫蟲昼晝鋳鑄庁廳聴聽勅敕鎮鎭塚塚逓遞鉄鐵点點転轉伝傳都都灯燈当當党黨盗盜稲稻徳德独獨読讀届屆弐貳悩惱脳腦覇霸拝拜廃廢売 賣麦麥発發髪髮抜拔蛮蠻秘祕浜濱瓶甁福福払拂仏佛並竝餅餠辺邊辺邉変變弁辨弁瓣弁辯宝寶豊豐褒襃翻飜万萬満滿黙默弥彌訳譯薬藥 与與予豫余餘誉譽揺搖様樣謡謠来來頼賴乱亂覧覽竜龍隆隆両兩猟獵緑綠塁壘礼禮励勵霊靈齢齡恋戀炉爐労勞郎郞朗朗楼樓湾灣亘亙凜 凛尭堯巌巖晃晄桧檜槙槇猪猪祢禰禄祿穣穰萌萠遥遙亜亞悪惡為爲栄榮衛衞円圓園薗応應桜櫻奥奧横橫価價壊壞懐懷楽樂巻卷陥陷寛寬 気氣偽僞戯戲峡峽狭狹暁曉駆駈勲勳薫薰恵惠鶏鷄芸藝県縣倹儉剣劍険險圏圈検檢顕顯験驗厳嚴広廣恒恆国國黒黑砕碎雑雜児兒湿濕実 實寿壽収收従從渋澁獣獸縦縱緒緖諸諸叙敍将將祥祥焼燒奨奬条條乗乘浄淨剰剩畳疊嬢孃譲讓醸釀神神真眞寝寢慎愼尽盡粋粹酔醉穂穗 瀬瀨斉齊静靜摂攝専專戦戰繊纖禅禪壮壯争爭荘莊捜搜曽曾装裝騒騷増增蔵藏臓臟帯帶滞滯滝瀧単單団團弾彈昼晝鋳鑄庁廳聴聽鎮鎭転 轉伝傳都都島嶋灯燈盗盜稲稻徳德拝拜杯盃売賣髪髮抜拔秘祕富冨福福払拂仏佛峰峯翻飜万萬黙默野埜弥彌薬藥与與揺搖様樣謡謠来來 頼賴覧覽竜龍涼凉緑綠塁壘礼禮郎郞朗朗"
[[0,0.0,0.0],{"normalized":"高高崎崎亜亜悪悪圧圧囲囲医億為為壱一隠隠栄栄営営衛衛駅駅円元 塩塩艶艶応応欧欧殴殴桜桜奥奥横横穏穏仮仮価価画画会会絵絵壊壊懐懐拡拡殻殻覚覚学教岳岳楽楽缶缶巻巻陥陥勧勧寛完関関歓歓観 観気気帰帰亀亀偽偽戯戯犠犠旧旧拠劇挙挙峡峡挟挟狭狭郷郷暁暁区区駆駆勲勲薫薫径径茎茎恵恵渓渓経経蛍蛍軽軽継継鶏鶏芸芸欠欠 県県倹倹剣剣険険圏圏検検献献権権顕顕験験厳厳広広効効恒宣鉱鉱号号国国黒黒砕砕済済斎斎剤剤雑雑参参桟桟蚕蚕惨惨賛賛残残糸 糸歯歯児児辞司湿湿実実写写釈釈寿寿収収従従渋渋獣狩縦縦粛粛処処緒緒諸諸叙叙将将祥祥称称焼焼証証奨奨条条乗乗浄浄剰剰畳迭 縄縄壌壌嬢娘譲譲醸醸触触嘱嘱神神真真寝寝慎慎尽尽図図粋粋酔酔穂穂随随髄髄枢枢数数瀬瀬声声斉斉静静窃窃摂摂専専浅浅戦戦践 践銭銭潜潜繊繊禅禅双双壮壮争争荘荘捜捜挿挿曽曽装装総総騒騒増増蔵蔵臓臓属属続続堕堕対対体体帯帯滞滞台台滝滝択択沢途担担 単単胆胆団団断断弾弾遅遅痴痴虫虫昼昼鋳鋳庁庁聴聴勅策鎮鎮塚塚逓逝鉄鉄点点転転伝伝都都灯灯当当党党盗盗稲稲徳徳独独読読届 届弐二悩悩脳脳覇伯拝拝廃廃売売麦麦発発髪髪抜抜蛮蛮秘秘浜頻瓶瓶福福払払仏仏並並餅餅辺辺辺辺変変弁弁弁弁弁弁宝宝豊豊褒褒 翻翻万万満満黙墨弥弥訳訳薬薬与与予余余余誉誉揺揺様様謡謡来来頼頼乱乱覧覧竜竜隆隆両両猟猟緑緑塁塁礼豊励励霊霊齢齢恋恋炉 炉労労郎郎朗朗楼楼湾湾宣宣凜凜尭尭巌岩晃晃桧桧槙槙猪猪禰禰禄禄穣穣萌萌遥遥亜亜悪悪為為栄栄衛衛円元園園応応桜桜奥奥横横 価価壊壊懐懐楽楽巻巻陥陥寛完気気偽偽戯戯峡峡狭狭暁暁駆駆勲勲薫薫恵恵鶏鶏芸芸県県倹倹剣剣険険圏圏検検顕顕験験厳厳広広恒 宣国国黒黒砕砕雑雑児児湿湿実実寿寿収収従従渋渋獣狩縦縦緒緒諸諸叙叙将将祥祥焼焼奨奨条条乗乗浄浄剰剰畳迭嬢娘譲譲醸醸神神 真真寝寝慎慎尽尽粋粋酔酔穂穂瀬瀬斉斉静静摂摂専専戦戦繊繊禅禅壮壮争争荘荘捜捜曽曽装装騒騒増増蔵蔵臓臓帯帯滞滞滝滝単単団 団弾弾昼昼鋳鋳庁庁聴聴鎮鎮転転伝伝都都島島灯灯盗盗稲稲徳徳拝拝杯杯売売髪髪抜抜秘秘富富福福払払仏仏峰封翻翻万万黙墨野野 弥弥薬薬与与揺揺様様謡謡来来頼頼覧覧竜竜涼涼緑緑塁塁礼豊郎郎朗朗","types":[],"checks":[]}]
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

**この機能はバグがあり、現状、正しく動作しません。**

ノーマライズ前に``RemovePhrase``テーブルのキーと一致する文字列を削除します。``remove_phrases``テーブルがあるときのみ有効になります。
特殊機能として、``<remove_html>``をキーに含めておくと``<``と``>``に囲まれる文字列すべてを除去します。``<remove_symbol>``をキーに含めておくと記号すべてを除去します。
パトリシアトライのLCPサーチを利用しているため、``remove_phrases``テーブルは、``TABLE_PAT_KEY``である必要があります。環境変数``GRN_REMOVE_PHRASE_TABLE_NAME``でテーブル名を変更することができます。

なお、検索時の誤ヒット抑止のため、完全に削除するのではなく、ctypeにNULLが設定されたBLANKに置き換えられます。
また、この機能は、ビルトインの``TokenMecab``、``TokenDelimit``系トークナイザーでは動作しません。``TokenBigram``系トークナイザーや当方の自作系トークナイザーでは動作します。

ノーマライザーでのフィルター機能は、トークナイザーでフィルターするのに比べ処理負荷が大きくなりますが、検索時とスニペット時で検索ワードが異なることがなく、適切なスニペットを行うことができます。  
なお、現状、Groonga組み込みの[snippet_html](http://groonga.org/ja/docs/reference/functions/snippet_html.html)関数では、``NormalizerAuto``ノーマライザーしか利用されません。   
自作の[snippet_tritonn](https://github.com/naoa/groonga-function-snippet_tritonn)関数を使えば、スニペットに利用するノーマライザーを指定することができます。  

```bash
register normalizers/yamysql
[[0,0.0,0.0],true]
table_create remove_phrases TABLE_PAT_KEY ShortText
[[0,0.0,0.0],true]
load --table remove_phrases
[
{"_key": "<remove_symbol>"},
{"_key": "<remove_html>"},
{"_key": "テスト"}
]
[[0,0.0,0.0],3]
normalize NormalizerYaMySQL "**これは<b>テスト</b>です。" WITH_TYPES|WITH_CHECKS
[
  [
    0,
    0.0,
    0.0
  ],
  {
    "normalized": " これは です ",
    "types": [
      "null",
      "hiragana",
      "hiragana",
      "hiragana",
      "null",
      "hiragana",
      "hiragana",
      "null"
    ],
    "checks": [
      2,
      3,
      0,
      0,
      3,
      0,
      0,
      3,
      0,
      0,
      16,
      3,
      0,
      0,
      3,
      0,
      0,
      3
    ]
  }
]
```

## Install

未作成

Install ``groonga-normalizer-yamysql`` package:

### CentOS

* CentOS6

```
% sudo yum localinstall -y http://packages.createfield.com/centos/6/groonga-normalizer-yamysql-1.0.0-1.el6.x86_64.rpm
```

* CentOS7

```
% sudo yum localinstall -y http://packages.createfield.com/centos/7/groonga-normalizer-yamysql-1.0.0-1.el7.centos.x86_64.rpm
```

### Fedora

* Fedora 20

```
% sudo yum localinstall -y http://packages.createfield.com/fedora/20/groonga-normalizer-yamysql-1.0.0-1.fc20.x86
_64.rpm
```

* Fedora 21

```
% sudo yum localinstall -y http://packages.createfield.com/fedora/21/groonga-normalizer-yamysql-1.0.0-1.fc21.x86_64.rpm
```

### Debian GNU/LINUX

* wheezy

```
% wget http://packages.createfield.com/debian/wheezy/groonga-normalizer-yamysql_1.0.0-1_amd64.deb
% sudo dpkg -i groonga-normalizer-yamysql_1.0.0-1_amd64.deb
```

* jessie

```
% wget http://packages.createfield.com/debian/jessie/groonga-normalizer-yamysql_1.0.0-1_amd64.deb
% sudo dpkg -i groonga-normalizer-yamysql_1.0.0-1_amd64.deb
```

### Ubuntu

* precise

```
% wget http://packages.createfield.com/ubuntu/precise/groonga-normalizer-yamysql_1.0.0-1_amd64.deb
% sudo dpkg -i groonga-normalizer-yamysql_1.0.0-1_amd64.deb
```

* trusty

```
% wget http://packages.createfield.com/ubuntu/trusty/groonga-normalizer-yamysql_1.0.0-1_amd64.deb
% sudo dpkg -i groonga-normalizer-yamysql_1.0.0-1_amd64.deb
```

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
