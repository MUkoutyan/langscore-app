#include "LanguageSelectComponent.h"
#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QFileInfo>

LanguageSelectComponent::LanguageSelectComponent(QString iconPath, QString text, QWidget* parent)
    : QWidget(parent)
    , button(new QPushButton(QIcon(iconPath), std::move(text), this))
    , defaultCheck(new QCheckBox(tr("Default"), this))
    , fontList(new QComboBox(this))
    , fontPreview(new QLineEdit(text, this))
{
    this->setAcceptDrops(true);

    this->button->setFixedHeight(26);
    this->button->setCheckable(true);
    this->button->setChecked(false);

    this->fontPreview->setReadOnly(true);
    this->fontPreview->setAlignment(Qt::AlignHCenter);

    auto* vLayout = new QVBoxLayout(this);
    vLayout->setSpacing(0);
    vLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding));
    vLayout->addWidget(button);
    vLayout->addWidget(fontList);
    vLayout->addWidget(fontPreview);
    vLayout->addWidget(defaultCheck, 0, Qt::AlignHCenter);
    vLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding));
}

void LanguageSelectComponent::setUseLang(bool is){
    this->button->setChecked(is);
}

void LanguageSelectComponent::setDefault(bool is){
    this->defaultCheck->setChecked(is);
}

void LanguageSelectComponent::setPreviewText(QString text){
    this->fontPreview->setText(std::move(text));
}

void LanguageSelectComponent::setFont(QFont font)
{
    this->fontPreview->setFont(font);
    this->update();
}

void LanguageSelectComponent::setFontList(std::vector<QFont> fonts)
{
    QStringList families;
    int size = this->fontList->count();
    for(int i=0; i<size; ++i){
        families.emplace_back(this->fontList->itemText(i));
    }

    for(auto& font : fonts)
    {
        auto name = font.family();
        if(families.contains(name)){ continue; }

        this->fontList->addItem(name, QVariant(font));
    }
}

void LanguageSelectComponent::dropEvent(QDropEvent *event)
{
    const QMimeData* mimeData = event->mimeData();
    //URLがあれば通す
    if(mimeData->hasUrls() == false){ return; }

    QList<QUrl> urlList = mimeData->urls();

    std::vector<QFont> fonts;
    for(auto& url : urlList)
    {
        QFileInfo fileInfo(url.toLocalFile());
        if(fileInfo.suffix() != "ttf"){ continue; }

        QFont font(fileInfo.path());
        fonts.emplace_back(font.family());
    }
    setFontList(std::move(fonts));
}

void LanguageSelectComponent::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData* mimeData = event->mimeData();
    //URLがあれば通す
    if(mimeData->hasUrls() == false){ return; }

    QList<QUrl> urlList = mimeData->urls();
    for(auto& url : urlList)
    {
        QFileInfo fileInfo(url.toLocalFile());
        if(fileInfo.suffix() != "ttf"){ continue; }

        event->acceptProposedAction();
        return;
    }
}
