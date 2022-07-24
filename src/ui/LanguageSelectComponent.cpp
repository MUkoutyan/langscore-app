#include "LanguageSelectComponent.h"
#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QDir>
#include <QFontDatabase>
#include <unordered_map>

static std::unordered_map<QString, QString> languagePairs = {
    {QLocale{QLocale::English}.bcp47Name(), "us.svg"},
    {QLocale{QLocale::Spanish}.bcp47Name(), "es.svg"},
    {QLocale{QLocale::German}.bcp47Name(), "de.svg"},
    {QLocale{QLocale::French}.bcp47Name(), "fr.svg"},
    {QLocale{QLocale::Italian}.bcp47Name(), "it.svg"},
    {QLocale{QLocale::Japanese}.bcp47Name(), "jp.svg"},
    {QLocale{QLocale::Korean}.bcp47Name(), "kr.svg"},
    {QLocale{QLocale::Russian}.bcp47Name(), "ru.svg"},
    {QLocale{QLocale::Chinese, QLocale::SimplifiedChineseScript}.bcp47Name(), "cn.svg"},
    {QLocale{QLocale::Chinese, QLocale::TraditionalChineseScript}.bcp47Name(), "cn.svg"}
};

LanguageSelectComponent::LanguageSelectComponent(QLocale locale, ComponentBase* component, QWidget* parent)
    : QWidget(parent)
    , ComponentBase(component)
    , locale(std::move(locale))
    , button(new QPushButton("", this))
    , defaultCheck(new QCheckBox(tr("Default"), this))
    , fontList(new QComboBox(this))
    , fontSize(new QSpinBox(this))
    , fontPreview(new QLineEdit("", this))
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

    //connectの前に呼ぶ
    setupData();

    connect(button, &QPushButton::clicked, this, [this](bool is){
        this->history->push(new LanguageButtonUndo(this, LanguageButtonUndo::Enable, is, !is));
        this->setUseLang(is);
    });
    connect(defaultCheck, &QCheckBox::clicked, this, [this](bool is){
        this->history->push(new LanguageButtonUndo(this, LanguageButtonUndo::Default, is, !is));
        this->setDefault(is);
    });

    connect(fontList, &QComboBox::currentTextChanged, this, [this](const QString& text){
        this->history->push(new LanguageButtonUndo(this, LanguageButtonUndo::FontFamilyIndex, text, this->fontList->currentText()));
        this->setFont(text);
    });
    connect(fontSize, &QSpinBox::valueChanged, this, [this](int value){
        this->history->push(new LanguageButtonUndo(this, LanguageButtonUndo::FontSize, value, this->fontSize->value()));
        this->setFontSize(value);
    });
}

void LanguageSelectComponent::setUseLang(bool is)
{
    this->button->blockSignals(true);
    this->button->setChecked(is);
    this->button->blockSignals(false);

    auto shortName = this->getLowerBcp47Name();
    auto& lang = this->setting->fetchLanguage(shortName);
    lang.enable = is;
}

void LanguageSelectComponent::setDefault(bool is)
{
    this->defaultCheck->blockSignals(true);
    this->defaultCheck->setChecked(is);
    this->defaultCheck->blockSignals(false);

    this->setting->defaultLanguage = this->getLowerBcp47Name();
}

void LanguageSelectComponent::setPreviewText(QString text){
    this->fontPreview->setText(std::move(text));
}

void LanguageSelectComponent::setFont(QString fontFamily)
{
    QFont f(fontFamily);
    this->font = f;
    this->font.setPixelSize(this->fontSize->value());
    this->fontPreview->setFont(this->font);
    this->update();

    auto shortName = this->getLowerBcp47Name();
    auto& lang = this->setting->fetchLanguage(shortName);
    lang.font = settings::Font{font.family(), static_cast<std::uint32_t>(font.pixelSize())};
}

void LanguageSelectComponent::setFontSize(int size)
{
    this->font.setPixelSize(size);
    this->fontPreview->setFont(this->font);
    this->update();

    auto shortName = this->getLowerBcp47Name();
    auto& lang = this->setting->fetchLanguage(shortName);
    lang.font = settings::Font{font.family(), static_cast<std::uint32_t>(font.pixelSize())};
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

        this->fontSize->setValue(font.pixelSize());
        this->fontList->addItem(name, QVariant(font));
    }

    if(defaultFont.family().isEmpty() == false){
        this->setFont(defaultFont.family());
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

void LanguageSelectComponent::setupData()
{
    if(this->setting->fontIndexList.empty())
    {
        auto fontExtensions = QStringList() << "*.ttf" << "*.otf";
        {
            QFileInfoList files = QDir(qApp->applicationDirPath()+"/resources/fonts").entryInfoList(fontExtensions, QDir::Files);
            for(auto& info : files){
                this->setting->fontIndexList.emplace_back(QFontDatabase::addApplicationFont(info.absoluteFilePath()));
            }
        }
        //プロジェクトに登録されたフォント
        {
            QFileInfoList files = QDir(this->setting->gameProjectPath+"/Fonts").entryInfoList(fontExtensions, QDir::Files);
            for(auto& info : files){
                this->setting->fontIndexList.emplace_back(QFontDatabase::addApplicationFont(info.absoluteFilePath()));
            }
        }
    }

    std::vector<QFont> fontList;
    QFont defaultFont;
    const auto projType = this->setting->projectType;
    for(auto fontIndex : this->setting->fontIndexList)
    {
        auto familyList = QFontDatabase::applicationFontFamilies(fontIndex);
        for(const auto& family : familyList)
        {
            auto familyName = family.toLower();
            if(projType == settings::VXAce && familyName.contains("vl gothic")){
                defaultFont = QFont(family);
            }
            else if((projType == settings::MV || projType == settings::MZ) && familyName.contains("m+ 1m")) {
                defaultFont = QFont(family);
            }
            auto font = QFont(family);
            font.setPixelSize(22);
            fontList.emplace_back(font);
        }
    }
    defaultFont.setPixelSize(22);

    auto bcp47Name = getLowerBcp47Name();
    auto langName = locale.nativeLanguageName() + "(" + bcp47Name + ")";
    this->button->setText(langName);
    if(languagePairs.find(locale.bcp47Name()) != languagePairs.end()){
        auto flagName = languagePairs[locale.bcp47Name()];
        this->button->setIcon(QIcon(":flags/"+flagName));
    }

    this->fontPreview->setText(locale.nativeLanguageName());

    //プロジェクトの設定を反映
    if(this->setting->defaultLanguage == bcp47Name){
        this->setDefault(true);
    }
    auto& langList = this->setting->languages;
    auto langData = std::find(langList.begin(), langList.end(), bcp47Name);
    if(langData != langList.end()){
        this->setUseLang(langData->enable);
        QFont f(langData->font.name);
        f.setPixelSize(langData->font.size);
        this->setFontList(fontList, f);
    }
    else{
        this->setUseLang(false);
        this->setFontList(fontList, defaultFont);
    }
}

QString LanguageSelectComponent::getLowerBcp47Name() const
{
    auto bcp47Name = this->locale.bcp47Name().toLower();
    if(bcp47Name == "zh"){ bcp47Name = "zh-cn"; }
    return bcp47Name;
}


void LanguageSelectComponent::LanguageButtonUndo::undo(){
    this->setValue(type, oldValue);
}

void LanguageSelectComponent::LanguageButtonUndo::redo(){
    this->setValue(type, newValue);
}

void LanguageSelectComponent::LanguageButtonUndo::setValue(Type t, const ValueType &value)
{
    switch(t)
    {
    case Type::Enable:
        button->setUseLang(std::get<bool>(value));
        break;
    case Type::Default:
        button->setDefault(std::get<bool>(value));
        break;
    case Type::FontFamilyIndex:
        button->setFont(std::get<QString>(value));
        break;
    case Type::FontSize:
        button->setFontSize(std::get<int>(value));
        break;
    }
}
