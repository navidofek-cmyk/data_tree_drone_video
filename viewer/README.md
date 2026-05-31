# Point Cloud Viewer (Qt5, softwarový — bez OpenGL)

Prohlížeč mračna bodů z PLY. 3D se kreslí **softwarově přes QPainter + vlastní
z-buffer** (žádný OpenGL/GLX), takže běží i přes **vzdálené X11 (ssh -X /
MobaXterm)**, kde moderní OpenGL většinou selže. Ovládání ve stylu Blenderu
(navigační gizmo, perspektiva/ortho, kolmé pohledy).

Vzor převzat z `../../cfd_simple_cube_qt` (`DomainView3D`).

---

## 1) Build (jednou)
```bash
cd /home/ivand/projects/learning_cpp/cpp_pohovor/data_tree_drone_video/viewer
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```
Potřebuje `qtbase5-dev`, `cmake`, `g++`. (`run.sh` build spustí i sám, pokud chybí.)

---

## 2) Jak to pustíme

Nejjednodušší je skript **`run.sh`** (sám zkontroluje build i displej):

```bash
cd /home/ivand/projects/learning_cpp/cpp_pohovor/data_tree_drone_video/viewer

./run.sh tree.ply              # strom zblízka (95k bodů)
./run.sh cloud.ply             # celá scéna (1,33 M bodů)
./run.sh cloud.ply --max 300000  # podvzorkovat (pomalé spojení)
```

### A) Přes MobaXterm (SSH z Windows) — náš případ
1. V MobaXterm zapni **X server** (ikona „X" vpravo nahoře svítí).
2. V session: *Advanced SSH settings → ☑ X11-Forwarding* a **připoj se znovu**.
3. Ověř, že máš displej:
   ```bash
   echo $DISPLAY        # má vypsat třeba  localhost:10.0
   ```
4. Spusť:
   ```bash
   cd .../viewer && ./run.sh tree.ply
   ```
   Okno naskočí na tvém Windows monitoru.

> Když `echo $DISPLAY` nic nevypíše → X11-Forwarding není zapnutý (krok 2),
> nebo se připoj přes `ssh -X ivand@<stroj>`.

### B) Lokálně na monitoru OptiPlexu (Wayland/XWayland)
```bash
DISPLAY=:0 ./run.sh tree.ply        # případně DISPLAY=:1
```

### C) Bez okna — render rovnou do PNG (headless, na test/náhled)
```bash
QT_QPA_PLATFORM=offscreen ./build/viewer tree.ply --snapshot nahled.png 1100 800
```

### Přímé spuštění binárky (bez run.sh)
```bash
./build/viewer <soubor.ply> [--max N] [--snapshot out.png [W H]]
```

---

## 3) Ovládání (styl Blenderu)

**Myš**
- **levé / prostřední tažení** — orbit (rotace)
- **pravé tažení** nebo **Shift + prostřední** — posun (pan)
- **kolečko** — zoom

**Navigační gizmo vpravo nahoře**
- barevné kuličky **X/Y/Z** se otáčejí s pohledem; **klik na osu** = kolmý pohled
  (automaticky přepne na ortho, jako v Blenderu)
- svislý sloupec tlačítek: `+` / `−` zoom · `⌂` reset · **`∞`/`⊥`** persp↔ortho ·
  `#` mřížka · **`PNG`** export snímku

**Boční panel — řezací roviny (kolmé na osy)**
- pro osy **X / Y / Z** dvojice posuvníků **min / max** → schová body mimo box
  (např. odřízneš zem, pozadí nebo necháš jen korunu stromu)
- tlačítko **Zrušit řez** vrátí plný rozsah

**Klávesnice**
| klávesa | akce |
|---|---|
| šipky | rotace |
| W A S D | posun |
| + / − | zoom |
| `[` `]` | velikost bodu |
| 1 / 3 / 7 | pohled zepředu / zboku / shora |
| 5 | přepnout perspektiva ↔ ortho |
| g | mřížka on/off |
| P | uložit PNG (`view_<datum>.png` do aktuální složky) |
| r | reset pohledu |

---

## 4) Data / formát PLY
- `cloud.ply` — celá scéna; `tree.ply` — vyříznutý strom. Obě vyrobena ze
  `../odm_project/strom.las` (vycentrovaná, offset v hlavičce jako komentář).
- Formát: `binary_little_endian`, `float x,y,z` + `uchar red,green,blue`
  (loader je tolerantní k pořadí/typu vlastností).

## 5) TODO (nápady na rozšíření)
- přímé čtení `.las`/`.laz`
- obarvení podle výšky, ořezový box, měření vzdáleností
- octree/LOD pro plynulost u velkých mračen
