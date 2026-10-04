# Презентация: Energy Crisis · отбор „Волтова дъга“

- **index.html** — HTML презентация (16 слайда), работи без интернет. Отворете я в браузър.
  - Стрелки / Space / клик — следващ слайд; Home / End — първи / последен;
  - **F** — цял екран; **N** — бележки за говорещия; `#5` в адреса отваря слайд 5.
- **Energy-Crisis-Presentation.pdf** — същите слайдове като PDF (по един на страница).
- **media/trailer.mp4** — 29-секунден трейлър от играта; **media/trailer-loop.gif** — кратък цикъл за README.
- **slides.md** — сценарият със съдържанието и бележките. След промяна: `perl build_deck.pl` (в тази папка) пресъздава index.html от template.html.
- PDF наново: `msedge --headless=new --no-pdf-header-footer --print-to-pdf=Energy-Crisis-Presentation.pdf "file:///<път>/index.html?print"`.
