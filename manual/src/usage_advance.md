# より細かい使い方 {#usage_advance}

ざっくりとした内容は[基本的な使い方](#usage)をご覧ください。

## 言語選択

![](language.png)

1. 言語の有効無効ボタン
2. ゲームで使用するフォント
3. フォントサイズ
4. フォントの簡易プレビュー
5. 原文の言語チェック

### 言語で使用するフォントを追加する

「翻訳ファイル生成モード」が表示されているときに、フォントファイルをドラッグアンドドロップすることでフォントを追加することが出来ます。

### 原文の言語チェックについて

@attention 必ずゲーム制作時に使用した言語を選択してください

この設定は原文が何語かを判定する際に使用しているため、誤った言語にチェックを付けると正しく翻訳されない可能性があります。


## スクリプト内の文を翻訳する

スクリプトによっては、ゲーム中で表示される文が書かれている場合があります。

大体の文は翻訳する必要はありませんが、稀によく翻訳が必要になる箇所があります。

![](script_trans.png)

翻訳が必要な文章に、ハイライトした列のチェックを付けてください。

スクリプトごとチェックを変更したい場合は、左側のツリーから変更すると一括で変えることが出来ます。

### プログラム側の対応

スクリプトの翻訳は性質上、CSVのみで対応することが出来ません。

文ごとに以下の2パターンどちらかの対応が必要になります。

* 文を格納している変数の書き換え
* 文を直接書き換え

#### 変数の書き換え

定数に文字列が格納されている場合、langscore_customスクリプト内に処理を記述します。

```対象スクリプト
  BATTLE_ATTACK_TEXT = "攻撃"
```

```langscore_custom
  BATTLE_ATTACK_TEXT = Langscore.translate_for_script("19127899:52:15")
```

langscore_customにはコメントアウトされたtranslate_for_scriptが記述されています。

```ruby
	#Scripts/65903110#37,38
	#original : Data/Actors.rvdata2
	#Langscore.translate_for_script("65903110:37:38")

	#Scripts/65903110#38,38
	#original : Data/Classes.rvdata2
	#Langscore.translate_for_script("65903110:38:38")

	#Scripts/65903110#39,38
	#original : Data/Skills.rvdata2
	#Langscore.translate_for_script("65903110:39:38")
```


#### 直接書き換え

関数内のローカル変数に翻訳したい文字列が格納されている場合などは、こちらの手法を使います。

```ruby
def draw_attack_text
  draw_text = "%1の攻撃！"
  draw_function(draw_text)
end
```

上記のケースの場合、draw_textを外部から操作する方法がありません。

この場合、以下のような方法で翻訳を適用します。

```ruby
def draw_attack_text
  draw_text = "%1の攻撃！".lstrans("Script:123:45")
  draw_function(draw_text)
end
```

"Script:123:45"は翻訳文の識別子になります。

スクリプト名:行数:列数 から成る識別子ですが、langscore_customに記載されていますのでコピペしてお使いください。
