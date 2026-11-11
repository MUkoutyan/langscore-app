//=============================================================================
// LangscoreBootCheck.js
// test/tst_FirstRunGamePlay.cpp で使うテスト用のプラグイン。テストプレイで起動したゲームを
// タイトル → ニューゲーム → マップまで自動で進め、各言語でのアクター名を
// ゲームフォルダの ls_boot_result.txt に書き込んでから終了する。
// エラーが発生した場合は ERROR の行を書き込んで即座に終了する。
// Langscore.js の読み込み時のエラーも拾えるよう、plugins.js の先頭に登録する。
//=============================================================================
(() => {
    const fs = require("fs");
    const path = require("path");
    const resultPath = path.join(path.dirname(process.mainModule.filename), "ls_boot_result.txt");
    const lines = [];
    let finished = false;

    const finish = (status) => {
        if(finished) { return; }
        finished = true;
        lines.push(status);
        fs.writeFileSync(resultPath, lines.join("\n") + "\n", "utf-8");
        nw.App.quit();
    };
    const fail = (message) => finish("ERROR\t" + String(message).replace(/[\t\r\n]+/g, " "));

    window.addEventListener("error", (e) => fail(`${e.message} @ ${e.filename}:${e.lineno}`));
    window.addEventListener("unhandledrejection", (e) => fail(e.reason && e.reason.stack ? e.reason.stack : e.reason));
    // ツクールが捕捉した例外・読み込みの失敗は、画面に表示する処理で捕捉する
    const printError = Graphics.printError;
    Graphics.printError = function(name, message) {
        fail(`${name} ${message}`);
        printError.apply(this, arguments);
    };
    if(Graphics.printLoadingError) {
        const printLoadingError = Graphics.printLoadingError;
        Graphics.printLoadingError = function(url) {
            fail(`Failed to load: ${url}`);
            printLoadingError.apply(this, arguments);
        };
    }

    // タイトル画面が表示されたら、ニューゲームを選んだのと同じ処理を行う
    const titleUpdate = Scene_Title.prototype.update;
    Scene_Title.prototype.update = function() {
        titleUpdate.call(this);
        if(this._lsBootStarted || this.isBusy()) { return; }
        this._lsBootStarted = true;
        lines.push("title");
        this.commandNewGame();
    };

    // マップで少し動かしてから言語を切り替え、切り替えた後も少し動かしてから終了する
    const mapUpdate = Scene_Map.prototype.update;
    Scene_Map.prototype.update = function() {
        mapUpdate.call(this);
        this._lsBootFrames = (this._lsBootFrames || 0) + 1;
        if(this._lsBootFrames === 30) {
            if(!window.Langscore || !window._langscore) {
                fail("Langscore plugin is not loaded");
                return;
            }
            const languages = Langscore.Support_Language;
            lines.push("languages\t" + languages.join(","));
            for(const lang of languages) {
                _langscore.changeLanguage(lang);
                lines.push(`actor1\t${lang}\t${$gameActors.actor(1).name()}`);
            }
        }
        else if(this._lsBootFrames === 60) {
            finish("DONE");
        }
    };
})();
