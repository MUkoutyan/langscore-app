#pragma once

#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QButtonGroup>

#include "ComponentBase.h"

class LanguageSelectComponent : public QWidget, public ComponentBase
{
    Q_OBJECT
public:
    LanguageSelectComponent(QLocale locale, ComponentBase* component, QWidget *parent);

    void setPreviewText(QString text);
    QFont currentFont();
    void attachButtonGroup(QButtonGroup* group);

    void setFontList(std::vector<QFont> fonts, QFont defaultFont);

protected:
    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

private:
    QLocale locale;
    QPushButton* button;
    QCheckBox* defaultCheck;
    QComboBox* fontList;
    QSpinBox* fontSize;
    QFont font;
    QLineEdit* fontPreview;

    void setUseLang(bool is);
    void setDefault(bool is);
    void setFont(QString fontFamily);
    void setFontSize(int size);
    void setupData();

    QString getLowerBcp47Name() const;

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

        void undo() override;
        void redo() override;

    private:
        LanguageSelectComponent* button;
        Type type;
        ValueType newValue;
        ValueType oldValue;

        void setValue(Type t, const ValueType& value);
    };
};

