#include "CSVEditDataManager.h"

#include "MainCSVTableModel.h"
#include "ScriptCSVTableModel.h"

#include <QFileInfo>
#include <QDir>


CSVEditDataManager::CSVEditDataManager(QObject* parent)
    : QObject(parent)
    , undoStack(nullptr)
    , ownsUndoStack(false)
{
}

CSVEditDataManager::~CSVEditDataManager()
{
    if (ownsUndoStack && undoStack) {
        delete undoStack;
    }
}

void CSVEditDataManager::setUndoStack(QUndoStack* stack)
{
    if (ownsUndoStack && undoStack) {
        delete undoStack;
        ownsUndoStack = false;
    }
    
    undoStack = stack;
    emit undoStackChanged(undoStack);
}

QUndoStack* CSVEditDataManager::ensureUndoStack()
{
    if (!undoStack) {
        undoStack = new QUndoStack(this);
        ownsUndoStack = true;
        emit undoStackChanged(undoStack);
    }
    return undoStack;
}

QAbstractTableModel* CSVEditDataManager::getOrCreateModel(const QString& filePath, ModelType type) {
    auto it = sessions_.find(filePath);
    if(it != sessions_.end()) {
        return it->second->model.get();
    }

    auto session = std::make_unique<EditSession>();
    session->filePath = filePath;
    session->modelType = type;
    session->model = createModel(type);
    session->lastModified = QFileInfo(filePath).lastModified();

    auto* modelPtr = session->model.get();
    sessions_[filePath] = std::move(session);

    return modelPtr;
}

MainCSVTableModel* CSVEditDataManager::getMainCSVModel(const QString& filePath) {
    auto* model = getOrCreateModel(filePath, ModelType::MainCSV);
    return qobject_cast<MainCSVTableModel*>(model);
}

ScriptCSVTableModel* CSVEditDataManager::getScriptCSVModel(const QString& filePath) {
    auto* model = getOrCreateModel(filePath, ModelType::ScriptCSV);
    return qobject_cast<ScriptCSVTableModel*>(model);
}

std::unique_ptr<QAbstractTableModel> CSVEditDataManager::createModel(ModelType type) {
    switch(type) {
    case ModelType::MainCSV:
        return std::make_unique<MainCSVTableModel>();
    case ModelType::ScriptCSV:
        return std::make_unique<ScriptCSVTableModel>();
    default:
        return std::make_unique<MainCSVTableModel>();
    }
}

bool CSVEditDataManager::saveModel(const QString& filePath)
{
    auto it = sessions_.find(filePath);
    if (it == sessions_.end()) {
        return false; // セッションが存在しない
    }

    auto& session = it->second;
    if (!session->model) {
        return false; // モデルが存在しない
    }

    // 保存処理をモデルタイプに応じて分岐
    bool success = false;
    switch (session->modelType) {
        case ModelType::MainCSV: {
            auto* mainModel = qobject_cast<MainCSVTableModel*>(session->model.get());
            if (mainModel) {
                success = mainModel->saveToFile(filePath);
            }
            break;
        }
        case ModelType::ScriptCSV: {
            auto* scriptModel = qobject_cast<ScriptCSVTableModel*>(session->model.get());
            if (scriptModel) {
                success = scriptModel->saveToFile(filePath);
            }
            break;
        }
    }

    if (success) {
        session->isDirty = false;
        session->lastModified = QDateTime::currentDateTime();
    }

    return success;
}

bool CSVEditDataManager::saveAllModels(const QString& editingDirectory)
{
    bool allSuccess = true;
    
    // editingディレクトリが存在しない場合は作成
    QDir dir;
    if (!dir.exists(editingDirectory)) {
        if (!dir.mkpath(editingDirectory)) {
            return false;
        }
    }

    // 全てのセッションを保存
    for (const auto& [originalPath, session] : sessions_) 
    {
        //if (!session->isDirty) {
        //    continue; // 変更されていないセッションはスキップ
        //}

        // 元のファイルパスから編集用のパスを生成
        QFileInfo fileInfo(originalPath);
        QString baseName = fileInfo.completeBaseName();
        QString extension = fileInfo.suffix();
        
        // CSVファイルとして保存
        QString savePath = editingDirectory + "/" + baseName + ".csv";
        
        if (!saveModel(originalPath)) {
            allSuccess = false;
        }
    }

    return allSuccess;
}
