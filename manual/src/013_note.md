# 注意点 {#points_to_note}

## ウイルス対策ソフトに検出される

Langscore内部で使用しているrvcnv.exeが、トロイの木馬として誤検出されることがあります。

該当ソフトはウイルスではないので、除外をお願いします。


## 競合する可能性のあるスクリプト

Langscoreでは下記の関数の処理を上書きしています。

他のスクリプトが同様の事を行っていた場合、Langscoreの処理と競合する可能性があります。

### VX Ace

* Game_Map.setup
* Window_Base.convert_escape_characters
* Window_Base.draw_text
* DataManager.load_normal_database (eval)
* DataManager.make_save_contents (eval)
* Cache.load_bitmap (eval)



### 各ソフトのMD5値

Powershellやハッシュ値比較ソフトでMD5の比較を行う際に利用してください。

