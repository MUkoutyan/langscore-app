#pragma once

#include <QAbstractTableModel>
#include <QDateTime>
#include <QUndoStack>

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

    explicit CSVEditDataManager(QObject* parent = nullptr);
    ~CSVEditDataManager();

    // History management
    void setUndoStack(QUndoStack* undoStack);
    QUndoStack* getUndoStack() const { return undoStack; }
    
    // Create a new undo stack if none exists
    QUndoStack* ensureUndoStack();

    // ファイルごとのセッション管理
    QAbstractTableModel* getOrCreateModel(const QString& filePath, ModelType type = ModelType::MainCSV);
    MainCSVTableModel* getMainCSVModel(const QString& filePath);
    ScriptCSVTableModel* getScriptCSVModel(const QString& filePath);

    bool saveModel(const QString& filePath);
    bool saveAllModels(const QString& editingDirectory);

    bool hasUnsavedChanges(const QString& filePath) const;
    void closeSession(const QString& filePath);

signals:
    void undoStackChanged(QUndoStack* newStack);

private:
    std::unordered_map<QString, std::unique_ptr<EditSession>> sessions_;
    QUndoStack* undoStack;
    bool ownsUndoStack;

    std::unique_ptr<QAbstractTableModel> createModel(ModelType type);
};