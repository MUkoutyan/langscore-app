#include "test_common.h"
#include <QTreeWidget>
#include <QCheckBox>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QImage>
#include "FileTree.h"
#include "ValidationFilterButton.h"
#include <QToolButton>
#include <QMenu>
#include "EditDataManager.h"

// FileTree.cpp の UserRole::FILE_NAME / VISIBLE と同じ値
constexpr int ItemTypeRole = Qt::UserRole + 1;
constexpr int FileNameRole = Qt::UserRole + 2;
constexpr int VisibleRole = Qt::UserRole + 4;

// runtimeData を参照するために公開する
struct TestComponent : public ComponentBase
{
    using ComponentBase::runtimeData;
    using ComponentBase::setting;
    using ComponentBase::addDispatch;
    using ComponentBase::dispatch;
    using ComponentBase::history;
};

class FileTreeTest : public ::testing::Test
{
protected:
    std::unique_ptr<TestComponent> component;
    std::shared_ptr<EditDataManager> manager;
    std::unique_ptr<FileTree> tree;

    void SetUp() override
    {
        component = std::make_unique<TestComponent>();
        manager = std::make_shared<EditDataManager>();
        tree = std::make_unique<FileTree>(component.get(), manager);
    }

    void TearDown() override
    {
        tree.reset();
        manager.reset();
        component.reset();
    }
};

// 不具合の再発防止
TEST_F(FileTreeTest, SearchShowsVisibleItems)
{
    // 検索時、表示設定の項目は一致すれば表示され、非表示設定の項目は隠れること
    auto* treeWidget = tree->findChild<QTreeWidget*>();
    auto* searchEdit = tree->findChild<QLineEdit*>();
    ASSERT_NE(treeWidget, nullptr);
    ASSERT_NE(searchEdit, nullptr);

    auto* top = new QTreeWidgetItem(treeWidget, {"Basics"});
    auto* visibleItem = new QTreeWidgetItem(top, {"Actors"});
    visibleItem->setData(0, VisibleRole, true);
    auto* hiddenItem = new QTreeWidgetItem(top, {"ActorsHidden"});
    hiddenItem->setData(0, VisibleRole, false);

    searchEdit->setText("Actor");

    EXPECT_FALSE(visibleItem->isHidden());
    EXPECT_TRUE(hiddenItem->isHidden());
}

TEST_F(FileTreeTest, ValidationErrorShownOnFileWithExtension)
{
    // 検証結果は拡張子なしの名前（Actors）で届くが、拡張子付きの項目（Actors.json）に表示されること。
    // 次の検証が終わったら、前回の表示は消えること
    auto* treeWidget = tree->findChild<QTreeWidget*>();
    ASSERT_NE(treeWidget, nullptr);
    auto* top = new QTreeWidgetItem(treeWidget, {"Basics"});
    auto* actors = new QTreeWidgetItem(top, {"Actors"});
    actors->setData(0, FileNameRole, "Actors.json");

    ValidationErrorInfo info;
    info.type = ValidationErrorInfo::Error;
    info.summary = ValidationErrorInfo::EmptyCol;
    info.row = 3;
    info.language = "en";
    component->runtimeData->errors["Actors"].push_back(info);
    component->runtimeData->updateList["Actors"] = true;

    component->addDispatch(tree.get());
    component->dispatch(ComponentBase::NotifyFinishValidateCSV, {});
    EXPECT_EQ(actors->childCount(), 1) << "Actors.json の項目に検証結果が表示されていません";

    //次の検証で結果が無くなったら（ValidationService が消す）、終わったときに表示も消えること
    component->runtimeData->errors.clear();
    component->dispatch(ComponentBase::NotifyFinishValidateCSV, {});
    EXPECT_EQ(actors->childCount(), 0) << "次の検証の後も前回の検証結果が残っています";
    EXPECT_TRUE(actors->icon(0).isNull());
}

TEST_F(FileTreeTest, IgnorePictureWrittenInDivisiFormat)
{
    // 画像のチェックを外すと、divisi が比較する形式（ゲームフォルダからの相対パス・拡張子なし）で除外リストに入ること
    QTemporaryDir tempDir;
    const auto gameDir = tempDir.filePath("Game");
    QDir().mkpath(gameDir + "/img/faces");
    QFile projectFile(gameDir + "/Game.rmmzproject");
    ASSERT_TRUE(projectFile.open(QFile::WriteOnly));
    projectFile.close();
    QImage image(1, 1, QImage::Format_RGB32);
    image.fill(Qt::white);
    ASSERT_TRUE(image.save(gameDir + "/img/faces/Actor1.png"));

    //画像一覧は設定の読み込み時に作られる
    auto& setting = *component->setting;
    setting.setGameProjectPath(gameDir);
    const auto configPath = setting.langscoreProjectDirectory + "/config.json";
    setting.write(configPath);
    setting.load(configPath);
    tree->setupGraphicsTree();

    auto* treeWidget = tree->findChild<QTreeWidget*>();
    ASSERT_NE(treeWidget, nullptr);
    const auto found = treeWidget->findItems("Actor1.png", Qt::MatchExactly | Qt::MatchRecursive, 0);
    ASSERT_EQ(found.size(), 1);
    found[0]->setCheckState(0, Qt::Unchecked);

    EXPECT_EQ(setting.writeObj.ignorePicturePath, std::vector<QString>{"img/faces/Actor1"});
}

// 不具合の再発防止
TEST_F(FileTreeTest, SelectValidationErrorShowsItsFile)
{
    // 検証結果の項目を押すと、結果が付いているファイルを開くこと（空のファイル名で表を空にしない）
    auto* treeWidget = tree->findChild<QTreeWidget*>();
    ASSERT_NE(treeWidget, nullptr);
    auto* top = new QTreeWidgetItem(treeWidget, {"Basics"});
    top->setData(0, ItemTypeRole, FileTree::Basic);
    auto* armors = new QTreeWidgetItem(top, {"Armors"});
    armors->setData(0, FileNameRole, "Armors.json");

    ValidationErrorInfo info;
    info.type = ValidationErrorInfo::Warning;
    info.summary = ValidationErrorInfo::EmptyCol;
    info.row = 1;
    info.language = "en";
    info.id = 5;    //ValidationService は 1 から番号を振る
    component->runtimeData->errors["Armors"].push_back(info);
    component->runtimeData->updateList["Armors"] = true;
    component->addDispatch(tree.get());
    component->dispatch(ComponentBase::NotifyFinishValidateCSV, {});
    ASSERT_EQ(armors->childCount(), 1);

    QStringList shownFiles;
    QObject::connect(tree.get(), &FileTree::showMainFile, [&shownFiles](const QString&, const QString& fileName) {
        shownFiles.append(fileName);
    });
    std::vector<quint64> selectedErrors;
    QObject::connect(tree.get(), &FileTree::selectMainError, [&selectedErrors](quint64 id) {
        selectedErrors.push_back(id);
    });

    treeWidget->setCurrentItem(armors->child(0));
    EXPECT_EQ(shownFiles, QStringList{"Armors.json"});
    // 開いた後に、その検証結果のセルへ移動すること
    EXPECT_EQ(selectedErrors, std::vector<quint64>{5});

    // 見出しを押しても表を開き直さない
    shownFiles.clear();
    treeWidget->setCurrentItem(top);
    EXPECT_TRUE(shownFiles.isEmpty());
}

TEST_F(FileTreeTest, ValidationFilterRebuildsErrorItems)
{
    // 言語で絞り込むと、その言語の検証結果の項目が消え、戻すと再び出ること
    auto* treeWidget = tree->findChild<QTreeWidget*>();
    ASSERT_NE(treeWidget, nullptr);
    auto* top = new QTreeWidgetItem(treeWidget, {"Basics"});
    auto* actors = new QTreeWidgetItem(top, {"Actors"});
    actors->setData(0, FileNameRole, "Actors.json");

    ValidationErrorInfo info;
    info.type = ValidationErrorInfo::Warning;
    info.summary = ValidationErrorInfo::EmptyCol;
    for(const auto* language : {"en", "en", "fr"}) {
        info.row++;
        info.language = language;
        component->runtimeData->errors["Actors"].push_back(info);
    }
    component->runtimeData->updateList["Actors"] = true;
    component->addDispatch(tree.get());
    component->dispatch(ComponentBase::NotifyFinishValidateCSV, {});
    ASSERT_EQ(actors->childCount(), 3);

    component->runtimeData->hiddenErrorLanguages.insert("en");
    component->dispatch(ComponentBase::ChangeValidationFilter, {});
    EXPECT_EQ(actors->childCount(), 1);

    component->runtimeData->hiddenErrorLanguages.clear();
    component->dispatch(ComponentBase::ChangeValidationFilter, {});
    EXPECT_EQ(actors->childCount(), 3);
}

namespace
{
    QTreeWidgetItem* FindChild(QTreeWidgetItem* parent, const QString& fileName)
    {
        for(int i = 0; i < parent->childCount(); ++i) {
            if(parent->child(i)->data(0, FileNameRole).toString() == fileName) { return parent->child(i); }
        }
        return nullptr;
    }
}

TEST_F(FileTreeTest, MultiSelectTogglesChecks)
{
    // 複数の項目を選んでチェックを外すと、選んだ項目がすべて翻訳対象から外れ、元に戻せること
    component->setting->fetchBasicDataInfo("Actors");
    component->setting->fetchBasicDataInfo("Armors");
    component->setting->fetchBasicDataInfo("Items");
    tree->setupBasicsTree();
    auto* treeWidget = tree->findChild<QTreeWidget*>();
    auto* top = treeWidget->topLevelItem(0);
    auto* actors = FindChild(top, "Actors.json");
    auto* armors = FindChild(top, "Armors.json");
    ASSERT_NE(actors, nullptr);
    ASSERT_NE(armors, nullptr);

    actors->setSelected(true);
    armors->setSelected(true);
    actors->setCheckState(0, Qt::Unchecked);

    EXPECT_TRUE(component->setting->fetchBasicDataInfo("Actors").ignore);
    EXPECT_TRUE(component->setting->fetchBasicDataInfo("Armors").ignore);
    EXPECT_FALSE(component->setting->fetchBasicDataInfo("Items").ignore) << "選んでいない項目まで変わっています";
    EXPECT_EQ(armors->checkState(0), Qt::Unchecked);

    component->history->undo();
    EXPECT_FALSE(component->setting->fetchBasicDataInfo("Actors").ignore);
    EXPECT_FALSE(component->setting->fetchBasicDataInfo("Armors").ignore);
    EXPECT_EQ(armors->checkState(0), Qt::Checked);
}

TEST_F(FileTreeTest, ShowUntranslatedOnly)
{
    // 翻訳が終わっていないファイルだけを表示できること。Animations は最初から表示しないこと
    component->setting->fetchBasicDataInfo("Actors");
    component->setting->fetchBasicDataInfo("Armors");
    component->setting->fetchBasicDataInfo("Animations");
    tree->setupBasicsTree();
    tree->updateTreeVisibility();
    auto* top = tree->findChild<QTreeWidget*>()->topLevelItem(0);
    auto* actors = FindChild(top, "Actors.json");
    auto* armors = FindChild(top, "Armors.json");
    EXPECT_TRUE(FindChild(top, "Animations.json")->isHidden());
    tree->setFileProgress("Actors.json", 1, 1);
    tree->setFileProgress("Armors.json", 0, 1);

    QCheckBox* untranslatedOnly = nullptr;
    for(auto* check : tree->findChildren<QCheckBox*>()) {
        if(check->text() == "Show Untranslated Files Only") { untranslatedOnly = check; }
    }
    ASSERT_NE(untranslatedOnly, nullptr);
    const bool before = untranslatedOnly->isChecked();
    untranslatedOnly->setChecked(true);
    EXPECT_TRUE(actors->isHidden()) << "翻訳済みの Actors が表示されています";
    EXPECT_FALSE(armors->isHidden());

    untranslatedOnly->setChecked(false);
    EXPECT_FALSE(actors->isHidden());
    untranslatedOnly->setChecked(before);   //アプリの設定を元に戻す
}

TEST_F(FileTreeTest, ScriptErrorsShownUnderScripts)
{
    // スクリプトの検証結果（editing/Scripts.csv の行）は、その原文を含むスクリプトの項目に付くこと。
    // ファイル全体の結果は「Script」の見出しに付く
    QTemporaryDir tempDir;
    component->setting->langscoreProjectDirectory = tempDir.path();
    QDir().mkpath(tempDir.filePath("editing"));
    QFile csv(tempDir.filePath("editing/Scripts.csv"));
    ASSERT_TRUE(csv.open(QFile::WriteOnly));
    csv.write("original,en\nHello,\nBye,\n");
    csv.close();
    for(const auto& [fileName, originals] : std::vector<std::pair<QString, QStringList>>{{"A.js", {"Hello"}}, {"B.js", {"Hello", "Bye"}}})
    {
        settings::ScriptInfo info;
        info.fileName = fileName;
        for(const auto& original : originals) {
            langscore::ScriptTextPosition line;
            line.originalText = original;
            info.lines.push_back(line);
        }
        component->setting->writeObj.scriptInfo.push_back(info);
    }

    auto* treeWidget = tree->findChild<QTreeWidget*>();
    ASSERT_NE(treeWidget, nullptr);
    auto* top = new QTreeWidgetItem(treeWidget, {"Script"});
    top->setData(0, ItemTypeRole, FileTree::Script);
    auto* a = new QTreeWidgetItem(top, {"A"});
    a->setData(0, FileNameRole, "A");
    auto* b = new QTreeWidgetItem(top, {"B"});
    b->setData(0, FileNameRole, "B");

    ValidationErrorInfo info;
    info.type = ValidationErrorInfo::Warning;
    info.summary = ValidationErrorInfo::EmptyCol;
    for(const auto row : {0, 1, 2}) {
        info.row = row;
        component->runtimeData->errors["Scripts"].push_back(info);
    }
    component->runtimeData->updateList["Scripts"] = true;
    component->addDispatch(tree.get());
    component->dispatch(ComponentBase::NotifyFinishValidateCSV, {});

    EXPECT_EQ(a->childCount(), 1);
    EXPECT_EQ(b->childCount(), 2);
    EXPECT_EQ(top->childCount(), 3);
}

TEST_F(FileTreeTest, ValidationFilterIsSaved)
{
    // 検証結果の絞り込みはアプリの設定に保存され、次に作ったボタン（次の起動）でも使われること
    QToolButton* filterButton = nullptr;
    for(auto* b : tree->findChildren<QToolButton*>()) {
        if(b->menu() != nullptr) { filterButton = b; }
    }
    ASSERT_NE(filterButton, nullptr);
    emit filterButton->menu()->aboutToShow();
    auto actions = filterButton->menu()->actions();
    ASSERT_FALSE(actions.isEmpty());
    actions[0]->toggle();   //「エラー」を隠す
    EXPECT_TRUE(component->runtimeData->hideErrors);

    TestComponent other;
    ValidationFilterButton reloaded(&other, nullptr);
    EXPECT_TRUE(other.runtimeData->hideErrors);

    //他のテストに残さない
    actions[0]->toggle();
}

TEST_F(FileTreeTest, MainFileNameWithoutChildCount)
{
    // 子の数「(n/n)」が付いた項目を選んでも、表に渡す名前には付かないこと
    auto* treeWidget = tree->findChild<QTreeWidget*>();
    ASSERT_NE(treeWidget, nullptr);
    auto* top = new QTreeWidgetItem(treeWidget, {"Main"});
    top->setData(0, ItemTypeRole, FileTree::Map);
    auto* map = new QTreeWidgetItem(top, {"Map001 (1/2)"});
    map->setData(0, FileNameRole, "Map001.json");
    map->setData(0, Qt::UserRole + 8, "Map001");   // BASE_TEXT_ITEM

    QString shownName;
    QObject::connect(tree.get(), &FileTree::showMainFile, [&shownName](const QString& name, const QString&) { shownName = name; });
    treeWidget->setCurrentItem(map);
    map->setSelected(true);
    EXPECT_EQ(shownName, "Map001");
}
