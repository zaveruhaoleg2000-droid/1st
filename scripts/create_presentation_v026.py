"""Build a Ukrainian offline HTML slideshow and PDF from actual engine captures."""
from pathlib import Path
import base64, html, json, struct
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.utils import ImageReader
ROOT=Path(__file__).resolve().parent.parent;IMAGES=ROOT/'docs/review/v026'
OUT=Path('/workspace');version='v0.0.26'
slide_data=[
 ('GRAY ZONE: 1989','Стратегія ЧВК + керування бійцем. Робочий процедурний прототип v0.0.26.', 'review_first_person.png', ['Реальний час із паузою','Командування → перша особа → карта','Орієнтир: RTX 3050 Ti, 4 GB VRAM']),
 ('Оцінка ідеї','Сильна сторона — можливість побачити результати власних наказів із поля бою.', 'review_tactical.png', ['Командна тактика й особиста участь доповнюють одна одну.','Гібрид моделей і спрайтів підходить для обмеженої відеопам’яті.','Головний наступний пріоритет: колізії, навігація та укриття.']),
 ('Гра тепер у реальному часі','Покрокову систему видалено. Симуляція працює безперервно; пауза зупиняє бій.', 'review_tactical.png', ['Space / кнопка «ПАУЗА»','1 — тактика; 2 — перша особа; 3 — карта','Enter — висадка; F5/F9 — збереження','Контракти, кошти, перемога/втрати й старі збереження збережені.']),
 ('Жива камера й командний HUD','Новий пропорційний шрифт, панелі відділення, цілі, картки та мінікарта.', 'review_tactical.png', ['WASD/стрілки — рух камери; Shift — швидше.','Q/E — обертання; середня кнопка + рух миші — обертання/нахил.','Колесо — масштаб 2–8000 м.','ЛКМ — вибір; ПКМ — наказ; Tab — загін.']),
 ('Перша особа','Керування обраним живим бійцем. Камера й ручний вогонь використовують той самий напрямок.', 'review_first_person.png', ['Миша — огляд; WASD — рух.','Shift — біг; Ctrl — присісти.','ЛКМ — вогонь; ПКМ — приціл.','R — перезарядка; F — пістолет/основна зброя.','Tab — боєць; 1/Esc — тактика.']),
 ('Зброя, здоров’я та броня','П’ять процедурних типів зброї, окремі магазини й пістолетний боєзапас.', 'review_soldier.png', ['AK-74: 28 шкоди / магазин 30','SMG: 18 / 32; RPK: 26 / 45','SVD: 65 / 10; пістолет: 20 / 12','Здоров’я 100; броня 60 поглинає до 60% шкоди тіла й зношується.','Голова: подвійна шкода. Рельєф блокує ручний постріл.']),
 ('Ефекти пострілів','Прозорі ефекти прив’язані до подій стрільби та часу симуляції.', 'review_weapon_effects.png', ['Спалах біля ствола й дим.','Короткий трасер, пил/влучання та іскри.','Працюють для ручного й автоматичного вогню.','До 128 недавніх подій; пауза зупиняє їхній час.']),
 ('Карта з об’ємним рельєфом','Плавні кольори, затінення схилів, сніг високогір’я та тонкі політичні кордони.', 'review_global.png', ['Стилізований вигляд супутникової карти.','Наявна географічна сітка 257×153.','Це графіка рушія, а не реальні нові космічні знімки.','Мапа кампанії використовує CPU raster/upload path.']),
 ('Рослинність і хмари — спрайти','Ближні дерева зберігають об’ємні стовбури; крони, дальні дерева й трава — картки.', 'review_foliage.png', ['16 RGBA шарів: рослини, хмари та ефекти.','Alpha cutoff + змішування, сортування за дальністю.','GPU alpha-tested тіні рослинності.','Трава: 40/65/90 м; до 4096 карток на кадр.','CPU fallback також малює спрайти.']),
 ('Процедурний RGBA каталог','Прозорі краї зображень і mip-рівні дозволяють повторно використовувати ресурси.', 'sprite_catalogue.png', ['Дерева, сухі гілки, трава, очерет, квіти, крони.','Хмара, дим, спалах, іскра/трасер, пил/влучання.','Папороть, дощ і вогонь — запас каталогу; окремих emitter систем ще немає.','Ресурси відтворюються за seed 1989.']),
 ('Чотири рівні LOD','Кількість геометрії зменшується зі збільшенням дистанції.', 'review_vehicle.png', ['LOD 0: <40 м — максимальні деталі.','LOD 1: 40–120 м — менше сегментів і деталей.','LOD 2: 120–320 м — прості частини.','LOD 3: ≥320 м — силуети/маркери.','Пороги масштабуються для великих об’єктів; у бійців гістерезис 12%.']),
 ('Більше моделей і матеріалів','Оточення: 16 → 32 моделі. Базові матеріали: 31 → 39, із normal maps.', 'review_terrain.png', ['Намет, антена, опора, джип, лавка, контейнер, вежа.','Уламки, бар’єри, палети, колодязь, гідрант, знаки.','Нові матеріали: кора, мох, штукатурка, іржа, плитка, рослини.','Техніка поки декоративна; художні форми залишаються стилізованими.']),
 ('Бюджети ресурсів','Верхні межі дотримано. Обсяг не збільшується штучним заповненням.', 'review_foliage.png', ['Текстури: 98,15 МБ резидентного payload / 100 МБ.','Матеріали + sprites на диску: близько 52,12 МБ.','Зброя з LOD каталогом: 0,427 / 50 МБ.','Каталог оточення: 1,016 / 200 МБ.','Depth/MSAA/shadow targets, staging і пам’ять драйвера рахуються окремо.']),
 ('Що перевірено','Реальні виконані перевірки, збірки та кадри рушія.', 'review_first_person.png', ['Linux Vulkan: 30/30, synchronization validation.','Linux CPU Debug/Release: по 29/29.','Windows CPU/Vulkan під Wine: по 27/27.','Пакет: CPU/native Vulkan, спрайти, FPS HUD, ручний постріл.','XTest input: PASS на Linux; під Wine тест не знайшов вікно. Фізична RTX/Win11 ще не перевірені.']),
 ('Що найбільше покращить гру','Графічна база стала насиченішою. Наступний крок — глибина бою.', 'review_tactical.png', ['Колізії й пошук шляху.','LOS, стіни, укриття та придушення.','Різні задачі контрактів.','Одна інтерактивна машина з екіпажем.','Наразі стіни не блокують рух/вогонь, а техніка не керована.']),
 ('Запуск готової збірки','Повністю розпакуйте Windows ZIP і запустіть PLAY_HIGH.bat.', 'review_menu.png', ['Для RTX 3050 Ti 4 GB почніть із High.','Якщо повільно — PLAY_MEDIUM.bat / PLAY_LOW.bat.','Резервний запуск: PLAY_CPU.bat.','Не запускайте EXE всередині архіву; assets мають бути поруч.','Посилання на білд і документацію є на сторінці GitHub Release v0.0.26.'])]

def wrapped(text,font,size,width):
 words=text.split();lines=[];line=''
 for word in words:
  candidate=(line+' '+word).strip()
  if line and pdfmetrics.stringWidth(candidate,font,size)>width:lines.append(line);line=word
  else:line=candidate
 if line:lines.append(line)
 return lines

def main():
 for name,path in [('DejaVu','/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'),('DejaVuBold','/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf')]:pdfmetrics.registerFont(TTFont(name,path))
 pdf=OUT/'GRAY_ZONE_1989_v026_PRESENTATION_UA.pdf';c=canvas.Canvas(str(pdf),pagesize=(960,540));c.setTitle('GRAY ZONE: 1989 — v0.0.26');c.setAuthor('GRAY ZONE project')
 html_slides=[]
 for i,(title,subtitle,picture,bullets) in enumerate(slide_data):
  path=IMAGES/picture;assert path.exists(),path
  c.setFillColorRGB(.055,.085,.092);c.rect(0,0,960,540,fill=1,stroke=0)
  c.setFillColorRGB(.82,.74,.52);c.setFont('DejaVuBold',26);c.drawString(30,494,title)
  c.setFillColorRGB(.80,.84,.82);c.setFont('DejaVu',12);y=468
  for line in wrapped(subtitle,'DejaVu',12,890):c.drawString(30,y,line);y-=17
  bodyY=min(414,y-20);y=bodyY
  for bullet in bullets:
   c.setFillColorRGB(.86,.88,.82);c.setFont('DejaVu',13)
   for n,line in enumerate(wrapped(bullet,'DejaVu',13,272)):
    c.drawString(30,y,('• ' if n==0 else '  ')+line);y-=19
   y-=13
  c.drawImage(ImageReader(str(path)),340,95,width=590,height=320,preserveAspectRatio=True,anchor='c',mask='auto')
  c.setFillColorRGB(.58,.65,.65);c.setFont('DejaVu',9)
  provenance='CPU Windows EXE / Wine' if picture in ('review_global.png','review_menu.png') else 'Процедурні ресурси рушія' if picture=='sprite_catalogue.png' else 'Native Vulkan / Mesa Lavapipe / 1280×720'
  c.drawString(340,78,provenance);c.drawString(30,23,f'GRAY ZONE: 1989  •  v0.0.26  •  14.10.2026');c.drawRightString(930,23,f'{i+1} / {len(slide_data)}');c.showPage()
  data=base64.b64encode(path.read_bytes()).decode();html_slides.append('<section class="slide '+('active' if i==0 else '')+'"><h1>'+html.escape(title)+'</h1><p class="subtitle">'+html.escape(subtitle)+'</p><div class="body"><ul>'+''.join('<li>'+html.escape(b)+'</li>' for b in bullets)+'</ul><figure><img src="data:image/png;base64,'+data+'"><figcaption>'+html.escape(provenance)+'</figcaption></figure></div><footer>v0.0.26 · 14.10.2026 <span>'+str(i+1)+' / '+str(len(slide_data))+'</span></footer></section>')
 c.save()
 page='''<!doctype html><html lang="uk"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>GRAY ZONE — презентація v0.0.26</title><style>body{margin:0;background:#10191b;color:#e0e4d7;font:20px/1.5 system-ui,sans-serif}.slide{display:none;min-height:90vh;box-sizing:border-box;padding:32px 4vw 48px}.slide.active{display:block}h1{font-size:2em;line-height:1.15;color:#d6c08d;margin:0 0 16px}.subtitle{color:#bdcbc8;margin-bottom:24px}.body{display:grid;grid-template-columns:30% 1fr;gap:3vw;align-items:center}ul{padding-left:22px}li{margin-bottom:18px}figure{margin:0}img{width:100%;max-height:63vh;object-fit:contain;border:1px solid #465954}figcaption{font-size:13px;color:#9caeab}footer{font-size:14px;color:#96aaa3;margin-top:22px}footer span{float:right}nav{position:fixed;bottom:10px;right:4vw;display:flex;gap:8px}button{font:inherit;padding:5px 18px;border:1px solid #66786a;background:#273b35;color:#e2d8b6;cursor:pointer}@media(max-width:750px){body{font-size:16px}.body{display:flex;flex-direction:column-reverse;gap:10px}.slide{padding:20px}img{max-height:45vh}h1{font-size:1.6em}li{margin-bottom:9px}}@media print{nav{display:none}.slide{display:block;break-after:page;height:100vh;min-height:0}.body{display:grid;grid-template-columns:30% 1fr}body{font-size:16px}img{max-height:60vh}}</style>'''+''.join(html_slides)+'''<nav><button id="prev" aria-label="Попередній слайд">←</button><button id="next" aria-label="Наступний слайд">→</button></nav><script>const slides=[...document.querySelectorAll('.slide')];let current=0;function show(n){slides[current].classList.remove('active');current=(n+slides.length)%slides.length;slides[current].classList.add('active');window.scrollTo(0,0)}document.querySelector('#prev').onclick=()=>show(current-1);document.querySelector('#next').onclick=()=>show(current+1);document.addEventListener('keydown',e=>{if(['ArrowRight','PageDown',' '].includes(e.key)){e.preventDefault();show(current+1)}if(['ArrowLeft','PageUp'].includes(e.key)){e.preventDefault();show(current-1)}});</script></html>'''
 web=OUT/'GRAY_ZONE_1989_v026_PRESENTATION_UA.html';web.write_text(page)
 print(f'Presentation: {len(slide_data)} slides; PDF {pdf.stat().st_size} bytes; offline HTML {web.stat().st_size} bytes')
if __name__=='__main__':main()
