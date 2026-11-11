#include "RPGMakerPathDialog.h"
#include "../ComponentBase.h"
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QLabel>
#include <QDir>

namespace
{
    QString SettingKey(settings::ProjectType type) {
        return type == settings::MV ? "RPGMakerPath/MV" : "RPGMakerPath/MZ";
    }
}

RPGMakerPathDialog::RPGMakerPathDialog(QWidget* parent)
    : QDialog(parent)
    , mvPathEdit(new QLineEdit(this))
    , mzPathEdit(new QLineEdit(this))
{
    this->setWindowTitle(tr("RPG Maker Location"));
    this->setMinimumWidth(560);

    //入力欄の右に、フォルダーを選択するボタンを並べる
    const auto PathRow = [this](QLineEdit* edit, QString placeholder) {
        edit->setPlaceholderText(placeholder);
        auto* browse = new QPushButton(tr("Browse..."), this);
        connect(browse, &QPushButton::clicked, this, [this, edit]() {
            auto path = QFileDialog::getExistingDirectory(this, tr("RPG Maker Location"), edit->text());
            if(path.isEmpty() == false) { edit->setText(path); }
        });
        auto* row = new QHBoxLayout();
        row->addWidget(edit);
        row->addWidget(browse);
        return row;
    };

    auto* formLayout = new QFormLayout();
    formLayout->addRow("MV", PathRow(mvPathEdit, "C:/Program Files (x86)/Steam/steamapps/common/RPG Maker MV"));
    formLayout->addRow("MZ", PathRow(mzPathEdit, "C:/Program Files (x86)/Steam/steamapps/common/RPG Maker MZ"));

    //VXAce はゲームのフォルダーにある Game.exe で起動するため、指定は不要
    auto* note = new QLabel(tr("Specify the folder where RPG Maker is installed.\nVX Ace does not need to be specified because the Game.exe in the game project is used."), this);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]()
    {
        auto appSettings = ComponentBase::getAppSettings();
        appSettings.setValue(SettingKey(settings::MV), QDir::fromNativeSeparators(mvPathEdit->text().trimmed()));
        appSettings.setValue(SettingKey(settings::MZ), QDir::fromNativeSeparators(mzPathEdit->text().trimmed()));
        appSettings.sync();
        this->accept();
    });

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(note);
    layout->addLayout(formLayout);
    layout->addWidget(buttons);

    mvPathEdit->setText(rpgMakerPath(settings::MV));
    mzPathEdit->setText(rpgMakerPath(settings::MZ));
}

QString RPGMakerPathDialog::rpgMakerPath(settings::ProjectType type)
{
    return ComponentBase::getAppSettings().value(SettingKey(type)).toString();
}

std::pair<QString, QStringList> RPGMakerPathDialog::testPlayCommand(settings::ProjectType type, const QString& gameProjectPath, const QString& rpgMakerPath)
{
    switch(type)
    {
    case settings::VXAce:
        return {gameProjectPath + "/Game.exe", {"console", "test"}};
    case settings::MV:
        return {rpgMakerPath + "/nwjs-win-test/Game.exe", {gameProjectPath, "test"}};
    case settings::MZ:
        return {rpgMakerPath + "/nwjs-win/nw.exe", {gameProjectPath, "test"}};
    default:
        return {};
    }
}
