#include <catch2/catch.hpp>
#include <common/path_types.h>
#include <common/fileutils.h>
#include <fstream>
#include <type_traits>
#include <unordered_set>

using namespace eka2l1;

static_assert(!std::is_constructible_v<host_path, guest_path>);
static_assert(!std::is_constructible_v<guest_path, host_path>);
static_assert(!std::is_assignable_v<guest_path &, std::u16string>);
static_assert(!std::is_convertible_v<guest_path, host_path>);
static_assert(!std::is_convertible_v<host_path, guest_path>);
static_assert(!std::is_convertible_v<guest_path, std::u16string>);
static_assert(!std::is_convertible_v<host_path, std::string>);
static_assert(!std::is_convertible_v<std::u16string, guest_path>);
static_assert(!std::is_convertible_v<std::string, host_path>);

TEST_CASE("Guest path identity ignores case and separators without changing spelling", "[path_types]") {
    const guest_path original(u"Z:\\Private\\12345678\\Intro_Groove 01.mid");
    const guest_path other(u"z:/private/12345678/INTRO_GROOVE 01.MID");
    REQUIRE(original == other);
    REQUIRE_FALSE(original < other);
    REQUIRE_FALSE(other < original);
    REQUIRE(original.utf16() == u"Z:\\Private\\12345678\\Intro_Groove 01.mid");
    std::unordered_set<guest_path> files{original};
    REQUIRE(files.contains(other));
    REQUIRE(files.size() == 1);
    REQUIRE(guest_path(u"C:relative") != guest_path(u"C:\\relative"));
    REQUIRE(guest_path(u"C:\\one") != guest_path(u"C:\\two"));
}

TEST_CASE("Guest path case folding is independent of the host locale", "[path_types]") {
    REQUIRE(guest_path(u"C:\\ÄÖÜ\\ДАННЫЕ\\\U00010400.dat") == guest_path(u"c:\\äöü\\данные\\\U00010428.DAT"));
    REQUIRE(guest_path(u"C:\\café") != guest_path(u"C:\\cafe"));
    REQUIRE(guest_path::from_utf8("C:\\音乐\\Ä.dat").utf8() == "C:\\音乐\\Ä.dat");
}

TEST_CASE("Host paths keep lexical spelling and require an explicit guest mapping", "[path_types]") {
    const host_path root("typed_path_fixture");
    common::delete_folder(root);
    common::create_directories(root / "GameData");
    {
        std::ofstream file((root / "GameData" / "Settings.Dat").utf8());
        file << "settings";
    }
    REQUIRE(host_path("GameData") != host_path("gamedata"));
    const auto resolved = root.resolve_guest(u"gamedata\\settings.dat");
    REQUIRE(resolved.filename() == host_path("Settings.Dat"));
    REQUIRE(common::exists(resolved));
    const auto created = root.resolve_guest(u"GAMEDATA\\NewSave.Dat");
    REQUIRE(created.filename() == host_path("NewSave.Dat"));
    REQUIRE(common::is_dir(created.parent()));
    common::delete_folder(root);
}

TEST_CASE("Path operations preserve the domain and original filename", "[path_types]") {
    const auto path = guest_path(u"C:\\Media") / u"Track.MP3";
    REQUIRE(path.filename() == guest_path(u"track.mp3"));
    REQUIRE(path.filename().utf16() == u"Track.MP3");
    REQUIRE(path.has_extension(u".mp3"));
    REQUIRE(path.with_extension(u".Wav").utf16() == u"C:\\Media\\Track.Wav");
    REQUIRE(guest_path(u"Data\\Save.Dat").absolute(u"C:\\Games\\").utf16() == u"C:\\Games\\Data\\Save.Dat");
    REQUIRE_FALSE((host_path("host root") / "Track.MP3").has_extension(".mp3"));
    REQUIRE_THROWS_AS(host_path(std::string("a\0b", 3)), std::invalid_argument);
    REQUIRE_THROWS_AS(guest_path(std::u16string(u"a\0b", 3)), std::invalid_argument);
}

TEST_CASE("Path searches fold guest names and keep filename boundaries", "[path_types]") {
    const guest_path path(u"C:\\Dotted.Dir\\Track.MP3");
    REQUIRE(path.find(u"track.mp3") == 14);
    REQUIRE(path.find(u"dotted/dir") == guest_path::npos);
    REQUIRE(path.starts_with(u"c:/dotted.dir/"));
    REQUIRE(guest_path(u"C:\\Dotted.Dir\\Track").extension().empty());
    REQUIRE(host_path("Dotted.Dir/Track").extension().empty());
    REQUIRE(host_path("Dotted.Dir/Track").with_extension(".dat").utf8() == "Dotted.Dir/Track.dat");
    auto changed = path;
    changed.replace(14, 5, u"Other");
    REQUIRE(changed == guest_path(u"c:/dotted.dir/other.mp3"));
    REQUIRE(std::hash<guest_path>{}(changed) == std::hash<guest_path>{}(guest_path(u"C:\\DOTTED.DIR\\OTHER.MP3")));
}

TEST_CASE("Guest path moves leave both lookup keys consistent", "[path_types]") {
    guest_path original(u"C:\\Mixed.Dat");
    guest_path moved(std::move(original));
    REQUIRE(original.empty());
    REQUIRE(original == guest_path{});
    REQUIRE(moved == guest_path(u"c:/mixed.dat"));
    original = moved;
    moved = guest_path(u"D:\\Other.Dat");
    REQUIRE(original == guest_path(u"C:\\MIXED.DAT"));
    REQUIRE(moved == guest_path(u"d:/other.dat"));
}

TEST_CASE("Native folder copy keeps distinct Linux filename spellings", "[path_types]") {
    const host_path root("native_path_copy_fixture");
    common::delete_folder(root);
    const auto source = root / "Source";
    const auto destination = root / "Destination";
    common::create_directories(source);
    if (!common::is_path_case_insensitive(source)) {
        { std::ofstream file((source / "Track.Dat").c_str()); file << "upper"; }
        { std::ofstream file((source / "track.dat").c_str()); file << "lower"; }
        REQUIRE(common::copy_folder(source, destination, 0));
        std::string upper, lower;
        std::ifstream((destination / "Track.Dat").c_str()) >> upper;
        std::ifstream((destination / "track.dat").c_str()) >> lower;
        REQUIRE(upper == "upper");
        REQUIRE(lower == "lower");
    }
    REQUIRE(common::delete_folder(root));
}
