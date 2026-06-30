#include "ScriptCSVTable.h"

#include <QHeaderView>
#include <QFile>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QActionGroup>
#include <QCheckBox>
#include <QSpinBox>
#include <QTimer>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <icu.h>
#include "../utility.hpp"
#include "../csv.hpp"
#include "../graphics.hpp"

#include <ranges>
#include <algorithm>

using namespace langscore;

namespace
{
    bool isJapaneseLanguage(QChar ch) {
        UErrorCode status = U_ZERO_ERROR;
        UScriptCode script = uscript_getScript(ch.unicode(), &status);
        if(U_SUCCESS(status)) {
            switch(script) {
            case USCRIPT_JAPANESE:
            case USCRIPT_KATAKANA_OR_HIRAGANA:
            case USCRIPT_KATAKANA:
            case USCRIPT_HIRAGANA:
            case USCRIPT_HAN:
                return true;
            default: break;
            }
        }
        return false;
    }
}

ScriptCSVTable::ScriptCSVTable(ComponentBase* component,
                               std::weak_ptr<CSVEditDataManager> loadFileManager,
                               QWidget* parent)
    : ComponentBase(component)
    , QWidget(parent)
    , loadFileManager(std::move(loadFileManager))
    , showAllScriptContents(true)
    , scriptFileName(new QLabel(this))
    , scriptFileWordCount(new QLabel(this))
    , autoCheckButton(new QToolButton(this))
    , scriptFilterButton(new QToolButton(this))
    , settingButton(new QToolButton(this))
    , settingPane(new QWidget(this))
    , hideLanguageColumnsAction(new QPushButton(tr("Filter Columns"), this))
    , filterEdit(new QLineEdit(this))
    , tableView(new QTableView(this))
    , currentModel(nullptr)
    , _proxyModel(nullptr)
{
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tableView->setAlternatingRowColors(true);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    tableView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tableView->verticalHeader()->setMinimumSectionSize(28);
    tableView->verticalHeader()->hide();
    tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    tableView->setSortingEnabled(false);

    {
        settingPane->setWindowFlags(Qt::Popup);
        settingPane->setVisible(false);
        auto* paneLayout = new QVBoxLayout(settingPane);
        settingPane->setLayout(paneLayout);
        paneLayout->addWidget(hideLanguageColumnsAction);

        auto* fontCheck = new QCheckBox(tr("Apply language font"), settingPane);
        paneLayout->addWidget(fontCheck);
        connect(fontCheck, &QCheckBox::clicked, this, [this](bool checked) {
            if(currentModel) { currentModel->setUseLanguageFont(checked); }
        });

        auto* fwLayout  = new QHBoxLayout();
        auto* fwCheck   = new QCheckBox(tr("Fixed Cell Width"), settingPane);
        auto* fwSpinBox = new QSpinBox(settingPane);
        fwSpinBox->setRange(10, 2000);
        fwSpinBox->setValue(100);
        fwSpinBox->setSuffix(" px");
        fwSpinBox->setEnabled(false);
        fwLayout->addWidget(fwCheck);
        fwLayout->addWidget(fwSpinBox);
        paneLayout->addLayout(fwLayout);

        connect(fwCheck, &QCheckBox::toggled, this, [this, fwSpinBox](bool checked) {
            fwSpinBox->setEnabled(checked);
            auto* hdr = tableView->horizontalHeader();
            if(checked) {
                hdr->setSectionResizeMode(QHeaderView::Fixed);
                hdr->setDefaultSectionSize(fwSpinBox->value());
            } else {
                hdr->setSectionResizeMode(QHeaderView::Interactive);
            }
        });
        connect(fwSpinBox, &QSpinBox::valueChanged, this, [this, fwCheck](int v) {
            if(fwCheck->isChecked()) {
                tableView->horizontalHeader()->setDefaultSectionSize(v);
            }
        });

        settingButton->setIcon(QIcon(":/images/resources/image/settings.png"));
        connect(settingButton, &QToolButton::clicked, this, [this]() {
            settingPane->setVisible(true);
            settingPane->move(QCursor::pos());
        });
    }

    filterEdit->setPlaceholderText(tr("Filter rows..."));
    filterEdit->setClearButtonEnabled(true);

    {
        auto icon = scriptFilterButton->icon();
        auto image = icon.pixmap(scriptFilterButton->size()).toImage();
        graphics::ReverceHSVValue(image);
        scriptFilterButton->setIcon(QIcon(QPixmap::fromImage(image)));

        auto* showAllAction  = new QAction(tr("Show All Contents"), this);
        showAllAction->setCheckable(true);
        showAllAction->setChecked(true);
        auto* hideIgnoreAction = new QAction(tr("Hide Ignore Contents"), this);
        hideIgnoreAction->setCheckable(true);
        hideIgnoreAction->setChecked(false);

        auto* ag = new QActionGroup(this);
        ag->setExclusive(true);
        ag->addAction(showAllAction);
        ag->addAction(hideIgnoreAction);
        scriptFilterButton->addAction(showAllAction);
        scriptFilterButton->addAction(hideIgnoreAction);

        connect(showAllAction, &QAction::triggered, this, [this]() {
            showAllScriptContents = true;  setupScriptTable();
        });
        connect(hideIgnoreAction, &QAction::triggered, this, [this]() {
            showAllScriptContents = false; setupScriptTable();
        });
        connect(scriptFilterButton, &QToolButton::clicked,
                scriptFilterButton, &QToolButton::showMenu);
    }

    {
        auto* uncheckSignOnly = new QAction(tr("Uncheck Sign Only Text"), this);
        autoCheckButton->addAction(uncheckSignOnly);
        connect(uncheckSignOnly, &QAction::triggered, this, &ScriptCSVTable::unckeckSignOnlyText);

        auto* uncheckNoHiragana = new QAction(tr("Uncheck text that does not contain hiragana"), this);
        autoCheckButton->addAction(uncheckNoHiragana);
        connect(uncheckNoHiragana, &QAction::triggered, this, &ScriptCSVTable::uncheckNotContainHiragana);

        connect(autoCheckButton, &QToolButton::clicked,
                autoCheckButton, &QToolButton::showMenu);
    }

    connect(hideLanguageColumnsAction, &QPushButton::clicked,
            this, &ScriptCSVTable::showLanguageColumnMenu);

    connect(tableView, &QTableView::customContextMenuRequested,
            this, &ScriptCSVTable::onContextMenuRequested);

    auto* vLayout = new QVBoxLayout();
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(2);
    {
        auto* hLayout = new QHBoxLayout();
        hLayout->setContentsMargins(0, 0, 0, 0);
        hLayout->addWidget(scriptFileName);
        hLayout->addWidget(scriptFileWordCount);
        hLayout->addStretch(1);
        hLayout->addWidget(autoCheckButton);
        hLayout->addWidget(scriptFilterButton);
        hLayout->addWidget(settingButton);
        vLayout->addLayout(hLayout);
    }
    vLayout->addWidget(filterEdit);
    vLayout->addWidget(tableView, 1);
    setLayout(vLayout);
}

void ScriptCSVTable::clear()
{
    if(currentModel)  { 
        currentModel->deleteLater();  
        currentModel = nullptr; 
    }
    if(_proxyModel)   { 
        _proxyModel->deleteLater();   
        _proxyModel  = nullptr; 
    }
    tableView->setModel(nullptr);
    filterEdit->clear();
    scriptFileName->setText("");
    scriptFileWordCount->setText("");
}

void ScriptCSVTable::setupScriptTable()
{
    if(setting == nullptr) { return; }

    if(currentModel == nullptr) {
        currentModel = new ScriptTableViewModel(this);
        connect(currentModel, &ScriptTableViewModel::checkStateChanged,
                this, [this](const QString& scriptFile, Qt::CheckState state) {
                    emit notifyScriptTableChangeItemCheck(withoutExtension(scriptFile), state);
                });
    }
    if(_proxyModel == nullptr) {
        _proxyModel = new CSVEditorSortFilterProxyModel(this);
        connect(filterEdit, &QLineEdit::textChanged, _proxyModel, &CSVEditorSortFilterProxyModel::setFilterText);
        connect(_proxyModel, &CSVEditorSortFilterProxyModel::columnVisibilityChanged, this, &ScriptCSVTable::restoreColumnWidths);
    }

    const auto editFolder = setting->langscoreProjectDirectory + "/editing";
    currentModel->setSettings(setting);
    currentModel->loadFromSettings(editFolder, showAllScriptContents);

    _proxyModel->setSortState(0, CSVEditorSortFilterProxyModel::SortOrder::None);
    _proxyModel->setSourceModel(currentModel);

    tableView->blockSignals(true);
    tableView->setModel(_proxyModel);
    tableView->blockSignals(false);

    _proxyModel->loadColumnFilterFromSettings();

    QTimer::singleShot(0, this, [this]() {
        tableView->setUpdatesEnabled(false);
        restoreColumnWidths();
        tableView->setUpdatesEnabled(true);
        tableView->update();
    });

    if(tableView->selectionModel()) {
        connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &ScriptCSVTable::onScriptTableSelected, Qt::UniqueConnection);
    }
}

void ScriptCSVTable::filterScriptTextData(std::vector<ScriptTextData>&) {}

void ScriptCSVTable::restoreColumnWidths()
{
    if(_proxyModel == nullptr || setting == nullptr) { return; }
    const int colCount = _proxyModel->columnCount();
    for(int c = 0; c < colCount; ++c) {
        const QString hdr = _proxyModel->headerData(c, Qt::Horizontal, Qt::UserRole).toString();
        for(const auto& lang : setting->languages) {
            if(lang.languageName == hdr) {
                tableView->setColumnWidth(c, lang.columnSize);
                break;
            }
        }
    }
}

void ScriptCSVTable::showLanguageColumnMenu()
{
    if(_proxyModel == nullptr || setting == nullptr) { return; }
    auto* menu = new QMenu(this);
    for(const auto& lang : setting->languages) {
        if(!lang.enable) { continue; }
        auto* action = menu->addAction(lang.languageName);
        action->setCheckable(true);
        action->setChecked(!_proxyModel->isLanguageColumnHidden(lang.languageName));
        connect(action, &QAction::toggled, this, [this, code = lang.languageName](bool visible) {
            _proxyModel->setLanguageColumnHidden(code, !visible);
        });
    }
    menu->popup(QCursor::pos());
}

void ScriptCSVTable::updateScriptIgnoreState(QString scriptName, Qt::CheckState check)
{
    if(currentModel == nullptr) { return; }
    currentModel->updateCheckStateFromTree(scriptName, check);
}

void ScriptCSVTable::updateTableTextColor()
{
    if(currentModel == nullptr || currentModel->rowCount() == 0) { return; }
    auto tl = currentModel->index(0, 0);
    auto br = currentModel->index(currentModel->rowCount()-1, currentModel->columnCount()-1);
    emit currentModel->dataChanged(tl, br, {Qt::ForegroundRole});

    auto icon  = scriptFilterButton->icon();
    auto image = icon.pixmap(scriptFilterButton->size()).toImage();
    if(getColorTheme().getCurrentTheme() == ColorTheme::Dark) {
        graphics::ReverceHSVValue(image);
    }
    scriptFilterButton->setIcon(QIcon(QPixmap::fromImage(image)));
    scriptFilterButton->update();
}

QString ScriptCSVTable::getScriptFileNameFromTable(int sourceRow)
{
    if(currentModel == nullptr) { return {}; }
    return withoutExtension(currentModel->getScriptFileName(sourceRow));
}

std::vector<int> ScriptCSVTable::fetchScriptTableSameFileRows(QString scriptName)
{
    if(currentModel == nullptr) { return {}; }
    return currentModel->getRowsForScript(scriptName);
}

Qt::CheckState ScriptCSVTable::getTreeCheckStateBasedOnTable(QString scriptName)
{
    if(currentModel == nullptr) { return Qt::Unchecked; }
    return currentModel->computeTreeCheckState(scriptName);
}

void ScriptCSVTable::unckeckSignOnlyText()
{
    if(currentModel == nullptr) { return; }
    const QSignalBlocker blocker(tableView);
    const int rows = currentModel->rowCount();
    for(int r = 0; r < rows; ++r) {
        const auto text = currentModel->getOriginalText(r);
        bool hasAlpha = false;
        for(QChar qc : text) {
            int c = qc.toLatin1();
            if(c == 0) { c = qc.unicode(); if(c == 0) { continue; } }
            if(0 <= c && c < 127) { hasAlpha |= (std::isalpha(c) != 0); }
            else                  { hasAlpha = true; }
        }
        if(!hasAlpha) {
            auto si = currentModel->index(r, ScriptTableViewModel::COL_INCLUDE);
            currentModel->setData(si, static_cast<int>(Qt::Unchecked), Qt::CheckStateRole);
        }
    }
}

void ScriptCSVTable::uncheckNotContainHiragana()
{
    if(currentModel == nullptr) { return; }
    const QSignalBlocker blocker(tableView);
    const int rows = currentModel->rowCount();
    for(int r = 0; r < rows; ++r) {
        const auto text = currentModel->getOriginalText(r);
        bool hasJp = false;
        for(QChar qc : text) { if(isJapaneseLanguage(qc)) { hasJp = true; break; } }
        if(!hasJp) {
            auto si = currentModel->index(r, ScriptTableViewModel::COL_INCLUDE);
            currentModel->setData(si, static_cast<int>(Qt::Unchecked), Qt::CheckStateRole);
        }
    }
}

void ScriptCSVTable::onScriptTableScrollToRow(const QString& scriptFileName)
{
    if(currentModel == nullptr || _proxyModel == nullptr) { return; }
    const int rows = currentModel->rowCount();
    for(int r = 0; r < rows; ++r) {
        if(withoutExtension(currentModel->getScriptFileName(r)).contains(scriptFileName)) {
            auto src  = currentModel->index(r, ScriptTableViewModel::COL_SCRIPT_NAME);
            auto prox = _proxyModel->mapFromSource(src);
            if(prox.isValid()) { tableView->scrollTo(prox, QAbstractItemView::PositionAtCenter); }
            break;
        }
    }
}

void ScriptCSVTable::onScriptTableSelectRow(const QString& scriptFileName)
{
    if(currentModel == nullptr || _proxyModel == nullptr) { return; }
    const int rows = currentModel->rowCount();
    for(int r = 0; r < rows; ++r) {
        if(withoutExtension(currentModel->getScriptFileName(r)).contains(scriptFileName)) {
            auto src  = currentModel->index(r, ScriptTableViewModel::COL_SCRIPT_NAME);
            auto prox = _proxyModel->mapFromSource(src);
            if(prox.isValid()) {
                tableView->selectionModel()->select(
                    prox,
                    QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
            break;
        }
    }
}

void ScriptCSVTable::onScriptTableSelected()
{
    if(currentModel == nullptr || _proxyModel == nullptr) { return; }
    const auto selected = tableView->selectionModel()->selectedRows();
    if(selected.isEmpty()) { return; }

    const auto proxyIdx  = selected.first();
    const auto sourceIdx = _proxyModel->mapToSource(proxyIdx);
    if(!sourceIdx.isValid()) { return; }

    const int row = sourceIdx.row();
    const auto textPoint = currentModel->data(
        currentModel->index(row, ScriptTableViewModel::COL_TEXT_POINT), Qt::DisplayRole
    ).toString();
    auto [textRow, textCol] = parseScriptWithRowCol(textPoint);

    const auto origText  = currentModel->getOriginalText(row);
    const auto scriptExt = currentModel->getScriptFileName(row);
    const auto scriptNoExt = withoutExtension(scriptExt);
    const auto displayName = currentModel->data(
        currentModel->index(row, ScriptTableViewModel::COL_SCRIPT_NAME), Qt::DisplayRole
    ).toString();

    scriptFileName->setText(displayName + "(" + scriptNoExt + ")");
    emit scriptTableSelected(displayName, scriptNoExt,
                             textRow, textCol, static_cast<int>(origText.length()));
}

void ScriptCSVTable::onContextMenuRequested(const QPoint& pos)
{
    if(currentModel == nullptr || _proxyModel == nullptr) { return; }
    const auto proxyIdx = tableView->indexAt(pos);
    if(!proxyIdx.isValid()) { return; }

    const auto sourceIdx = _proxyModel->mapToSource(proxyIdx);
    const bool isLangCol = (sourceIdx.column() >= ScriptTableViewModel::FIXED_COLS);

    QMenu menu(this);

    auto* copyAction = menu.addAction(tr("Copy"));
    connect(copyAction, &QAction::triggered, this, [this]() 
    {
        const auto indexes = tableView->selectionModel()->selectedIndexes();
        if(indexes.isEmpty()) { return; }

        QMap<int, QMap<int, QString>> cells;
        for(const auto& i : indexes) {
            cells[i.row()][i.column()] = i.data(Qt::DisplayRole).toString();
        }
        QStringList rows;
        for(auto& row : cells) {
            QStringList cols;
            for(auto& col : row) { 
                cols << col; 
            }
            rows << cols.join('\t');
        }
        QApplication::clipboard()->setText(rows.join('\n'));
    });

    if(isLangCol) {
        auto* pasteAction = menu.addAction(tr("Paste"));
        connect(pasteAction, &QAction::triggered, this, [this]() {
            const auto text  = QApplication::clipboard()->text();
            const auto lines = text.split('\n');
            const auto selRows = tableView->selectionModel()->selectedRows();
            if(selRows.isEmpty()) { return; }
            const auto srcTop = _proxyModel->mapToSource(selRows.first());
            int baseCol = std::max(srcTop.column(), ScriptTableViewModel::FIXED_COLS);
            for(int li = 0; li < lines.size(); ++li) {
                const int srcRow = srcTop.row() + li;
                if(srcRow >= currentModel->rowCount()) { break; }
                const auto cols = lines[li].split('\t');
                for(int ci = 0; ci < cols.size(); ++ci) {
                    const int srcCol = baseCol + ci;
                    if(srcCol >= currentModel->columnCount()) { break; }
                    currentModel->setData(currentModel->index(srcRow, srcCol), cols[ci], Qt::EditRole);
                }
            }
        });

        menu.addSeparator();
        auto* clearAction = menu.addAction(tr("Clear Selection"));
        connect(clearAction, &QAction::triggered, this, [this]() {
            for(const auto& i : tableView->selectionModel()->selectedIndexes()) {
                auto src = _proxyModel->mapToSource(i);
                if(src.column() >= ScriptTableViewModel::FIXED_COLS) {
                    currentModel->setData(src, QString(), Qt::EditRole);
                }
            }
        });
    }

    menu.addSeparator();

    auto* checkAction   = menu.addAction(tr("Check rows"));
    auto* uncheckAction = menu.addAction(tr("Uncheck rows"));
    connect(checkAction, &QAction::triggered, this, [this]() {
        for(const auto& i : tableView->selectionModel()->selectedRows()) {
            auto src = _proxyModel->mapToSource(i);
            currentModel->setData(currentModel->index(src.row(), ScriptTableViewModel::COL_INCLUDE),
                                  static_cast<int>(Qt::Checked), Qt::CheckStateRole);
        }
    });
    connect(uncheckAction, &QAction::triggered, this, [this]() {
        for(const auto& i : tableView->selectionModel()->selectedRows()) {
            auto src = _proxyModel->mapToSource(i);
            currentModel->setData(currentModel->index(src.row(), ScriptTableViewModel::COL_INCLUDE),
                                  static_cast<int>(Qt::Unchecked), Qt::CheckStateRole);
        }
    });

    menu.exec(tableView->viewport()->mapToGlobal(pos));
}

void ScriptCSVTable::changeScriptTableItemCheck(QString scriptName, Qt::CheckState check)
{
    updateScriptIgnoreState(scriptName, check);
    updateTableTextColor();
}

void ScriptCSVTable::setScriptFileName(QString fileName)
{
    scriptFileName->setText(fileName);
}

void ScriptCSVTable::receive(DispatchType type, const QVariantList& args)
{
    Q_UNUSED(args);
    if(type == DispatchType::SaveProject) {
        if(currentModel == nullptr || !setting) { return; }
        const auto editFolder = setting->langscoreProjectDirectory + "/editing";
        currentModel->saveAllModifiedFiles(editFolder);
    }
    else if(type == DispatchType::ChangeColor) {
        updateTableTextColor();
    }
}

void ScriptCSVTable::TableUndo::setValue(ValueType value)
{
    if(!target.isValid() || !model) { return; }
    model->setData(target, static_cast<int>(value), Qt::CheckStateRole);
}

void ScriptCSVTable::TableUndo::undo()
{
    setValue(oldValue);
    auto displayName = model->data(
        model->index(target.row(), ScriptTableViewModel::COL_SCRIPT_NAME),
        Qt::DisplayRole).toString();
    setText(displayName.isEmpty()
        ? QObject::tr("Change Table State")
        : QObject::tr("Change Table State : %1").arg(displayName));
}

void ScriptCSVTable::TableUndo::redo()
{
    setValue(newValue);
    auto displayName = model->data(
        model->index(target.row(), ScriptTableViewModel::COL_SCRIPT_NAME),
        Qt::DisplayRole).toString();
    setText(displayName.isEmpty()
        ? QObject::tr("Change Table State")
        : QObject::tr("Change Table State : %1").arg(displayName));
}