﻿#pragma once

#include <QTreeView>
#include <QStyledItemDelegate>
#include <QStandardItemModel>
#include <QUndoCommand>
#include <QListWidget>
#include "../settings.h"
#include <set>
#include <map>

class TagWidget : public QWidget {
    Q_OBJECT

public:

    struct CtrlCharUndo : QUndoCommand
    {
        CtrlCharUndo(std::shared_ptr<settings> setting, const QVariant& oldValue, const QVariant& newValue)
            : setting(setting), oldValue(oldValue), newValue(newValue)
        {
            setText("Change Control Characters");
        }
        ~CtrlCharUndo() {}
        int id() const override { return 5; }
        void undo() override;
        void redo() override;

    private:
        std::shared_ptr<settings> setting;
        QVariant oldValue;
        QVariant newValue;
    };

    TagWidget(std::shared_ptr<settings> setting, QUndoStack* history, QWidget* parent = nullptr);

    void setup(QStringList tagTextList);
    void addTag(const QString& text);

    QStringList getTagList() const {
        return tagStrList;
    }

private slots:
    void removeTag(QListWidgetItem* item);

private:

    bool createTagItem(const QString& text);

    QUndoStack* history;
    std::shared_ptr<settings> setting;
    QListWidget* tagList;
    QStringList tagStrList;
    QRect lastClickedCellRect;
};

class ClipDetectSettingTreeModel;
class ClipDetectSettingTreeDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    ClipDetectSettingTreeDelegate(ClipDetectSettingTreeModel* model, QObject* parent = nullptr);
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;

private:
    ClipDetectSettingTreeModel* model;
    std::array<QString, 3> textValidateModeText;

};

class ClipDetectSettingTree;
class ClipDetectSettingTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    struct TreeNode 
    {
        enum Type : char {
            Batch, Files, Other
        };
        QString name;
        Type type;
        bool isGroup = false;
        settings::TextValidationLangMap validateLangMap;
        settings::TextValidationLangMap* settingData = nullptr;
        std::vector<std::unique_ptr<TreeNode>> children;
        TreeNode* parent = nullptr;

        //const settings::ValidateTextInfo& getValidateTextInfo(QString langName) const;
        settings::ValidateTextInfo& getValidateTextInfo(QString langName);

    };

    struct DetectTreeUndo : QUndoCommand
    {
        using ValueType = bool;
        DetectTreeUndo(ClipDetectSettingTreeModel* parent, TreeNode* item, QString langName, const QVariant& oldValue, const QVariant& newValue)
            : parent(parent), item(item), langName(langName), oldValue(oldValue), newValue(newValue)
        {
            setText("Change Detect Settings Value");
        }
        ~DetectTreeUndo() {}

        int id() const override { return 4; }
        void undo() override;
        void redo() override;


    private:
        ClipDetectSettingTreeModel* parent;
        TreeNode* item;
        QString langName;
        QVariant oldValue;
        QVariant newValue;

        void setValue(settings::ValidateTextInfo info);
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

    std::shared_ptr<settings> getSetting() { return setting; }
    QUndoStack* getUndoStack() { return history; }
    TreeNode* getItem(const QModelIndex& index) const;

private:

    ClipDetectSettingTree* tree;
    QUndoStack* history;
    std::shared_ptr<settings> setting;
    std::unique_ptr<TreeNode> rootItem;
    QStringList enableLangNames;
    std::unordered_map<QString, QString> typeNameTranslateList;
    std::vector<std::vector<QString>> mapInfosCsv;


    TreeNode* getRootItem() const;

    QString getLangName(const QModelIndex& index) const;

    void setValidateInfo(TreeNode* node, QString langName, const QVariant& newValue, const QVariant& oldValue);
    void setControlChars(const QVariant& tagStrList);

    void SetMode(TreeNode* node, const QModelIndex& index, settings::ValidateTextMode value, QUndoStack* undoStack);
    void SetCount(TreeNode* node, const QModelIndex& index, int value, QUndoStack* undoStack);
    void SetWidth(TreeNode* node, const QModelIndex& index, int value, QUndoStack* undoStack);
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

