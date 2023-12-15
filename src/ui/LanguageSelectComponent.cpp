#include "LanguageSelectComponent.h"
#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QFileInfo>
#include <QDir>
#include <QFontDatabase>
#include <unordered_map>

LanguageSelectComponent::LanguageSelectComponent(QLocale locale, ComponentBase* component, QWidget* parent)
    : QWidget(parent)
    , ComponentBase(component)
    , locale(std::move(locale))
    , button(new QPushButton("", this))
    , defaultCheck(new QCheckBox(tr("Original Language"), this))
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

    auto* hLayout = new QHBoxLayout();
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
    });
    connect(defaultCheck, &QCheckBox::clicked, this, [this](bool is){
        this->history->push(new LanguageButtonUndo(this, LanguageButtonUndo::Default, is, !is));
    });

    connect(fontList, &QComboBox::currentTextChanged, this, [this](const QString& text){
        this->history->push(new LanguageButtonUndo(this, LanguageButtonUndo::FontFamilyIndex, text, this->fontList->currentText()));
    });
    connect(fontSize, &QSpinBox::valueChanged, this, [this](int value){
        this->history->push(new LanguageButtonUndo(this, LanguageButtonUndo::FontSize, value, this->fontSize->value()));
    });
}

void LanguageSelectComponent::setUseLang(bool is)
{
    this->button->blockSignals(true);
    this->button->setChecked(is);
    this->button->blockSignals(false);

    auto shortName = settings::getLowerBcp47Name(this->locale);
    auto& lang = this->setting->fetchLanguage(shortName);
    lang.enable = is;
}

void LanguageSelectComponent::setDefault(bool is)
{
    this->defaultCheck->blockSignals(true);
    this->defaultCheck->setChecked(is);
    this->defaultCheck->blockSignals(false);

    if(this->button->isChecked() == false){
        this->button->setChecked(true);
    }

    this->setting->defaultLanguage = settings::getLowerBcp47Name(this->locale);
}

void LanguageSelectComponent::setPreviewText(QString text){
    this->fontPreview->setText(std::move(text));
}

void LanguageSelectComponent::setFont(QString fontFamily)
{
    QFont f(fontFamily);
    this->font.fontData = f;
    this->font.fontData.setPixelSize(this->fontSize->value());
    this->fontPreview->setFont(this->font.fontData);
    this->update();

    auto shortName = settings::getLowerBcp47Name(this->locale);
    auto& lang = this->setting->fetchLanguage(shortName);
    lang.font = this->font;
}

void LanguageSelectComponent::setFontSize(int size)
{
    this->font.fontData.setPixelSize(size);
    this->font.size = size;
    this->fontPreview->setFont(this->font.fontData);
    this->fontSize->setValue(size);
    this->update();

    auto shortName = settings::getLowerBcp47Name(this->locale);
    auto& lang = this->setting->fetchLanguage(shortName);
    lang.font = this->font;
}

QFont LanguageSelectComponent::currentFont(){
    return this->font.fontData;
}

void LanguageSelectComponent::attachButtonGroup(QButtonGroup *group)
{
    group->addButton(this->defaultCheck);
}

void LanguageSelectComponent::setFontList(std::vector<settings::Font> fonts, QString familyName)
{
    QStringList families;
    int size = this->fontList->count();
    for(int i=0; i<size; ++i){
        families.emplace_back(this->fontList->itemText(i));
    }

    auto currentFont = this->fontList->currentText();
    for(auto& font : fonts)
    {
        auto name = font.name;
        if(families.contains(name)){ continue; }

        this->fontList->addItem(name, QVariant(font.fontData));
    }

    if(familyName.isEmpty() == false){
        this->setFont(familyName);
        currentFont = familyName;
    }
    this->fontList->setCurrentText(currentFont);
}

void LanguageSelectComponent::setupData()
{
    auto bcp47Name = settings::getLowerBcp47Name(this->locale);
    auto& langList = this->setting->languages;
    auto langData = std::find(langList.begin(), langList.end(), bcp47Name);

    settings::Language attachInfo;
    if(langData != langList.end()){
        attachInfo = *langData;
    }

    std::vector<settings::Font> fontList;
    settings::Font defaultFont;
    const auto projType = this->setting->projectType;
    for(const auto& [type, index, family, path] : this->setting->fontIndexList)
    {
        auto familyList = QFontDatabase::applicationFontFamilies(index);
        for(const auto& family : familyList)
        {
            auto familyName = family.toLower();
            if(projType == settings::VXAce && familyName.contains("vl gothic")){
                defaultFont.fontData = QFont(family);
                defaultFont.filePath = path;
                defaultFont.name = familyName;
            }
            else if((projType == settings::MV || projType == settings::MZ) && familyName.contains("m+ 1m")) {
                defaultFont.fontData = QFont(family);
                defaultFont.filePath = path;
                defaultFont.name = familyName;
            }
            auto font = QFont(family);
            font.setPixelSize(attachInfo.font.size);
            fontList.emplace_back(settings::Font{family, path, font, static_cast<std::uint32_t>(font.pixelSize())});
        }
    }
    defaultFont.fontData.setPixelSize(22);
    if(langData == langList.end()){
        attachInfo.font = defaultFont;
    }


    auto langName = locale.nativeLanguageName() + "(" + bcp47Name + ")";
    this->button->setText(langName);

    this->fontPreview->setText(locale.nativeLanguageName());

    if(this->setting->defaultLanguage == bcp47Name){
        this->setDefault(true);
        this->setUseLang(true);
    }
    else{
        this->setUseLang(attachInfo.enable);
    }

    this->setFontList(std::move(fontList), attachInfo.font.name);
    this->setFontSize(attachInfo.font.size);

    auto& lang = this->setting->fetchLanguage(bcp47Name);
    lang.font = attachInfo.font;
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
