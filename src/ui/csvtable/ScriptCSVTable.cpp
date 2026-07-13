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
                               std::weak_ptr<EditDataManager> loadFileManager,
                               QWidget* parent)
    : ComponentBase(component)
    , QWidget(parent)
    , loadFileManager(loadFileManager)
    , scriptFileName(new QLabel(this))
    , scriptFileWordCount(new QLabel(this))
    , autoCheckButton(new QToolButton(this))
    , scriptFilterButton(new QToolButton(this))
    , settingButton(new QToolButton(this))
    , settingPane(new QWidget(this))
    , hideLanguageColumnsAction(new QPushButton(tr("Filter Columns"), this))
    , filterEdit(new QLineEdit(this))
    , csvEditor(new CSVEditor(this->loadFileManager, this, this))
    , currentModel(nullptr)
    , _proxyModel(nullptr)
    , showAllAction(nullptr)
    , hideIgnoreAction(nullptr)
    , uncheckSignOnlyAction(nullptr)
    , uncheckNoHiraganaAction(nullptr)
{
    csvEditor->setSelectionBehavior(QAbstractItemView::SelectItems);
    csvEditor->setSelectionMode(QAbstractItemView::ExtendedSelection);
    csvEditor->setAlternatingRowColors(true);
    csvEditor->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    csvEditor->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    csvEditor->verticalHeader()->setMinimumSectionSize(28);
    csvEditor->verticalHeader()->hide();
    csvEditor->setContextMenuPolicy(Qt::CustomContextMenu);
    csvEditor->setSortingEnabled(false);

    autoCheckButton->setText(tr("Automatic check"));
    scriptFilterButton->setText(tr("Display settings"));

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
            auto* hdr = csvEditor->horizontalHeader();
            if(checked) {
                hdr->setSectionResizeMode(QHeaderView::Fixed);
                hdr->setDefaultSectionSize(fwSpinBox->value());
            } else {
                hdr->setSectionResizeMode(QHeaderView::Interactive);
            }
        });
        connect(fwSpinBox, &QSpinBox::valueChanged, this, [this, fwCheck](int v) {
            if(fwCheck->isChecked()) {
                csvEditor->horizontalHeader()->setDefaultSectionSize(v);
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

        showAllAction = new QAction(tr("Show All Contents"), this);
        showAllAction->setCheckable(true);
        hideIgnoreAction = new QAction(tr("Hide Ignore Contents"), this);
        hideIgnoreAction->setCheckable(true);

        {
            auto appSettings = ComponentBase::getAppSettings();
            showAllScriptContents = appSettings.value("ScriptCSVTable/showAllContents", true).toBool();
        }
        showAllAction->setChecked(showAllScriptContents);
        hideIgnoreAction->setChecked(!showAllScriptContents);

        auto* ag = new QActionGroup(this);
        ag->setExclusive(true);
        ag->addAction(showAllAction);
        ag->addAction(hideIgnoreAction);
        scriptFilterButton->addAction(showAllAction);
        scriptFilterButton->addAction(hideIgnoreAction);

        connect(showAllAction, &QAction::triggered, this, [this]()
        {
            showAllScriptContents = true;
            auto settings = ComponentBase::getAppSettings();
            settings.setValue("ScriptCSVTable/showAllContents", true);
            if(this->_proxyModel) {
                this->_proxyModel->setHideUncheckRow(false);
            }
        });
        connect(hideIgnoreAction, &QAction::triggered, this, [this]()
        {
            showAllScriptContents = false;
            auto settings = ComponentBase::getAppSettings();
            settings.setValue("ScriptCSVTable/showAllContents", false);
            if(this->_proxyModel) {
                this->_proxyModel->setHideUncheckRow(true);
            }
        });
        connect(scriptFilterButton, &QToolButton::clicked,
                scriptFilterButton, &QToolButton::showMenu);
    }

    {
        uncheckSignOnlyAction = new QAction(tr("Uncheck Sign Only Text"), this);
        uncheckSignOnlyAction->setCheckable(true);
        autoCheckButton->addAction(uncheckSignOnlyAction);

        uncheckNoHiraganaAction = new QAction(tr("Uncheck text that does not contain hiragana"), this);
        uncheckNoHiraganaAction->setCheckable(true);
        autoCheckButton->addAction(uncheckNoHiraganaAction);

        {
            auto appSettings = ComponentBase::getAppSettings();
            uncheckSignOnlyAction->setChecked(appSettings.value("ScriptCSVTable/uncheckSignOnly", false).toBool());
            uncheckNoHiraganaAction->setChecked(appSettings.value("ScriptCSVTable/uncheckNoHiragana", false).toBool());
        }

        connect(uncheckSignOnlyAction, &QAction::triggered, this, [this](bool checked) {
            auto settings = ComponentBase::getAppSettings();
            settings.setValue("ScriptCSVTable/uncheckSignOnly", checked);
            if(checked) { unckeckSignOnlyText(); }
        });
        connect(uncheckNoHiraganaAction, &QAction::triggered, this, [this](bool checked) {
            auto settings = ComponentBase::getAppSettings();
            settings.setValue("ScriptCSVTable/uncheckNoHiragana", checked);
            if(checked) { uncheckNotContainHiragana(); }
        });

        connect(autoCheckButton, &QToolButton::clicked,
                autoCheckButton, &QToolButton::showMenu);
    }

    connect(hideLanguageColumnsAction, &QPushButton::clicked,
            this, &ScriptCSVTable::showLanguageColumnMenu);

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
    vLayout->addWidget(csvEditor, 1);
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
    csvEditor->setModel(nullptr);
    filterEdit->clear();
    scriptFileName->setText("");
    scriptFileWordCount->setText("");
}

void ScriptCSVTable::setupScriptTable()
{
    if(setting == nullptr) { return; }

    if(currentModel == nullptr) 
    {

        const auto editFolder = this->setting->langscoreProjectDirectory + "/editing";
        const auto editedData = editFolder + "/" + "Scripts.lsjson";
        auto* mainModel = loadFileManager.lock()->getScriptCSVModel(editedData);

        currentModel = mainModel;
        connect(currentModel, &ScriptCSVTableModel::checkStateChanged,
                this, [this](const QString& scriptFile, Qt::CheckState state) {
                    emit notifyScriptTableChangeItemCheck(withoutExtension(scriptFile), state);
                });
    }
    if(_proxyModel == nullptr) {
        _proxyModel = new ScriptEditorSortFilterProxyModel(this);
        connect(filterEdit, &QLineEdit::textChanged, _proxyModel, &ScriptEditorSortFilterProxyModel::setFilterText);
        connect(_proxyModel, &ScriptEditorSortFilterProxyModel::columnVisibilityChanged, this, &ScriptCSVTable::restoreColumnWidths);
    }

    const auto editFolder = setting->langscoreProjectDirectory + "/editing";
    currentModel->setSettings(setting);
    currentModel->loadFromEditJSONWithSettings(editFolder);

    if(uncheckSignOnlyAction->isChecked())   { 
        unckeckSignOnlyText(); 
    }
    if(uncheckNoHiraganaAction->isChecked()) { 
        uncheckNotContainHiragana(); 
    }

    _proxyModel->setSortState(0, SortOrder::None);
    _proxyModel->setSourceModel(currentModel);
    csvEditor->setModel(_proxyModel);

    _proxyModel->setHideUncheckRow(showAllScriptContents == false);
    _proxyModel->loadColumnFilterFromSettings();

    currentModel->setSettings(this->setting);
    currentModel->setRuntimeData(this->runtimeData);

    QTimer::singleShot(0, this, [this]() {
        csvEditor->setUpdatesEnabled(false);
        restoreColumnWidths();
        csvEditor->setUpdatesEnabled(true);
        csvEditor->update();
    });

    if(csvEditor->selectionModel()) {
        connect(csvEditor->selectionModel(), &QItemSelectionModel::selectionChanged,
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
                csvEditor->setColumnWidth(c, lang.columnSize);
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
    const QSignalBlocker blocker(csvEditor);
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
            auto si = currentModel->index(r, Columns::Include);
            currentModel->setData(si, static_cast<int>(Qt::Unchecked), Qt::CheckStateRole);
        }
    }
}

void ScriptCSVTable::uncheckNotContainHiragana()
{
    if(currentModel == nullptr) { return; }
    const QSignalBlocker blocker(csvEditor);
    const int rows = currentModel->rowCount();
    for(int r = 0; r < rows; ++r) {
        const auto text = currentModel->getOriginalText(r);
        bool hasJp = false;
        for(QChar qc : text) { if(isJapaneseLanguage(qc)) { hasJp = true; break; } }
        if(!hasJp) {
            auto si = currentModel->index(r, Columns::Include);
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
            auto src  = currentModel->index(r, Columns::ScriptName);
            auto prox = _proxyModel->mapFromSource(src);
            if(prox.isValid()) { csvEditor->scrollTo(prox, QAbstractItemView::PositionAtCenter); }
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
            auto src  = currentModel->index(r, Columns::ScriptName);
            auto prox = _proxyModel->mapFromSource(src);
            if(prox.isValid()) {
                csvEditor->selectionModel()->select(
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
    const auto selected = csvEditor->selectionModel()->selectedIndexes();
    if(selected.isEmpty()) { return; }

    const auto proxyIdx  = selected.first();
    const auto sourceIdx = _proxyModel->mapToSource(proxyIdx);
    if(!sourceIdx.isValid()) { return; }

    const int row = sourceIdx.row();
    auto textPoint = currentModel->data(
        currentModel->index(row, Columns::TextPoint), Qt::DisplayRole
    ).toString();

    constexpr size_t invalid = std::numeric_limits<size_t>::max();
    auto [textRow, textCol] = parseScriptWithRowCol(textPoint);

    if(textRow == invalid && textCol == invalid) {
        textPoint = currentModel->data(
            currentModel->index(row, Columns::OriginalText), Qt::DisplayRole
        ).toString();
    }

    const auto origText  = currentModel->getOriginalText(row);
    const auto scriptExt = currentModel->getScriptFileName(row);
    const auto scriptNoExt = withoutExtension(scriptExt);
    const auto displayName = currentModel->data(
        currentModel->index(row, Columns::ScriptName), Qt::DisplayRole
    ).toString();

    scriptFileName->setText(displayName + "(" + scriptNoExt + ")");
    emit scriptTableSelected(displayName, scriptNoExt, textPoint, static_cast<int>(origText.length()));
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
        model->index(target.row(), Columns::ScriptName),
        Qt::DisplayRole).toString();
    setText(displayName.isEmpty()
        ? QObject::tr("Change Table State")
        : QObject::tr("Change Table State : %1").arg(displayName));
}

void ScriptCSVTable::TableUndo::redo()
{
    setValue(newValue);
    auto displayName = model->data(
        model->index(target.row(), Columns::ScriptName),
        Qt::DisplayRole).toString();
    setText(displayName.isEmpty()
        ? QObject::tr("Change Table State")
        : QObject::tr("Change Table State : %1").arg(displayName));
}