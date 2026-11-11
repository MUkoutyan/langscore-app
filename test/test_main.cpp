#include <gtest/gtest.h>
#include <QApplication>

int main(int argc, char* argv[])
{
    // 画面のない環境でもウィジェットやクリップボードを使えるよう、指定がなければ offscreen で動かす
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    // exe の隣にコピーした Qt の DLL からはプラグインの場所が分からないため、Qt のインストール先を指定する
    if(qEnvironmentVariableIsEmpty("QT_PLUGIN_PATH")) {
        qputenv("QT_PLUGIN_PATH", LANGSCORE_QT_PLUGIN_DIR);
    }

    // gtest 用の引数を取り除いてから QApplication に渡す
    ::testing::InitGoogleTest(&argc, argv);
    QApplication app(argc, argv);
    return RUN_ALL_TESTS();
}
