#pragma once

#include <QAbstractTableModel>
#include <QDateTime>

class MainCSVTableModel;
class ScriptCSVTableModel;

// CSVファイルごとの編集データ管理クラス
class CSVEditDataManager : public QObject
{
    Q_OBJECT
public:
    enum class ModelType {
        MainCSV,
        ScriptCSV
    };

    struct EditSession {
        QString filePath;
        std::unique_ptr<QAbstractTableModel> model;
        ModelType modelType;
        bool isDirty = false;
        QDateTime lastModified;
    };

    // ファイルごとのセッション管理
    QAbstractTableModel* getOrCreateModel(const QString& filePath, ModelType type = ModelType::MainCSV);
    MainCSVTableModel* getMainCSVModel(const QString& filePath);
    ScriptCSVTableModel* getScriptCSVModel(const QString& filePath);

    bool saveModel(const QString& filePath);
    bool saveAllModels(const QString& editingDirectory);

    bool hasUnsavedChanges(const QString& filePath) const;
    void closeSession(const QString& filePath);

private:
    std::unordered_map<QString, std::unique_ptr<EditSession>> sessions_;

    std::unique_ptr<QAbstractTableModel> createModel(ModelType type);
};