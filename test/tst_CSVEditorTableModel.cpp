#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include "CSVEditorTableModel.h"
#include "EditorTableDefines.h"

class TestCSVEditorTableModel : public QObject
{
    Q_OBJECT

private slots:
    void testLoadJson();

    // 不具合の再発防止
    void testLoadJsonKeepsQuotes();
    // 異常終了する可能性があるため最後に実行する
    void testIsLanguageColumnWithoutSettings();

private:
    QTemporaryDir tempDir;
    QString writeJson(const QByteArray& content);
};

QString TestCSVEditorTableModel::writeJson(const QByteArray& content)
{
    auto path = tempDir.filePath("Actors.json");
    QFile file(path);
    if(file.open(QFile::WriteOnly) == false){ qFatal("cannot create test file"); }
    file.write(content);
    return path;
}

void TestCSVEditorTableModel::testLoadJson()
{
    langscore::CSVEditorTableModel model;
    QVERIFY(model.loadFromJsonFile(writeJson(R"([{"original": "Hello", "type": ["name"]}])")));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.index(0, 0).data().toString(), QString("Hello"));
}

void TestCSVEditorTableModel::testLoadJsonKeepsQuotes()
{
    // JSON の文字列は CSV エスケープされていないので、引用符がそのまま残ること
    langscore::CSVEditorTableModel model;
    QVERIFY(model.loadFromJsonFile(writeJson(R"([
        {"original": "\"Hello\"", "type": []},
        {"original": "a\"\"b", "type": []}
    ])")));
    QCOMPARE(model.index(0, 0).data().toString(), QString("\"Hello\""));
    QCOMPARE(model.index(1, 0).data().toString(), QString("a\"\"b"));
}

void TestCSVEditorTableModel::testIsLanguageColumnWithoutSettings()
{
    // settings 未設定や範囲外の列でも、IsLanguageColumn の問い合わせで落ちないこと
    // （MainCSVTable は setSettings より前に proxy へ setSourceModel している）
    langscore::CSVEditorTableModel model;
    QVERIFY(model.loadFromJsonFile(writeJson(R"([{"original": "Hello", "type": []}])")));
    QCOMPARE(model.headerData(0, Qt::Horizontal, DataType::IsLanguageColumn).toBool(), false);
    QCOMPARE(model.headerData(99, Qt::Horizontal, DataType::IsLanguageColumn).toBool(), false);
}

QTEST_MAIN(TestCSVEditorTableModel)
#include "tst_CSVEditorTableModel.moc"
