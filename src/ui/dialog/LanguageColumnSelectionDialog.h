#pragma once

#include <QDialog>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include "../editor/CSVEditor.h"
#include "service/LanguageNames.h"

class LanguageColumnSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    LanguageColumnSelectionDialog(const QStringList& languages, CSVEditor* csvEditor, QWidget* parent = nullptr)
        : QDialog(parent), selectedLanguages()
    {
        setWindowTitle(tr("Hide Language Columns"));
        setModal(true);
        resize(300, 400);

        setupUI(languages, csvEditor);
    }

    std::vector<std::pair<QString, bool>> getLanguagesVisibleState() const {
        std::vector<std::pair<QString, bool>> result;
        for(const auto& [lang, checkBox] : checkBoxes.asKeyValueRange()) {
            result.emplace_back(std::pair(lang, checkBox->isChecked()));
        }
        return result;
    }

private slots:
    void onSelectAll() {
        for(auto* checkBox : checkBoxes) {
            checkBox->setChecked(true);
        }
    }

    void onDeselectAll() {
        for(auto* checkBox : checkBoxes) {
            checkBox->setChecked(false);
        }
    }

private:
    void setupUI(const QStringList& languages, CSVEditor* csvEditor) {
        auto* mainLayout = new QVBoxLayout(this);

        // Title label
        auto* titleLabel = new QLabel(tr("Select language columns to hide:"), this);
        titleLabel->setWordWrap(true);
        mainLayout->addWidget(titleLabel);

        // Checkbox area with scroll if needed
        auto* scrollArea = new QScrollArea(this);
        auto* scrollWidget = new QWidget();
        auto* scrollLayout = new QVBoxLayout(scrollWidget);

        // Create checkboxes for each language
        for(const QString& language : languages)
        {
            auto* checkBox = new QCheckBox(langscore::languageDisplayName(language), scrollWidget);

            // Set initial state based on current column visibility
            bool isCurrentlyVisible = (false == csvEditor->isLanguageColumnHidden(language));
            checkBox->setChecked(isCurrentlyVisible);

            checkBoxes[language] = checkBox;
            scrollLayout->addWidget(checkBox);
        }

        scrollLayout->addStretch();
        scrollArea->setWidget(scrollWidget);
        scrollArea->setWidgetResizable(true);
        scrollArea->setMaximumHeight(250);
        mainLayout->addWidget(scrollArea);

        // Selection buttons
        auto* selectionLayout = new QHBoxLayout();
        auto* selectAllBtn = new QPushButton(tr("Select All"), this);
        auto* deselectAllBtn = new QPushButton(tr("Deselect All"), this);

        connect(selectAllBtn, &QPushButton::clicked, this, &LanguageColumnSelectionDialog::onSelectAll);
        connect(deselectAllBtn, &QPushButton::clicked, this, &LanguageColumnSelectionDialog::onDeselectAll);

        selectionLayout->addWidget(selectAllBtn);
        selectionLayout->addWidget(deselectAllBtn);
        selectionLayout->addStretch();
        mainLayout->addLayout(selectionLayout);

        // Dialog buttons
        auto* buttonLayout = new QHBoxLayout();
        auto* okButton = new QPushButton(tr("OK"), this);
        auto* cancelButton = new QPushButton(tr("Cancel"), this);

        connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

        buttonLayout->addStretch();
        buttonLayout->addWidget(okButton);
        buttonLayout->addWidget(cancelButton);
        mainLayout->addLayout(buttonLayout);
    }


    QMap<QString, QCheckBox*> checkBoxes;
    QStringList selectedLanguages;
};