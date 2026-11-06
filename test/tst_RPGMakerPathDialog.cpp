#include "test_common.h"
#include "dialog/RPGMakerPathDialog.h"

TEST(RPGMakerPathDialogTest, TestPlayCommand)
{
    // ツクールごとに、テストプレイを起動する実行ファイルと引数が正しいこと
    const QString proj = "C:/Games/Project";
    const QString maker = "D:/Steam/RPG Maker";

    auto [vxace, vxaceArgs] = RPGMakerPathDialog::testPlayCommand(settings::VXAce, proj, maker);
    EXPECT_EQ(vxace, proj + "/Game.exe");
    EXPECT_EQ(vxaceArgs, QStringList({"console", "test"}));

    auto [mv, mvArgs] = RPGMakerPathDialog::testPlayCommand(settings::MV, proj, maker);
    EXPECT_EQ(mv, maker + "/nwjs-win-test/Game.exe");
    EXPECT_EQ(mvArgs, QStringList({proj, "test"}));

    auto [mz, mzArgs] = RPGMakerPathDialog::testPlayCommand(settings::MZ, proj, maker);
    EXPECT_EQ(mz, maker + "/nwjs-win/nw.exe");
    EXPECT_EQ(mzArgs, QStringList({proj, "test"}));

    EXPECT_TRUE(RPGMakerPathDialog::testPlayCommand(settings::None, proj, maker).first.isEmpty());
}
