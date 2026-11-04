#include "test_common.h"
#include <QTreeWidget>
#include <QCheckBox>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QImage>
#include "FileTree.h"
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
    // 次の検証を始めたら、前回の表示は消えること
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

    component->dispatch(ComponentBase::ValidateCSV, {});
    EXPECT_EQ(actors->childCount(), 0) << "次の検証を始めても前回の検証結果が残っています";
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
