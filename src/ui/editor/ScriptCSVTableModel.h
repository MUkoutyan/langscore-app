#pragma once

#include <QAbstractTableModel>
#include "ComponentBase.h"
#include <QVector>
#include <QSet>
#include <QJsonObject>
#include <memory>
#include <unordered_map>
#include "../../text_position.h"

class settings;

class ScriptCSVTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:

    explicit ScriptCSVTableModel(QObject* parent = nullptr);

    void setSettings(std::shared_ptr<settings> setting);
    void setRuntimeData(std::shared_ptr<ComponentBase::RuntimeData> setting);

    void loadFromEditJSONWithSettings(const QString& editingDir);
    //editing/Scripts.lsjson と、書き出し・検証用の editing/Scripts.csv に保存する
    bool saveToFile(const QString& editJsonFile) const;
    bool saveToEditJsonFile(const QString& editingDir) const;
    bool saveToEditCsvFile(const QString& csvPath) const;
    void refreshLanguages();

    void setUseLanguageFont(bool use);

    // 指定行の原文テキストを返す（auto-check 操作向け）
    QString getOriginalText(int row) const;

    // 指定行の位置を旧形式の Scripts.csv の original と同じ "ファイル名:行:列" で返す。行・列を持たない行は空
    QString getPositionKey(int row) const;

    // 指定行のスクリプトファイル名を返す（拡張子付き）
    QString getScriptFileName(int row) const;

    // 指定行のチェック状態を返す
    Qt::CheckState getCheckState(int row) const;

    // ツリー側からのチェック状態変更（settings の更新なし）
    void updateCheckStateFromTree(const QString& scriptName, Qt::CheckState state);

    // 指定スクリプト名に対応する行インデックス一覧を返す
    std::vector<int> getRowsForScript(const QString& scriptName) const;

    // ツリー表示用のチェック状態を計算して返す
    Qt::CheckState computeTreeCheckState(const QString& scriptFileName) const;


    // QAbstractTableModel 実装
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    int textPointColumn() const;

signals:
    void checkStateChanged(const QString& scriptFileName, Qt::CheckState treeCheckState);

private:
    struct ScriptRowData {
        bool include = true;
        bool scriptIgnored = false;  // 親スクリプトがスクリプトレベルで無視されているか 
        QString scriptDisplayName;   // 表示用スクリプト名（拡張子付き）
        QString scriptFileName;      // ファイル名（拡張子付き、settings 検索用）
        QString textPoint;           // "10:5" または引数名
        langscore::ScriptTextPosition position;
        QString originalText;
        QVector<QString> translations; // _languages と同順
        QJsonObject savedTranslates;   // 読み込んだ翻訳。_languagesに依存せず、ファイルの内容をそのまま保持する。
    };

    QVector<ScriptRowData>    _rows;
    QVector<QString>          _languages;  // 言語コード一覧
    std::shared_ptr<settings> _settings;
    std::shared_ptr<ComponentBase::RuntimeData> _runtimeData = nullptr;
    bool                      _useLanguageFont = false;

    // scriptDisplayName → 行インデックス のキャッシュ
    std::unordered_map<QString, std::vector<int>> _scriptNameToRows;


    void rebuildCache();
};
