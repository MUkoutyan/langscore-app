# より細かい使い方 {#usage_advance}

ざっくりとした内容は [基本的な使い方](#basic_usage) をご覧ください。

## 言語選択

### 言語で使用するフォントを追加する

「翻訳ファイル生成モード」が表示されているときに、フォントファイルをドラッグ&ドロップすることで、フォントを追加することが出来ます。

対応フォントは.ttf, .otfの2種類です。

フォントを追加した場合、フォントのプレビューには再起動後に反映されます。


## スクリプト内の文を翻訳する

スクリプトによっては、ゲーム中で表示される文が内部に書かれている場合があります。

大体の文は翻訳する必要はありませんが、稀によく翻訳が必要になる箇所があります。

![](script_trans.png)

翻訳が必要な文章に、ハイライトした列のチェックを付けてください。

スクリプト丸ごとチェックを変更したい場合は、左側のツリーから変更すると一括で変えることが出来ます。

### プログラム側の対応

スクリプトの翻訳は性質上、CSVのみで対応することが出来ません。

文章ごとに以下の2パターンどちらかの対応が必要になります。

* 文章を格納している変数の書き換え
* 文章を直接書き換え

#### 文章を格納している変数の書き換え

定数に文字列が格納されている場合、langscore_customスクリプト内に処理を記述します。

~~~~~{rb}
  BATTLE_ATTACK_TEXT = "攻撃"
~~~~~

~~~~~{ruby}
  BATTLE_ATTACK_TEXT = Langscore.translate_for_script("19127899:52:15")
~~~~~

langscore_customにはコメントアウトされたtranslate_for_scriptが記述されています。

コメントを外すことで、記述を多少簡略化出来ます。

~~~~~{.rb}
	#Scripts/65903110#37,38
	#original : Data/Actors.rvdata2
	#Langscore.translate_for_script("65903110:37:38") #<<これ1

	#Scripts/65903110#38,38
	#original : Data/Classes.rvdata2
	#Langscore.translate_for_script("65903110:38:38") #<<これ2

	#Scripts/65903110#39,38
	#original : Data/Skills.rvdata2
	#Langscore.translate_for_script("65903110:39:38") #<<これ3
~~~~~


#### 直接書き換え

関数内のローカル変数に翻訳したい文字列が格納されている場合などは、こちらの手法を使います。

~~~~~{.rb}
def draw_attack_text
  draw_text = "%1の攻撃！"
  draw_function(draw_text)
end
~~~~~

上記のケースの場合、langscore_customからdraw_text変数が見れないため、書き換えることが出来ません。

この場合、以下のような方法で翻訳を適用します。

~~~~~{.rb}
def draw_attack_text
  draw_text = "%1の攻撃！".lstrans("Script:123:45")
  draw_function(draw_text)
end
~~~~~

.lstransがLangscoreの翻訳関数、"Script:123:45"は翻訳文の識別子になります。

スクリプト名:行数:列数 から成る識別子ですが、この文字列もlangscore_customに記載されていますので、コピペしてお使いください。

@note .lstrans内の文字列は翻訳対象となりません。


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

翻訳対象が英日中の場合、CSVには以下のように記載します。

~~~~~
original,en,ja,zh-cn
./Picture/TitleLogo.png,Picture/TitleLogo_2.png, Picture/TitleLogo.png, Picture/TitleLogo_3.png
~~~~~

#### 詳解:2.

翻訳先が空の場合、ファイルパスから検索します。

~~~~~
original,en,ja,zh-cn
./Picture/TitleLogo.png,,./Picture/TitleLogo.png,
~~~~~



翻訳先のパスが空なので、ゲームで現在設定中の言語を元にファイルを検索します。
* 英語表示の場合
	- ./Picture/TitleLogo_en.png

* 日本語表示の場合
	- パスが埋まっているので、CSVの内容を適用

* 中国語(簡体)表示の場合
	- ./Picture/TitleLogo_zh-cn.png