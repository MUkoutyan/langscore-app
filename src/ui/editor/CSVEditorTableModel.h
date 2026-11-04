#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <vector>
#include <optional>
#include <QSet>
#include "FastCSVContainer.h"
#include "../ComponentBase.h"
#include "service/ValidationErrorInfo.h"
#include "../../settings.h"

namespace langscore {

class CSVEditorTableModel : public QAbstractTableModel 
{
    Q_OBJECT
public:

    CSVEditorTableModel(QObject* parent = nullptr);
    explicit CSVEditorTableModel(const QString& path, QObject* parent = nullptr);

    bool loadFromEditCSVFile(const QString& path);
    bool loadFromJsonFile(const QString& filePath);
    bool saveToFile(const QString& path);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    size_t rowCountRaw() const;
    size_t colCountRaw() const;
    std::optional<QString> getCell(size_t row, size_t col) const;
    bool setCell(size_t row, size_t col, const QString& value);

    void insertColumn(size_t index, const QString& columnName);
    void removeColumn(int column);

    const std::vector<std::vector<QString>>& dataRaw() const;

    void setSettings(std::shared_ptr<settings> setting);
    void setRuntimeData(std::shared_ptr<ComponentBase::RuntimeData> setting);

    void setUseLanguageFont(bool use);

    bool mergeTranslationsFromCsv(const QString& csvPath);

    QString getCurrentShowFileName() const {
        return currentShowFileName;
    }

    //保存してから編集されたか
    bool isModified() const { return _modified; }
    //検証の後に編集したセルの記録を消す（検証が終わったときに呼ぶ）
    void clearEditedSinceValidation();
    //すべての言語が埋まっている行の数と、全体の行数
    std::pair<int, int> translatedRowCount() const;

    void clearAll() {
        beginResetModel();
        csvContainer.clear();
        endResetModel();
    }


signals:
    void modifiedChanged(bool modified);

private:
    void setModified(bool modified);
    std::vector<const ValidationErrorInfo*> cellErrors(int row, int col) const;

    langscore::FastCSVContainer csvContainer;
    bool _modified = false;
    QSet<std::pair<int, int>> _editedSinceValidation;
    QString currentShowFileName;
    bool useLanguageFont = false;
    std::shared_ptr<settings> _settings = nullptr;
    std::shared_ptr<ComponentBase::RuntimeData> _runtimeData = nullptr;
    int _sortedColumn = -1;
    int _sortOrder = 0;
};

} // namespace langscore