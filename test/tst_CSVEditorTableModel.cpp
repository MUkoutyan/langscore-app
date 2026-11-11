#include "test_common.h"
#include <QTemporaryDir>
#include <QFile>
#include "CSVEditorTableModel.h"
#include "CSVEditorSortFilterProxyModel.h"
#include "EditorTableDefines.h"

class CSVEditorTableModelTest : public ::testing::Test
{
protected:
    QTemporaryDir tempDir;

    QString WriteJson(const QByteArray& content)
    {
        auto path = tempDir.filePath("Actors.json");
        QFile file(path);
        if(file.open(QFile::WriteOnly) == false)
        {
            ADD_FAILURE() << "cannot create test file: " << path.toStdString();
            return path;
        }
        file.write(content);
        return path;
    }
};

TEST_F(CSVEditorTableModelTest, LoadJson)
{
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([{"original": "Hello", "type": ["name"]}])")));
    EXPECT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.index(0, 0).data().toString(), QString("Hello"));
}

// 不具合の再発防止
TEST_F(CSVEditorTableModelTest, LoadJsonKeepsQuotes)
{
    // JSON の文字列はCSVエスケープされていないため、引用符がそのまま残ること
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([
        {"original": "\"Hello\"", "type": []},
        {"original": "a\"\"b", "type": []}
    ])")));
    EXPECT_EQ(model.index(0, 0).data().toString(), QString("\"Hello\""));
    EXPECT_EQ(model.index(1, 0).data().toString(), QString("a\"\"b"));
}

TEST_F(CSVEditorTableModelTest, SetDataRejectsReadOnlyColumn)
{
    // 編集できない original・type 列は、setData で書き換えられないこと
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([{"original": "Hello", "type": ["name"]}])")));
    EXPECT_FALSE(model.setData(model.index(0, 0), "Changed"));
    EXPECT_FALSE(model.setData(model.index(0, 1), "Changed"));
    EXPECT_EQ(model.getCell(0, 0).value(), QString("Hello"));
    EXPECT_EQ(model.getCell(0, 1).value(), QString("name"));

    // 言語列は従来どおり編集できること
    model.insertColumn(1, "en");
    EXPECT_TRUE(model.setData(model.index(0, 1), "Hi"));
    EXPECT_EQ(model.getCell(0, 1).value(), QString("Hi"));
}

TEST_F(CSVEditorTableModelTest, TypeColumnWithMultipleTypes)
{
    // 種類が複数ある行や未知の種類の行でも、Type 列が空欄にならないこと
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([
        {"original": "A", "type": ["name", "description"]},
        {"original": "B", "type": ["unknownType"]}
    ])")));
    EXPECT_EQ(model.index(0, 1).data().toString(), QString("name, description"));
    EXPECT_EQ(model.index(1, 1).data().toString(), QString("unknownType"));
}

TEST_F(CSVEditorTableModelTest, ModifiedUntilSaved)
{
    // 編集すると未保存状態になり、保存すると解除されること
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([{"original": "Hello", "type": []}])")));
    model.insertColumn(1, "en");
    EXPECT_FALSE(model.isModified());
    EXPECT_TRUE(model.setData(model.index(0, 1), "Hi"));
    EXPECT_TRUE(model.isModified());
    EXPECT_TRUE(model.saveToFile(tempDir.filePath("Actors.csv")));
    EXPECT_FALSE(model.isModified());
}

TEST_F(CSVEditorTableModelTest, TranslatedRowCountIgnoresDisabledLanguage)
{
    // 有効な言語がすべて埋まった行のみ翻訳済みとして数えること
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([
        {"original": "A", "type": []},
        {"original": "B", "type": []}
    ])")));
    auto setting = std::make_shared<settings>();
    setting->languages.push_back({.languageName = "en", .enable = true});
    setting->languages.push_back({.languageName = "ja", .enable = false});
    model.setSettings(setting);
    model.insertColumn(1, "en");
    model.insertColumn(2, "ja");

    EXPECT_EQ(model.translatedRowCount(), std::make_pair(0, 2));
    EXPECT_TRUE(model.setData(model.index(0, 1), "a"));
    EXPECT_EQ(model.translatedRowCount(), std::make_pair(1, 2));
}

TEST_F(CSVEditorTableModelTest, EditedCellHidesStaleError)
{
    // 検証後に編集したセルは、次の検証まで結果の色とアイコンを表示しないこと
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([{"original": "Hello", "type": []}])")));
    model.insertColumn(1, "en");
    auto runtimeData = std::make_shared<ComponentBase::RuntimeData>();
    ValidationErrorInfo info;
    info.type = ValidationErrorInfo::Error;
    info.summary = ValidationErrorInfo::EmptyCol;
    info.row = 1;
    info.language = "en";
    runtimeData->errors["Actors"].push_back(info);
    model.setRuntimeData(runtimeData);

    const auto cell = model.index(0, 1);
    EXPECT_TRUE(model.data(cell, Qt::BackgroundRole).isValid());
    EXPECT_TRUE(model.data(cell, Qt::DecorationRole).isValid());
    EXPECT_TRUE(model.data(cell, Qt::ToolTipRole).toString().contains("[Error]"));

    EXPECT_TRUE(model.setData(cell, "Hi"));
    EXPECT_FALSE(model.data(cell, Qt::BackgroundRole).isValid());
    EXPECT_FALSE(model.data(cell, Qt::DecorationRole).isValid());

    model.clearEditedSinceValidation();
    EXPECT_TRUE(model.data(cell, Qt::BackgroundRole).isValid());
}

TEST_F(CSVEditorTableModelTest, MergeTranslationWithLineBreak)
{
    // \n・\r\n の改行を含む原文の翻訳文を保存し、読み直して戻せること
    for(const QByteArray original : {QByteArray(R"(a\nb)"), QByteArray(R"(a\r\nb)")})
    {
        const auto json = "[{\"original\": \"" + original + "\", \"type\": []}]";
        langscore::CSVEditorTableModel model;
        ASSERT_TRUE(model.loadFromJsonFile(WriteJson(json)));
        model.insertColumn(1, "en");
        ASSERT_TRUE(model.setData(model.index(0, 1), "Translated"));
        const auto csvPath = tempDir.filePath("Actors.csv");
        ASSERT_TRUE(model.saveToFile(csvPath));

        langscore::CSVEditorTableModel reopened;
        ASSERT_TRUE(reopened.loadFromJsonFile(WriteJson(json)));
        reopened.insertColumn(1, "en");
        ASSERT_TRUE(reopened.mergeTranslationsFromCsv(csvPath));
        EXPECT_EQ(reopened.getCell(0, 1).value_or(""), QString("Translated")) << original.toStdString();
    }
}

// 異常終了する可能性があるため最後に実行する
TEST_F(CSVEditorTableModelTest, IsLanguageColumnWithoutSettings)
{
    // settings 未設定や範囲外の列でも、IsLanguageColumn の問い合わせで落ちないこと
    // MainCSVTable は setSettings より前に proxy へ setSourceModel している
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([{"original": "Hello", "type": []}])")));
    EXPECT_FALSE(model.headerData(0, Qt::Horizontal, DataType::IsLanguageColumn).toBool());
    EXPECT_FALSE(model.headerData(99, Qt::Horizontal, DataType::IsLanguageColumn).toBool());
}

TEST_F(CSVEditorTableModelTest, ValidationFilterHidesLanguage)
{
    // 言語で絞り込んだ検証結果はセルに表示されないこと。検証中に結果が増えても反映されること
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([{"original": "Hello", "type": []}])")));
    model.insertColumn(1, "en");
    model.insertColumn(2, "fr");
    auto runtimeData = std::make_shared<ComponentBase::RuntimeData>();
    ValidationErrorInfo info;
    info.type = ValidationErrorInfo::Warning;
    info.summary = ValidationErrorInfo::EmptyCol;
    info.row = 1;
    info.language = "en";
    runtimeData->errors["Actors"].push_back(info);
    model.setRuntimeData(runtimeData);

    const auto enCell = model.index(0, 1);
    const auto frCell = model.index(0, 2);
    EXPECT_TRUE(model.data(enCell, Qt::BackgroundRole).isValid());
    EXPECT_FALSE(model.data(frCell, Qt::BackgroundRole).isValid());

    info.language = "fr";
    runtimeData->errors["Actors"].push_back(info);
    EXPECT_TRUE(model.data(frCell, Qt::BackgroundRole).isValid()) << "増えた検証結果がセルに出ていません";

    runtimeData->hiddenErrorLanguages.insert("en");
    EXPECT_FALSE(model.data(enCell, Qt::BackgroundRole).isValid()) << "隠した言語の検証結果が出ています";
    EXPECT_TRUE(model.data(frCell, Qt::BackgroundRole).isValid());

    runtimeData->hideWarnings = true;
    EXPECT_FALSE(model.data(frCell, Qt::BackgroundRole).isValid()) << "隠した種類の検証結果が出ています";
}

TEST_F(CSVEditorTableModelTest, RowNumberMatchesValidationRow)
{
    // 垂直ヘッダーの行番号は1から始まり、検索で行を絞り込んでも元の行番号のままであること。
    // CSV のヘッダーを0行目とする検証結果の行番号と一致する
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([{"original": "Hello", "type": []}, {"original": "World", "type": []}])")));
    EXPECT_EQ(model.headerData(0, Qt::Vertical).toInt(), 1);

    langscore::CSVEditorSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.setFilterText("World");
    ASSERT_EQ(proxy.rowCount(), 1);
    EXPECT_EQ(proxy.headerData(0, Qt::Vertical).toInt(), 2);
}

//-------------------------------------------------------------------
// 複数人で同じ editing の CSV を共有したときの保存
//-------------------------------------------------------------------
namespace
{
    void WriteCsv(const QString& path, const QByteArray& content)
    {
        QFile file(path);
        if(file.open(QFile::WriteOnly) == false) {
            ADD_FAILURE() << "cannot create test file: " << path.toStdString();
            return;
        }
        file.write(content);
    }

    QString CellOf(const langscore::CSVEditorTableModel& model, const QString& original, const QString& language)
    {
        int originalColumn = -1;
        int languageColumn = -1;
        for(int column = 0; column < model.columnCount(); ++column)
        {
            const auto header = model.headerData(column, Qt::Horizontal, Qt::UserRole).toString();
            if(header == "original") { originalColumn = column; }
            if(header == language) { languageColumn = column; }
        }
        if(originalColumn < 0 || languageColumn < 0) { return "(no column)"; }
        for(int row = 0; row < model.rowCount(); ++row) {
            if(model.getCell(row, originalColumn) == original) { return model.getCell(row, languageColumn).value_or(""); }
        }
        return "(no row)";
    }
}

TEST_F(CSVEditorTableModelTest, SaveKeepsOtherPersonsEdit)
{
    // 読み込み後にリモートで別のセルが保存されていたら、その翻訳文を消さずに取り込むこと
    const auto csvPath = tempDir.filePath("Actors.csv");
    WriteCsv(csvPath, "original,en,fr,type\nA,,,name\nB,,,name\n");
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromEditCSVFile(csvPath));

    WriteCsv(csvPath, "original,en,fr,type\nA,Hello,,name\nB,,,name\n");
    ASSERT_TRUE(model.setData(model.index(0, 2), "Bonjour"));
    ASSERT_TRUE(model.saveToFile(csvPath));

    EXPECT_TRUE(model.mergeConflicts().empty());
    EXPECT_EQ(CellOf(model, "A", "en"), QString("Hello")) << "他の人の訳が表に取り込まれていません";
    langscore::CSVEditorTableModel saved(csvPath);
    EXPECT_EQ(CellOf(saved, "A", "en"), QString("Hello")) << "他の人の訳が保存で消えています";
    EXPECT_EQ(CellOf(saved, "A", "fr"), QString("Bonjour"));
}

TEST_F(CSVEditorTableModelTest, SaveReportsConflict)
{
    // 同じセルをローカルとリモートで別の翻訳文にしたら、ローカルの翻訳文で保存して衝突として残すこと
    const auto csvPath = tempDir.filePath("Actors.csv");
    WriteCsv(csvPath, "original,en,type\nA,Old,name\n");
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromEditCSVFile(csvPath));

    WriteCsv(csvPath, "original,en,type\nA,Theirs,name\n");
    ASSERT_TRUE(model.setData(model.index(0, 1), "Mine"));
    ASSERT_TRUE(model.saveToFile(csvPath));

    ASSERT_EQ(model.mergeConflicts().size(), 1u);
    EXPECT_EQ(model.mergeConflicts()[0].row, 0);
    EXPECT_EQ(model.mergeConflicts()[0].column, 1);
    EXPECT_EQ(model.mergeConflicts()[0].mine, QString("Mine"));
    EXPECT_EQ(model.mergeConflicts()[0].theirs, QString("Theirs"));
    EXPECT_EQ(CellOf(langscore::CSVEditorTableModel(csvPath), "A", "en"), QString("Mine"));

    // リモートの翻訳文を選んで再保存すると、衝突は残らないこと
    ASSERT_TRUE(model.setData(model.index(0, 1), "Theirs"));
    ASSERT_TRUE(model.saveToFile(csvPath));
    EXPECT_TRUE(model.mergeConflicts().empty());
    EXPECT_EQ(CellOf(langscore::CSVEditorTableModel(csvPath), "A", "en"), QString("Theirs"));
}

TEST_F(CSVEditorTableModelTest, SaveKeepsUnknownRowsAndColumns)
{
    // リモートで再解析して増えた原文の行や追加された言語の列を、保存で削除しないこと
    const auto csvPath = tempDir.filePath("Actors.csv");
    WriteCsv(csvPath, "original,en,type\nA,,name\n");
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromEditCSVFile(csvPath));

    WriteCsv(csvPath, "original,en,de,type\nA,,Hallo,name\nNew,Added,Neu,name\n");
    ASSERT_TRUE(model.setData(model.index(0, 1), "Hello"));
    ASSERT_TRUE(model.saveToFile(csvPath));

    langscore::CSVEditorTableModel saved(csvPath);
    EXPECT_EQ(CellOf(saved, "A", "en"), QString("Hello"));
    EXPECT_EQ(CellOf(saved, "A", "de"), QString("Hallo"));
    EXPECT_EQ(CellOf(saved, "New", "en"), QString("Added"));
    EXPECT_EQ(CellOf(saved, "New", "de"), QString("Neu"));
}

TEST_F(CSVEditorTableModelTest, SaveDropsRowsMovedToOldTranslations)
{
    // 古い翻訳へ移した行など、読み込み時に表に無かった行は、従来どおり保存で削除すること
    const auto csvPath = tempDir.filePath("Actors.csv");
    WriteCsv(csvPath, "original,en,type\nHello,Hi,name\nRemoved,Gone,name\n");
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([{"original": "Hello", "type": ["name"]}])")));
    model.insertColumn(1, "en");
    ASSERT_TRUE(model.mergeTranslationsFromCsv(csvPath));
    ASSERT_TRUE(model.setData(model.index(0, 1), "Hello!"));
    ASSERT_TRUE(model.saveToFile(csvPath));

    langscore::CSVEditorTableModel saved(csvPath);
    EXPECT_EQ(CellOf(saved, "Hello", "en"), QString("Hello!"));
    EXPECT_EQ(CellOf(saved, "Removed", "en"), QString("(no row)"));
}

TEST_F(CSVEditorTableModelTest, NeedsSaveOnlyWhenChanged)
{
    // 開いただけのファイルは保存しない。編集したか、ファイルに無い列を追加したら保存する
    const auto csvPath = tempDir.filePath("Actors.csv");
    WriteCsv(csvPath, "original,en,type\nA,,name\n");
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromEditCSVFile(csvPath));
    EXPECT_FALSE(model.needsSave());

    model.insertColumn(2, "fr");
    EXPECT_TRUE(model.needsSave());
    ASSERT_TRUE(model.saveToFile(csvPath));
    EXPECT_FALSE(model.needsSave());

    ASSERT_TRUE(model.setData(model.index(0, 1), "Hi"));
    EXPECT_TRUE(model.needsSave());

    // 検証は editing の CSV のみを対象にするため、editing の CSV がまだ無いファイルは保存する
    langscore::CSVEditorTableModel unsaved;
    ASSERT_TRUE(unsaved.loadFromJsonFile(WriteJson(R"([{"original": "A", "type": []}])")));
    EXPECT_TRUE(unsaved.needsSave());
}

TEST_F(CSVEditorTableModelTest, SyncTakesOtherPersonsEditWithoutSaving)
{
    // 保存しなくても、リモートのみで変更されたセルを表へ取り込めること。取り込みのみでは未保存状態にならないこと
    const auto csvPath = tempDir.filePath("Actors.csv");
    WriteCsv(csvPath, "original,en,type\nA,,name\nB,,name\n");
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromEditCSVFile(csvPath));

    WriteCsv(csvPath, "original,en,type\nA,Hello,name\nB,,name\n");
    EXPECT_TRUE(model.syncWithFile(csvPath).empty());
    EXPECT_EQ(CellOf(model, "A", "en"), QString("Hello"));
    EXPECT_FALSE(model.isModified());
    EXPECT_FALSE(model.needsSave());
}

TEST_F(CSVEditorTableModelTest, SyncReportsConflictUntilResolved)
{
    // 両方で変更されたセルは表を変えずに返し、解決するまで読み直すたびに返すこと
    const auto csvPath = tempDir.filePath("Actors.csv");
    WriteCsv(csvPath, "original,en,type\nA,Old,name\n");
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromEditCSVFile(csvPath));
    ASSERT_TRUE(model.setData(model.index(0, 1), "Mine"));

    WriteCsv(csvPath, "original,en,type\nA,Theirs,name\n");
    const auto conflicts = model.syncWithFile(csvPath);
    ASSERT_EQ(conflicts.size(), 1u);
    EXPECT_EQ(conflicts[0].mine, QString("Mine"));
    EXPECT_EQ(conflicts[0].theirs, QString("Theirs"));
    EXPECT_EQ(CellOf(model, "A", "en"), QString("Mine"));
    EXPECT_EQ(model.syncWithFile(csvPath).size(), 1u) << "あとで決めた衝突が、読み直しても出ません";

    // リモートの翻訳文を選ぶとファイルと同じ値になるため、未保存状態が解除されること
    model.resolveConflict(conflicts[0], true);
    EXPECT_EQ(CellOf(model, "A", "en"), QString("Theirs"));
    EXPECT_FALSE(model.isModified());
    EXPECT_TRUE(model.syncWithFile(csvPath).empty());
}

TEST_F(CSVEditorTableModelTest, ConflictResolvedAsMineIsSaved)
{
    // ローカルの翻訳文を選んだ衝突は、次の保存で衝突にならずにローカルの翻訳文を書き込むこと
    const auto csvPath = tempDir.filePath("Actors.csv");
    WriteCsv(csvPath, "original,en,type\nA,Old,name\n");
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromEditCSVFile(csvPath));
    ASSERT_TRUE(model.setData(model.index(0, 1), "Mine"));

    WriteCsv(csvPath, "original,en,type\nA,Theirs,name\n");
    const auto conflicts = model.syncWithFile(csvPath);
    ASSERT_EQ(conflicts.size(), 1u);
    model.resolveConflict(conflicts[0], false);
    EXPECT_TRUE(model.isModified());
    EXPECT_TRUE(model.syncWithFile(csvPath).empty());

    ASSERT_TRUE(model.saveToFile(csvPath));
    EXPECT_TRUE(model.mergeConflicts().empty());
    EXPECT_EQ(CellOf(langscore::CSVEditorTableModel(csvPath), "A", "en"), QString("Mine"));
}

TEST_F(CSVEditorTableModelTest, GitConflictMarkersNeedSaveAfterTakingTheirs)
{
    // Git の競合をすべてリモートの翻訳文にして表に未保存の編集が無くなっても、競合マーカーを消すために保存が必要なこと
    const auto csvPath = tempDir.filePath("Actors.csv");
    WriteCsv(csvPath, "original,en,type\nA,Mine,name\nB,,name\n");
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromEditCSVFile(csvPath));

    WriteCsv(csvPath, "original,en,type\n<<<<<<< HEAD\nA,Mine,name\n=======\nA,Theirs,name\n>>>>>>> 1234567\nB,,name\n");
    const auto conflicts = model.syncWithFile(csvPath);
    ASSERT_EQ(conflicts.size(), 1u);
    model.resolveConflict(conflicts[0], true);
    EXPECT_FALSE(model.isModified());
    EXPECT_TRUE(model.needsSave()) << "競合の印が残ったファイルを保存しません";

    ASSERT_TRUE(model.saveToFile(csvPath));
    EXPECT_FALSE(model.needsSave());
    QFile file(csvPath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_FALSE(file.readAll().contains("<<<<<<<"));
    EXPECT_EQ(CellOf(langscore::CSVEditorTableModel(csvPath), "A", "en"), QString("Theirs"));
}
