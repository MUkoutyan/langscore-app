#include "test_common.h"
#include <QTemporaryDir>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QEnterEvent>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QProcess>
#include <QUndoCommand>
#include <QAbstractProxyModel>
#include <QApplication>
#include <QMessageBox>
#include <QPainter>
#include <QTimer>
#include <map>
#include "settings.h"
#include "WriteModeComponent.h"
#include "MainCSVTable.h"
#include "CSVEditor.h"
#include "CSVEditorTableModel.h"
#include "EditDataManager.h"
#include "EditingTranslationExporter.h"
#include "FollowGameService.h"
#include "dialog/UpdatePluginDialog.h"

//-------------------------------------------------------------------
// テストプレイ中のゲームへの追従・反映を行う Langscore_RemoteHub のテスト
//
// 確認する内容。優先度順
//  1. ゲームのプロジェクトを壊さないこと
//  2. 壊れた pid / current.json で落ちないこと
//  3. 翻訳の表・編集履歴など、Langscore のプロジェクトを壊さないこと
//  4. ボタンの状態・枠の位置などの動き
//-------------------------------------------------------------------
namespace
{
    //dispatch を受け取ったものを記録する
    struct RemoteHubTestComponent : public ComponentBase
    {
        using ComponentBase::setting;
        using ComponentBase::history;
        std::vector<DispatchType> received;

        RemoteHubTestComponent() { this->addDispatch(this); }
        ~RemoteHubTestComponent() override { this->removeDispatch(this); }

        void receive(DispatchType type, const QVariantList&) override { received.push_back(type); }
    };

    void WriteFile(const QString& path, const QByteArray& content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if(file.open(QFile::WriteOnly) == false) {
            ADD_FAILURE() << "cannot create test file: " << path.toStdString();
            return;
        }
        file.write(content);
    }

    //フォルダ以下の全ファイルの内容のハッシュ。キーはフォルダからの相対パス
    std::map<QString, QByteArray> Snapshot(const QString& root)
    {
        std::map<QString, QByteArray> result;
        QDirIterator it(root, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while(it.hasNext()) {
            const auto path = it.next();
            QFile file(path);
            if(file.open(QFile::ReadOnly) == false) { continue; }
            result[QDir(root).relativeFilePath(path)] = QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha1);
        }
        return result;
    }

    QStringList ChangedFiles(const std::map<QString, QByteArray>& before, const std::map<QString, QByteArray>& after)
    {
        QStringList result;
        for(const auto& [path, hash] : before) {
            auto other = after.find(path);
            if(other == after.end() || other->second != hash) { result.append(path); }
        }
        for(const auto& [path, hash] : after) {
            if(before.contains(path) == false) { result.append(path); }
        }
        result.sort();
        return result;
    }

    //MZ のゲームプロジェクトを作り、原文 A, B を持つ Actors の解析結果を置く
    void SetupProject(RemoteHubTestComponent& component, const QString& gameDir)
    {
        WriteFile(gameDir + "/Game.rmmzproject", "RPGMZ");
        WriteFile(gameDir + "/data/Actors.json", "[]");
        WriteFile(gameDir + "/js/plugins.js", "var $plugins = [];\n");
        auto& setting = *component.setting;
        setting.setGameProjectPath(gameDir);
        WriteFile(setting.analyzeDirectoryPath() + "/Actors.json", "[]");
        WriteFile(setting.analyzeDirectoryPath() + "/Actors.lsjson", R"([
            {"original": "A", "type": ["name"]},
            {"original": "B", "type": ["name"]},
            {"original": "Line1\nLine2", "type": ["name"]}
        ])");
        setting.languages = {settings::Language{.languageName = "en", .enable = true}};
        const auto configPath = setting.langscoreProjectDirectory + "/config.json";
        setting.write(configPath);
        setting.load(configPath);
    }

    QJsonObject ShownText(const QString& original, const QString& file, bool choice = false, bool selected = false)
    {
        return QJsonObject{{"window", choice ? "Window_ChoiceList" : "Window_Message"}, {"file", file},
                           {"original", original}, {"translated", ""}, {"choice", choice}, {"selected", selected}};
    }

    class RemoteHubTest : public ::testing::Test
    {
    protected:
        QTemporaryDir tempDir;
        RemoteHubTestComponent component;
        //アプリと同じく、ゲームとの連携は WriteModeComponent とは別のサービスが dispatch でつながる
        FollowGameService service{&component};
        QString gameDir;
        QString hubDir;

        void SetUp() override
        {
            gameDir = tempDir.filePath("Game");
            SetupProject(component, gameDir);
            hubDir = component.setting->langscoreProjectDirectory + "/remote";
        }

        //ゲームの起動を模して、このプロセスのIDを pid に書く
        void WritePid(const QByteArray& pid, const QDateTime& writtenAt = QDateTime::currentDateTime())
        {
            const auto path = hubDir + "/pid";
            WriteFile(path, pid);
            QFile file(path);
            ASSERT_TRUE(file.open(QFile::ReadWrite));
            ASSERT_TRUE(file.setFileTime(writtenAt, QFileDevice::FileModificationTime));
        }

        void WriteOwnPid() { WritePid(QByteArray::number(QCoreApplication::applicationPid())); }

        void WriteCurrent(const QJsonArray& texts, const QString& language = "en")
        {
            WriteCurrentRaw(QJsonDocument(QJsonObject{{"language", language}, {"mapId", 1}, {"texts", texts}}).toJson());
        }

        void WriteCurrentRaw(const QByteArray& content)
        {
            WriteFile(hubDir + "/current.json", content);
        }
    };

    //WriteModeComponent の追従・反映のボタンと、表のビューを取り出す
    struct WriteModeParts
    {
        QPushButton* follow = nullptr;
        QPushButton* apply = nullptr;
        QWidget* langTab = nullptr;
        CSVEditor* editor = nullptr;
    };

    WriteModeParts FindParts(WriteModeComponent& writeMode)
    {
        WriteModeParts parts;
        parts.follow = writeMode.findChild<QPushButton*>("followGameButton");
        parts.apply = writeMode.findChild<QPushButton*>("applyToGameButton");
        parts.langTab = writeMode.findChild<QWidget*>("langTab");
        //スクリプトの表にも CSVEditor があるため、メインの表から探す
        if(auto* table = writeMode.findChild<MainCSVTable*>()) { parts.editor = table->findChild<CSVEditor*>(); }
        return parts;
    }

    //枠の付いたセルの原文と言語の列。枠が無ければ空
    std::pair<QString, QString> FramedCell(CSVEditor* editor)
    {
        const auto index = editor->framedCell();
        if(index.isValid() == false) { return {}; }
        const auto* model = index.model();
        return {model->index(index.row(), 0).data().toString(),
                model->headerData(index.column(), Qt::Horizontal, Qt::UserRole).toString()};
    }

    //表の全セルの内容
    QStringList AllCells(const QAbstractItemModel* model)
    {
        QStringList cells;
        for(int row = 0; row < model->rowCount(); ++row) {
            for(int column = 0; column < model->columnCount(); ++column) {
                cells.append(model->index(row, column).data().toString());
            }
        }
        return cells;
    }

    const QAbstractItemModel* SourceModel(CSVEditor* editor)
    {
        const auto* proxy = qobject_cast<const QAbstractProxyModel*>(editor->model());
        return proxy ? proxy->sourceModel() : editor->model();
    }
}

//===================================================================
// 1. ゲームのプロジェクトを壊さないこと
//===================================================================

TEST_F(RemoteHubTest, FollowingDoesNotWriteToProjects)
{
    // 追従している間、アプリはゲームのプロジェクトにも Langscore_RemoteHub フォルダにも書き込まないこと
    WriteModeComponent writeMode(&component, nullptr);
    writeMode.show();
    auto parts = FindParts(writeMode);
    ASSERT_NE(parts.follow, nullptr);
    WriteOwnPid();
    WriteCurrent({ShownText("A", "Actors.csv")});
    const auto gameBefore = Snapshot(gameDir);
    const auto hubBefore = Snapshot(hubDir);

    ASSERT_TRUE(WaitUntil([&]() { return parts.follow->isEnabled(); }, 3000));
    parts.follow->click();
    WriteCurrent({ShownText("B", "Actors.csv")});
    ASSERT_TRUE(WaitUntil([&]() { return FramedCell(parts.editor).first == "B"; }, 5000));
    parts.follow->click();

    EXPECT_EQ(ChangedFiles(gameBefore, Snapshot(gameDir)), QStringList{});
    //current.json は上でテストが書き換えている
    auto hubAfter = Snapshot(hubDir);
    hubAfter.erase("current.json");
    auto expected = hubBefore;
    expected.erase("current.json");
    EXPECT_EQ(ChangedFiles(expected, hubAfter), QStringList{});
}

TEST_F(RemoteHubTest, ApplyWritesOnlyEditedTranslationFiles)
{
    // ゲームへの反映で書き換えるのは、編集した翻訳ファイルだけ。他のゲームのファイルと、編集中の CSV はそのまま
    auto& setting = *component.setting;
    const auto translateDir = setting.translateDirectoryPath();
    WriteFile(translateDir + "/Actors.csv", "original,en\nA,old\nB,keep\n");
    WriteFile(translateDir + "/Items.csv", "original,en\nX,item\n");
    langscore::ApplyEditingTranslations(setting);
    WriteFile(setting.langscoreProjectDirectory + "/editing/Actors.csv", "original,en,type\nA,new,name\nB,keep,name\n");

    const auto gameBefore = Snapshot(gameDir);
    const auto editingBefore = Snapshot(setting.langscoreProjectDirectory + "/editing");
    const auto files = langscore::UnexportedEditingFiles(setting);
    ASSERT_EQ(files, QStringList{"Actors.csv"});
    langscore::ApplyEditingTranslations(setting, files);

    const auto changed = ChangedFiles(gameBefore, Snapshot(gameDir));
    ASSERT_EQ(changed.size(), 1);
    EXPECT_TRUE(changed[0].endsWith("translate/Actors.csv")) << changed[0].toStdString();
    //exported.json は反映した内容の記録。編集中の CSV は変えない
    auto editingChanged = ChangedFiles(editingBefore, Snapshot(setting.langscoreProjectDirectory + "/editing"));
    editingChanged.removeAll("exported.json");
    EXPECT_EQ(editingChanged, QStringList{});

    QFile actors(translateDir + "/Actors.csv");
    ASSERT_TRUE(actors.open(QFile::ReadOnly));
    const auto content = actors.readAll();
    EXPECT_TRUE(content.contains("new"));
    EXPECT_TRUE(content.contains("keep"));
}

//===================================================================
// 2. 壊れた pid / current.json で落ちないこと
//===================================================================

TEST_F(RemoteHubTest, BrokenPidMeansNotRunning)
{
    // pid が読めない・数字でない・存在しないプロセスなら、ゲームは起動していないものとして扱う
    WriteModeComponent writeMode(&component, nullptr);
    auto parts = FindParts(writeMode);
    ASSERT_NE(parts.follow, nullptr);

    QProcess exited;
    exited.start("cmd.exe", {"/c", "exit"});
    ASSERT_TRUE(exited.waitForFinished(5000));
    const auto exitedPid = QByteArray::number(exited.processId());

    for(const QByteArray& pid : {QByteArray(""), QByteArray("abc"), QByteArray("-1"), QByteArray("0"),
                                  QByteArray("99999999999999999999"), QByteArray("\xff\xfe"), exitedPid}) {
        WritePid(pid);
        //ゲームの確認は 1 秒ごと
        WaitUntil([]() { return false; }, 1300);
        EXPECT_FALSE(parts.follow->isEnabled()) << "pid: " << pid.toStdString();
    }
}

TEST_F(RemoteHubTest, ReusedPidIsNotRunning)
{
    // プロセスIDの再利用により pid の書き込み後に起動した別のプロセスは、ゲームとして扱わない
    WriteModeComponent writeMode(&component, nullptr);
    auto parts = FindParts(writeMode);
    ASSERT_NE(parts.follow, nullptr);
    WritePid(QByteArray::number(QCoreApplication::applicationPid()), QDateTime::currentDateTime().addDays(-1));
    WaitUntil([]() { return false; }, 1300);
    EXPECT_FALSE(parts.follow->isEnabled());
}

TEST_F(RemoteHubTest, BrokenCurrentJsonIsIgnored)
{
    // 書き込み途中などで current.json が壊れていても落ちず、正しい内容が来たら枠を付けること
    WriteModeComponent writeMode(&component, nullptr);
    writeMode.show();
    auto parts = FindParts(writeMode);
    WriteOwnPid();
    WriteCurrent({});
    ASSERT_TRUE(WaitUntil([&]() { return parts.follow->isEnabled(); }, 3000));
    parts.follow->click();

    for(const QByteArray& content : {QByteArray(""), QByteArray("{"), QByteArray("[]"), QByteArray("null"),
                                      QByteArray(R"({"texts": 5})"), QByteArray(R"({"texts": [1, "a", null, []]})"),
                                      QByteArray(R"({"texts": [{"file": 3, "original": {}}]})"),
                                      QByteArray(R"({"texts": [{"file": "../../../Windows/win.ini", "original": "A"}]})")}) {
        WriteCurrentRaw(content);
        WaitUntil([]() { return false; }, 300);
        EXPECT_TRUE(parts.follow->isChecked()) << content.toStdString();
    }
    WriteCurrent({ShownText("A", "Actors.csv")});
    EXPECT_TRUE(WaitUntil([&]() { return FramedCell(parts.editor).first == "A"; }, 5000));
}

//===================================================================
// 3. Langscore のプロジェクトを壊さないこと
//===================================================================

TEST_F(RemoteHubTest, HighlightDoesNotEditTable)
{
    // 枠を付けても、表の内容・選択・編集履歴は変わらないこと
    WriteModeComponent writeMode(&component, nullptr);
    writeMode.show();
    auto parts = FindParts(writeMode);
    WriteOwnPid();
    WriteCurrent({});
    ASSERT_TRUE(WaitUntil([&]() { return parts.follow->isEnabled(); }, 3000));
    parts.follow->click();

    WriteCurrent({ShownText("A", "Actors.csv")});
    ASSERT_TRUE(WaitUntil([&]() { return FramedCell(parts.editor).first == "A"; }, 5000));
    const auto* model = SourceModel(parts.editor);
    const auto cells = AllCells(model);
    parts.editor->setCurrentIndex(parts.editor->model()->index(0, 0));
    const auto current = parts.editor->currentIndex();
    const auto historyIndex = component.history->index();

    WriteCurrent({ShownText("B", "Actors.csv")});
    ASSERT_TRUE(WaitUntil([&]() { return FramedCell(parts.editor).first == "B"; }, 5000));
    EXPECT_EQ(AllCells(SourceModel(parts.editor)), cells);
    EXPECT_EQ(parts.editor->currentIndex(), current);
    EXPECT_EQ(component.history->index(), historyIndex);
}

TEST_F(RemoteHubTest, LanguageTabIsLockedWhileFollowing)
{
    // 接続中は言語タブを操作できず、切断・ゲームの終了で戻ること
    WriteModeComponent writeMode(&component, nullptr);
    auto parts = FindParts(writeMode);
    ASSERT_NE(parts.langTab, nullptr);
    WriteOwnPid();
    ASSERT_TRUE(WaitUntil([&]() { return parts.follow->isEnabled(); }, 3000));

    parts.follow->click();
    EXPECT_FALSE(parts.langTab->isEnabled());
    parts.follow->click();
    EXPECT_TRUE(parts.langTab->isEnabled());

    // ゲームを閉じたら自動で切断し、言語タブを戻す
    parts.follow->click();
    ASSERT_FALSE(parts.langTab->isEnabled());
    QFile::remove(hubDir + "/pid");
    ASSERT_TRUE(WaitUntil([&]() { return parts.follow->isChecked() == false; }, 3000));
    EXPECT_TRUE(parts.langTab->isEnabled());
    EXPECT_FALSE(parts.follow->isEnabled());
}

TEST_F(RemoteHubTest, ApplySavesProject)
{
    // 接続中の反映では、反映する前にプロジェクトを保存すること
    component.setting->isFirstExported = true;
    WriteModeComponent writeMode(&component, nullptr);
    auto parts = FindParts(writeMode);
    ASSERT_NE(parts.apply, nullptr);
    WriteOwnPid();

    component.history->push(new QUndoCommand("edit"));
    ASSERT_TRUE(WaitUntil([&]() { return parts.apply->isEnabled(); }, 3000));
    component.received.clear();
    parts.apply->click();
    EXPECT_NE(std::ranges::find(component.received, ComponentBase::SaveProject), component.received.end());
}

//===================================================================
// 4. ボタンの状態・枠の位置
//===================================================================

TEST_F(RemoteHubTest, FollowButtonFollowsGameState)
{
    // ゲームが起動していなければ無効。起動したら有効になり、接続中は「接続中...」になる
    WriteModeComponent writeMode(&component, nullptr);
    auto parts = FindParts(writeMode);
    ASSERT_NE(parts.follow, nullptr);
    EXPECT_FALSE(parts.follow->isEnabled());

    WriteOwnPid();
    ASSERT_TRUE(WaitUntil([&]() { return parts.follow->isEnabled(); }, 3000));
    EXPECT_EQ(parts.follow->text(), QString("Follow Game"));

    parts.follow->click();
    EXPECT_TRUE(parts.follow->isChecked());
    EXPECT_EQ(parts.follow->text(), QString("Connected."));
    EXPECT_EQ(parts.follow->toolTip(), QString("Click to disconnect"));

    parts.follow->click();
    EXPECT_EQ(parts.follow->text(), QString("Follow Game"));
}

TEST_F(RemoteHubTest, OnlyAddPluginButtonBeforeRegistered)
{
    // プラグインの追加前は追加ボタンのみ。追加後はテストプレイ・追従・反映・プラグインの更新・書き出しを表示する
    component.setting->isPluginRegistered = false;
    WriteModeComponent writeMode(&component, nullptr);
    writeMode.show();
    auto* addPlugin = writeMode.findChild<QPushButton*>("addPluginButton");
    auto* exportButtons = writeMode.findChild<QWidget*>("exportButtonsWidget");
    ASSERT_NE(addPlugin, nullptr);
    ASSERT_NE(exportButtons, nullptr);
    EXPECT_TRUE(addPlugin->isVisibleTo(&writeMode));
    EXPECT_FALSE(exportButtons->isVisibleTo(&writeMode));

    component.setting->isPluginRegistered = true;
    writeMode.show();
    EXPECT_FALSE(addPlugin->isVisibleTo(&writeMode));
    EXPECT_TRUE(exportButtons->isVisibleTo(&writeMode));
    for(const char* name : {"testPlayButton", "followGameButton", "applyToGameButton", "updatePluginButton", "writeButton"}) {
        auto* button = writeMode.findChild<QPushButton*>(name);
        ASSERT_NE(button, nullptr) << name;
        EXPECT_TRUE(button->isVisibleTo(&writeMode)) << name;
    }
}

TEST_F(RemoteHubTest, ExportButtonsAreDisabledWhileFollowing)
{
    // ゲームに接続している間は、プラグインの更新と翻訳CSVの書き出しを無効にし、切断したら戻す
    component.setting->isPluginRegistered = true;
    WriteModeComponent writeMode(&component, nullptr);
    writeMode.show();
    auto parts = FindParts(writeMode);
    auto* updatePlugin = writeMode.findChild<QPushButton*>("updatePluginButton");
    auto* write = writeMode.findChild<QPushButton*>("writeButton");
    ASSERT_NE(updatePlugin, nullptr);
    ASSERT_NE(write, nullptr);
    ASSERT_TRUE(write->isEnabled());
    WriteOwnPid();
    ASSERT_TRUE(WaitUntil([&]() { return parts.follow->isEnabled(); }, 3000));

    parts.follow->click();
    EXPECT_FALSE(updatePlugin->isEnabled());
    EXPECT_FALSE(write->isEnabled());

    parts.follow->click();
    EXPECT_TRUE(updatePlugin->isEnabled());
    EXPECT_TRUE(write->isEnabled());
}

TEST_F(RemoteHubTest, ApplyButtonIsEnabledOnlyWithEdits)
{
    // ゲームの実行中でも、反映するものが無ければ無効。編集すると有効になり、反映した直後は無効になる
    component.setting->isFirstExported = true;
    WriteModeComponent writeMode(&component, nullptr);
    auto parts = FindParts(writeMode);
    ASSERT_NE(parts.apply, nullptr);
    WriteOwnPid();
    ASSERT_TRUE(WaitUntil([&]() { return parts.follow->isEnabled(); }, 3000));
    EXPECT_FALSE(parts.apply->isEnabled());

    component.history->push(new QUndoCommand("edit"));
    EXPECT_TRUE(parts.apply->isEnabled());
    parts.apply->click();
    EXPECT_FALSE(parts.apply->isEnabled());
}

TEST_F(RemoteHubTest, ApplyButtonIsEnabledOnlyWhileGameRunning)
{
    // 編集があっても、ゲームが起動していなければ無効。起動すると有効になり、終了すると無効に戻る
    component.setting->isFirstExported = true;
    WriteModeComponent writeMode(&component, nullptr);
    auto parts = FindParts(writeMode);
    ASSERT_NE(parts.apply, nullptr);
    component.history->push(new QUndoCommand("edit"));
    EXPECT_FALSE(parts.apply->isEnabled());

    WriteOwnPid();
    ASSERT_TRUE(WaitUntil([&]() { return parts.apply->isEnabled(); }, 3000));

    QFile::remove(hubDir + "/pid");
    ASSERT_TRUE(WaitUntil([&]() { return parts.apply->isEnabled() == false; }, 3000));
}

TEST_F(RemoteHubTest, ApplyBeforeFirstExportDoesNothing)
{
    // 一度も書き出していなければ、ゲームの翻訳ファイルを作らずに書き出しを促す
    component.setting->isFirstExported = false;
    WriteModeComponent writeMode(&component, nullptr);
    auto parts = FindParts(writeMode);
    component.history->push(new QUndoCommand("edit"));
    const auto gameBefore = Snapshot(gameDir);

    bool asked = false;
    QTimer::singleShot(0, [&asked]() {
        if(auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            asked = true;
            box->accept();
        }
    });
    parts.apply->setEnabled(true);
    parts.apply->click();
    EXPECT_TRUE(asked);
    EXPECT_EQ(ChangedFiles(gameBefore, Snapshot(gameDir)), QStringList{});
}

TEST_F(RemoteHubTest, FrameFollowsShownText)
{
    // ゲームで表示している文章の行・言語の列に枠を付ける
    WriteModeComponent writeMode(&component, nullptr);
    writeMode.show();
    auto parts = FindParts(writeMode);
    WriteOwnPid();
    WriteCurrent({});
    ASSERT_TRUE(WaitUntil([&]() { return parts.follow->isEnabled(); }, 3000));
    parts.follow->click();

    // 未翻訳の行は原文の列
    WriteCurrent({ShownText("A", "Actors.csv")});
    ASSERT_TRUE(WaitUntil([&]() { return FramedCell(parts.editor) == std::pair<QString, QString>{"A", "original"}; }, 5000));

    // 翻訳がある行は、ゲームの言語の列
    auto* model = const_cast<QAbstractItemModel*>(SourceModel(parts.editor));
    int enColumn = -1;
    for(int c = 0; c < model->columnCount(); ++c) {
        if(model->headerData(c, Qt::Horizontal, Qt::UserRole).toString() == "en") { enColumn = c; }
    }
    ASSERT_GE(enColumn, 0);
    ASSERT_TRUE(model->setData(model->index(1, enColumn), "Bee"));
    WriteCurrent({ShownText("B", "Actors.csv")});
    ASSERT_TRUE(WaitUntil([&]() { return FramedCell(parts.editor) == std::pair<QString, QString>{"B", "en"}; }, 5000));

    // VXAce の改行コード \r\n でも同じ行
    WriteCurrent({ShownText("Line1\r\nLine2", "Actors.csv")});
    EXPECT_TRUE(WaitUntil([&]() { return FramedCell(parts.editor).first == "Line1\nLine2"; }, 5000));

    // 表に無い文章・翻訳ファイルの分からない文章では枠を消す
    WriteCurrent({ShownText("C", "Actors.csv")});
    EXPECT_TRUE(WaitUntil([&]() { return parts.editor->framedCell().isValid() == false; }, 5000));
    WriteCurrent({ShownText("A", "Actors.csv")});
    ASSERT_TRUE(WaitUntil([&]() { return FramedCell(parts.editor).first == "A"; }, 5000));
    WriteCurrent({ShownText("A", "")});
    EXPECT_TRUE(WaitUntil([&]() { return parts.editor->framedCell().isValid() == false; }, 5000));

    // 切断したら枠を消す
    WriteCurrent({ShownText("A", "Actors.csv")});
    ASSERT_TRUE(WaitUntil([&]() { return FramedCell(parts.editor).first == "A"; }, 5000));
    parts.follow->click();
    EXPECT_FALSE(parts.editor->framedCell().isValid());
}

TEST_F(RemoteHubTest, SelectedTextIsPreferred)
{
    // カーソルの当たっている項目 > メッセージ > カーソルの無い選択肢 の順に選ぶ
    WriteModeComponent writeMode(&component, nullptr);
    writeMode.show();
    auto parts = FindParts(writeMode);
    WriteOwnPid();
    WriteCurrent({});
    ASSERT_TRUE(WaitUntil([&]() { return parts.follow->isEnabled(); }, 3000));
    parts.follow->click();

    // 選択肢にカーソルが無ければメッセージ
    WriteCurrent({ShownText("A", "Actors.csv"), ShownText("B", "Actors.csv", true)});
    ASSERT_TRUE(WaitUntil([&]() { return FramedCell(parts.editor).first == "A"; }, 5000));
    // カーソルの当たっている選択肢
    WriteCurrent({ShownText("B", "Actors.csv"), ShownText("A", "Actors.csv", true, true), ShownText("B", "Actors.csv", true)});
    ASSERT_TRUE(WaitUntil([&]() { return FramedCell(parts.editor).first == "A"; }, 5000));
    // 翻訳ファイルの分からないものは選ばない
    WriteCurrent({ShownText("B", "Actors.csv"), ShownText("A", "", true, true)});
    EXPECT_TRUE(WaitUntil([&]() { return FramedCell(parts.editor).first == "B"; }, 5000));
}

TEST(RemoteHubFrame, FramedCellIsDrawnGreen)
{
    // 枠は選択とは別に、そのセルの内側に緑で描くこと。他のモデルの位置では描かない
    QTemporaryDir tempDir;
    const auto jsonPath = tempDir.filePath("Actors.json");
    WriteFile(jsonPath, R"([{"original": "A", "type": ["name"]}, {"original": "B", "type": ["name"]}])");
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(jsonPath));
    ComponentBase component;
    CSVEditor editor(&component);
    editor.setModel(&model);
    editor.resize(400, 200);

    const auto target = model.index(1, 0);
    editor.setFramedCell(target);
    const auto rect = editor.visualRect(target);
    const auto image = editor.viewport()->grab().toImage();
    EXPECT_EQ(image.pixelColor(rect.left() + 1, rect.center().y()), QColor(0, 200, 83));
    //他の行には描かない
    const auto other = editor.visualRect(model.index(0, 0));
    EXPECT_NE(image.pixelColor(other.left() + 1, other.center().y()), QColor(0, 200, 83));

    //他のモデルの位置を渡しても描かず、落ちない
    langscore::CSVEditorTableModel otherModel;
    ASSERT_TRUE(otherModel.loadFromJsonFile(jsonPath));
    editor.setFramedCell(otherModel.index(1, 0));
    const auto otherImage = editor.viewport()->grab().toImage();
    EXPECT_NE(otherImage.pixelColor(rect.left() + 1, rect.center().y()), QColor(0, 200, 83));
}

TEST(RemoteHubDialog, RemoteHubCheckBoxEnablesWrite)
{
    // デバッグ用のプラグインの追加・削除のみでも書き出せる。元に戻したら書き出せない
    auto setting = std::make_shared<settings>();
    setting->isPluginRegistered = true;
    setting->writeObj.overwriteLangscore = false;
    setting->writeObj.overwriteLangscoreCustom = false;
    setting->writeObj.enableRemoteHub = false;
    UpdatePluginDialog dialog(setting);
    auto* check = dialog.findChild<QCheckBox*>("enableRemoteHub");
    auto* buttons = dialog.findChild<QDialogButtonBox*>();
    ASSERT_NE(check, nullptr);
    ASSERT_NE(buttons, nullptr);
    auto* ok = buttons->button(QDialogButtonBox::Ok);
    EXPECT_FALSE(ok->isEnabled());

    check->click();
    EXPECT_TRUE(setting->writeObj.enableRemoteHub);
    EXPECT_TRUE(ok->isEnabled());
    check->click();
    EXPECT_FALSE(setting->writeObj.enableRemoteHub);
    EXPECT_FALSE(ok->isEnabled());
}

TEST(RemoteHubDialog, AddPluginNeedsNoOverwrite)
{
    // プラグインの追加では上書きの項目を表示せず、何も選ばなくても書き出せる
    auto setting = std::make_shared<settings>();
    setting->isPluginRegistered = false;
    setting->writeObj.overwriteLangscore = false;
    setting->writeObj.overwriteLangscoreCustom = false;
    UpdatePluginDialog dialog(setting);
    auto* buttons = dialog.findChild<QDialogButtonBox*>();
    ASSERT_NE(buttons, nullptr);
    EXPECT_TRUE(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    for(const char* name : {"updateLsScript", "updateLsCustomScript"}) {
        auto* check = dialog.findChild<QCheckBox*>(name);
        ASSERT_NE(check, nullptr) << name;
        EXPECT_FALSE(check->isVisibleTo(&dialog)) << name;
    }
}

TEST(RemoteHubDialog, HoveredOptionIsDescribed)
{
    // オプションにマウスを乗せると説明欄にそのオプションの説明を表示し、離すと消す
    auto setting = std::make_shared<settings>();
    setting->isPluginRegistered = true;
    UpdatePluginDialog dialog(setting);
    auto* description = dialog.findChild<QLabel*>("descriptionLabel");
    ASSERT_NE(description, nullptr);
    for(auto* check : dialog.findChildren<QCheckBox*>()) {
        EXPECT_FALSE(check->statusTip().isEmpty()) << check->objectName().toStdString();
        QEnterEvent enter(QPointF(1, 1), QPointF(1, 1), QPointF(1, 1));
        QCoreApplication::sendEvent(check, &enter);
        EXPECT_EQ(description->text(), check->statusTip()) << check->objectName().toStdString();
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(check, &leave);
        EXPECT_TRUE(description->text().isEmpty()) << check->objectName().toStdString();
    }
}

