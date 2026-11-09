#pragma once

#include <QAbstractTableModel>
#include <QDateTime>
#include <QUndoStack>

class MainCSVTableModel;
class ScriptCSVTableModel;

// CSVファイルごとの編集データ管理クラス
class EditDataManager : public QObject
{
    Q_OBJECT
public:
    enum class ModelType : char{
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

    explicit EditDataManager(QObject* parent = nullptr);
    ~EditDataManager();

    // History management
    void setUndoStack(QUndoStack* undoStack);
    QUndoStack* getUndoStack() const { return undoStack; }
    
    // Create a new undo stack if none exists
    QUndoStack* ensureUndoStack();

    // ファイルごとのセッション管理
    QAbstractTableModel* getOrCreateModel(const QString& filePath, ModelType type = ModelType::MainCSV);
    MainCSVTableModel* getMainCSVModel(const QString& filePath);
    ScriptCSVTableModel* getScriptCSVModel(const QString& filePath);
    //読み込み済みのモデルを返す。無ければ作らずに nullptr を返す
    MainCSVTableModel* findMainCSVModel(const QString& filePath) const;

    bool saveModel(const QString& filePath);
    //変更したファイルだけを保存する。他の人が同じファイルを保存していれば、その変更を取り込む
    bool saveAllModels(const QString& editingDirectory);
    //読み込んだメインの表のファイル（editing のCSVのパス）
    QStringList mainCSVFiles() const;

    bool hasUnsavedChanges(const QString& filePath) const;
    //検証が終わったときに呼ぶ。検証の後に編集したセルの記録を、開いたすべてのファイルで消す
    void clearEditedSinceValidation();
    void closeSession(const QString& filePath);
    // 全セッションを破棄する（再解析・プロジェクト切り替えで内容が古くなるため）
    void closeAllSessions() { sessions_.clear(); }

signals:
    void undoStackChanged(QUndoStack* newStack);

private:
    std::unordered_map<QString, std::unique_ptr<EditSession>> sessions_;
    QUndoStack* undoStack;
    bool ownsUndoStack;

    std::unique_ptr<QAbstractTableModel> createModel(ModelType type);
};