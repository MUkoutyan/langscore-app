# アンインストール {#uninstall}

Langscoreをゲームプロジェクトから外す場合、以下のファイルを削除してください。

## ゲームプロジェクト側

### RPGツクールの場合

以下のスクリプトを削除してください

* langscore
* langscore_custom


### RPGツクールVX Aceの場合

.lstrans関数を使用している場合は削除してください。

削除が面倒な場合は、スクリプト一覧の一番上に以下の関数を追加することで、lstrans関数を実質無視する事ができます。

```
String.class_eval <<-eval
  def lstrans line_info
    self
  end
eval
```


## OS上のファイル/フォルダ

### 共通

* (プロジェクト名)_langscore フォルダ
  - プロジェクトフォルダと同じ階層に作成されています。

* プロジェクト/Data/Translate フォルダ

* プロジェクト/Font フォルダ

### RPG VX Aceの場合

* .iniファイル内の[Langscore]グループ