# UI Audio Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Озвучить диалог переименования штатными звуками игры: открытие, наведение, подтверждение, отмена, сброс и отказ.

**Architecture:** Движок регистрирует сквозной приёмник `C_UICommonEventHandler` на каждом flash-элементе и играет глобальный аудиотриггер по имени, когда элемент шлёт объявленное событие `onPlayAudio`. Мод объявляет это событие в своём `UIElements/SavegameRenamer.xml` и шлёт его из `renamer.as` одной обёрткой над `fscommand`. Своих звуковых файлов мод не несёт.

**Tech Stack:** ActionScript 2 (Flash 8, Scaleform GFx), JPEXS FFDec, Python 3 для упаковки, KCSE + libKCD2.

Спецификация: [2026-08-19-ui-audio-design.md](2026-08-19-ui-audio-design.md).

## Global Constraints

- Целевая версия игры `1.5.6-15693-release_1_5`, `WHGame.dll 1.5.6 (kd7u)`.
- Ванильные файлы игры не подменяются ни при каких условиях.
- Своих звуковых файлов мод не несёт и ни одного игрового не подменяет: берутся только имена триггеров.
- Берётся только семейство `MENU_*`. `APSE_*`, `QAM_*` и `PICKPOCKET_*` не используются, кроме разбора отказа в задаче 1.
- Объявляется только событие `OnPlayAudio`. `OnStopAudio` и `OnSetVolumeAudio` не объявляются.
- Элемент `SavegameRenamerHint` не трогается ни в XML, ни по точкам вызова.
- C++ не трогается. Локализация не трогается, новых строк мод не заводит.
- ActionScript пишется под парсер FFDec: одно объявление на `var`, без объектных литералов, без тернарного оператора, без цепочек присваивания.
- Разделители секций в `renamer.as` выровнены по 78-й колонке.
- Корень проекта: `D:\Games\Self-Mods\KCD2\savegame_renamer`. Все пути ниже относительны ему.
- Сборка требует FFDec и закрытой игры. Пути берутся из `build.env`.

### Об автоматических тестах

Их здесь нет и быть не может: Catch2-набор покрывает модель `.whs` и не знает ни про flash, ни про движок. Вместо них в каждой задаче стоит красно-зелёный цикл по собранному артефакту (строка обязана появиться в `renamer.gfx` и в паке) плюс проверка на слух в игре. Отказ парсера FFDec на некорректном ActionScript валит сборку, это и есть проверка компиляции.

---

## File Structure

| Файл | Что меняется |
|---|---|
| `src/Data/Libs/UI/UIElements/SavegameRenamer.xml` | объявление события `OnPlayAudio` в элементе `SavegameRenamer` |
| `flash/renamer.as` | секция констант, обёртка `playAudio`, шесть точек вызова |
| `docs/nexus/description.bbcode.txt` | строка о звуке в списке возможностей (задача 3, вне спецификации) |

Пересобираемые артефакты, которые попадут в коммит: `src/Data/Libs/UI/renamer.gfx` и `src/Data/savegame_renamer.pak`.

---

## Task 1: Канал звука и открытие диалога

Самая рискованная часть: имена `ui_menu_*` в игре из flash не вызываются ни разу, их играет нативный код. Задача поднимает канал целиком на одной точке вызова, чтобы отказ вскрылся до того, как будут расставлены остальные пять.

**Files:**
- Modify: `src/Data/Libs/UI/UIElements/SavegameRenamer.xml:47-49`
- Modify: `flash/renamer.as:120-122`, `flash/renamer.as:478-487`

**Interfaces:**
- Consumes: ничего
- Produces: в `flash/renamer.as` — `function playAudio(trigger)` и константы `SND_OPEN`, `SND_CLOSE`, `SND_FOCUS`, `SND_CHOICE`, `SND_DISABLE`, все со строковыми значениями имён триггеров. Задача 2 расставляет вызовы `playAudio` с этими константами и ничего нового не заводит.

- [x] **Step 1: Снять исходное состояние артефакта**

```bash
python -c "import zipfile; d=zipfile.ZipFile('src/Data/savegame_renamer.pak').read('Libs/UI/UIElements/SavegameRenamer.xml').decode('utf-8'); print('onPlayAudio' in d)"
```

Ожидается: `False`. Это красная фаза: событие ещё не объявлено.

- [x] **Step 2: Объявить событие в XML**

В `src/Data/Libs/UI/UIElements/SavegameRenamer.xml`, внутрь блока `<events>` элемента `SavegameRenamer`, после объявления `OnReset` и перед закрывающим `</events>`:

```xml
      <!-- Audio -->
      <event name="OnPlayAudio" fscommand="onPlayAudio" desc="execute global audio trigger">
        <param name="TriggerName" desc="" type="string" />
      </event>
```

Отступы 6 и 8 пробелов, как у соседних объявлений. Элемент `SavegameRenamerHint` ниже по файлу не трогается.

- [x] **Step 3: Проверить, что XML остался разборным**

```bash
python -c "import xml.etree.ElementTree as ET; r=ET.parse('src/Data/Libs/UI/UIElements/SavegameRenamer.xml').getroot(); print([e.get('fscommand') for e in r.iter('event')])"
```

Ожидается: `['onRenameAccept', 'onRenameCancel', 'onRenameReset', 'onPlayAudio']`.

- [x] **Step 4: Завести секцию звуков в ActionScript**

В `flash/renamer.as`, между `var Y_KEYS = ...` и разделителем `// ---- dialog box --`, отдельной секцией:

```as
// -------------------------------------------------------------- ui sounds --

// The game's own interface triggers, taken from the Audio class every vanilla
// movie carries. The engine plays one for any element that declares the
// onPlayAudio event: C_UICommonEventHandler is attached to every flash element
// and is what receives it. The menu family is the one this dialog belongs to,
// being drawn over the load list; the panel screens sound their own way.
var SND_OPEN = "ui_menu_open";
var SND_CLOSE = "ui_menu_close";
var SND_FOCUS = "ui_menu_change_focus";
var SND_CHOICE = "ui_menu_change_choice";
var SND_DISABLE = "ui_menu_disable";

// Plays one of the game's global audio triggers.
//
// @param trigger Name of the trigger.
function playAudio(trigger) {
    fscommand("onPlayAudio", trigger);
}
```

Разделитель ровно 78 символов, как остальные в файле.

- [x] **Step 5: Озвучить открытие диалога**

В `flash/renamer.as` дописать последней строкой тела `fc_open`:

```as
function fc_open(currentName, canReset) {
    box._visible = true;
    openedWith = currentName;
    input.text = currentName;
    input.setTextFormat(inputFmt);
    layoutKeys(canReset);
    updateCounter();
    Selection.setFocus(input);
    Selection.setSelection(input.text.length, input.text.length);
    playAudio(SND_OPEN);
}
```

Звук идёт последним, когда окно уже поднято и фокус выставлен.

- [x] **Step 6: Собрать**

```bash
python tools\build.py
```

Ожидается: `built 16 localization paks`, затем `packed src\Data\savegame_renamer.pak` со списком файлов и кодом возврата 0. Отказ FFDec на разборе `renamer.as` валит сборку здесь же.

- [x] **Step 7: Проверить артефакт, зелёная фаза**

```bash
python -c "import zipfile,zlib; z=zipfile.ZipFile('src/Data/savegame_renamer.pak'); print('onPlayAudio' in z.read('Libs/UI/UIElements/SavegameRenamer.xml').decode('utf-8')); d=zlib.decompress(z.read('Libs/UI/renamer.gfx')[8:]); print(b'onPlayAudio' in d, b'ui_menu_open' in d)"
```

Ожидается: `True`, затем `True True`. Первое — событие в объявлении элемента, второе — имя команды и имя триггера в скомпилированном ActionScript.

- [x] **Step 8: Проверить на слух**

```bash
python tools\build.py --deploy
```

Запустить игру, открыть список сохранений, встать на любую строку, нажать F2. Окно должно подняться со звуком открытия меню, тем же, что звучит при входе в меню игры.

- [x] **Step 9: Если тихо, разделить два отказа**

Отказывать могут имя триггера или канал доставки. Пробовать по одному, возвращая правку перед следующей пробой.

1. **Имя.** Временно заменить значение `SND_OPEN` на `"ui_apse_expand"`, пересобрать с `--deploy`, нажать F2. Зазвучало — канал рабочий, а `ui_menu_*` из flash не резолвится: все пять констант переезжают на семейство `APSE_*`, у которого вызов из flash доказан (`ApseInventoryList.gfx` играет `ui_apse_select` при выборе предмета).

   | Константа | Замена |
   |---|---|
   | `SND_OPEN` | `ui_apse_expand` |
   | `SND_CLOSE` | `ui_apse_collapse` |
   | `SND_FOCUS` | `ui_apse_change_focus` |
   | `SND_CHOICE` | `ui_apse_select` |
   | `SND_DISABLE` | `ui_apse_map_zoom_deny` |

   Это первое приближение по смыслу констант, а не по звучанию: пары подбираются на слух в шаге 8 и в проверке задачи 2.

2. **Канал.** Если и с `ui_apse_expand` тихо, вернуть `SND_OPEN` и заменить тело обёртки на ванильный путь:

```as
function playAudio(trigger) {
    flash.external.ExternalInterface.call("onPlayAudio", trigger);
}
```

   Зазвучало — дело было в канале, обёртка остаётся в этом виде, а имена не трогаются.

- [x] **Step 10: Коммит**

```bash
git add src/Data/Libs/UI/UIElements/SavegameRenamer.xml flash/renamer.as src/Data/Libs/UI/renamer.gfx src/Data/savegame_renamer.pak
git commit -m "feat(ui): sound the rename dialog opening with the game's own trigger"
```

---

## Task 2: Наведение, подтверждение, отмена, сброс и отказ

**Files:**
- Modify: `flash/renamer.as:299-302`, `flash/renamer.as:540-550`

**Interfaces:**
- Consumes: `playAudio(trigger)`, `SND_FOCUS`, `SND_CHOICE`, `SND_CLOSE`, `SND_DISABLE` из задачи 1
- Produces: ничего, задача последняя в коде

- [x] **Step 1: Озвучить наведение**

В `flash/renamer.as`, в теле `mkKeyHint`, первой строкой обработчика:

```as
    clip.onRollOver = function () {
        playAudio(SND_FOCUS);
        this.labelText.styleFmt.color = COLOR_HOVER;
        setText(this.labelText, this.labelValue);
    };
```

`onRollOut` не трогается: уход из строки в меню не звучит.

Подсказка F2 через это не зазвучит: `mkKeyHint` строит и её, но сразу после постройки `hint.onRollOver` присваивается `null`, а её элемент вдобавок объявлен с `mouseevents="0"`.

- [x] **Step 2: Озвучить три исхода и отказ**

Заменить тело `fc_setInput` целиком:

```as
// Acts on a key the engine delivers to the plugin rather than to the movie.
function fc_setInput(action) {
    if (action == "accept") {
        playAudio(SND_CHOICE);
        closeWith("onRenameAccept", stripBars(input.text));
    } else if (action == "cancel") {
        playAudio(SND_CLOSE);
        closeWith("onRenameCancel", "");
    } else if (action == "reset") {
        if (keyReset._visible) {
            playAudio(SND_CHOICE);
            closeWith("onRenameReset", "");
        } else {
            playAudio(SND_DISABLE);
        }
    }
}
```

Ветка `else` новая: сейчас `reset` при скрытой строке сброса не делает ничего и молчит.

Клик мышью по строке подсказки приходит сюда же, `clip.onRelease` зовёт `fc_setInput(this.action)`. Отдельных точек вызова для мыши не заводить, иначе клавиша и клик разойдутся.

- [x] **Step 3: Собрать**

```bash
python tools\build.py
```

Ожидается: код возврата 0 и `packed src\Data\savegame_renamer.pak`.

- [x] **Step 4: Проверить, что все пять триггеров попали в артефакт**

```bash
python -c "import zipfile,zlib; d=zlib.decompress(zipfile.ZipFile('src/Data/savegame_renamer.pak').read('Libs/UI/renamer.gfx')[8:]); print([t.decode() for t in [b'ui_menu_open',b'ui_menu_close',b'ui_menu_change_focus',b'ui_menu_change_choice',b'ui_menu_disable'] if t in d])"
```

Ожидается список из всех пяти имён. Если задача 1 закончилась на семействе `APSE_*`, подставить его имена.

- [x] **Step 5: Проверить на слух**

```bash
python tools\build.py --deploy
```

Пройти по списку целиком:

1. Открыть список сохранений. Подсказка F2 появляется молча.
2. F2: звук открытия меню.
3. Провести мышью по строкам Enter, Esc, Del. Каждый вход в строку щёлкает, уход молчит, повторный вход щёлкает снова.
4. Esc: звук закрытия. Снова F2, затем Enter: звук подтверждения. Два звука различимы между собой.
5. Del на сохранении с припрятанным оригинальным именем: звук подтверждения, окно закрывается, имя вернулось.
6. Del на сохранении без него: отказной звук, окно остаётся открытым, строка сброса по-прежнему скрыта.
7. Нажать Enter и Esc с клавиатуры и кликнуть те же строки мышью: звук одинаковый обоими способами.

- [x] **Step 6: Коммит**

```bash
git add flash/renamer.as src/Data/Libs/UI/renamer.gfx src/Data/savegame_renamer.pak
git commit -m "feat(ui): sound hover, accept, cancel and reset in the rename dialog"
```

---

## Task 3: Строка о звуке на странице мода

**Вне спецификации.** Задача добавлена потому, что список возможностей на Nexus перечисляет ровно то, что игрок замечает, и молчание про звук после этой работы станет неточностью. Если страницу решено не трогать, задача выбрасывается целиком, на код это не влияет.

**Files:**
- Modify: `docs/nexus/description.bbcode.txt:16`

**Interfaces:**
- Consumes: ничего
- Produces: ничего

- [x] **Step 1: Дописать строку в список возможностей**

В `docs/nexus/description.bbcode.txt`, в блоке `Main features`, сразу после строки про графику из ванильных меню:

```
[*]Sounds like the menu it sits in: hover, confirm, cancel and refusal all play the game's own interface triggers. No sound file ships with the mod[/*]
```

- [x] **Step 2: Проверить, что разметка списка не разъехалась**

```bash
python -c "import io; s=io.open('docs/nexus/description.bbcode.txt',encoding='utf-8').read(); print(s.count('[list'), s.count('[/list]'), s.count('[*]'), s.count('[/*]'))"
```

Ожидается: `4 4 17 17`. До правки было `4 4 16 16`. Считается `[list` без закрывающей скобки: один из списков в файле открыт как `[list=1]`.

- [x] **Step 3: Коммит**

```bash
git add docs/nexus/description.bbcode.txt
git commit -m "docs(nexus): list the interface sounds among the features"
```

---

## Открытые вопросы

Версия мода в `src/mod.manifest` остаётся `1.0`. Поднимать ли её до `1.1` и выпускать ли релиз — решение о публикации, а не часть этой работы.
