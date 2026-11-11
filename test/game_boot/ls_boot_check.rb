# encoding: utf-8
#-------------------------------------------------------------------
# test/tst_FirstRunGamePlay.cpp で使うテスト用のスクリプト。Main の直前に追加して実行する。
# テストプレイで起動したゲームをタイトル → ニューゲーム → マップまで自動で進め、
# 各言語でのアクター名をゲームフォルダの ls_boot_result.txt に書き込んでから終了する。
# エラーが発生した場合は ERROR の行を書き込んで即座に終了する。
#-------------------------------------------------------------------
module LsBootCheck
  RESULT_FILE = "ls_boot_result.txt"
  @lines = []

  def self.add(*columns)
    @lines.push(columns.join("\t"))
  end

  def self.finish(status)
    @lines.push(status)
    File.open(RESULT_FILE, "wb") { |f| f.write(@lines.join("\n") + "\n") }
    exit
  end
end

class << SceneManager
  alias ls_boot_check_run run
  def run
    ls_boot_check_run
  rescue SystemExit, RGSSReset
    raise
  rescue Exception => e
    where = (e.backtrace || []).first(3).join(" ")
    LsBootCheck.finish("ERROR\t#{e.class}: #{e.message} @ #{where}".gsub(/[\r\n]+/, " "))
  end
end

# タイトル画面が表示されたら、ニューゲームを選んだのと同じ処理を行う
class Scene_Title
  alias ls_boot_check_update update
  def update
    ls_boot_check_update
    return if @ls_boot_started
    @ls_boot_started = true
    LsBootCheck.add("title")
    command_new_game
  end
end

# マップで少し動かしてから言語を切り替え、切り替えた後も少し動かしてから終了する
class Scene_Map
  alias ls_boot_check_update update
  def update
    ls_boot_check_update
    @ls_boot_frames = (@ls_boot_frames || 0) + 1
    if @ls_boot_frames == 30
      languages = Langscore::SUPPORT_LANGUAGE
      LsBootCheck.add("languages", languages.join(","))
      languages.each do |lang|
        Langscore.changeLanguage(lang)
        LsBootCheck.add("actor1", lang, $game_actors[1].name)
      end
    elsif @ls_boot_frames == 60
      LsBootCheck.finish("DONE")
    end
  end
end
