#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <vector>
#include <optional>
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

    bool loadFromFile(const QString& path);
    bool loadFromJsonFile(const QString& filePath);
    bool saveToFile(const QString& path) const;

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

    // Custom sorting: order = 1 (asc), -1 (desc), 0 (none = restore original)
    void applySort(int column, int order);

    bool isLanguageColumnHidden(const QString& language) const;

    void setUseLanguageFont(bool use);

    QString getCurrentShowFileName() const {
        return currentShowFileName;
    }

    void clearAll() {
        beginResetModel();
        csvContainer.clear();
        endResetModel();
    }


private:
    langscore::FastCSVContainer csvContainer;
    QString currentShowFileName;
    bool useLanguageFont = false;
    std::shared_ptr<settings> _settings = nullptr;
    std::shared_ptr<ComponentBase::RuntimeData> _runtimeData = nullptr;
    // Keep original data for restoring when sort is cleared
    std::vector<std::vector<QString>> _originalData;
    int _sortedColumn = -1;
    int _sortOrder = 0;
};

} // namespace langscore