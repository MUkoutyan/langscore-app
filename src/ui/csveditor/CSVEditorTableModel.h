#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <vector>
#include <optional>
#include "FastCSVContainer.h"
#include "../../settings.h"

namespace langscore {

class CSVEditorTableModel : public QAbstractTableModel 
{
    Q_OBJECT
public:
    CSVEditorTableModel(QObject* parent = nullptr);
    explicit CSVEditorTableModel(const QString& path, QObject* parent = nullptr);

    // CSV file operations
    bool loadFromFile(const QString& path);
    bool loadFromJsonFile(const QString& filePath);
    bool saveToFile(const QString& path) const;

    // QAbstractTableModel overrides
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Convenience methods
    size_t rowCountRaw() const;
    size_t colCountRaw() const;
    std::optional<QString> getCell(size_t row, size_t col) const;
    bool setCell(size_t row, size_t col, const QString& value);

    void insertColumn(size_t index, const QString& columnName);
    void removeColumn(int column);

    const std::vector<std::vector<QString>>& dataRaw() const;

    void setSettings(std::shared_ptr<settings> setting);

    void setUseLanguageFont(bool use) {
        this->useLanguageFont = use;
    }

    void clearAll() {
        beginResetModel();
        csvContainer.clear();
        endResetModel();
    }


private:
    langscore::FastCSVContainer csvContainer;
    bool useLanguageFont = false;
    std::shared_ptr<settings> _settings = nullptr;
};

} // namespace langscore