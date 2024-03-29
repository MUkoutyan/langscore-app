# より細かい使い方(スクリプト編) {#usage_advance_script}

ざっくりとした内容は [基本的な使い方](#basic_usage) をご覧ください。


スクリプト内の文章を翻訳する場合、CSVファイルだけで対応することが出来ません。

文章ごとに以下の2パターンどちらかの対応が必要になります。

* A.文章を格納している変数の書き換え
* B.文章を直接書き換え

#### A.文章を格納している変数の書き換え

以下のような、定数に文字列が格納されているパターンについて解説します。

~~~~~{rb}
module Battle_Helper
	#ここの"攻撃"を翻訳したい
  BATTLE_ATTACK_TEXT = "攻撃"
end
~~~~~

バトル中に表示される"攻撃"をBATTLE_ATTACK_TEXT変数に代入している処理です。

"攻撃"を"Hit"にしたい場合、BATTLE_ATTACK_TEXTを書き換える処理が必要ですが、

Langscoreではその処理をlangscore_customに記述します。

* まず、langscore_customの冒頭に```Langscore.translate_(識別ID)	#(スクリプト名)``` という行があるので、そこから翻訳したいスクリプトを探してください。

* 次に、該当のスクリプトから関数```def Langscore.translate_(識別ID)```と記述されている箇所を探します。

翻訳対象に"攻撃"が正しく含まれている場合、関数内に以下の様な記述がされています。

~~~~~{.rb}
	#Scripts/01234567#37,38
	#original : 攻撃
	#Langscore.translate_for_script("01234567:37:38")
~~~~~

1行目から、

1. スクリプト識別子と行,列の情報
2. スクリプトに書かれている原文
3. 翻訳文を返す関数

になります。

* ここに処理を記述します。例が以下の通りです。

~~~~~{.rb}
	#Scripts/01234567#37,38
	#original : 攻撃
	BATTLE_ATTACK_TEXT.replace(Langscore.translate_for_script("01234567:37:38"))
~~~~~

translate_for_script関数は、入力した文字列と現在選択されている言語を元に、翻訳文を返します。

返された結果を.replace関数を使うことで上書きします。

@note BATTLE_ATTACK_TEXTの様な全て大文字の変数は定数と呼ばれ、本来は書き換えることを想定していません。<br>
Langscoreでは仕方なく.replaceを使用して書き換えていますが、本来はお行儀の悪い関数であることを認識して頂ければと思います。


#### B.直接書き換え

関数内のローカル変数に翻訳したい文字列が格納されている場合などは、こちらの手法を使います。

~~~~~{.rb}
def draw_attack_text
  draw_text = "%1の攻撃！"
  draw_function(draw_text)
end
~~~~~

上記のケースの場合、langscore_customからdraw_text変数が見れないため、書き換えることが出来ません。

この場合、lstrans関数を使用する方法で翻訳を適用します。

~~~~~{.rb}
def draw_attack_text
  draw_text = "%1の攻撃！".lstrans("Script:123:45")
	#                   ^^^^^^^^^^^^^^^^^^^^^^^^^
  draw_function(draw_text)
end
~~~~~

.lstransは文字列のみに使用できるLangscoreの翻訳関数になります。

引数に渡す識別子はlangscore_customに記載されていますので、コピペしてお使いください。

@note .lstrans関数に渡す文字列は翻訳対象となりません。


### 画像を変える

画像の検索方法は2通りあります。

1. Graphics.csvから使用するファイルパスを読み込み
2. 翻訳対象だがGraphics.csvで翻訳先のファイルパスが空の場合、[ファイル名_言語名]となるファイルを検索

上記の処理でも見つからない場合、元の画像を表示します。

#### 詳解:1.

以下の画像ファイルを翻訳する場合を例とします。

~~~~~
Picture/TitleLogo.png
~~~~~

翻訳対象が英日中の場合、CSVには以下のように記述します。

~~~~~
original,en,ja,zh-cn
./Picture/TitleLogo.png,Picture/TitleLogo_2.png,Picture/TitleLogo.png,Picture/TitleLogo_3.png
~~~~~

#### 詳解:2.

翻訳先が空の場合、ファイルパスから検索します。

~~~~~
original,en,ja,zh-cn
./Picture/TitleLogo.png,,./Picture/TitleLogo.png,
~~~~~

* 英語表示の場合
	- ./Picture/TitleLogo_en.png

* 日本語表示の場合
	- パスが書かれているので、CSVの内容を適用

* 中国語(簡体)表示の場合
	- ./Picture/TitleLogo_zh-cn.png

英・中の場合、TitleLogo_(lang).pngが見つからない場合はTitleLogo.pngを表示します。
