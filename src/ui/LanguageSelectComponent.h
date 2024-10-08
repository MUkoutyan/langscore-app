﻿#pragma once

#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QButtonGroup>

#include "ComponentBase.h"

//言語選択ボタンやフォント選択を構成するコンポーネント。
//このクラスを使用して言語タブが構成される。
class LanguageSelectComponent : public QWidget, public ComponentBase
{
    Q_OBJECT
public:
    LanguageSelectComponent(QLocale locale, ComponentBase* component, QWidget *parent);
    ~LanguageSelectComponent();

    void setPreviewText(QString text);
    QFont currentFont();
    void attachButtonGroup(QButtonGroup* group);

    void setSelectableFontList(std::vector<settings::Font> fonts, QString familyName);

signals:
    void ChangeUseLanguageState();

private:
    QLocale locale;
    QPushButton* button;
    QCheckBox* defaultCheck;
    QComboBox* selectableFontList;
    QSpinBox* fontSize;
    settings::Font font;
    QLineEdit* fontPreview;

    void setUseLang(bool is);
    void setDefault(bool is);
    void setFont(QString fontFamily);
    void setFontSize(int size);
    void setupData();

    void changeColor(ColorTheme::Theme t);

    void receive(DispatchType type, const QVariantList& args) override;

    struct LanguageButtonUndo : QUndoCommand
    {
        enum Type
        {
            Enable,
            Default,
            FontFamilyIndex,
            FontSize
        };
        using ValueType = std::variant<bool, QString, int>;

        LanguageButtonUndo(LanguageSelectComponent* button, Type type, ValueType newValue, ValueType oldValue)
            : button(button), type(type), newValue(std::move(newValue)), oldValue(std::move(oldValue))
        {}
        ~LanguageButtonUndo(){}

        int id() const override { return 1; }
        void undo() override;
        void redo() override;

    private:
        LanguageSelectComponent* button;
        Type type;
        ValueType newValue;
        ValueType oldValue;

        void setValue(Type t, const ValueType& value);
    };

#ifdef LANGSCORE_GUIAPP_TEST
    friend class LangscoreAppTest;
#endif
};

