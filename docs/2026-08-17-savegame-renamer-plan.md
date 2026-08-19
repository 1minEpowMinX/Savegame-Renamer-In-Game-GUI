# Savegame Renamer - In-Game GUI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Переименование существующего сохранения Kingdom Come: Deliverance II из штатного меню загрузки, средствами интерфейса самой игры.

**Architecture:** Нативный плагин под KCSE плюс собственный элемент Scaleform поверх меню. Четыре модуля: `whs::Description` (чтение и запись заголовка `.whs`, чистый C++ без зависимостей), `SaveCatalog` (мост к `C_SaveGameManager`), `RenameDialog` (собственный `.gfx`), `SaveLoadHook` (склейка, хук на `C_UISaveLoad`). Ванильные файлы игры не подменяются.

**Tech Stack:** C++17, CMake, Ninja, vcpkg, Catch2, KCSE + libKCD2, ActionScript 2 (Flash 8, Scaleform GFx), JPEXS FFDec, Python 3 для упаковки.

Спецификация: [2026-08-17-savegame-renamer-design.md](2026-08-17-savegame-renamer-design.md).

## Global Constraints

- modid, имя папки, имя pak и имя DLL: `savegame_renamer`. Отображаемое имя: `Savegame Renamer - In-Game GUI`. Автор: `Lefxxx`.
- Целевая версия игры `1.5.6-15693-release_1_5`, `WHGame.dll 1.5.6 (kd7u)`.
- Ванильные файлы игры не подменяются ни при каких условиях.
- В заголовок `.whs` разрешено добавлять **только атрибуты корневого элемента**. Неизвестный дочерний элемент ломает разбор описания, сохранение исчезает из списка. Проверено экспериментально.
- Атрибут для хранения оригинала: `RenamerOriginal="<questName>|<objectiveName>"`.
- Payload сохранения при любой записи обязан остаться побайтово неизменным.
- Запись только атомарная: временный файл в том же каталоге, затем замена.
- Кодировка заголовка UTF-8. `length` в префиксе включает завершающий NUL.
- Мягкий лимит имени 40 символов: счётчик подсвечивается, запись не блокируется.
- Клавиши: F2 открывает диалог, Enter принимает, Esc отменяет. Сброс к оригиналу — кнопка в диалоге.
- ActionScript пишется под парсер FFDec: одно объявление на `var`, без объектных литералов, без тернарного оператора, без цепочек присваивания.
- Корень проекта: `D:\Games\Self-Mods\KCD2\savegame_renamer`. Все пути ниже относительны ему.
- Лицензия **GPLv3**: KCSE распространяется под ней, мод линкуется с `kcd_re` и публикуется с исходниками.
- Стандарт **C++17**: тот же, что у `kcd_re`, иначе ABI статической библиотеки не сойдётся.
- XML элементов интерфейса кладётся в `Libs/UI/UIElements/<Name>.xml` внутри собственного pak мода.
- Модель правит заголовок точечными строковыми заменами, а не пересериализацией через pugixml: всё, что мод не трогает, обязано остаться в файле байт в байт.

### Окружение, проверено на машине

| Компонент | Путь или состояние |
|---|---|
| MSVC | `D:\IDE\Microsoft Visual Studio\18\Community`, toolset 14.51.36231 |
| vcvars | `D:\IDE\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat` |
| cmake, ninja | `...\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\{CMake\bin,Ninja}` |
| libKCD2 | склонирован в `D:\Games\Self-Mods\KCD2\_deps\libKCD2` |
| KCSE | установлен, `<игра>\Bin\Win64MasterMasterSteamPGO\dinput8.dll` |
| Address Library | установлена, `<игра>\KCSE\addresslib\kcd_addresslib_steam_release_1_5-15693.bin` совпадает со сборкой игры |
| vcpkg | отсутствует, поднимается в задаче 1 |
| JPEXS FFDec | отсутствует, нужен начиная с задачи 8 |

---

## File Structure

| Файл | Ответственность |
|---|---|
| `src/mod.manifest` | Манифест мода для загрузчика игры |
| `cpp/include/whs/Description.h` | Публичный интерфейс модели заголовка |
| `cpp/src/whs/Description.cpp` | Разбор, правка и атомарная запись заголовка |
| `cpp/include/whs/Sanitise.h` | Очистка пользовательского ввода |
| `cpp/src/whs/Sanitise.cpp` | Реализация очистки и XML-экранирования |
| `cpp/src/game/SaveCatalog.h/.cpp` | Доступ к `C_SaveGameManager`, поиск файла по id, обновление списка |
| `cpp/src/game/RenameDialog.h/.cpp` | Владение UI-элементом, push имени в flash, приём событий |
| `cpp/src/game/SaveLoadHook.h/.cpp` | Хук `C_UISaveLoad`, F2, определение выделенной строки |
| `cpp/src/game/InputForwarder.h/.cpp` | `IInputEventListener`, форвард Enter и Esc в movie |
| `cpp/src/plugin.cpp` | Точка входа KCSE, сборка модулей |
| `cpp/tests/*.cpp` | Тесты модели на Catch2 |
| `flash/renamer.as` | ActionScript диалога |
| `flash/renamer.xml` | Описание UI-элемента для движка |
| `tools/build.py` | Упаковка pak, сборка релизного архива |
| `prototypes/whs_header.py` | Эталонный прототип модели, уже написан и проверен |

Разделение модель/мост/вид/склейка держится жёстко: `whs::Description` не знает про игру и собирается в тестах без единого заголовка libKCD2, `RenameDialog` не знает про сохранения.

---

## Task 1: Каркас проекта и работающий плагин

**Files:**
- Create: `cpp/.buildenv/CMakeLists.txt`, `cpp/src/plugin.cpp`, `src/mod.manifest`, `LICENSE`
- Modify: `_deps/libKCD2/.buildenv/vcpkg.json` (добавить Catch2), `_deps/libKCD2/.buildenv/CMakePresets.json` (путь к ninja)
- Reference: `_deps/libKCD2/Projects/MCM/.buildenv/CMakeLists.txt`, `_deps/libKCD2/include/KCSE/KCSEAPI.h`

**Interfaces:**
- Consumes: ничего
- Produces: собранная `SavegameRenamer.dll`, которую KCSE подхватывает из `Mods/savegame_renamer/KCSE/Plugins/`

Сборка идёт по схеме libKCD2: его корневой `CMakeLists.txt` сам находит подпроекты по маске `Projects/*/.buildenv/CMakeLists.txt` и линкует их со статической библиотекой `kcd_re`. Исходники остаются в нашем репозитории, а внутрь клона заводится junction.

Шаги 1 и 2 (репозиторий, структура, клон libKCD2) выполнены заранее, коммит `e853c1e`.

- [x] **Step 3: Поднять vcpkg**

```bash
git clone https://github.com/microsoft/vcpkg.git "D:/Games/Self-Mods/KCD2/_deps/vcpkg"
"D:/Games/Self-Mods/KCD2/_deps/vcpkg/bootstrap-vcpkg.bat" -disableMetrics
```

Задать `VCPKG_ROOT=D:\Games\Self-Mods\KCD2\_deps\vcpkg` в окружении пользователя: корневой `CMakeLists.txt` libKCD2 читает именно эту переменную.

- [x] **Step 4: Добавить Catch2 в манифест зависимостей**

В `_deps/libKCD2/.buildenv/vcpkg.json` дописать `"catch2"` в `dependencies`. Остальные пакеты (`spdlog`, `boost-container`, `boost-optional`, `boost-smart-ptr`, `minhook`, `pugixml`, `xbyak`) уже там.

- [x] **Step 5: Завести junction и CMake подпроекта**

```bash
cmd //c mklink //J "D:\Games\Self-Mods\KCD2\_deps\libKCD2\Projects\SavegameRenamer" "D:\Games\Self-Mods\KCD2\savegame_renamer\cpp"
```

`cpp/.buildenv/CMakeLists.txt`, по образцу MCM:

```cmake
project(SavegameRenamer LANGUAGES CXX)

find_package(minhook CONFIG REQUIRED)

file(GLOB_RECURSE PLUGIN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp")

add_library(SavegameRenamer SHARED ${PLUGIN_SOURCES})

target_precompile_headers(SavegameRenamer PRIVATE "${RE_BUILDENV}/PCH.h")

target_compile_definitions(SavegameRenamer PRIVATE
    _ITERATOR_DEBUG_LEVEL=0 WIN32_LEAN_AND_MEAN NOMINMAX)

target_compile_options(SavegameRenamer PRIVATE /utf-8 /W4 /wd4819 /wd4100)

target_include_directories(SavegameRenamer PRIVATE
    ${RE_ROOT}/include ${RE_ROOT}/include/CryEngine/CryCommon
    ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/../src)

target_link_libraries(SavegameRenamer PRIVATE kcd_re minhook::minhook)

# Тесты модели собираются отдельной целью: whs:: не зависит ни от игры, ни от kcd_re,
# поэтому тестам не нужен ни один заголовок libKCD2.
find_package(Catch2 CONFIG REQUIRED)
enable_testing()
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../tests/*.cpp")
add_executable(SavegameRenamerTests ${TEST_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/whs/Description.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/whs/Sanitise.cpp")
target_include_directories(SavegameRenamerTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/../tests)
target_compile_options(SavegameRenamerTests PRIVATE /utf-8 /W4)
target_link_libraries(SavegameRenamerTests PRIVATE Catch2::Catch2WithMain)
include(Catch)
catch_discover_tests(SavegameRenamerTests)
```

Цель тестов сознательно не тянет `kcd_re` и PCH: модель обязана собираться и проверяться в отрыве от игры.

- [x] **Step 6: Поправить пресет под установленную Visual Studio**

В `_deps/libKCD2/.buildenv/CMakePresets.json` заменить `CMAKE_MAKE_PROGRAM` на путь установленной VS:

```
D:/IDE/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe
```

Пресет в апстриме прибит к `18/Community` на диске C, у нас установка на D.

- [x] **Step 7: Написать точку входа**

`cpp/src/plugin.cpp`:

`cpp/src/Log.h` повторяет приём MCM: `gEnv` на момент загрузки плагина может быть ещё не готов, поэтому каждый вызов проверяет его сам.

```cpp
#pragma once

#include "crysystem/SSystemGlobalEnvironment.h"

#define SR_LOG(fmt, ...)                                                       \
    do {                                                                       \
        if (auto* _env = SSystemGlobalEnvironment::GetInstance(); _env && _env->pLog) \
            _env->pLog->LogAlways("[SavegameRenamer] " fmt, ##__VA_ARGS__);    \
    } while (0)
```

`cpp/src/plugin.cpp`:

```cpp
#include "KCSE/KCSEAPI.h"
#include "Log.h"

KCSE_PLUGIN_INFO("SavegameRenamer", "Lefxxx", 1);

namespace {

void OnKcseMessage(KCSE::Message* msg)
{
    if (msg && msg->type == KCSE::IMessagingInterface::kMessage_DataLoaded)
        SR_LOG("data loaded");
}

}  // namespace

KCSE_PLUGIN_LOAD(kcse)
{
    SR_LOG("loaded, KCSE v%u, game build %u",
           kcse->GetKCSEVersion(), kcse->GetGameVersion());
    if (auto* messaging = kcse->GetMessagingInterface())
        messaging->RegisterListener(&OnKcseMessage);
    return true;
}
```

`KCSE_PLUGIN_LOAD` разворачивается в функцию, возвращающую `bool`; без `return true` загрузчик отбросит плагин с сообщением «returned false from KCSEPlugin_Load». Слушатель `DataLoaded` нужен как страховка проверки: если на момент загрузки `gEnv->pLog` ещё не создан, первая строка в лог не попадёт, а вторая попадёт гарантированно.

- [x] **Step 8: Собрать**

Сборка запускается из корня libKCD2, из окружения `vcvars64`:

```bash
cmd //c ""D:\IDE\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cmake --preset release -S "D:\Games\Self-Mods\KCD2\_deps\libKCD2\.buildenv" && cmake --build "D:\Games\Self-Mods\KCD2\_deps\libKCD2\.buildenv\build-release" --target SavegameRenamer SavegameRenamerTests"
```

Ожидается: `SavegameRenamer.dll` и `SavegameRenamerTests.exe` в каталоге сборки. Первый прогон долгий: vcpkg соберёт boost и spdlog.

- [x] **Step 9: Развернуть и проверить загрузку**

Скопировать DLL в `<игра>/Mods/savegame_renamer/KCSE/Plugins/`, положить `src/mod.manifest`:

```xml
<?xml version="1.0" encoding="utf-8"?>
<kcd_mod xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <info>
    <name>Savegame Renamer - In-Game GUI</name>
    <description>Rename an existing savegame from the in-game load menu. No console, no external tools.</description>
    <author>Lefxxx</author>
    <version>1.0</version>
    <created_on>2026-08-17</created_on>
    <modid>savegame_renamer</modid>
    <modifies_level>false</modifies_level>
  </info>
  <supports />
</kcd_mod>
```

Запустить игру, найти в `kcd.log` строку `[SavegameRenamer] loaded`.

Ожидается: строка есть. Если её нет, дальше идти нельзя: вся остальная работа опирается на то, что плагин грузится. Проверить при отказе: экспортируются ли `KCSEPlugin_Version` и `KCSEPlugin_Load` (`dumpbin /exports`), совпадает ли ожидаемая версия KCSE.

- [x] **Step 10: Положить лицензию**

Файл `LICENSE` с текстом GNU GPL v3: KCSE распространяется под GPLv3, мод линкуется с `kcd_re` и публикуется с исходниками.

- [x] **Step 11: Прогнать пустые тесты**

```bash
ctest --test-dir "D:/Games/Self-Mods/KCD2/_deps/libKCD2/.buildenv/build-release" --output-on-failure
```

Ожидается: цель тестов собралась и запустилась. Тестов пока нет, это проверка того, что Catch2 подключён.

- [x] **Step 12: Коммит**

```bash
git add -A && git commit -m "chore: scaffold KCSE plugin project"
```

---

## Task 2: Модель, разбор заголовка

**Files:**
- Create: `cpp/include/whs/Description.h`, `cpp/src/whs/Description.cpp`, `cpp/tests/TestFixture.h`, `cpp/tests/DescriptionRead.cpp`
- Modify: `cpp/CMakeLists.txt` (цель тестов на Catch2 + CTest)

**Interfaces:**
- Consumes: ничего
- Produces:
  - `std::optional<whs::Description> whs::Description::Read(const std::filesystem::path&)`
  - `int whs::Description::SaveId() const`
  - `std::string whs::Description::DisplayName() const`
  - `whs::MakeSave(path, xml, payload)` — генератор фикстур для тестов

Модель держит в памяти только заголовок: payload переписывается потоковым копированием из исходного файла. Сохранение весит мегабайты, читать его целиком незачем.

- [x] **Step 1: Написать генератор фикстур**

`cpp/tests/TestFixture.h`:

```cpp
#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

/// Writes a .whs file with the given description XML and payload bytes.
inline void MakeSave(const std::filesystem::path& path,
                     const std::string& xml,
                     const std::string& payload)
{
    const uint32_t magic = 0xFFFFFFFFu;
    const int32_t length = static_cast<int32_t>(xml.size() + 1);
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(&magic), 4);
    f.write(reinterpret_cast<const char*>(&length), 4);
    f.write(xml.data(), static_cast<std::streamsize>(xml.size()));
    f.put('\0');
    f.write(payload.data(), static_cast<std::streamsize>(payload.size()));
}

/// Returns a description XML shaped like a real permanent save.
inline std::string SampleXml(int saveId = 3754,
                             const std::string& quest = "@qname_navsteva_lekare_sxxH",
                             const std::string& objective = "@jmena_obj_zacatek_questu_JJyn",
                             const std::string& extraAttrs = "")
{
    return "<C_SaveGameDescription FormatVersion=\"0\" SaveType=\"PermanentSave\" SaveId=\""
         + std::to_string(saveId)
         + "\" SaveTime=\"1786705444\" LevelName=\"kutnohorsko\" PlayerId=\"0\" UIDescription=\"0|"
         + std::to_string(saveId) + "|" + quest + "|" + objective
         + "|location_suchdol|1786705444|14/08/2026 13:04|60.478825|\""
         + " BuildInfo=\"1.5.6-15693-release_1_5\" GameMode=\"normal\"" + extraAttrs + ">\n"
           "\t<Locations>\n\t\t<structwh::rpgmodule::S_LocationId>39a52acd</structwh::rpgmodule::S_LocationId>\n"
           "\t</Locations>\n</C_SaveGameDescription>\n";
}
```

- [x] **Step 2: Написать падающий тест**

`cpp/tests/DescriptionRead.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "TestFixture.h"
#include "whs/Description.h"

TEST_CASE("Read parses id and display name", "[description]")
{
    const auto path = std::filesystem::temp_directory_path() / "read_basic.whs";
    MakeSave(path, SampleXml(), std::string(64, '\xAB'));

    const auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    CHECK(d->SaveId() == 3754);
    CHECK(d->DisplayName() == "@qname_navsteva_lekare_sxxH");
}

TEST_CASE("Read rejects a file with the wrong magic", "[description]")
{
    const auto path = std::filesystem::temp_directory_path() / "read_bad_magic.whs";
    std::ofstream(path, std::ios::binary) << "not a savegame at all";

    CHECK_FALSE(whs::Description::Read(path).has_value());
}

TEST_CASE("Read rejects a truncated header", "[description]")
{
    const auto path = std::filesystem::temp_directory_path() / "read_truncated.whs";
    MakeSave(path, SampleXml(), "");
    std::filesystem::resize_file(path, 20);   // header claims far more than the file holds

    CHECK_FALSE(whs::Description::Read(path).has_value());
}
```

- [x] **Step 3: Прогнать тест, убедиться что падает**

Run: `ctest --test-dir cpp/build -R description --output-on-failure`
Expected: FAIL, `whs/Description.h` не найден.

- [x] **Step 4: Написать заголовок модели**

`cpp/include/whs/Description.h`:

```cpp
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace whs {

/// Field positions inside the pipe-separated UIDescription attribute.
enum class UiField {
    Type = 0,
    Id = 1,
    Quest = 2,
    Objective = 3,
    Location = 4,
    Timestamp = 5,
    Date = 6,
    Playtime = 7,
};

/// The description header of one .whs savegame.
///
/// Holds the header text only; the payload stays in the file on disk and is
/// copied through on write.
class Description {
public:
    /// Reads the header of the save at `path`, or nothing when the file is not
    /// a readable savegame.
    ///
    /// @param path Savegame to read.
    /// @return The parsed description, or an empty optional.
    static std::optional<Description> Read(const std::filesystem::path& path);

    /// Returns the save id declared by the SaveId attribute.
    int SaveId() const;

    /// Returns the name the load list shows for this save.
    std::string DisplayName() const;

private:
    std::filesystem::path m_path;
    std::string m_xml;             ///< Header XML, without the terminating NUL.
    std::uint64_t m_payloadOffset; ///< First byte of the payload in m_path.

    std::string Attribute(const std::string& name) const;
    std::vector<std::string> UiFields() const;
};

}  // namespace whs
```

- [x] **Step 5: Реализовать разбор**

`cpp/src/whs/Description.cpp`:

```cpp
#include "whs/Description.h"

#include <cstdint>
#include <fstream>
#include <regex>

namespace whs {
namespace {

constexpr std::uint32_t kMagic = 0xFFFFFFFFu;
constexpr std::size_t kPrefixSize = 8;

}  // namespace

std::optional<Description> Description::Read(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto fileSize = std::filesystem::file_size(path, ec);
    if (ec || fileSize < kPrefixSize)
        return std::nullopt;

    std::ifstream f(path, std::ios::binary);
    if (!f)
        return std::nullopt;

    std::uint32_t magic = 0;
    std::int32_t length = 0;
    f.read(reinterpret_cast<char*>(&magic), 4);
    f.read(reinterpret_cast<char*>(&length), 4);
    if (!f || magic != kMagic || length <= 1)
        return std::nullopt;
    if (kPrefixSize + static_cast<std::uint64_t>(length) > fileSize)
        return std::nullopt;

    std::string xml(static_cast<std::size_t>(length), '\0');
    f.read(xml.data(), length);
    if (!f)
        return std::nullopt;
    xml.resize(xml.find('\0') == std::string::npos ? xml.size() : xml.find('\0'));

    Description d;
    d.m_path = path;
    d.m_xml = std::move(xml);
    d.m_payloadOffset = kPrefixSize + static_cast<std::uint64_t>(length);
    if (d.Attribute("UIDescription").empty() || d.Attribute("SaveId").empty())
        return std::nullopt;
    return d;
}

std::string Description::Attribute(const std::string& name) const
{
    const std::regex re(name + "=\"([^\"]*)\"");
    std::smatch m;
    if (!std::regex_search(m_xml, m, re))
        return {};
    return m[1].str();
}

std::vector<std::string> Description::UiFields() const
{
    std::vector<std::string> out;
    const std::string packed = Attribute("UIDescription");
    std::size_t start = 0;
    while (true) {
        const std::size_t bar = packed.find('|', start);
        if (bar == std::string::npos) {
            out.push_back(packed.substr(start));
            break;
        }
        out.push_back(packed.substr(start, bar - start));
        start = bar + 1;
    }
    return out;
}

int Description::SaveId() const
{
    return std::stoi(Attribute("SaveId"));
}

std::string Description::DisplayName() const
{
    const auto fields = UiFields();
    const auto quest = static_cast<std::size_t>(UiField::Quest);
    return quest < fields.size() ? fields[quest] : std::string{};
}

}  // namespace whs
```

- [x] **Step 6: Прогнать тесты**

Run: `ctest --test-dir cpp/build -R description --output-on-failure`
Expected: PASS, три теста.

- [x] **Step 7: Коммит**

```bash
git add cpp && git commit -m "feat(whs): parse the savegame description header"
```

---

## Task 3: Модель, очистка ввода

**Files:**
- Create: `cpp/include/whs/Sanitise.h`, `cpp/src/whs/Sanitise.cpp`, `cpp/tests/Sanitise.cpp`

**Interfaces:**
- Consumes: ничего
- Produces:
  - `std::string whs::SanitiseName(std::string_view raw)` — убирает `|`, управляющие символы, схлопывает пробелы, обрезает края
  - `std::string whs::XmlEscape(std::string_view raw)` — экранирует `&`, `<`, `>`, `"`
  - `constexpr std::size_t whs::kSoftNameLimit = 40`

Очистка отделена от `Description` намеренно: это чистая функция над строкой, её тесты не трогают файловую систему, и её же будет вызывать слой ввода для подсветки счётчика.

- [x] **Step 1: Написать падающий тест**

`cpp/tests/Sanitise.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "whs/Sanitise.h"

TEST_CASE("SanitiseName drops the field separator", "[sanitise]")
{
    CHECK(whs::SanitiseName("before|after") == "beforeafter");
}

TEST_CASE("SanitiseName drops control characters and collapses runs of spaces", "[sanitise]")
{
    CHECK(whs::SanitiseName("a\r\nb\tc") == "abc");
    CHECK(whs::SanitiseName("  two   words  ") == "two words");
}

TEST_CASE("SanitiseName keeps non-ASCII bytes untouched", "[sanitise]")
{
    const std::string cyrillic = "\xD0\x9F\xD0\xB8\xD1\x81\xD0\xB0\xD1\x80\xD1\x8C";
    CHECK(whs::SanitiseName(cyrillic) == cyrillic);
}

TEST_CASE("XmlEscape escapes ampersand first", "[sanitise]")
{
    CHECK(whs::XmlEscape("a & b") == "a &amp; b");
    CHECK(whs::XmlEscape("\"<>\"") == "&quot;&lt;&gt;&quot;");
    CHECK(whs::XmlEscape("&lt;") == "&amp;lt;");
}
```

Последний случай важен: если `&` экранируется не первым, уже экранированный текст испортится.

- [x] **Step 2: Прогнать, убедиться что падает**

Run: `ctest --test-dir cpp/build -R sanitise --output-on-failure`
Expected: FAIL, `whs/Sanitise.h` не найден.

- [x] **Step 3: Реализовать**

`cpp/include/whs/Sanitise.h`:

```cpp
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace whs {

/// Name length past which the dialog highlights its character counter.
constexpr std::size_t kSoftNameLimit = 40;

/// Returns `raw` with the UIDescription field separator and control characters
/// removed, runs of spaces collapsed and the edges trimmed.
///
/// @param raw Text as typed by the player.
/// @return The cleaned name, possibly empty.
std::string SanitiseName(std::string_view raw);

/// Returns `raw` with the characters that would corrupt an XML attribute value
/// replaced by entities.
///
/// @param raw Text to place inside a double-quoted attribute.
/// @return The escaped text.
std::string XmlEscape(std::string_view raw);

}  // namespace whs
```

`cpp/src/whs/Sanitise.cpp`:

```cpp
#include "whs/Sanitise.h"

namespace whs {

std::string SanitiseName(std::string_view raw)
{
    std::string out;
    out.reserve(raw.size());
    bool pendingSpace = false;
    for (const char c : raw) {
        const auto byte = static_cast<unsigned char>(c);
        if (c == '|' || (byte < 0x20) || byte == 0x7F)
            continue;
        if (c == ' ') {
            pendingSpace = !out.empty();
            continue;
        }
        if (pendingSpace) {
            out.push_back(' ');
            pendingSpace = false;
        }
        out.push_back(c);
    }
    return out;
}

std::string XmlEscape(std::string_view raw)
{
    std::string out;
    out.reserve(raw.size());
    for (const char c : raw) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}

}  // namespace whs
```

- [x] **Step 4: Прогнать тесты**

Run: `ctest --test-dir cpp/build -R sanitise --output-on-failure`
Expected: PASS, четыре теста.

- [x] **Step 5: Коммит**

```bash
git add cpp && git commit -m "feat(whs): sanitise and escape player-typed names"
```

---

## Task 4: Модель, переименование и сброс

**Files:**
- Modify: `cpp/include/whs/Description.h`, `cpp/src/whs/Description.cpp`
- Create: `cpp/tests/DescriptionRename.cpp`

**Interfaces:**
- Consumes: `whs::SanitiseName`, `whs::XmlEscape` из Task 3
- Produces:
  - `void Description::SetDisplayName(std::string_view name)`
  - `void Description::ResetName()`
  - `bool Description::HasCustomName() const`
  - `std::string Description::Xml() const` — текущий заголовок, нужен Task 5 для записи

`SetDisplayName` меняет только текст в памяти. Запись на диск появится в Task 5, поэтому тесты этой задачи проверяют состояние объекта, а не файл.

- [x] **Step 1: Написать падающий тест**

`cpp/tests/DescriptionRename.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "TestFixture.h"
#include "whs/Description.h"

namespace {

whs::Description Load(const std::string& xml, const char* file)
{
    const auto path = std::filesystem::temp_directory_path() / file;
    MakeSave(path, xml, std::string(32, '\x01'));
    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    return *d;
}

}  // namespace

TEST_CASE("SetDisplayName replaces the quest field and clears the objective", "[rename]")
{
    auto d = Load(SampleXml(), "rename_basic.whs");
    d.SetDisplayName("Before the duel");

    CHECK(d.DisplayName() == "Before the duel");
    CHECK(d.Xml().find("|Before the duel||location_suchdol|") != std::string::npos);
}

TEST_CASE("The first rename stashes the original quest and objective", "[rename]")
{
    auto d = Load(SampleXml(), "rename_stash.whs");
    CHECK_FALSE(d.HasCustomName());

    d.SetDisplayName("First");
    CHECK(d.HasCustomName());
    CHECK(d.Xml().find(
        "RenamerOriginal=\"@qname_navsteva_lekare_sxxH|@jmena_obj_zacatek_questu_JJyn\"")
        != std::string::npos);
}

TEST_CASE("A second rename keeps the originally stashed name", "[rename]")
{
    auto d = Load(SampleXml(), "rename_twice.whs");
    d.SetDisplayName("First");
    d.SetDisplayName("Second");

    CHECK(d.DisplayName() == "Second");
    CHECK(d.Xml().find("RenamerOriginal=\"@qname_navsteva_lekare_sxxH|") != std::string::npos);
    CHECK(d.Xml().find("RenamerOriginal=\"First") == std::string::npos);
}

TEST_CASE("ResetName restores both fields and drops the attribute", "[rename]")
{
    auto d = Load(SampleXml(), "rename_reset.whs");
    d.SetDisplayName("Temporary");
    d.ResetName();

    CHECK(d.DisplayName() == "@qname_navsteva_lekare_sxxH");
    CHECK(d.Xml().find("@jmena_obj_zacatek_questu_JJyn") != std::string::npos);
    CHECK(d.Xml().find("RenamerOriginal") == std::string::npos);
    CHECK_FALSE(d.HasCustomName());
}

TEST_CASE("ResetName on a save that was never renamed changes nothing", "[rename]")
{
    auto d = Load(SampleXml(), "rename_reset_noop.whs");
    const std::string before = d.Xml();
    d.ResetName();

    CHECK(d.Xml() == before);
}

TEST_CASE("SetDisplayName sanitises and escapes the input", "[rename]")
{
    auto d = Load(SampleXml(), "rename_escape.whs");
    d.SetDisplayName("Rock & \"roll\" |cut|");

    CHECK(d.Xml().find("Rock &amp; &quot;roll&quot; cut") != std::string::npos);
    CHECK(d.DisplayName() == "Rock & \"roll\" cut");
}

TEST_CASE("An empty name resets instead of blanking the entry", "[rename]")
{
    auto d = Load(SampleXml(), "rename_empty.whs");
    d.SetDisplayName("Something");
    d.SetDisplayName("   ");

    CHECK(d.DisplayName() == "@qname_navsteva_lekare_sxxH");
    CHECK_FALSE(d.HasCustomName());
}
```

Последний тест фиксирует правило спецификации: пустой ввод означает сброс.

- [x] **Step 2: Прогнать, убедиться что падает**

Run: `ctest --test-dir cpp/build -R rename --output-on-failure`
Expected: FAIL, `SetDisplayName` не объявлен.

- [x] **Step 3: Расширить заголовок**

В `cpp/include/whs/Description.h` добавить в публичную секцию:

```cpp
    /// Returns true when the save carries a name written by this mod.
    bool HasCustomName() const;

    /// Replaces the displayed name, stashing the original quest and objective
    /// on the first call. An input that sanitises to nothing resets instead.
    ///
    /// @param name Text as typed by the player.
    void SetDisplayName(std::string_view name);

    /// Restores the quest and objective stashed by the first SetDisplayName
    /// call, and drops the stash. Does nothing when there is no stash.
    void ResetName();

    /// Returns the current header XML.
    const std::string& Xml() const { return m_xml; }
```

и в приватную:

```cpp
    void SetAttribute(const std::string& name, const std::string& value);
    void RemoveAttribute(const std::string& name);
    void SetUiFields(const std::vector<std::string>& fields);
```

Заголовок должен включать `<string_view>`.

- [x] **Step 4: Реализовать**

Дописать в `cpp/src/whs/Description.cpp`, добавив `#include "whs/Sanitise.h"`:

```cpp
namespace {

constexpr const char* kStashAttribute = "RenamerOriginal";

}  // namespace

void Description::SetAttribute(const std::string& name, const std::string& value)
{
    const std::regex re(name + "=\"[^\"]*\"");
    if (std::regex_search(m_xml, re)) {
        m_xml = std::regex_replace(m_xml, re, name + "=\"" + value + "\"",
                                   std::regex_constants::format_first_only);
        return;
    }
    // New attributes go last on the root element, just before its closing '>'.
    const std::size_t close = m_xml.find('>');
    m_xml.insert(close, " " + name + "=\"" + value + "\"");
}

void Description::RemoveAttribute(const std::string& name)
{
    const std::regex re(" " + name + "=\"[^\"]*\"");
    m_xml = std::regex_replace(m_xml, re, "", std::regex_constants::format_first_only);
}

void Description::SetUiFields(const std::vector<std::string>& fields)
{
    std::string packed;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i)
            packed += '|';
        packed += fields[i];
    }
    SetAttribute("UIDescription", packed);
}

bool Description::HasCustomName() const
{
    return !Attribute(kStashAttribute).empty();
}

void Description::SetDisplayName(std::string_view name)
{
    const std::string clean = SanitiseName(name);
    if (clean.empty()) {
        ResetName();
        return;
    }

    auto fields = UiFields();
    const auto quest = static_cast<std::size_t>(UiField::Quest);
    const auto objective = static_cast<std::size_t>(UiField::Objective);
    if (fields.size() <= objective)
        return;

    if (!HasCustomName())
        SetAttribute(kStashAttribute, fields[quest] + "|" + fields[objective]);

    fields[quest] = XmlEscape(clean);
    fields[objective].clear();
    SetUiFields(fields);
}

void Description::ResetName()
{
    const std::string stash = Attribute(kStashAttribute);
    if (stash.empty())
        return;

    const std::size_t bar = stash.find('|');
    auto fields = UiFields();
    const auto quest = static_cast<std::size_t>(UiField::Quest);
    const auto objective = static_cast<std::size_t>(UiField::Objective);
    if (fields.size() <= objective)
        return;

    fields[quest] = stash.substr(0, bar);
    fields[objective] = bar == std::string::npos ? std::string{} : stash.substr(bar + 1);
    SetUiFields(fields);
    RemoveAttribute(kStashAttribute);
}
```

`DisplayName` должен возвращать разэкранированный текст, иначе тест `rename_escape` не пройдёт. Заменить его тело на:

```cpp
std::string Description::DisplayName() const
{
    const auto fields = UiFields();
    const auto quest = static_cast<std::size_t>(UiField::Quest);
    return quest < fields.size() ? XmlUnescape(fields[quest]) : std::string{};
}
```

и добавить в `Sanitise.h`/`Sanitise.cpp` обратную функцию:

```cpp
/// Returns `raw` with XML entities replaced by the characters they stand for.
///
/// @param raw Text taken from an attribute value.
/// @return The decoded text.
std::string XmlUnescape(std::string_view raw);
```

```cpp
std::string XmlUnescape(std::string_view raw)
{
    static const std::pair<std::string_view, char> kEntities[] = {
        {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&amp;", '&'},
    };
    std::string out;
    out.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size();) {
        bool matched = false;
        if (raw[i] == '&') {
            for (const auto& [entity, ch] : kEntities) {
                if (raw.compare(i, entity.size(), entity) == 0) {
                    out.push_back(ch);
                    i += entity.size();
                    matched = true;
                    break;
                }
            }
        }
        if (!matched)
            out.push_back(raw[i++]);
    }
    return out;
}
```

`&amp;` стоит в таблице последним, чтобы `&amp;lt;` расшифровался в `&lt;`, а не в `<`.

- [x] **Step 5: Дописать тест на обратную функцию**

В `cpp/tests/Sanitise.cpp`:

```cpp
TEST_CASE("XmlUnescape is the inverse of XmlEscape", "[sanitise]")
{
    const std::string raw = "a & b < c > d \" e";
    CHECK(whs::XmlUnescape(whs::XmlEscape(raw)) == raw);
    CHECK(whs::XmlUnescape("&amp;lt;") == "&lt;");
}
```

- [x] **Step 6: Прогнать все тесты**

Run: `ctest --test-dir cpp/build --output-on-failure`
Expected: PASS, все тесты Task 2, 3, 4.

- [x] **Step 7: Коммит**

```bash
git add cpp && git commit -m "feat(whs): rename a savegame and reset it to its original name"
```

---

## Task 5: Модель, атомарная запись

**Files:**
- Modify: `cpp/include/whs/Description.h`, `cpp/src/whs/Description.cpp`
- Create: `cpp/tests/DescriptionWrite.cpp`, `cpp/tests/RealSave.cpp`

**Interfaces:**
- Consumes: всё из Task 4
- Produces: `bool Description::Write() const`

Запись обязана оставить payload побайтово неизменным и не должна оставлять файл в промежуточном состоянии.

- [x] **Step 1: Написать падающий тест**

`cpp/tests/DescriptionWrite.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <fstream>
#include "TestFixture.h"
#include "whs/Description.h"

namespace {

std::string ReadAll(const std::filesystem::path& p)
{
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

std::string PayloadOf(const std::filesystem::path& p)
{
    const std::string all = ReadAll(p);
    std::int32_t length = 0;
    std::memcpy(&length, all.data() + 4, 4);
    return all.substr(8 + static_cast<std::size_t>(length));
}

}  // namespace

TEST_CASE("Write keeps the payload byte for byte", "[write]")
{
    const auto path = std::filesystem::temp_directory_path() / "write_payload.whs";
    std::string payload;
    for (int i = 0; i < 5000; ++i)
        payload.push_back(static_cast<char>(i % 251));
    MakeSave(path, SampleXml(), payload);

    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    d->SetDisplayName("A considerably longer name than the original");
    REQUIRE(d->Write());

    CHECK(PayloadOf(path) == payload);
}

TEST_CASE("Write updates the length prefix so the file re-reads", "[write]")
{
    const auto path = std::filesystem::temp_directory_path() / "write_roundtrip.whs";
    MakeSave(path, SampleXml(), std::string(128, '\x02'));

    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    d->SetDisplayName("Short");
    REQUIRE(d->Write());

    const auto reread = whs::Description::Read(path);
    REQUIRE(reread.has_value());
    CHECK(reread->DisplayName() == "Short");
    CHECK(reread->SaveId() == 3754);
    CHECK(reread->HasCustomName());
}

TEST_CASE("Write shrinks the header when the name gets shorter", "[write]")
{
    const auto path = std::filesystem::temp_directory_path() / "write_shrink.whs";
    MakeSave(path, SampleXml(), std::string(128, '\x03'));

    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    d->SetDisplayName("x");
    REQUIRE(d->Write());
    auto reloaded = whs::Description::Read(path);
    REQUIRE(reloaded.has_value());
    reloaded->ResetName();
    REQUIRE(reloaded->Write());

    const auto final = whs::Description::Read(path);
    REQUIRE(final.has_value());
    CHECK(final->DisplayName() == "@qname_navsteva_lekare_sxxH");
    CHECK(PayloadOf(path) == std::string(128, '\x03'));
}

TEST_CASE("Write refuses when the file on disk is a different save", "[write]")
{
    const auto path = std::filesystem::temp_directory_path() / "write_guard.whs";
    MakeSave(path, SampleXml(3754), std::string(64, '\x04'));

    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    d->SetDisplayName("Stale");

    MakeSave(path, SampleXml(9999), std::string(64, '\x05'));   // game overwrote the slot
    CHECK_FALSE(d->Write());

    const auto onDisk = whs::Description::Read(path);
    REQUIRE(onDisk.has_value());
    CHECK(onDisk->SaveId() == 9999);
}

TEST_CASE("Write leaves no temporary file behind", "[write]")
{
    const auto dir = std::filesystem::temp_directory_path() / "write_tmp_check";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const auto path = dir / "permanent0001.whs";
    MakeSave(path, SampleXml(), std::string(64, '\x06'));

    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    d->SetDisplayName("Tidy");
    REQUIRE(d->Write());

    CHECK(std::distance(std::filesystem::directory_iterator(dir),
                        std::filesystem::directory_iterator{}) == 1);
}
```

- [x] **Step 2: Прогнать, убедиться что падает**

Run: `ctest --test-dir cpp/build -R write --output-on-failure`
Expected: FAIL, `Write` не объявлен.

- [x] **Step 3: Реализовать**

В `cpp/include/whs/Description.h`:

```cpp
    /// Writes the header back, copying the payload through unchanged.
    ///
    /// Refuses when the file on disk no longer holds the save this object was
    /// read from. The write goes to a temporary file in the same directory and
    /// replaces the original only once it is complete.
    ///
    /// @return True when the file was replaced.
    bool Write() const;
```

В `cpp/src/whs/Description.cpp`:

```cpp
bool Description::Write() const
{
    const auto current = Read(m_path);
    if (!current.has_value() || current->Attribute("SaveId") != Attribute("SaveId"))
        return false;

    const auto temp = m_path.parent_path() / (m_path.filename().string() + ".renamer-tmp");
    {
        std::ifstream src(m_path, std::ios::binary);
        std::ofstream dst(temp, std::ios::binary | std::ios::trunc);
        if (!src || !dst)
            return false;

        const std::uint32_t magic = kMagic;
        const std::int32_t length = static_cast<std::int32_t>(m_xml.size() + 1);
        dst.write(reinterpret_cast<const char*>(&magic), 4);
        dst.write(reinterpret_cast<const char*>(&length), 4);
        dst.write(m_xml.data(), static_cast<std::streamsize>(m_xml.size()));
        dst.put('\0');

        src.seekg(static_cast<std::streamoff>(m_payloadOffset));
        std::vector<char> buffer(1 << 20);
        while (src) {
            src.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            dst.write(buffer.data(), src.gcount());
        }
        if (!dst)
            return false;
    }

    std::error_code ec;
    std::filesystem::rename(temp, m_path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        return false;
    }
    return true;
}
```

`std::filesystem::rename` на Windows заменяет существующий файл, что здесь и требуется.

- [x] **Step 4: Прогнать тесты**

Run: `ctest --test-dir cpp/build -R write --output-on-failure`
Expected: PASS, пять тестов.

- [x] **Step 5: Написать проверку на настоящем сохранении**

`cpp/tests/RealSave.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <fstream>
#include "whs/Description.h"

// Points at a COPY of a real .whs. Скопируйте один сейв во временный каталог и
// задайте переменную окружения; без неё тест пропускается.
TEST_CASE("A real savegame survives a rename and a reset", "[real]")
{
    const char* env = std::getenv("KCD2_TEST_SAVE");
    if (!env) {
        SKIP("KCD2_TEST_SAVE is not set");
    }
    const std::filesystem::path path{env};
    const auto original = std::filesystem::file_size(path);

    auto d = whs::Description::Read(path);
    REQUIRE(d.has_value());
    const std::string before = d->DisplayName();

    d->SetDisplayName("Renamer smoke test");
    REQUIRE(d->Write());

    auto renamed = whs::Description::Read(path);
    REQUIRE(renamed.has_value());
    CHECK(renamed->DisplayName() == "Renamer smoke test");

    renamed->ResetName();
    REQUIRE(renamed->Write());

    const auto restored = whs::Description::Read(path);
    REQUIRE(restored.has_value());
    CHECK(restored->DisplayName() == before);
    CHECK(std::filesystem::file_size(path) == original);
}
```

- [x] **Step 6: Прогнать сверку с эталонным прототипом**

Скопировать реальный сейв во временный каталог, задать `KCD2_TEST_SAVE` и прогнать. Затем той же операцией через `prototypes/whs_header.py` получить второй файл и сравнить результаты побайтово:

```bash
python -c "import sys; sys.path.insert(0,'prototypes'); from whs_header import Header,F_QUEST,F_OBJECTIVE; h=Header(sys.argv[1]); f=h.ui_fields(); f[F_QUEST]='Renamer smoke test'; f[F_OBJECTIVE]=''; h.set_ui_fields(f); h.write(sys.argv[2])" "$KCD2_TEST_SAVE" /tmp/python_out.whs
```

Ожидается: заголовки совпадают по полю `UIDescription`, payload у обоих идентичен исходному. Прототип не пишет `RenamerOriginal`, поэтому сравнивать надо поле, а не весь файл.

- [x] **Step 7: Коммит**

```bash
git add cpp && git commit -m "feat(whs): write the header back atomically"
```

---

## Task 6: Каталог сохранений и консольная проверка

**Files:**
- Create: `cpp/src/game/SaveCatalog.h`, `cpp/src/game/SaveCatalog.cpp`
- Modify: `cpp/src/plugin.cpp`
- Reference: `_deps/libKCD2/include/framework/C_SaveGameManager.h`, `C_SaveGameDescription.h`, `_deps/libKCD2/include/Offsets/Offsets.h`

**Interfaces:**
- Consumes: `whs::Description` из Task 5
- Produces:
  - `struct SaveEntry { int id; std::string displayName; std::filesystem::path file; }`
  - `std::vector<SaveEntry> SaveCatalog::List()`
  - `std::optional<SaveEntry> SaveCatalog::Find(int saveId)`

Это первая задача, которая трогает игру, поэтому автоматических тестов у неё нет: проверка идёт консольной командой в живой игре.

- [x] **Step 1: Найти путь к менеджеру**

Прочитать `C_SaveGameManager.h` и выписать: как добраться до экземпляра (заголовок прямо говорит, что это не синглтон, а поле `C_PlayerProfileWHManager` по смещению `+0x48`, и что корневой адрес профиль-менеджера автором не разрешён), какие поля дают списки, какой адрес у `UpdateSaveGameDescriptions`.

Если корневой адрес действительно не разрешён, взять его из Address Library или найти сканированием сигнатуры по вызову `CreateSaveGame`. Записать найденный способ комментарием в `SaveCatalog.cpp`: это самое хрупкое место всего мода.

- [x] **Step 2: Реализовать перечисление**

`cpp/src/game/SaveCatalog.h`:

```cpp
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

/// One savegame as the load list shows it.
struct SaveEntry {
    int id = 0;
    std::string displayName;
    std::filesystem::path file;
};

/// Reads the game's savegame list and asks it to rebuild after a file changed.
class SaveCatalog {
public:
    /// Returns every savegame known to the game, newest first.
    std::vector<SaveEntry> List() const;

    /// Returns the savegame with `saveId`, or nothing when it is gone.
    ///
    /// @param saveId Id as shown in the load list.
    /// @return The matching entry.
    std::optional<SaveEntry> Find(int saveId) const;

    /// Makes the game re-read every .whs and rebuild its per-type lists.
    void Refresh() const;
};
```

`SaveCatalog::List` идёт по `m_slotsByType`, для каждого описания берёт `m_fileName`, достраивает полный путь к каталогу сохранений и читает отображаемое имя через `whs::Description::Read`. Имя берётся из файла, а не из описания в памяти: так каталог и модель не разъезжаются.

- [x] **Step 3: Зарегистрировать временную консольную команду**

В `cpp/src/plugin.cpp` добавить команду `renamer_list`, печатающую `id`, имя и путь для каждой записи. Команда временная, служит проверкой этой задачи и удаляется в Task 12.

- [x] **Step 4: Проверить в игре**

Запустить игру, открыть консоль, выполнить `renamer_list`.

Ожидается: перечислены те же сохранения и с теми же именами, что видны в меню загрузки. Расхождение по количеству означает, что перебор слотов неполон.

- [x] **Step 5: Коммит**

```bash
git add cpp && git commit -m "feat(game): enumerate savegames through C_SaveGameManager"
```

---

## Task 7: Переименование с обновлением списка на месте

**Files:**
- Modify: `cpp/src/game/SaveCatalog.cpp`, `cpp/src/plugin.cpp`

**Interfaces:**
- Consumes: `SaveCatalog::Find`, `whs::Description::Write`
- Produces: `void SaveCatalog::Refresh() const`, консольная команда `renamer_set <id> <name>`

Задача даёт первый пригодный к использованию результат: мод уже делает то же, что 3488, но обновляет список без выхода из меню.

- [x] **Step 1: Реализовать обновление**

`SaveCatalog::Refresh` вызывает `UpdateSaveGameDescriptions` по найденному в Task 6 адресу.

- [x] **Step 2: Зарегистрировать временную консольную команду**

`renamer_set <id> <name...>` собирает остаток аргументов в имя, находит запись через `Find`, читает `whs::Description`, вызывает `SetDisplayName`, `Write`, затем `Refresh`.

- [x] **Step 3: Проверить в игре**

Не выходя из меню загрузки, выполнить `renamer_set <id> Перед боем с Истваном`.

Ожидается: список перерисовался прямо на экране, имя изменилось, выход на экран плейлайнов не потребовался. Это ключевое отличие от мода 3488.

- [x] **Step 4: Проверить сброс**

Выполнить `renamer_set <id>` без имени.

Ожидается: вернулось исходное квестовое имя вместе с хвостом задания.

- [x] **Step 5: Проверить, что сохранение грузится**

Загрузить переименованное сохранение.

Ожидается: загружается нормально.

- [x] **Step 6: Коммит**

```bash
git add cpp && git commit -m "feat(game): rename a save and refresh the list in place"
```

---

## Task 8: Элемент интерфейса, пустой диалог

**Files:**
- Create: `flash/renamer.as`, `flash/renamer.xml`, `cpp/src/game/RenameDialog.h`, `cpp/src/game/RenameDialog.cpp`
- Modify: `cpp/src/plugin.cpp`
- Reference: `_deps/libKCD2/Projects/MCM/flash/MCM.xml`, `mcm.as`, `_deps/libKCD2/Projects/MCM/src/mcm.h`

**Interfaces:**
- Consumes: ничего из предыдущих задач
- Produces: `void RenameDialog::Show(const std::string& currentName)`, `void RenameDialog::Hide()`

- [x] **Step 1: Написать описание элемента**

`flash/renamer.xml`:

```xml
<UIElements name="Menus">
  <UIElement name="SavegameRenamer" mouseevents="1" cursor="1" keyevents="1" controller_input="1" events_exclusive="1">
    <GFx file="renamer.gfx" layer="47">
      <Constraints>
        <Align mode="fullscreen" scale="1" />
      </Constraints>
    </GFx>
    <functions>
      <function name="Open" funcname="fc_open" desc="shows the dialog with the current name prefilled">
        <param name="currentName" desc="name shown in the load list right now" type="string" />
        <param name="canReset" desc="1 when the save carries a stashed original" type="bool" />
      </function>
      <function name="Close" funcname="fc_close" desc="hides the dialog without emitting an event" />
      <function name="SetInput" funcname="fc_setInput" desc="key feed from the plugin's input listener">
        <param name="action" desc="accept | cancel" type="string" />
      </function>
    </functions>
    <events>
      <event name="OnAccept" fscommand="onRenameAccept" desc="player confirmed a name">
        <param name="name" desc="text as typed" type="string" />
      </event>
      <event name="OnCancel" fscommand="onRenameCancel" desc="player dismissed the dialog" />
      <event name="OnReset" fscommand="onRenameReset" desc="player asked for the original name back" />
    </events>
  </UIElement>
</UIElements>
```

Слой 47 берётся на единицу выше слоя MCM, чтобы диалоги не спорили, если оба мода стоят.

- [x] **Step 2: Собрать пустой .gfx**

Взять `_deps/libKCD2/Projects/MCM/flash/mcm.gfx` как заготовку, в JPEXS FFDec заменить кадровый скрипт на минимальный `renamer.as`:

```actionscript
// Savegame Renamer dialog -- frame 1 DoAction (AS2 / Flash 8 / Scaleform GFx).
// Written for FFDec's AS2 parser: one declaration per var, no object literals,
// no ternary, no chained assignments.

var box = _root.createEmptyMovieClip("box", 1);
box._visible = false;
box.beginFill(0x000000, 80);
box.moveTo(150, 150);
box.lineTo(650, 150);
box.lineTo(650, 300);
box.lineTo(150, 300);
box.endFill();

function fc_open(currentName, canReset) {
    box._visible = true;
}

function fc_close() {
    box._visible = false;
}

function fc_setInput(action) {
}

stop();
```

Сохранить как `flash/renamer.gfx`, положить рядом `gfxfontlib.gfx` и `gfxfontlib_glyphs.gfx` из MCM.

- [x] **Step 3: Разложить ресурсы в pak-дерево**

Описание элемента кладётся в `src/Data/Libs/UI/UIElements/SavegameRenamer.xml`, `renamer.gfx` и шрифтовые библиотеки — в `src/Data/Libs/UI/`. Путь взят из шапки `Projects/MCM/src/plugin.cpp`: «Libs/UI/UIElements/MCM.xml + mcm.gfx, shipped in Mods/MCM/Data/MCM.pak».

- [x] **Step 4: Реализовать владение элементом**

`RenameDialog` находит элемент по имени `SavegameRenamer`, вызывает `Open`/`Close` и подписывается на три события. Пока обработчики только пишут в лог.

- [x] **Step 5: Проверить в игре**

Временная консольная команда `renamer_dialog` вызывает `Show("test")`.

Ожидается: поверх меню появился полупрозрачный прямоугольник. `renamer_dialog` повторно скрывает его. В логе видны события при нажатиях, если они уже проброшены.

Если прямоугольника нет, проблема в регистрации элемента, а не в скрипте: проверить, что XML попал в pak по правильному пути и что имя элемента совпадает.

- [x] **Step 6: Коммит**

```bash
git add flash cpp src && git commit -m "feat(ui): register the rename dialog element"
```

---

## Task 9: Поле ввода и события

**Files:**
- Modify: `flash/renamer.as`, `cpp/src/game/RenameDialog.cpp`
- Create: `cpp/src/game/InputForwarder.h`, `cpp/src/game/InputForwarder.cpp`

**Interfaces:**
- Consumes: `RenameDialog::Show` из Task 8
- Produces:
  - `RenameDialog::SetAcceptHandler(std::function<void(const std::string&)>)`
  - `RenameDialog::SetCancelHandler(std::function<void()>)`
  - `RenameDialog::SetResetHandler(std::function<void()>)`

- [x] **Step 1: Добавить поле ввода в ActionScript**

Дописать в `flash/renamer.as`, соблюдая ограничения парсера FFDec:

```actionscript
var resetBtn = box.createTextField("resetBtn", 4, 170, 265, 200, 24);
resetBtn.selectable = false;
resetBtn.text = "[R] Reset to original";
resetBtn._visible = false;

var input = box.createTextField("input", 2, 170, 190, 460, 40);
input.type = "input";
input.selectable = true;
input.maxChars = 120;
input.border = false;

var counter = box.createTextField("counter", 3, 170, 240, 460, 24);
counter.selectable = false;

var SOFT_LIMIT = 40;

function updateCounter() {
    counter.text = input.text.length + " / " + SOFT_LIMIT;
}

input.onChanged = function () {
    updateCounter();
};

function fc_open(currentName, canReset) {
    box._visible = true;
    input.text = currentName;
    Selection.setFocus(input);
    Selection.setSelection(input.text.length, input.text.length);
    updateCounter();
    resetBtn._visible = canReset;
}

function fc_setInput(action) {
    if (action == "accept") {
        Selection.setFocus(null);
        box._visible = false;
        fscommand("onRenameAccept", input.text);
    } else if (action == "cancel") {
        Selection.setFocus(null);
        box._visible = false;
        fscommand("onRenameCancel", "");
    }
}
```

Enter и Esc приходят не как char-события, поэтому обрабатываются через `fc_setInput`, а не внутри movie.

- [x] **Step 2: Реализовать форвардинг клавиш**

`InputForwarder` реализует `IInputEventListener`, и пока диалог открыт, перехватывает Enter и Esc, отправляя `SetInput("accept")` и `SetInput("cancel")`, а остальные клавиши пропускает дальше. Пока диалог открыт, F2 и навигация по списку не должны срабатывать.

- [x] **Step 3: Связать события с обработчиками**

`RenameDialog` разбирает `fscommand` и вызывает установленные обработчики.

- [x] **Step 4: Проверить в игре**

Открыть диалог командой `renamer_dialog`, набрать текст, в том числе кириллицу, нажать Enter.

Ожидается: символы появляются в поле, счётчик считает, Enter закрывает диалог и печатает в лог набранный текст, Esc закрывает и печатает отмену. Меню под диалогом на эти нажатия не реагирует.

- [x] **Step 5: Коммит**

```bash
git add flash cpp && git commit -m "feat(ui): accept typed names in the dialog"
```

---

## Task 10: Хук меню и полная связка

**Files:**
- Create: `cpp/src/game/SaveLoadHook.h`, `cpp/src/game/SaveLoadHook.cpp`
- Modify: `cpp/src/plugin.cpp`
- Reference: `_deps/libKCD2/include/guimodule/C_UISaveLoad.h`

**Interfaces:**
- Consumes: `SaveCatalog`, `RenameDialog`, `whs::Description`
- Produces: рабочий мод целиком

Здесь лежит главная неизвестная всего проекта: индекс выделенной строки.

- [x] **Step 1: Выяснить, как получить выделенную строку**

Заголовок `C_UISaveLoad.h` называет входящие диспетчеры `OnLoadButton` и `OnDeleteLoadButton`, они дают индекс нажатой строки. Нужна выделенная.

Проверить по порядку и остановиться на первом сработавшем:

1. Повторить приём MCM: он ставит MinHook на построитель страницы меню, дописывает кнопку сырым flash-вызовом `AddBasicButton` и **слушает события кнопок на ванильном элементе `Menu`** через `IUIElementEventListener` (`Projects/MCM/src/listener/MenuElementListener.h`, шапка `plugin.cpp`). Тем же слушателем ловится и событие выделения строки, если оно есть.
2. Хук на `BuildLoadGamePage`: запомнить порядок, в котором строки уходят во flash, и сопоставить его с индексом, приходящим от movie при наведении.
3. Чтение переменной курсора из ванильного movie `Menu` через `IUIElement::GetVariable`.
4. Если ничего не выходит, деградировать до пункта, который открывает диалог по `OnLoadButton` с подтверждением «переименовать или загрузить».

Записать выбранный способ комментарием в `SaveLoadHook.cpp`.

- [x] **Step 2: Повесить F2**

`InputForwarder` при открытой странице списка и закрытом диалоге ловит F2, спрашивает у хука выделенный `SaveEntry` и открывает диалог.

- [x] **Step 3: Связать результат**

Обработчик принятия: `SanitiseName`, затем `whs::Description::Read`, `SetDisplayName`, `Write`, `SaveCatalog::Refresh`. Обработчик сброса: `ResetName`, `Write`, `Refresh`. Обработчик отмены не делает ничего.

При отказе `Write` показать сообщение об ошибке и не трогать список.

- [x] **Step 4: Проверить сценарии в игре**

Пройти по каждому:

1. Открыть меню загрузки, выделить сохранение, F2, ввести имя, Enter. Ожидается: имя в списке поменялось сразу.
2. F2, Esc. Ожидается: ничего не изменилось.
3. F2, стереть всё, Enter. Ожидается: вернулось исходное квестовое имя с хвостом задания.
4. F2, нажать кнопку сброса. Ожидается: то же самое.
5. Ввести имя из 80 символов. Ожидается: счётчик подсвечен, имя записано, строка списка прокручивается.
6. Ввести `Rock & "roll" |cut|`. Ожидается: в списке `Rock & "roll" cut`, сохранение не пропало.
7. Загрузить переименованное сохранение. Ожидается: грузится.
8. Переименовать дважды подряд, затем сбросить. Ожидается: вернулось исходное квестовое имя, а не первое пользовательское.

- [x] **Step 5: Коммит**

```bash
git add cpp && git commit -m "feat: rename the selected save with F2 from the load menu"
```

---

## Task 11: Оформление и подсказка

**Files:**
- Modify: `flash/renamer.as`
- Create: `src/Localization/English_xml/text_ui__savegame_renamer.xml`

**Interfaces:**
- Consumes: рабочий диалог из Task 10
- Produces: диалог в стиле ванильного модального окна и подсказка в меню

- [x] **Step 1: Снять эталон**

Открыть в игре ванильное подтверждение удаления сохранения и сделать скриншот. Из него берутся: цвет фона и рамки, шрифт и кегль заголовка, отступы, вид кнопок, положение окна.

Соответствующие текстуры лежат в `Data/IPL_GameData.pak` по пути `Libs/UI/Textures/Apse/`, файл `modal_dialog_*.dds`.

- [x] **Step 2: Подогнать вёрстку**

Переписать построение `box` в `renamer.as` под снятые размеры. Способ подгонки тот же, что у MCM: итерации по скриншотам, константы абсолютные, вынесены в начало файла.

- [x] **Step 3: Добавить строки локализации**

Заголовок диалога, подписи кнопок и подсказка `F2 Переименовать` кладутся в таблицу локализации мода по образцу `better_arm_of_beowulf`. Английский обязателен, остальные языки по желанию.

- [x] **Step 4: Проверить в игре**

Ожидается: диалог визуально неотличим от ванильного модального окна, подсказка про F2 видна на странице списка, кириллица и латиница рендерятся одинаково.

- [x] **Step 5: Коммит**

```bash
git add flash src && git commit -m "style(ui): match the vanilla modal dialog"
```

---

## Task 12: Упаковка и релиз

**Files:**
- Create: `tools/build.py`, `docs/nexus-description.bbcode.txt`, `docs/nexus-readme.txt`
- Modify: `cpp/src/plugin.cpp` (удалить временные консольные команды)

**Interfaces:**
- Consumes: всё предыдущее
- Produces: `releases/savegame_renamer-1.0.zip`

- [x] **Step 1: Убрать временные команды**

Удалить `renamer_list`, `renamer_set` и `renamer_dialog`. Они были инструментом проверки задач 6, 7 и 8, в релизе им не место: мод заявлен как GUI-вариант.

- [x] **Step 2: Написать упаковщик**

Взять `better_arm_of_beowulf/tools/build.py` за основу, сохранив его требование к zip без extra field в центральном каталоге, иначе движок не читает pak. Добавить копирование собранной DLL в `src/KCSE/Plugins/`.

- [x] **Step 3: Собрать релиз**

```bash
python tools/build.py --release
```

Ожидается: `releases/savegame_renamer-1.0.zip` со структурой `savegame_renamer/mod.manifest`, `savegame_renamer/Data/*.pak`, `savegame_renamer/KCSE/Plugins/savegame_renamer.dll`.

- [x] **Step 4: Проверить установку с нуля**

Удалить рабочую копию мода из папки игры, распаковать релизный архив, запустить игру, пройти сценарий 1 из Task 10.

Ожидается: работает на чистой установке.

- [x] **Step 5: Написать описание для Nexus**

В `docs/nexus-description.bbcode.txt`: что делает мод, требование KCSE и Address Library, установка, клавиша F2, отличие от `Rename Your Savegame` (интерфейс вместо консоли и обновление списка на месте), совместимость, лицензия GPLv3 со ссылкой на исходники.

- [x] **Step 6: Коммит**

```bash
git add -A && git commit -m "build: package the release archive"
git tag v1.0
```

---

## Проверка плана по спецификации

| Требование спецификации | Задача |
|---|---|
| Формат `.whs`, префикс и NUL | 2, 5 |
| Имя из поля 2 `UIDescription`, очистка поля 3 | 4 |
| `QuestNameOverride` и `m_saveName` не трогаются | 4 (не упоминаются нигде в коде) |
| `RenamerOriginal` только атрибутом, только при первом переименовании | 4 |
| Сброс к оригиналу | 4, 10 |
| Атомарная запись, payload неизменен | 5 |
| Сверка `SaveId` перед записью | 5 |
| Очистка `\|`, экранирование XML | 3, 4 |
| Пустой ввод означает сброс | 4 |
| Мягкий лимит 40 символов | 3, 9 |
| Каталог через `C_SaveGameManager` | 6 |
| Обновление списка на месте | 7 |
| Собственный элемент, ванильные файлы не подменяются | 8 |
| Ввод символов, Enter, Esc | 9 |
| F2, выделенная строка | 10 |
| Стиль ванильного модального окна | 11 |
| Раскладка проекта, упаковка | 1, 12 |
| Сон и квестовый автосейв не трогаются | нигде: мод не имеет хуков на пути сохранения |
