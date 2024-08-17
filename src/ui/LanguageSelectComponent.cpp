#include "LanguageSelectComponent.h"
#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QFileInfo>
#include <QDir>
#include <QFontDatabase>

LanguageSelectComponent::LanguageSelectComponent(QLocale locale, ComponentBase* component, QWidget* parent)
    : QWidget(parent)
    , ComponentBase(component)
    , locale(std::move(locale))
    , button(new QPushButton("", this))
    , defaultCheck(new QCheckBox(tr("Original Language"), this))
    , selectableFontList(new QComboBox(this))
    , fontSize(new QSpinBox(this))
    , fontPreview(new QLineEdit("", this))
{
    this->addDispatch(this);
    this->setAcceptDrops(true);

    this->button->setFixedHeight(34);
    this->button->setIconSize({30, 30});
    this->button->setCheckable(true);
    this->button->setChecked(false);


    this->selectableFontList->setEditable(true);
    this->selectableFontList->lineEdit()->setReadOnly(true);
    this->selectableFontList->lineEdit()->setAlignment(Qt::AlignCenter);

    this->fontPreview->setMinimumHeight(48);
    this->fontPreview->setReadOnly(true);
    this->fontPreview->setAlignment(Qt::AlignHCenter);

    this->fontSize->setFixedWidth(42);
    this->fontSize->setRange(2, 64);
    this->fontSize->setSingleStep(1);
    this->fontSize->setValue(22);

    changeColor(this->getColorTheme().getCurrentTheme());

    auto* vLayout = new QVBoxLayout(this);
    vLayout->setSpacing(0);
    vLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding));
    vLayout->addWidget(button);

    auto* hLayout = new QHBoxLayout();
    hLayout->addWidget(selectableFontList);
    hLayout->addWidget(fontSize);
    vLayout->addLayout(hLayout);

    vLayout->addWidget(fontPreview);
    vLayout->addWidget(defaultCheck, 0, Qt::AlignHCenter);
    vLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding));

    //connectの前に呼ぶ
    setupData();

    connect(button, &QPushButton::clicked, this, [this](bool is){
        this->history->push(new LanguageButtonUndo(this, LanguageButtonUndo::Enable, is, !is));
        emit this->ChangeUseLanguageState();
    });
    connect(defaultCheck, &QCheckBox::clicked, this, [this](bool is){
        this->history->push(new LanguageButtonUndo(this, LanguageButtonUndo::Default, is, !is));
    });

    connect(selectableFontList, &QComboBox::currentTextChanged, this, [this](const QString& text){
        this->history->push(new LanguageButtonUndo(this, LanguageButtonUndo::FontFamilyIndex, text, this->selectableFontList->currentText()));
    });
    connect(fontSize, &QSpinBox::valueChanged, this, [this](int value){
        this->history->push(new LanguageButtonUndo(this, LanguageButtonUndo::FontSize, value, this->fontSize->value()));
    });
}

LanguageSelectComponent::~LanguageSelectComponent()
{
    this->removeDispatch(this);
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
    fontFamily = f.family();
    auto fontFamilyLower = fontFamily.toLower();

    for(const auto& [type, index, familyList, path] : this->setting->fontIndexList)
    {
        bool find = false;
        for(auto& family : familyList)
        {
            if(family.toLower() != fontFamilyLower){
                continue;
            }
            this->font.filePath = path;
            this->font.name = fontFamily;
            find = true;
            break;
        }
        if(find){ break; }
    }

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

void LanguageSelectComponent::setSelectableFontList(std::vector<settings::Font> fonts, QString familyName)
{
    QStringList families;
    int size = this->selectableFontList->count();
    for(int i=0; i<size; ++i){
        families.emplace_back(this->selectableFontList->itemText(i));
    }

    auto currentFont = this->selectableFontList->currentText();
    for(auto& font : fonts)
    {
        auto name = font.name;
        if(families.contains(name)){ continue; }

        this->selectableFontList->addItem(name, QVariant(font.fontData));
    }

    if(familyName.isEmpty() == false){
        this->setFont(familyName);
        currentFont = familyName;
    }
    this->selectableFontList->setCurrentText(currentFont);
}

void LanguageSelectComponent::setupData()
{
    auto bcp47Name = settings::getLowerBcp47Name(this->locale);
    const auto& langList = this->setting->languages;
    auto langData = std::find(langList.cbegin(), langList.cend(), bcp47Name);

    bool isButtonEnable = false;
    QString fontName = "";
    if(langData != langList.cend()){
        isButtonEnable = langData->enable;
        fontName = langData->font.name;
    }

    this->setFontSize(this->setting->getDefaultFontSize());
    auto langName = locale.nativeLanguageName() + "(" + bcp47Name + ")";
    this->button->setText(langName);

    this->fontPreview->setText(locale.nativeLanguageName());

    if(this->setting->defaultLanguage == bcp47Name){
        this->setDefault(true);
        isButtonEnable = true;
    }
    this->setUseLang(isButtonEnable);
    std::vector<settings::Font> fontList = this->setting->createFontList();
    this->setSelectableFontList(std::move(fontList), fontName);
}

void LanguageSelectComponent::changeColor(ColorTheme::Theme theme)
{
    if(theme == ColorTheme::Theme::Dark){
        this->button->setStyleSheet("QPushButton:checked{ background-color: #225b95; }");
        this->defaultCheck->setStyleSheet("QCheckBox:checked{ background-color: #225b95; }");
    }
    else{
        this->button->setStyleSheet("QPushButton:checked{ background-color: #3d9cdb; }");
        this->defaultCheck->setStyleSheet("QCheckBox:checked{ background-color: #3d9cdb; }");
    }
    this->selectableFontList->lineEdit()->setStyleSheet(this->button->styleSheet());
    this->button->update();
    this->defaultCheck->update();
}

void LanguageSelectComponent::receive(DispatchType type, const QVariantList &args)
{
    if(type != ComponentBase::ChangeColor){ return; }

    if(args.empty()){ return; }

    bool isOk = false;
    auto value = args[0].toInt(&isOk);
    if(isOk == false){ return; }

    auto theme = static_cast<ColorTheme::Theme>(value);
    changeColor(theme);
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
