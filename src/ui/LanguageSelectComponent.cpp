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
    , fontSize(new QSpinBox(this))
    , fontPreview(new QLineEdit(text, this))
{
    this->setAcceptDrops(true);

    this->button->setFixedHeight(34);
    this->button->setIconSize({30, 30});
    this->button->setCheckable(true);
    this->button->setChecked(false);


    this->fontList->setEditable(true);
    this->fontList->lineEdit()->setReadOnly(true);
    this->fontList->lineEdit()->setAlignment(Qt::AlignCenter);

    this->fontPreview->setMinimumHeight(48);
    this->fontPreview->setReadOnly(true);
    this->fontPreview->setAlignment(Qt::AlignHCenter);

    this->fontSize->setFixedWidth(42);
    this->fontSize->setRange(2, 64);
    this->fontSize->setSingleStep(1);
    this->fontSize->setValue(22);

    this->button->setStyleSheet("QPushButton:checked{ background-color: #225b95; }");
    this->fontList->lineEdit()->setStyleSheet(this->button->styleSheet());
    this->defaultCheck->setStyleSheet("QCheckBox:checked{ background-color: #225b95; }");

    auto* vLayout = new QVBoxLayout(this);
    vLayout->setSpacing(0);
    vLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding));
    vLayout->addWidget(button);

    auto* hLayout = new QHBoxLayout(this);
    hLayout->addWidget(fontList);
    hLayout->addWidget(fontSize);
    vLayout->addLayout(hLayout);

    vLayout->addWidget(fontPreview);
    vLayout->addWidget(defaultCheck, 0, Qt::AlignHCenter);
    vLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding));

    connect(button, &QPushButton::clicked, this, &LanguageSelectComponent::changeUseLang);
    connect(defaultCheck, &QCheckBox::clicked, this, &LanguageSelectComponent::changeDefault);

    connect(fontList, &QComboBox::currentTextChanged, this, [this](const QString& text){
        QFont f(text);
        this->font = f;
        this->font.setPixelSize(this->fontSize->value());
        this->fontPreview->setFont(this->font);
        this->update();
        emit this->changeFont(this->font);
    });
    connect(fontSize, &QSpinBox::valueChanged, this, [this](int value){
        this->font.setPixelSize(value);
        this->fontPreview->setFont(this->font);
        this->update();
        emit this->changeFont(this->font);
    });
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
    this->fontSize->setValue(font.pixelSize());
    this->fontPreview->setFont(font);
    this->update();
}

QFont LanguageSelectComponent::currentFont(){
    return this->font;
}

void LanguageSelectComponent::attachButtonGroup(QButtonGroup *group)
{
    group->addButton(this->defaultCheck);
}

void LanguageSelectComponent::setFontList(std::vector<QFont> fonts, QFont defaultFont)
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

    if(defaultFont.family().isEmpty() == false){
        this->setFont(defaultFont);
        this->fontList->setCurrentText(defaultFont.family());
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
    setFontList(std::move(fonts), QFont());
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
