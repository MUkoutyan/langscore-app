#pragma once

#include <QTreeView>
#include <QStyledItemDelegate>
#include <QStandardItemModel>
#include <QUndoCommand>
#include "../settings.h"
#include <set>
#include <map>

class ClipDetectSettingTreeDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    ClipDetectSettingTreeDelegate(QObject* parent = nullptr);
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    void detectModeIndexChanged(const QModelIndex& index, int value) const;

};

class ClipDetectSettingTree;
class ClipDetectSettingTreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    struct TreeNode 
    {
        enum Type : char {
            Batch, Files
        };
        QString name;
        Type type;
        bool isGroup = false;
        settings::TextValidationLangMap validateType;
        settings::TextValidationLangMap* settingData = nullptr;
        std::vector<std::unique_ptr<TreeNode>> children;
        TreeNode* parent = nullptr;

        //const settings::ValidateTextInfo& getValidateTextInfo(QString langName) const;
        settings::ValidateTextInfo& getValidateTextInfo(QString langName);

    };

    struct DetectTreeUndo : QUndoCommand
    {
        using ValueType = bool;
        DetectTreeUndo(ClipDetectSettingTreeModel* parent, const QModelIndex& index, int role, const QVariant& oldValue, const QVariant& newValue)
            : parent(parent), index(index), role(role), oldValue(oldValue), newValue(newValue)
        {
            setText("Change Detect Settings Value");
        }
        ~DetectTreeUndo() {}

        int id() const override { return 4; }
        void undo() override;
        void redo() override;

    private:
        void SetMode(ClipDetectSettingTreeModel::TreeNode* node, settings::ValidateTextMode value, QUndoStack* undoStack);
        void SetCount(ClipDetectSettingTreeModel::TreeNode* node, int value, QUndoStack* undoStack);
        void SetWidth(ClipDetectSettingTreeModel::TreeNode* node, int value, QUndoStack* undoStack);
        void propagateBatchChange(TreeNode* batchNode);

    private:
        ClipDetectSettingTreeModel* parent;
        int role;
        QModelIndex index;
        QVariant oldValue;
        QVariant newValue;

    };

    explicit ClipDetectSettingTreeModel(std::shared_ptr<settings> settings, QUndoStack* history, ClipDetectSettingTree* parent = nullptr);

    // 必要な仮想関数をオーバーライド
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setupClipDetectTree();

    QModelIndex getLastDescendantIndex(const QModelIndex& parentIndex, int column) const;

    void findFilesNodesByName(TreeNode* current, const QString& targetName, std::vector<TreeNode*>& result) const;

    QUndoStack* getUndoStack() { return history; }

private:

    ClipDetectSettingTree* tree;
    QUndoStack* history;
    std::shared_ptr<settings> setting;
    std::unique_ptr<TreeNode> rootItem;
    QStringList enableLangNames;

    TreeNode* getItem(const QModelIndex& index) const;

    TreeNode* getRootItem() const;

    QString getLangName(const QModelIndex& index) const;

};

class ClipDetectSettingTree : public QTreeView
{
    Q_OBJECT
public:
    explicit ClipDetectSettingTree(std::shared_ptr<settings> settings, QUndoStack* history, QWidget* parent = nullptr);

    void setup();
private:

    void mousePressEvent(QMouseEvent* event) override;

    std::shared_ptr<settings> setting;
    ClipDetectSettingTreeModel* model;
    QModelIndex lastClickedIndex;
};

