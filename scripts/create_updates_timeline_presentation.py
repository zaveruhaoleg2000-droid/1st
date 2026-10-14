"""Create the verified v022–v026 chronology as an offline HTML deck and PDF.

Run: python3 scripts/create_updates_timeline_presentation.py --out-dir /workspace
Dependencies: reportlab; DejaVu Sans fonts. Screenshots are repository assets.
"""
from pathlib import Path
import argparse
import base64
import html

from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas
from reportlab.lib.utils import ImageReader

ROOT = Path(__file__).resolve().parent.parent
REPO = "https://github.com/zaveruhaoleg2000-droid/1st"
NAME = "GRAY_ZONE_1989_UPDATES_TIMELINE_UA"
FONT_DIR = Path("/usr/share/fonts/truetype/dejavu")
BG, PANEL, GOLD, INK, MUTED = "#111e25", "#1c3039", "#efd69b", "#e6eded", "#a5b9c1"
TEAL = "#67cbbb"

MILESTONES = [
    ("v0.0.22 · вихідний архів", "11:21 UTC", "32ec255", "C++20, кампанія, тактика й CPU fallback."),
    ("v0.0.22 · надійність", "11:43 UTC", "721d42e", "Виправлення тестів і Windows-пакування."),
    ("v0.0.22 · GPU", "12:22 UTC", "afd02f8", "Кеш геометрії та часткові передачі."),
    ("v0.0.23 · 3D-графіка", "13:13 UTC", "3f3200f", "Матеріали, тіні, освітлення, моделі."),
    ("План UI / рельєфу", "13:27 UTC", "a0a4648", "Документований план; ще не реалізація."),
    ("v0.0.24 · світ і HUD", "14:38 UTC", "6e27201", "Seed, рельєф, макротекстура, GPU HUD."),
    ("v0.0.25 · бій від 1-ї особи", "15:32 UTC", "d144bba", "Реальний час, керування, здоров’я, броня."),
    ("v0.0.26 · спрайти / LOD", "16:27 UTC", "6723fe0", "Рослинність, ефекти, 4 LOD, більше ресурсів."),
]

def shot(version, name, label=None):
    return dict(path=f"docs/review/{version}/{name}.png", label=label or version.replace("v0", "v0.0."),
                provenance="Vulkan через Mesa Lavapipe · реальний кадр рушія")

SLIDES = [
    dict(kind="photo", title="GRAY ZONE: 1989", subtitle="Хронологія модернізації · v0.0.22 → v0.0.26", badge="14.10.2026", commit="6723fe0",
         images=[shot("v026", "review_first_person", "Поточний прототип · v0.0.26")],
         bullets=["Від вихідного архіву до стратегії з керуванням окремим бійцем.", "18 слайдів: етапи, скріншоти, порівняння та перевірені результати.", "Орієнтир для графіки: RTX 3050 Ti, 4 ГБ VRAM."],
         note="Хронологія спирається на Git-коміти та звіти перевірок. Кадри показують реальний вигляд прототипу."),
    dict(kind="timeline", title="Вісім зафіксованих етапів", subtitle="Усі наведені коміти створені 14.10.2026. Час у UTC; це час фіксації змін у Git.", badge="ПОСЛІДОВНІСТЬ", commit="6723fe0",
         note="Етап планування показано окремо: документ із пропозиціями не прирівнюється до реалізованої функції."),
    dict(kind="cards", title="Початкова точка: v0.0.22", subtitle="Імпорт наданого вихідного архіву · 11:21 UTC · 32ec255", badge="ВИХІДНИЙ ПРОЄКТ", commit="32ec255",
         cards=[("Рушій", "Власний C++20 / CMake. Програмний рендерер, Vulkan-шлях і резервний CPU-запуск."),
                ("Гра", "Карта Європи, тактичні ділянки 3×3 км, відділення, контракти та збереження."),
                ("Що вимагало перевірки", "Windows EXE та native Vulkan ще потребували відтворюваних збірок і перевірок запуску.")],
         note="Окремого підтвердженого скріншота вихідного v0.0.22 у цій презентації немає."),
    dict(kind="cards", title="v0.0.22: спочатку надійність", subtitle="Перший етап модернізації · 11:43 UTC · 721d42e", badge="ЗБІРКА І ПЕРЕВІРКИ", commit="721d42e",
         cards=[("Тести справді виконуються", "Assertions активні у Release. Виправлено передумови перевірок камери й видимості бійців."),
                ("Windows збирається", "Виправлено координати RECT і лінкування WinMM. Налаштовано Debug/Release CI."),
                ("Пакет можна перевірити", "CPU smoke перевіряє ресурси, меню, кадри тактики/карти й завершення бою. Пакувальник контролює assets.")],
         note="Цей етап створив основу для наступних графічних змін; CPU smoke сам по собі не підтверджує native Vulkan."),
    dict(kind="benchmark", title="v0.0.22: менше передач геометрії", subtitle="Оптимізація GPU-шляху · 12:22 UTC · afd02f8", badge="КЕШ І БУФЕРИ", commit="afd02f8",
         bullets=["Статична сцена кешується; повторний кадр використовує її геометрію.", "Vertex/index buffers — у device-local пам’яті. Передаються лише змінені діапазони.", "Виправлено синхронізацію swapchain; SPIR-V входить до Windows-пакета."],
         note="192 бійці, 120 кадрів, незмінна сцена. «До» — розрахунок повного завантаження щокадру. Це обсяг передач, а не FPS відеокарти."),
    dict(kind="photo", title="v0.0.23: текстури, світло і 3D", subtitle="Перший великий графічний етап · 13:13 UTC · 3f3200f", badge="ГРАФІКА", commit="3f3200f",
         images=[shot("v023", "soldiers", "Історичний кадр v0.0.23")],
         bullets=["16 матеріалів 512 px, mip-рівні та анізотропія до 8×.", "Тіні до 2048 px, PCF, MSAA до 4×, туман і освітлення.", "Об’ємні бійці, зброя, танк, БТР, вантажівка; деталі будинків і дерев.", "Вода, мокрі та засніжені поверхні; профілі High / Medium / Low."],
         note="Техніка декоративна. Форми процедурні та стилізовані. Історичний PNG збережено з опублікованої галереї v0.0.23."),
    dict(kind="cards", title="Між v0.0.23 і v0.0.24: план світу", subtitle="Документований план модернізації · 13:27 UTC · a0a4648", badge="ПЛАНУВАННЯ", commit="a0a4648",
         cards=[("Інтерфейс", "Native HUD, український текст, картки відділень, мінікарта й вибір об’єктів."),
                ("Генерація рельєфу", "Seed, регіони та версія генератора; узгоджені межі ділянок, кеш і фонова робота."),
                ("Покриття землі", "Одна велика макротекстура для далекого плану та детальні матеріали зблизька.")],
         note="План зафіксовано перед реалізацією v0.0.24. Зміни наступного слайда вже підтверджені кодом і перевірками."),
    dict(kind="photo", title="v0.0.24: генеративний світ і HUD", subtitle="Реалізація плану · 14:38 UTC · 6e27201", badge="РЕЛЬЄФ ТА UI", commit="6e27201",
         images=[shot("v024", "review_terrain", "Рельєф і native HUD · v0.0.24")],
         bullets=["Детермінований рельєф: seed, біом, регіон і версія генератора.", "Безперервні межі ділянок; фонова генерація й кеш.", "Макротекстура High 4096 px + детальні матеріали зблизька.", "GPU HUD, кнопки, картки, мінікарта й коректний вибір об’єктів."],
         note="Кампанія зберігається у форматі v3 з читанням v1/v2. Користувач підтвердив, що збірка працює й має зачатки геймплею."),
    dict(kind="compare", title="v0.0.24 → v0.0.25: інтерфейс", subtitle="Новий пропорційний шрифт і компонування командного HUD", badge="ДО / ПІСЛЯ", commit="d144bba",
         images=[shot("v024", "review_tactical", "v0.0.24 · попередній текст і нижні кнопки"), shot("v025", "review_tactical", "v0.0.25 · пропорційний шрифт, картки та цілі")],
         note="Історичні кадри з різними положеннями камери. Порівняння демонструє інтерфейс, а не швидкодію."),
    dict(kind="photo", title="v0.0.25: реальний час і перша особа", subtitle="Перехід від покрокового бою · 15:32 UTC · d144bba", badge="ГЕЙМПЛЕЙ", commit="d144bba",
         images=[shot("v025", "review_first_person", "Перша особа та окремий бойовий HUD · v0.0.25")],
         bullets=["Покрокову систему прибрано; залишено паузу.", "Вільна камера: обертання, нахил і масштаб 2–8000 м.", "Керування бійцем: огляд мишею, WASD, біг, присідання.", "Ручний вогонь, прицілювання, перезарядка; 5 типів зброї.", "Здоров’я 100, броня 60; влучання в тіло/голову, рельєф блокує постріли."],
         note="Командний і FPS-режим використовують одну симуляцію. Реальні події клавіатури й миші перевірено на Linux."),
    dict(kind="photo", title="v0.0.25: карта набуває об’єму", subtitle="Стилізація під вид із космосу; зміни карти кампанії", badge="СТРАТЕГІЧНИЙ ВИГЛЯД", commit="d144bba",
         images=[dict(path="docs/review/v026/review_global.png", label="Карта з пакета v0.0.26 · вигляд упроваджено у v0.0.25", provenance="CPU-рендерер · Windows EXE під Wine · реальний кадр")],
         bullets=["Затінення схилів, плавні кольори висот і сніг високогір’я.", "Тонкі політичні кордони й читабельні маркери.", "Наявна географічна сітка 257×153.", "Карта кампанії використовує CPU raster/upload path."],
         note="Це стилізована карта рушія, а не нові реальні супутникові знімки. Версія використаного скріншота вказана окремо."),
    dict(kind="photo", title="v0.0.26: рослинність і прозорість", subtitle="Останній графічний етап · 16:27 UTC · 6723fe0", badge="СПРАЙТИ", commit="6723fe0",
         images=[shot("v026", "review_foliage", "Трава та листяні крони · v0.0.26")],
         bullets=["RGBA SpriteBatch: прозорі краї, змішування й сортування за дальністю.", "Трава, крони, дальні дерева й хмари — спрайти.", "Ближні дерева зберігають об’ємні стовбури; GPU тіні враховують прозорість листя.", "16 RGBA шарів. CPU fallback також малює спрайти."],
         note="Рослинність обмежена дальністю й кількістю карток: до 4096 за кадр. Детальна трава не малюється по всій карті."),
    dict(kind="compare", title="v0.0.25 → v0.0.26: рослинність", subtitle="Трава й прозорі крони доповнюють рельєф", badge="ДО / ПІСЛЯ", commit="6723fe0",
         images=[shot("v025", "review_terrain", "v0.0.25 · матеріали й об’ємні крони"), shot("v026", "review_terrain", "v0.0.26 · трава та спрайтові крони")],
         note="Реальні кадри одного типу сцени. Положення об’єктів і час знімка можуть відрізнятися; це візуальне порівняння."),
    dict(kind="photo", title="v0.0.26: ефекти зброї", subtitle="Події стрільби створюють короткочасні прозорі ефекти", badge="БОЙОВІ ЕФЕКТИ", commit="6723fe0",
         images=[shot("v026", "review_weapon_effects", "Кадр ефектів пострілу · v0.0.26")],
         bullets=["Спалах біля ствола й дим.", "Короткий трасер, пил від влучання та іскри.", "Працюють від ручного й автоматичного вогню.", "Час ефектів прив’язаний до симуляції: пауза зупиняє їх.", "Каталог також містить резервні шари; окремі системи дощу й вогню ще не додані."],
         note="Не всі шари каталогу мають власний emitter. На слайді показано чинні ефекти пострілів із реального кадру."),
    dict(kind="photo", title="v0.0.26: чотири LOD і більше моделей", subtitle="Деталізація зменшується з віддаленням; каталоги ресурсів розширено", badge="ГЕОМЕТРІЯ", commit="6723fe0",
         images=[shot("v026", "review_vehicle", "Моделі техніки · v0.0.26")],
         bullets=["LOD 0: <40 м; LOD 1: 40–120 м.", "LOD 2: 120–320 м; LOD 3: ≥320 м.", "Пороги масштабуються для великих об’єктів; бійці мають гістерезис 12%.", "Каталог оточення: 16 → 32 моделі. Зброя також має LOD-варіанти.", "Нові об’єкти: намет, антена, джип, контейнер, вежа, бар’єри та уламки."],
         note="Чотири рівні застосовано до бійців, техніки та будинків. Водіння й повноцінна фізика машин ще не реалізовані."),
    dict(kind="table", title="Як зростали текстури та ресурси", subtitle="Роздільність і кількість матеріалів змінювалися з урахуванням бюджету", badge="БЮДЖЕТИ", commit="6723fe0",
         columns=["Версія", "Базові матеріали", "Macro High", "Нові ресурси"],
         rows=[["v0.0.23", "16 · 512 px", "—", "Об’ємна 3D-сцена"], ["v0.0.24", "16 · 512 px", "4096 px", "Генеративний рельєф + GPU HUD"], ["v0.0.25", "31 + normal maps", "2048 px", "5 типів зброї; 16 моделей оточення"], ["v0.0.26", "39 + normal maps", "2048 px", "16 RGBA шарів; 32 моделі оточення"]],
         bullets=["Резидентні текстури: 78,57 → 98,15 МБ / ліміт 100 МБ (v025 → v026).", "v026: каталог зброї 0,427 / 50 МБ; оточення 1,016 / 200 МБ.", "Macro зменшено з 4096 до 2048, щоб додати normal maps і матеріали в межах бюджету."],
         note="МБ десяткові. Текстурний payload не включає depth/MSAA/shadow targets, staging, геометрію та пам’ять драйвера."),
    dict(kind="table", title="Перевірки на кожному етапі", subtitle="Збережені результати зі звітів відповідних версій; повторний запуск тестів для презентації не потрібен", badge="ПІДТВЕРДЖЕННЯ", commit="6723fe0",
         columns=["Етап", "Linux CPU D/R", "Linux Vulkan", "Windows CPU/VK¹"],
         rows=[["v022 · GPU", "25/25", "26/26", "23/23"], ["v023", "25/25", "26/26", "23/23"], ["v024", "27/27", "28/28", "25/25"], ["v025", "28/28", "29/29", "26/26"], ["v026", "29/29", "30/30", "27/27"]],
         bullets=["Кожне число — кількість пройдених перевірок у відповідній конфігурації.", "¹ Windows EXE виконувалися під Wine; native Vulkan — через Mesa. Linux використовує Lavapipe.", "Фізичну RTX 3050 Ti / Windows 11 не перевірено. Windows XTest не знайшов вікно; цей input-тест не пройшов."],
         note="Кількість тестів зросла разом із функціями; вона не є вимірюванням FPS або гарантією відсутності помилок."),
    dict(kind="cards", title="Поточний результат і наступний крок", subtitle="Підсумок реалізації v0.0.26; майбутні задачі відокремлені від виконаних", badge="СТАН ПРОЄКТУ", commit="6723fe0",
         cards=[("Уже є", "Реальний час із паузою, командний/FPS HUD, керування бійцем, броня, зброя, процедурний рельєф, спрайти й 4 LOD."),
                ("Ще бракує", "Стіни не блокують рух і вогонь. Немає повноцінної системи укриттів та керованої техніки. Моделі залишаються стилізованими."),
                ("Рекомендований наступний етап", "Колізії й навігація → видимість та укриття → різні задачі контрактів → одна інтерактивна машина. Замір FPS на цільовому ПК.")],
         note="Джерела: Git-коміти, CHANGELOG і звіти GPU / GRAPHICS / UI_TERRAIN / GAMEPLAY / SPRITES_LOD. Посилання нижче відкривають код і реліз.",
         links=[("Завантаження v0.0.26", REPO + "/releases/tag/v0.0.26"), ("Історія коду", REPO + "/commits/graphics/v0.0.26/")]),
]

def lines(text, font, size, width):
    result, line = [], ""
    for word in text.split():
        trial = (line + " " + word).strip()
        if line and pdfmetrics.stringWidth(trial, font, size) > width:
            result.append(line)
            line = word
        else:
            line = trial
    return result + ([line] if line else [])

def paragraph(c, text, x, y, width, size=20, color=INK, bold=False, leading=None):
    font = "GZBold" if bold else "GZ"
    c.setFillColor(color)
    c.setFont(font, size)
    for line in lines(text, font, size, width):
        c.drawString(x, y, line)
        y -= leading or size * 1.4
    return y

def bullets_pdf(c, items, x, y, width, size=20):
    for item in items:
        c.setFillColor(TEAL)
        c.circle(x + 4, y + 6, 3, fill=1, stroke=0)
        y = paragraph(c, item, x + 21, y, width - 21, size=size)
        y -= 15
    assert y >= 115, f"Bullets overflow at {y}: {items}"

def image_pdf(c, item, x, y, width, height):
    path = ROOT / item["path"]
    c.setFillColor(PANEL)
    c.rect(x, y, width, height, fill=1, stroke=0)
    c.drawImage(ImageReader(str(path)), x, y, width=width, height=height,
                preserveAspectRatio=True, anchor="c", mask="auto")
    paragraph(c, item["label"], x, y - 24, width, size=15, bold=True, color=GOLD)
    paragraph(c, item["provenance"], x, y - 46, width, size=12, color=MUTED)

def html_image(item):
    data = base64.b64encode((ROOT / item["path"]).read_bytes()).decode()
    return (f'<figure><img alt="{html.escape(item["label"], quote=True)}" '
            f'src="data:image/png;base64,{data}"><figcaption><strong>{html.escape(item["label"])}</strong>'
            f'<small>{html.escape(item["provenance"])}</small></figcaption></figure>')

def html_bullets(items):
    return "<ul>" + "".join(f"<li>{html.escape(item)}</li>" for item in items) + "</ul>"

def render_pdf(out):
    c = canvas.Canvas(str(out), pagesize=(1280, 720), pageCompression=1)
    c.setTitle("GRAY ZONE: 1989 — хронологія оновлень v0.0.22–v0.0.26")
    c.setAuthor("GRAY ZONE project")
    for index, s in enumerate(SLIDES, 1):
        c.setFillColor(BG)
        c.rect(0, 0, 1280, 720, fill=1, stroke=0)
        paragraph(c, s["badge"], 44, 680, 1140, size=13, color=TEAL, bold=True)
        y = paragraph(c, s["title"], 44, 630, 1190, size=35, color=GOLD, bold=True)
        assert y >= 570, s["title"]
        y = paragraph(c, s["subtitle"], 44, 582, 1190, size=17, color=MUTED)
        assert y >= 525, s["subtitle"]
        kind = s["kind"]
        if kind == "photo":
            bullets_pdf(c, s["bullets"], 44, 507, 360, size=19)
            image_pdf(c, s["images"][0], 438, 140, 798, 389)
        elif kind == "compare":
            for col, item in enumerate(s["images"]):
                image_pdf(c, item, 44 + col * 610, 172, 582, 327.4)
        elif kind == "cards":
            for col, (title, text) in enumerate(s["cards"]):
                x = 44 + col * 404
                c.setFillColor(PANEL)
                c.roundRect(x, 168, 384, 363, 10, fill=1, stroke=0)
                y = paragraph(c, title, x + 24, 491, 336, size=22, color=GOLD, bold=True)
                y = paragraph(c, text, x + 24, y - 18, 336, size=21)
                assert y > 186, (title, y)
        elif kind == "timeline":
            for n, (title, time, commit, text) in enumerate(MILESTONES):
                col, row = n % 4, n // 4
                x, top = 44 + col * 303, 535 - row * 208
                c.setFillColor(PANEL)
                c.roundRect(x, top - 184, 283, 184, 8, fill=1, stroke=0)
                paragraph(c, time + " · " + commit, x + 17, top - 27, 249, size=13, color=TEAL)
                y = paragraph(c, title, x + 17, top - 57, 249, size=19, color=GOLD, bold=True)
                y = paragraph(c, text, x + 17, y - 8, 249, size=16)
                assert y > top - 184, (title, y)
                c.linkURL(REPO + "/commit/" + commit, (x, top - 184, x + 283, top), relative=0)
        elif kind == "benchmark":
            bullets_pdf(c, s["bullets"], 44, 506, 470, size=22)
            paragraph(c, "Передачі за 120 кадрів", 560, 496, 650, size=23, bold=True, color=GOLD)
            for label, value, top, color in [("Повне завантаження щокадру*", 363.631680, 422, MUTED),
                                               ("З кешем статичної сцени", 3.030264, 310, TEAL)]:
                paragraph(c, label, 560, top + 17, 650, size=19)
                c.setFillColor(color)
                c.roundRect(560, top - 43, 626 * value / 363.631680, 30, 3, fill=1, stroke=0)
                paragraph(c, f"{value:.2f}".replace(".", ",") + " МБ", 560, top - 74, 600, size=20, color=color, bold=True)
            paragraph(c, "−99,17% передач", 560, 160, 650, size=32, bold=True, color=GOLD)
        elif kind == "table":
            widths = [190, 280, 224, 498] if index == 17 else [180, 285, 215, 512]
            top = 529
            row_height = 40 if len(s["rows"]) == 5 else 49
            c.setFillColor(PANEL)
            c.rect(44, top - row_height, 1192, row_height, fill=1, stroke=0)
            x = 44
            for title, width in zip(s["columns"], widths):
                paragraph(c, title, x + 14, top - 31, width - 24, size=18, color=GOLD, bold=True)
                x += width
            for n, row in enumerate(s["rows"], 1):
                y = top - row_height * n
                c.setFillColor("#172830" if n % 2 else PANEL)
                c.rect(44, y - row_height, 1192, row_height, fill=1, stroke=0)
                x = 44
                for value, width in zip(row, widths):
                    assert len(lines(value, "GZ", 18, width - 24)) == 1, value
                    paragraph(c, value, x + 14, y - 31, width - 24, size=18)
                    x += width
            bullets_pdf(c, s["bullets"], 44, top - row_height * (len(s["rows"]) + 1) - 32, 1192, size=18)
        y = paragraph(c, s["note"], 44, 94, 1192, size=13, color=MUTED, leading=18)
        assert y >= 36, (s["title"], "Note overflow")
        c.setStrokeColor("#344954")
        c.line(44, 36, 1236, 36)
        paragraph(c, "14.10.2026 · GRAY ZONE: 1989 · Git " + s["commit"], 44, 17, 900, size=11, color=MUTED)
        c.linkURL(REPO + "/commit/" + s["commit"], (44, 10, 600, 31), relative=0)
        c.setFont("GZ", 11)
        c.drawRightString(1236, 17, f"{index:02d} / {len(SLIDES)}")
        if s.get("links"):
            for n, (label, url) in enumerate(s["links"]):
                paragraph(c, label + " ↗", 44 + 340 * n, 123, 330, size=14, color=TEAL, bold=True)
                c.linkURL(url, (44 + 340 * n, 115, 355 + 340 * n, 140), relative=0)
        c.showPage()
    c.save()

def render_html(out):
    sections = []
    for index, s in enumerate(SLIDES, 1):
        kind = s["kind"]
        if kind == "photo":
            body = '<div class="photo">' + html_bullets(s["bullets"]) + html_image(s["images"][0]) + '</div>'
        elif kind == "compare":
            body = '<div class="compare">' + ''.join(html_image(im) for im in s["images"]) + '</div>'
        elif kind == "cards":
            body = '<div class="cards">' + ''.join(f'<article><h2>{html.escape(t)}</h2><p>{html.escape(v)}</p></article>' for t,v in s["cards"]) + '</div>'
        elif kind == "timeline":
            body = '<div class="timeline">' + ''.join(f'<a href="{REPO}/commit/{commit}"><small>{time} · {commit}</small><h2>{html.escape(t)}</h2><p>{html.escape(v)}</p></a>' for t,time,commit,v in MILESTONES) + '</div>'
        elif kind == "benchmark":
            body = '<div class="benchmark">' + html_bullets(s["bullets"]) + '<div><h2>Передачі за 120 кадрів</h2><p>Повне завантаження щокадру*</p><div class="bar before"></div><strong>363,63 МБ</strong><p>З кешем статичної сцени</p><div class="bar after"></div><strong>3,03 МБ</strong><h2>−99,17% передач</h2></div></div>'
        else:
            body = '<div class="table-wrap"><table><thead><tr>' + ''.join(f'<th>{html.escape(v)}</th>' for v in s["columns"]) + '</tr></thead><tbody>'
            body += ''.join('<tr>' + ''.join(f'<td>{html.escape(v)}</td>' for v in row) + '</tr>' for row in s["rows"])
            body += '</tbody></table></div>' + html_bullets(s["bullets"])
        links = ''.join(f'<a href="{url}">{html.escape(label)} ↗</a> ' for label,url in s.get("links", []))
        sections.append(f'<section class="slide{" active" if index == 1 else ""}" id="slide-{index}" aria-label="Слайд {index}"{ "" if index == 1 else " hidden"}><header><p class="badge">{s["badge"]}</p><h1>{html.escape(s["title"])}</h1><p class="subtitle">{html.escape(s["subtitle"])}</p></header>{body}<div class="end"><p class="links">{links}</p><p class="note">{html.escape(s["note"])}</p></div><footer><a href="{REPO}/commit/{s["commit"]}">14.10.2026 · GRAY ZONE: 1989 · Git {s["commit"]}</a><span>{index:02d} / {len(SLIDES)}</span></footer></section>')
    options = ''.join(f'<option value="{n}">{n+1:02d} · {html.escape(s["title"])}</option>' for n,s in enumerate(SLIDES))
    font_css = ''.join(f"@font-face{{font-family:GZ;src:url(data:font/ttf;base64,{base64.b64encode((FONT_DIR / f'DejaVuSans{suffix}.ttf').read_bytes()).decode()}) format('truetype');font-weight:{weight};font-display:swap}}" for suffix,weight in [("",400),("-Bold",700)])
    css = """*{box-sizing:border-box}body{margin:0;background:#111e25;color:#e6eded;font:clamp(16px,1.65vw,22px)/1.4 GZ,system-ui,sans-serif}a{color:#67cbbb}h1,h2,p{margin:0}h1{font-size:clamp(27px,3vw,42px);line-height:1.16;color:#efd69b;margin:14px 0 16px}h2{font-size:1.1em;color:#efd69b;margin-bottom:18px}.slide{max-width:1440px;margin:auto;min-height:100vh;padding:34px 3.5vw 96px;display:none;flex-direction:column;gap:26px}.slide.active{display:flex}.badge{color:#67cbbb;font-size:14px;font-weight:bold;letter-spacing:.12em}.subtitle{color:#a5b9c1;font-size:.83em}.photo,.benchmark{display:grid;grid-template-columns:30% 1fr;gap:3vw;align-items:start}.benchmark{grid-template-columns:41% 1fr}.photo ul{margin:0}ul{padding:0 0 0 22px;margin:0}li{padding-left:2px;margin-bottom:16px}li::marker{color:#67cbbb}figure{margin:0}img{width:100%;max-height:57vh;object-fit:contain;background:#1c3039}figcaption{font-size:.68em;color:#efd69b;padding-top:8px}figcaption small{display:block;color:#a5b9c1;margin-top:5px}.compare{display:grid;grid-template-columns:1fr 1fr;gap:24px;align-items:start}.cards{display:grid;grid-template-columns:repeat(3,1fr);gap:20px}.cards article{padding:28px;background:#1c3039;border-radius:10px;min-height:310px}.timeline{display:grid;grid-template-columns:repeat(4,1fr);gap:18px}.timeline a{text-decoration:none;color:#e6eded;border-radius:8px;background:#1c3039;padding:20px;min-height:190px;font-size:.85em}.timeline small{color:#67cbbb}.timeline h2{margin:16px 0 12px;font-size:1em}.bar{height:28px;border-radius:3px;background:#a5b9c1;margin:10px 0}.after{width:.8333%;background:#67cbbb}.benchmark strong{color:#67cbbb}.benchmark p{margin-top:18px}.benchmark h2:last-child{font-size:1.5em;margin-top:28px}.table-wrap{overflow-x:auto}table{border-collapse:collapse;width:100%;font-size:.85em;text-align:left}th,td{padding:15px 17px;white-space:nowrap}th{color:#efd69b;background:#1c3039}tr:nth-child(even){background:#1c3039}tr:nth-child(odd){background:#172830}.end{margin-top:auto}.note{color:#a5b9c1;font-size:.65em}.links{font-size:.75em;margin-bottom:12px}.links a{margin-right:28px}footer{display:flex;justify-content:space-between;gap:12px;border-top:1px solid #344954;padding-top:12px;font-size:12px;color:#a5b9c1}footer a{color:inherit;text-decoration:none}nav{position:fixed;bottom:0;left:0;right:0;display:flex;justify-content:center;gap:12px;padding:10px;background:#111e25ed;border-top:1px solid #344954}button,select{font:14px GZ,sans-serif;color:#e6eded;border:1px solid #55717e;border-radius:5px;background:#1c3039;padding:8px 14px}select{max-width:50vw}button{cursor:pointer}button:focus-visible,select:focus-visible,a:focus-visible{outline:3px solid #efd69b;outline-offset:3px}button:disabled{opacity:.4;cursor:default}.sr-only{position:absolute;width:1px;height:1px;overflow:hidden;clip:rect(0,0,0,0)}
@media(max-width:900px){.slide{padding:24px 22px 90px}.photo,.benchmark,.cards{grid-template-columns:1fr}.photo figure{grid-row:1}.compare{grid-template-columns:1fr}.timeline{grid-template-columns:repeat(2,1fr)}.cards article{min-height:0}.photo li{margin-bottom:10px}img{max-height:none}nav{gap:6px}button{padding:8px}.compare figcaption{margin-bottom:10px}}
@media print{@page{size:landscape;margin:10mm}body{font-size:12px;print-color-adjust:exact;-webkit-print-color-adjust:exact}nav{display:none}.slide,.slide[hidden]{display:flex;break-after:page;min-height:0;height:175mm;padding:12px;gap:16px}h1{font-size:25px}.photo{grid-template-columns:30% 1fr}.compare{grid-template-columns:1fr 1fr}img{max-height:105mm}.cards{grid-template-columns:repeat(3,1fr)}.cards article{min-height:0;padding:18px}.timeline{grid-template-columns:repeat(4,1fr)}.timeline a{min-height:0;padding:12px}li{margin-bottom:9px}th,td{padding:9px}footer{font-size:9px}.note{font-size:10px}.slide:last-of-type{break-after:auto}}
"""
    js = """
const slides=[...document.querySelectorAll('.slide')],select=document.querySelector('#chapter'),prev=document.querySelector('#prev'),next=document.querySelector('#next'),status=document.querySelector('#status');let current=0;
function show(n,updateHash=true){n=Math.max(0,Math.min(slides.length-1,n));slides.forEach((slide,i)=>{slide.classList.toggle('active',i===n);slide.hidden=i!==n});current=n;select.value=String(n);prev.disabled=n===0;next.disabled=n===slides.length-1;status.textContent=`Слайд ${n+1} з ${slides.length}`;if(updateHash)history.replaceState(null,'',`#${n+1}`);window.scrollTo(0,0)}
function fromHash(){const n=Number(location.hash.slice(1));show(Number.isInteger(n)&&n>=1&&n<=slides.length?n-1:0,false)}
prev.addEventListener('click',()=>show(current-1));next.addEventListener('click',()=>show(current+1));select.addEventListener('change',()=>show(Number(select.value)));window.addEventListener('hashchange',fromHash);
document.addEventListener('keydown',e=>{if(/^(INPUT|SELECT|TEXTAREA|BUTTON)$/.test(e.target.tagName))return;if(['ArrowRight','PageDown',' '].includes(e.key)){e.preventDefault();show(current+1)}else if(['ArrowLeft','PageUp'].includes(e.key)){e.preventDefault();show(current-1)}else if(e.key==='Home'){e.preventDefault();show(0)}else if(e.key==='End'){e.preventDefault();show(slides.length-1)}});fromHash();
"""
    out.write_text('<!doctype html><html lang="uk"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>GRAY ZONE: 1989 — хронологія оновлень</title><style>' + font_css + css + '</style></head><body>' + ''.join(sections) + '<nav aria-label="Навігація презентації"><button id="prev" aria-label="Попередній слайд">←</button><label class="sr-only" for="chapter">Вибрати слайд</label><select id="chapter">' + options + '</select><button id="next" aria-label="Наступний слайд">→</button><span id="status" class="sr-only" aria-live="polite"></span></nav><script>' + js + '</script></body></html>', encoding="utf-8")

def render_markdown(out):
    text = ["# GRAY ZONE: 1989 — хронологія оновлень", "", "Доказова хронологія модернізації v0.0.22–v0.0.26. Усі зазначені коміти — 14.10.2026; час UTC.", "", "## Етапи", "", "| Етап | Час фіксації | Коміт | Результат |", "| --- | --- | --- | --- |"]
    for title,time,commit,desc in MILESTONES:
        text.append(f"| {title} | {time} | [{commit}]({REPO}/commit/{commit}) | {desc} |")
    for n,s in enumerate(SLIDES, 1):
        text += ["", f"## Слайд {n:02d}. {s['title']}", "", s["subtitle"], ""]
        for title,value in s.get("cards", []): text.append(f"- **{title}:** {value}")
        text += ["- " + v for v in s.get("bullets", [])]
        if s.get("columns"):
            text += ["", "| " + " | ".join(s["columns"]) + " |", "| " + " | ".join("---" for _ in s["columns"]) + " |"]
            text += ["| " + " | ".join(row) + " |" for row in s["rows"]]
        for image in s.get("images", []):
            text += ["", f"![{image['label']}]({image['path'].removeprefix('docs/')})", f"{image['provenance']}. Ресурс: `{image['path']}`."]
        text += ["", s["note"]]
        text += [f"[{label}]({url})" for label,url in s.get("links", [])]
    text += ["", "## Джерела та відтворення", "", "- `docs/CHANGELOG.md`", "- `docs/MODERNIZATION_PHASE1_UA.md`", "- `docs/GPU_OPTIMIZATION_UA.md`", "- `docs/GRAPHICS_RTX3050TI_UA.md`", "- `docs/UI_TERRAIN_v024_UA.md`", "- `docs/GAMEPLAY_v025_UA.md`", "- `docs/SPRITES_LOD_v026_UA.md`", "", "Історичний PNG v023 скопійовано без редагування з `previews/soldiers.png` опублікованої галереї v0.0.23. Інші PNG — з версійних каталогів `docs/review`. Карта на слайді 11 знята у v026; її стиль упроваджено у v025. Для вихідного v022 немає окремого знімка.", "", "Результати тестів наведено зі звітів етапів; вони не були повторно виконані під час створення презентації. Фізичний FPS на RTX 3050 Ti не виміряний.", "", "```sh", "python3 scripts/create_updates_timeline_presentation.py --out-dir /workspace", "```", "", "PDF має 18 сторінок. HTML містить зображення і шрифти всередині файла, відкривається офлайн; навігація кнопками, стрілками або вибором розділу. Посилання на GitHub потребують мережі.", ""]
    out.write_text("\n".join(text), encoding="utf-8")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, default=Path("/workspace"))
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    for name,suffix in [("GZ", ""), ("GZBold", "-Bold")]:
        pdfmetrics.registerFont(TTFont(name, str(FONT_DIR / f"DejaVuSans{suffix}.ttf")))
    for slide in SLIDES:
        for item in slide.get("images", []):
            assert (ROOT / item["path"]).is_file(), item["path"]
    pdf = args.out_dir / (NAME + ".pdf")
    web = args.out_dir / (NAME + ".html")
    render_pdf(pdf)
    render_html(web)
    render_markdown(ROOT / "docs/UPDATES_TIMELINE_UA.md")
    print(f"{len(SLIDES)} slides; PDF {pdf.stat().st_size:,} bytes; HTML {web.stat().st_size:,} bytes")

if __name__ == "__main__":
    main()
