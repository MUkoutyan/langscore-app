#pragma once

#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>

class LanguageSelectComponent : public QWidget
{
    Q_OBJECT
public:
    LanguageSelectComponent(QString iconPath, QString text ,QWidget *parent);

    void setUseLang(bool is);
    void setDefault(bool is);
    void setPreviewText(QString text);
    void setFont(QFont font);

    void setFontList(std::vector<QFont> fonts);

signals:
    void changeUseLang(bool is);
    void changeDefault(bool is);

protected:
    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

private:
    QPushButton* button;
    QCheckBox* defaultCheck;
    QComboBox* fontList;
    QFont font;
    QLineEdit* fontPreview;
};

