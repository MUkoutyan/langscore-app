#include <QtTest>
#include <QCoreApplication>
#include "invoker.h"
#include "src/settings.h"
#include "src/ui/WriteDialog.h"
#include "ui_WriteDialog.h"
#include "MainWindow.h"
#include "AnalyzeDialog.h"
#include "ui_AnalyzeDialog.h"
#include "src/ui/LanguageSelectComponent.h"
#include "src/ui/WriteModeComponent.h"
#include "src/utility.hpp"
#include "ui_WriteModeComponent.h"
#include "csv.hpp"


// add necessary includes here


class LangscoreAppTest : public QObject
{
    Q_OBJECT

public:
    LangscoreAppTest();
    ~LangscoreAppTest();

private slots:
    void initTestCase()
    {
        settings->gameProjectPath = "./TestProject";
        settings->langscoreProjectDirectory = "./TestProject_langscore";
        settings->writeObj.exportDirectory = "../TestProject/Translates";
    }
    void cleanupTestCase()
    {
        settings = nullptr;
    }

    void utility_withoutExtension();


    void writeDialog_writeByLangage();
    void writeDialog_outputPath();
    void writeDialog_duplicate();
    void writeDialog_disableWriteButton();

    void setting_load();

    void langselect_followDefault();

    void analyzeMode_getProjectType();

    void editMode_validLoad();
    void editMode_changeTreeScriptCheckState();
    void editMode_changeTreeGraphicsCheckState();
    void editMode_fetchScriptItemMethod();
    void editMode_changeTableCheckState();
    void editMode_hideScriptTable();
    void editMode_exportFile();

    void checkConfig_pictures();

private:
    std::shared_ptr<settings> settings;
};

LangscoreAppTest::LangscoreAppTest()
    : settings(new class settings())
{
    if(QDir(qApp->applicationDirPath()+"/Project1_langscore").exists()){
        QDir(qApp->applicationDirPath()+"/Project1_langscore").removeRecursively();
    }
}

LangscoreAppTest::~LangscoreAppTest()
{

}

void LangscoreAppTest::utility_withoutExtension()
{
    QCOMPARE(langscore::withoutExtension("Hoge.rb"), "Hoge");
    QCOMPARE(langscore::withoutExtension("8598120385"), "8598120385");
    QCOMPARE(langscore::withoutExtension("File.Name.Ext"), "File.Name");
}

void LangscoreAppTest::writeDialog_writeByLangage()
{
    WriteDialog dialog(settings, nullptr);

    QTest::mouseClick(dialog.ui->exportByLangCheck, Qt::LeftButton);
    QVERIFY(dialog.writeByLanguage() == true);
    QTest::mouseClick(dialog.ui->exportByLangCheck, Qt::LeftButton);
    QVERIFY(dialog.writeByLanguage() == false);

}
void LangscoreAppTest::writeDialog_outputPath()
{
    WriteDialog dialog(settings, nullptr);
    auto lsProjDir = QDir(settings->langscoreProjectDirectory);
    auto writePath = lsProjDir.cleanPath(lsProjDir.absoluteFilePath(settings->writeObj.exportDirectory));

    QCOMPARE(dialog.ui->lineEdit->text(), writePath);

    auto dialogOutPath = dialog.outputPath();
    QVERIFY(QFileInfo(dialogOutPath).isAbsolute());
    QCOMPARE(dialogOutPath, writePath);
}

void LangscoreAppTest::writeDialog_duplicate()
{
    QDir gameProjDir(settings->gameProjectPath+"/Translates");
    QVERIFY(QDir().mkpath(gameProjDir.absolutePath()));
    QFile file(gameProjDir.absolutePath()+"/Hoge.csv");
    QVERIFY(file.open(QFile::WriteOnly|QFile::Text));
    file.write("Hoge");
    file.close();

    WriteDialog dialog(settings, nullptr);
    dialog.show();
    QVERIFY(dialog.ui->baseWidget->isVisible());
    QTest::keyClicks(dialog.ui->lineEdit, "_none");
    QVERIFY(dialog.ui->baseWidget->isVisible() == false);
    dialog.close();

    file.remove();
    gameProjDir.rmpath(gameProjDir.absolutePath());
}

void LangscoreAppTest::writeDialog_disableWriteButton()
{
    QDir gameProjDir(settings->gameProjectPath+"/Translates");
    QVERIFY(QDir().mkpath(gameProjDir.absolutePath()));
    QFile file(gameProjDir.absolutePath()+"/Hoge.csv");
    QVERIFY(file.open(QFile::WriteOnly|QFile::Text));
    file.write("Hoge");
    file.close();

    WriteDialog dialog(settings, nullptr);
    dialog.show();
    QVERIFY(dialog.ui->buttonBox->button(QDialogButtonBox::Ok)->isEnabled() == false);
    QTest::mouseClick(dialog.ui->overwriteMode, Qt::LeftButton);
    QVERIFY(dialog.ui->buttonBox->button(QDialogButtonBox::Ok)->isEnabled());
    dialog.close();

    file.remove();
    gameProjDir.rmpath(gameProjDir.absolutePath());
}

void LangscoreAppTest::setting_load()
{
    class settings setting;
    setting.load("./config.json");

    QDir gameProjDir("./GameProj");
    QVERIFY(QDir().mkpath(gameProjDir.absolutePath()));

    const auto basePath = qApp->applicationDirPath();
    const auto gameProjPath = basePath+"/GameProj";
    const auto lsProjPath = basePath+"/GameProj_langscore";

//    setting.setGameProjectPath(QDir("./GameProj").absolutePath());
    setting.gameProjectPath = QDir("./GameProj").absolutePath();
    setting.projectType = settings::ProjectType::VXAce;
    setting.updateLangscoreProjectPath();
    QCOMPARE(setting.gameProjectPath, gameProjPath);

    QCOMPARE(setting.translateDirectoryPath(), gameProjPath+"/Data/Translate");
    QCOMPARE(setting.tempGraphicsFileDirectoryPath(), gameProjPath+"/Graphics");
    QCOMPARE(setting.tempFileDirectoryPath(), lsProjPath+"/analyze");
    QCOMPARE(setting.tempScriptFileDirectoryPath(), lsProjPath+"/analyze/Scripts");

    QCOMPARE(setting.defaultLanguage, "ja");
    QCOMPARE(setting.languages.size(), 10);
    for(const auto& lang : setting.languages){
        if(lang.languageName == "en"){
            QVERIFY(lang.enable);
            QCOMPARE(lang.font.name,  "VL Gothic");
            QCOMPARE(lang.font.size,  20);
        }
        else if(lang.languageName == "es"){
            QVERIFY(lang.enable == false);
            QCOMPARE(lang.font.name,  "VL Gothic");
            QCOMPARE(lang.font.size,  22);
        }
        else if(lang.languageName == "de"){
            QVERIFY(lang.enable == false);
            QCOMPARE(lang.font.name,  "VL Gothic");
            QCOMPARE(lang.font.size,  22);
        }
        else if(lang.languageName == "fr"){
            QVERIFY(lang.enable == false);
            QCOMPARE(lang.font.name,  "VL Gothic");
            QCOMPARE(lang.font.size,  22);
        }
        else if(lang.languageName == "it"){
            QVERIFY(lang.enable == false);
            QCOMPARE(lang.font.name,  "VL Gothic");
            QCOMPARE(lang.font.size,  22);
        }
        else if(lang.languageName == "ja"){
            QVERIFY(lang.enable);
            QCOMPARE(lang.font.name,  "KsoEikai");
            QCOMPARE(lang.font.size,  23);
        }
        else if(lang.languageName == "ko"){
            QVERIFY(lang.enable);
            QCOMPARE(lang.font.name,  "Shippori Mincho Ryu Regular");
            QCOMPARE(lang.font.size,  22);
        }
        else if(lang.languageName == "ru"){
            QVERIFY(lang.enable == false);
            QCOMPARE(lang.font.name,  "VL Gothic");
            QCOMPARE(lang.font.size,  22);
        }
        else if(lang.languageName == "zh-cn"){
            QVERIFY(lang.enable);
            QCOMPARE(lang.font.name,  "Source Han Sans HW HC");
            QCOMPARE(lang.font.size,  24);
        }
        else if(lang.languageName == "zh-tw"){
            QVERIFY(lang.enable);
            QCOMPARE(lang.font.name, "VL Gothic");
            QCOMPARE(lang.font.size, 24);
        }
    } //for(const auto& lang : setting.languages)

    QCOMPARE(setting.writeObj.exportByLanguage, false);
    QCOMPARE(setting.writeObj.exportDirectory, "../GameProj/Translate");
    auto path = QDir(lsProjPath).absoluteFilePath(setting.writeObj.exportDirectory);
    path = QDir().cleanPath(path);
    QCOMPARE(path, gameProjPath+"/Translate");

    gameProjDir.rmpath(gameProjDir.absolutePath());
}

void LangscoreAppTest::langselect_followDefault()
{
    MainWindow window;
    auto* analyzeDialog = window.analyzeDialog;
    auto* component = window.writeUi;

    analyzeDialog->setting->defaultLanguage = "en";
    LanguageSelectComponent langComponents(QLocale(QLocale::Japanese), component, component);

    langComponents.setDefault(true);
    QVERIFY(langComponents.button->isChecked());

}

void LangscoreAppTest::analyzeMode_getProjectType()
{
    auto result = settings::getProjectType(qApp->applicationDirPath()+"/Project1");
    QCOMPARE(result, settings::VXAce);
    result = settings::getProjectType(qApp->applicationDirPath()+"/DummyProject");
    QCOMPARE(result, settings::None);
}

void LangscoreAppTest::editMode_validLoad()
{
    MainWindow window;
    auto* analyzeDialog = window.analyzeDialog;
    auto* component = window.writeUi;

    if(QDir(qApp->applicationDirPath()+"/Project1_langscore").exists()){
        QDir(qApp->applicationDirPath()+"/Project1_langscore").removeRecursively();
    }

    analyzeDialog->openFile(qApp->applicationDirPath()+"/Project1");
    window.show();
    bool finished = false;
    connect(analyzeDialog, &AnalyzeDialog::toWriteMode, this, [&finished](QString){ finished = true; });

    QTest::mouseClick(analyzeDialog->ui->analyzeButton, Qt::LeftButton);
    while(finished == false){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        qApp->processEvents();
    }

    const auto numTopLevelItems = component->ui->treeWidget->topLevelItemCount();
    QCOMPARE(numTopLevelItems, 3);
    for(int i=0; i<numTopLevelItems; ++i){
        auto topLevelItem = component->ui->treeWidget->topLevelItem(i);
        auto text = topLevelItem->text(0);
        switch(i){
        case 0:
            QCOMPARE(text, "Main");
            QCOMPARE(topLevelItem->childCount(), 11);
            break;
        case 1:
            QCOMPARE(text, "Script");
            QCOMPARE(topLevelItem->childCount(), 31);
            break;
        case 2:
            QCOMPARE(text, "Graphics");
            QCOMPARE(topLevelItem->childCount(), 12);
            break;
        }
    }

    const auto numScriptTable = component->ui->tableWidget_script->rowCount();
    QVERIFY(0 < numScriptTable);

    QStringList scriptNameList;
    QStringList scriptFileNameList;
    for(int i=0; i<numScriptTable; ++i)
    {
        if(auto item = component->ui->tableWidget_script->item(i, 1)){
            if(scriptNameList.contains(item->text()) == false){
                scriptNameList.emplace_back(item->text());
                scriptFileNameList.emplace_back(item->data(Qt::UserRole).toString());
            }
        }
    }

    auto scriptList = langscore::readCsv(qApp->applicationDirPath()+"/Project1_langscore/analyze/Scripts/_list.csv");
    for(auto& scriptName : scriptNameList)
    {
        auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [scriptName](const auto& x){
            return x[1] == scriptName;
        });
        if(result == scriptList.cend()){
            QFAIL((QString("Not Found ScriptName : ") + scriptName).toStdString().c_str());
        }
    }
    for(auto& scriptFileName : scriptFileNameList)
    {
        auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [scriptFileName](const auto& x){
            return x[0] == scriptFileName;
        });
        if(result == scriptList.cend()){
            QFAIL((QString("Not Found Script FileName : ") + scriptFileName).toStdString().c_str());
        }
    }

    auto graphItem = component->ui->treeWidget->topLevelItem(2);
    for(int i=0; i<graphItem->childCount(); ++i)
    {
        auto folder = graphItem->child(i);
        for(int j=0; j<folder->childCount(); ++j)
        {
            auto picture = folder->child(j);
            auto path = picture->data(1, Qt::UserRole).toString();
            QVERIFY(QFileInfo(path).isRelative());
        }
    }
}

void LangscoreAppTest::editMode_fetchScriptItemMethod()
{
    MainWindow window;
    auto* analyzeDialog = window.analyzeDialog;
    auto* component = window.writeUi;

    if(QDir(qApp->applicationDirPath()+"/Project1_langscore").exists()){
        QDir(qApp->applicationDirPath()+"/Project1_langscore").removeRecursively();
    }

    analyzeDialog->openFile(qApp->applicationDirPath()+"/Project1");
    window.show();
    bool finished = false;
    connect(analyzeDialog, &AnalyzeDialog::toWriteMode, this, [&finished](QString){ finished = true; });
    QTest::mouseClick(analyzeDialog->ui->analyzeButton, Qt::LeftButton);
    while(finished == false){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        qApp->processEvents();
    }

    QString scriptName = "Cache";
    auto rows = component->fetchScriptTableSameFileRows(scriptName);
    QCOMPARE(rows.size(), 12);
    for(auto r : rows)
    {
        auto item = component->ui->tableWidget_script->item(r, 1);
        QVERIFY(item != nullptr);
        QCOMPARE(item->text(), scriptName);
    }

    auto treeItem = component->fetchScriptTreeSameFileItem(scriptName);
    QVERIFY(treeItem != nullptr);
    QCOMPARE(treeItem->parent(), component->ui->treeWidget->topLevelItem(1));
    QCOMPARE(treeItem->text(1), scriptName);
}

void LangscoreAppTest::editMode_changeTreeScriptCheckState()
{
    MainWindow window;
    auto* analyzeDialog = window.analyzeDialog;
    auto* component = window.writeUi;

    if(QDir(qApp->applicationDirPath()+"/Project1_langscore").exists()){
        QDir(qApp->applicationDirPath()+"/Project1_langscore").removeRecursively();
    }

    analyzeDialog->openFile(qApp->applicationDirPath()+"/Project1");
    analyzeDialog->invokeAnalyze();
    window.show();
    bool finished = false;
    connect(analyzeDialog, &AnalyzeDialog::toWriteMode, this, [&finished](QString){ finished = true; });
    while(finished == false){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        qApp->processEvents();
    }

    auto scriptItem = component->ui->treeWidget->topLevelItem(1);
    QCOMPARE(scriptItem->childCount(), 31);
    auto vocabItem = scriptItem->child(0);
    QVERIFY(vocabItem);
    auto beginNumHistory = window.history->count();
    vocabItem->setCheckState(0, Qt::Unchecked);
    auto afterNumHistory = window.history->count();
    QVERIFY(beginNumHistory != afterNumHistory);
    QCOMPARE(vocabItem->text(1), "Vocab");
    auto fileName = vocabItem->data(1, Qt::UserRole).toString();
    QCOMPARE(fileName, "97506006");

    const auto numScriptTable = component->ui->tableWidget_script->rowCount();
    QVERIFY(0 < numScriptTable);

    bool findScript = false;
    for(int i=0; i<numScriptTable; ++i)
    {
        auto item = component->ui->tableWidget_script->item(i, 1);
        if(item == nullptr){ continue; }
        if(item->data(Qt::UserRole).toString() != fileName){ continue; }

        findScript = true;
        QCOMPARE(item->foreground().color(), QColor("#5a5a5a"));
    }

    QVERIFY(findScript);
}

void LangscoreAppTest::editMode_changeTreeGraphicsCheckState()
{
    MainWindow window;
    auto* analyzeDialog = window.analyzeDialog;
    auto* component = window.writeUi;

    if(QDir(qApp->applicationDirPath()+"/Project1_langscore").exists()){
        QDir(qApp->applicationDirPath()+"/Project1_langscore").removeRecursively();
    }

    analyzeDialog->openFile(qApp->applicationDirPath()+"/Project1");
    window.show();

    bool finished = false;
    connect(analyzeDialog, &AnalyzeDialog::toWriteMode, this, [&finished](QString){ finished = true; });
    QTest::mouseClick(analyzeDialog->ui->analyzeButton, Qt::LeftButton);
    while(finished == false){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        qApp->processEvents();
    }

    auto graphicsItem = component->ui->treeWidget->topLevelItem(2);

    auto numFolder = graphicsItem->childCount();
    for(int i=0; i<numFolder; ++i){
        auto folderItem = graphicsItem->child(i);
        if(folderItem->childCount() == 1){
            auto pictureItem = folderItem->child(0);
            pictureItem->setCheckState(0, Qt::Unchecked);
            QVERIFY(folderItem->checkState(0) == Qt::Unchecked);
        }
        else if(1 < folderItem->childCount())
        {
            auto pictureItem = folderItem->child(0);
            pictureItem->setCheckState(0, Qt::Unchecked);
            QVERIFY(folderItem->checkState(0) == Qt::PartiallyChecked);
        }
    }
}

void LangscoreAppTest::editMode_changeTableCheckState()
{
    MainWindow window;
    auto* analyzeDialog = window.analyzeDialog;
    auto* component = window.writeUi;

    if(QDir(qApp->applicationDirPath()+"/Project1_langscore").exists()){
        QDir(qApp->applicationDirPath()+"/Project1_langscore").removeRecursively();
    }


    analyzeDialog->openFile(qApp->applicationDirPath()+"/Project1");
    window.show();
    bool finished = false;
    connect(analyzeDialog, &AnalyzeDialog::toWriteMode, this, [&finished](QString){ finished = true; });
    QTest::mouseClick(analyzeDialog->ui->analyzeButton, Qt::LeftButton);
    while(finished == false){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        qApp->processEvents();
    }

    auto scriptName = "Cache";
    auto treeItem = component->fetchScriptTreeSameFileItem(scriptName);
    auto rows = component->fetchScriptTableSameFileRows(scriptName);

    QVERIFY(rows.empty() == false);

    auto scriptNameItem = component->ui->tableWidget_script->item(rows[1], 1);
    QCOMPARE(scriptNameItem->text(), scriptName);
    auto checkItem = component->ui->tableWidget_script->item(rows[1], 0);
    checkItem->setCheckState(Qt::Unchecked);

    QVERIFY(treeItem->checkState(0) == Qt::PartiallyChecked);

    const auto scriptInfo = window.setting->fetchScriptInfo(scriptNameItem->data(Qt::UserRole).toString());
    QCOMPARE(scriptInfo.ignorePoint.size(), 1);

    for(auto r : rows){
        auto i = component->ui->tableWidget_script->item(r, 0);
        i->setCheckState(Qt::Unchecked);
    }
    QVERIFY(treeItem->checkState(0) == Qt::Unchecked);
    QVERIFY(scriptInfo.ignore);

}

void LangscoreAppTest::editMode_hideScriptTable()
{
    MainWindow window;
    auto* analyzeDialog = window.analyzeDialog;
    auto* component = window.writeUi;

    if(QDir(qApp->applicationDirPath()+"/Project1_langscore").exists()){
        QDir(qApp->applicationDirPath()+"/Project1_langscore").removeRecursively();
    }


    analyzeDialog->openFile(qApp->applicationDirPath()+"/Project1");
    window.show();
    bool finished = false;
    connect(analyzeDialog, &AnalyzeDialog::toWriteMode, this, [&finished](QString){ finished = true; });
    QTest::mouseClick(analyzeDialog->ui->analyzeButton, Qt::LeftButton);
    while(finished == false){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        qApp->processEvents();
    }

    auto scriptTreeItem = component->ui->treeWidget->topLevelItem(1);
    //一旦全部無効
    for(int i=0; i<scriptTreeItem->childCount(); ++i)
    {
        auto item = scriptTreeItem->child(i);
        item->setCheckState(0, Qt::Unchecked);
    }
    //特定のアイテム1つをチェック
    const auto targetScriptName = "Cache";
    auto targetTreeItem = component->fetchScriptTreeSameFileItem(targetScriptName);
    targetTreeItem->setCheckState(0, Qt::Checked);

    auto actions = component->ui->scriptFilterButton->actions();
    actions[1]->trigger();

    QVERIFY(component->showAllScriptContents == false);

    auto numRow = component->ui->tableWidget_script->rowCount();
    for(int i=0; i<numRow; ++i)
    {
        auto scriptNameItem = component->ui->tableWidget_script->item(i, 1);
        //ここでnullになる場合はテーブルの要素数にcsvのヘッダーを混ぜている可能性がある。
        QVERIFY(scriptNameItem != nullptr);
        auto scriptName = scriptNameItem->text();
        QCOMPARE(scriptName, targetScriptName);
    }
}

void LangscoreAppTest::editMode_exportFile()
{
    MainWindow window;
    auto* analyzeDialog = window.analyzeDialog;
    auto* component = window.writeUi;

    if(QDir(qApp->applicationDirPath()+"/Project1_langscore").exists()){
        QDir(qApp->applicationDirPath()+"/Project1_langscore").removeRecursively();
    }

    if(QDir(qApp->applicationDirPath()+"/Project1/Data/Translate").exists()){
        QDir(qApp->applicationDirPath()+"/Project1/Data/Translate").removeRecursively();
    }


    analyzeDialog->openFile(qApp->applicationDirPath()+"/Project1");
    window.show();
    bool finished = false;
    connect(analyzeDialog, &AnalyzeDialog::toWriteMode, this, [&finished](QString){ finished = true; });
    QTest::mouseClick(analyzeDialog->ui->analyzeButton, Qt::LeftButton);
    while(finished == false){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        qApp->processEvents();
    }

    QDir lsProjDir(window.setting->langscoreProjectDirectory);
    auto relativePath = lsProjDir.relativeFilePath(qApp->applicationDirPath()+"/Project1/Data/Translate");
    window.setting->writeObj.exportDirectory = relativePath;

    invoker invoker(component);
    finished = false;
    int exitCode = 0;
    connect(&invoker, &invoker::finish, this, [&finished, &exitCode](int _exitCode){
        finished = true;
        exitCode = _exitCode;
        if(exitCode != 0){
            QFile(qApp->applicationDirPath()+"/editMode_exportFile_failed.json").remove();
            QFile(qApp->applicationDirPath()+"/Project1_langscore/config.json").copy(qApp->applicationDirPath()+"/editMode_exportFile_failed.json");
            QFAIL("Failed Exit Code");
        }
    });
    invoker.write(true);

}

void LangscoreAppTest::checkConfig_pictures()
{
    {
        MainWindow window;
        auto* analyzeDialog = window.analyzeDialog;
        auto* component = window.writeUi;

        if(QDir(qApp->applicationDirPath()+"/Project1_langscore").exists()){
            QDir(qApp->applicationDirPath()+"/Project1_langscore").removeRecursively();
        }


        analyzeDialog->openFile(qApp->applicationDirPath()+"/Project1");
        window.show();
        bool finished = false;
        connect(analyzeDialog, &AnalyzeDialog::toWriteMode, this, [&finished](QString){ finished = true; });
        QTest::mouseClick(analyzeDialog->ui->analyzeButton, Qt::LeftButton);
        while(finished == false){
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            qApp->processEvents();
        }

        auto graphicsItem = component->ui->treeWidget->topLevelItem(2);
        auto numFolder = graphicsItem->childCount();
        for(int i=0; i<numFolder; ++i)
        {
            auto folderItem = graphicsItem->child(i);
            if(folderItem->text(1) != "Pictures"){ continue; }

            folderItem->setCheckState(0, Qt::Unchecked);
            break;
        }

        auto& ignorePicturePath = window.setting->writeObj.ignorePicturePath;
        QCOMPARE(ignorePicturePath.size(), 2);
        QVERIFY(std::find(ignorePicturePath.cbegin(), ignorePicturePath.cend(), "Pictures/nantoka8.jpg") != ignorePicturePath.cend());
        QVERIFY(std::find(ignorePicturePath.cbegin(), ignorePicturePath.cend(), "Pictures/nantoka10.jpg") != ignorePicturePath.cend());
    }

    {
        MainWindow window;
        auto* analyzeDialog = window.analyzeDialog;
        auto* component = window.writeUi;

        if(QDir(qApp->applicationDirPath()+"/Project1_langscore").exists()){
            QDir(qApp->applicationDirPath()+"/Project1_langscore").removeRecursively();
        }


        analyzeDialog->openFile(qApp->applicationDirPath()+"/Project1");
        window.show();
        bool finished = false;
        connect(analyzeDialog, &AnalyzeDialog::toWriteMode, this, [&finished](QString){ finished = true; });
        QTest::mouseClick(analyzeDialog->ui->analyzeButton, Qt::LeftButton);
        while(finished == false){
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            qApp->processEvents();
        }

        auto graphicsItem = component->ui->treeWidget->topLevelItem(2);
        auto numFolder = graphicsItem->childCount();
        for(int i=0; i<numFolder; ++i)
        {
            auto folderItem = graphicsItem->child(i);
            if(folderItem->text(1) != "Pictures"){ continue; }

            QCOMPARE(folderItem->childCount(), 2);

            folderItem->child(1)->setCheckState(0, Qt::Unchecked);
            break;
        }

        auto& ignorePicturePath = window.setting->writeObj.ignorePicturePath;
        QCOMPARE(ignorePicturePath.size(), 1);
        QVERIFY(std::find(ignorePicturePath.cbegin(), ignorePicturePath.cend(), "Pictures/nantoka8.jpg") != ignorePicturePath.cend());
    }
}

QTEST_MAIN(LangscoreAppTest)

#include "tst_langscoreapptest.moc"
