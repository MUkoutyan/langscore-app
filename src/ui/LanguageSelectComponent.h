#pragma once

#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QButtonGroup>

class LanguageSelectComponent : public QWidget
{
    Q_OBJECT
public:
    LanguageSelectComponent(QString iconPath, QString text ,QWidget *parent);

    void setUseLang(bool is);
    void setDefault(bool is);
    void setPreviewText(QString text);
    void setFont(QFont font);
    QFont currentFont();
    void attachButtonGroup(QButtonGroup* group);

    void setFontList(std::vector<QFont> fonts, QFont defaultFont);

signals:
    void changeUseLang(bool is);
    void changeDefault(bool is);
    void changeFont(const QFont&);

protected:
    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

private:
    QPushButton* button;
    QCheckBox* defaultCheck;
    QComboBox* fontList;
    QSpinBox* fontSize;
    QFont font;
    QLineEdit* fontPreview;
};

