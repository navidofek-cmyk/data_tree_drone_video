# Tree Drone Video → Point Cloud (.las) → Qt Viewer / 3D Forest

Z **videa obletu stromu pořízeného DJI dronem** vyrobit **mračno bodů ve formátu `.las`**
(formát, jaký používá lidar) pomocí **fotogrammetrie** a prohlížet/analyzovat ho ve vlastním
**Qt C++ vieweru** a v softwaru **3D Forest**.

> Učební/pohovorový projekt (`cpp_pohovor/`). Dron má jen RGB kameru (žádný laser), takže
> nejde o „pravý" lidar — stejný **typ dat (mračno bodů)** ale dostaneme přes
> **Structure-from-Motion + Multi-View Stereo (SfM+MVS)**.

## Obsah
1. [Jak to funguje](#1-jak-to-funguje)
2. [Prostředí a omezení](#2-prostředí-a-omezení)
3. [Předpoklady](#3-předpoklady)
4. [Kompletní reprodukce krok za krokem](#4-kompletní-reprodukce-krok-za-krokem)
5. [Viewer (Qt5)](#5-viewer-qt5)
6. [Varianta na Google Colab (GPU)](#6-varianta-na-google-colab-gpu)
7. [Výstupy](#7-výstupy)
8. [Struktura repozitáře](#8-struktura-repozitáře)
9. [Troubleshooting](#9-troubleshooting)
10. [Dokumentační soubory — README vs POSTUP vs CLAUDE.md](#10-dokumentační-soubory)

---

## 1) Jak to funguje

```
DJI_*.MP4  +  DJI_*.SRT (telemetrie po snímcích: GPS, výška, expozice)
   │
   │  ffmpeg            ── vytáhne ~95 snímků (každý 12. frame, 2560×1440)
   ▼
odm_project/images/frame_*.jpg
   │  make_geo.py       ── ke každému snímku přiřadí GPS ze SRT → geo.txt
   ▼
OpenDroneMap (Docker, CPU)   ── SfM + husté MVS, georeferencováno přes geo.txt
   ▼
odm_project/odm_georeferencing/odm_georeferenced_model.laz   (UTM 33N, metry)
   │  laspy             ── .laz → .las (3D Forest čte jen .las/.pcd)
   ▼
odm_project/strom.las     →  viewer/  (Qt5, software render)   |  3D Forest
```

**Pojmy:** *LiDAR* = laserový skener → mračno bodů (X,Y,Z + barva/intenzita/klasifikace),
standardní formát **`.las`/`.laz`**. *3D Forest* (VUKOZ) = analýza lidarových dat z lesa,
čte `.las`/`.pcd`.

---

## 2) Prostředí a omezení

Tahle rozhodnutí jsou daná hardwarem — neměň je bez důvodu:

| Co | Důsledek |
|---|---|
| **Intel UHD 630, žádná NVIDIA/CUDA** | husté MVS v COLMAP/Meshroom lokálně nejde → **OpenDroneMap (CPU)**; GPU jen na Colabu |
| **Slabé chlazení (OptiPlex)** | dlouhé CPU joby běží na **omezený počet jader** + **teplotní watchdog** |
| **Přístup přes SSH/MobaXterm (X11), plocha Wayland** | moderní OpenGL přes X11 selže → **GUI softwarově přes QPainter, bez OpenGL** |
| **`sudo` chce heslo** | žádné systémové instalace; **ffmpeg statický ve `.venv`** |
| **Python PEP 668** | instalovat jen do **`.venv/`**, ne globálně |
| **Docker bez sudo, běží i cizí kontejnery** | nevypínat `3d-forest-build` ani jiné služby |

HW: OptiPlex 12 jader / 32 GB; záloha i7 16 GB (lépe chlazený); Colab T4/A100.

---

## 3) Předpoklady

- `git`, `docker` (bez sudo), `python3` + `venv`
- pro viewer: `g++`, `cmake`, **`qtbase5-dev`** (Qt5 Widgets)
- vstupní dvojice souborů `DJI_*.MP4` + `DJI_*.SRT` v kořeni projektu

Ověření:
```bash
which git docker python3 g++ cmake qmake
docker images | grep opendronemap   # případně se stáhne v kroku 4
```

---

## 4) Kompletní reprodukce krok za krokem

Vše z kořene projektu:
`/home/ivand/projects/learning_cpp/cpp_pohovor/data_tree_drone_video`

### 4.0 Prostředí (jednorázově)
```bash
python3 -m venv .venv
.venv/bin/pip install imageio-ffmpeg "laspy[lazrs]" pyproj matplotlib
FF=$(.venv/bin/python -c "import imageio_ffmpeg,sys; sys.stdout.write(imageio_ffmpeg.get_ffmpeg_exe())")
```

### 4.1 Snímky z videa
```bash
mkdir -p odm_project/images
"$FF" -hide_banner -loglevel error -threads 2 -i DJI_*.MP4 \
  -vf "select=not(mod(n\,12)),scale=2560:1440" -vsync 0 -q:v 3 \
  -y odm_project/images/frame_%04d.jpg
```
Bere každý 12. frame (~5 sn./s → ~95 snímků), zmenšeno na 2560×1440 (úspora RAM/uploadu).
Mapování na telemetrii: `frame_<k>` ↔ `FrameCnt = 12*(k-1)+1`.

### 4.2 GPS na snímek → geo.txt
```bash
.venv/bin/python make_geo.py     # → odm_project/images/geo.txt  (EPSG:4326: jméno lon lat alt)
```

### 4.3 ODM rekonstrukce (na pozadí + watchdog)
```bash
docker pull opendronemap/odm
docker run --rm -v "$PWD":/datasets opendronemap/odm \
  --project-path /datasets odm_project \
  --geo /datasets/odm_project/images/geo.txt \
  --feature-quality medium --pc-quality medium --max-concurrency 4 &

# teplotní pojistka (zabije ODM při >90 °C):
while cid=$(docker ps -q --filter ancestor=opendronemap/odm); [ -n "$cid" ]; do
  t=$(($(cat /sys/class/thermal/thermal_zone3/temp)/1000)); echo "CPU ${t}C"
  [ "$t" -gt 90 ] && { docker stop "$cid"; break; }; sleep 15
done
```
- `--max-concurrency` = hlavní páka na teplotu (4 = bezpečné; `high` kvalitu radši na i7).
- Trvá ~15–60 min, nejteplejší je fáze `openmvs`.

### 4.4 .laz → .las
```bash
.venv/bin/python -c "import laspy; laspy.read('odm_project/odm_georeferencing/odm_georeferenced_model.laz').write('odm_project/strom.las')"
```

### 4.5 PLY pro viewer (vycentrované, s barvami)
Viz `viewer/` — `cloud.ply` (celá scéna) a `tree.ply` (vyříznutý strom) se generují ze
`strom.las` přes `laspy` (offset uložen v hlavičce PLY jako komentář; nutné kvůli přesnosti float32
na UTM souřadnicích).

---

## 5) Viewer (Qt5)

Vlastní prohlížeč mračna — **softwarový (QPainter + z-buffer), bez OpenGL**, aby běžel přes
MobaXterm. Ovládání ve stylu Blenderu (gizmo, perspektiva/ortho, kolmé pohledy, řezací roviny,
PNG export). **Detaily a build/run v [`viewer/README.md`](viewer/README.md).**

```bash
cd viewer && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"$(nproc)"
./run.sh tree.ply        # přes MobaXterm s X11-Forwarding (ověř echo $DISPLAY)
```

---

## 6) Varianta na Google Colab (GPU)

Pokud chceš husté MVS na GPU (rychlejší, bez zátěže OptiPlexu), je v `strom_colab.ipynb`
připravený notebook s **COLMAP (CUDA)** + georeferencí. Nahraješ `images/` na Google Disk
(`MyDrive/lidar/strom/`), pustíš notebook, stáhneš `strom.las`. Pozor: COLMAP z `apt` na Colabu
je GUI build → notebook nastavuje `QT_QPA_PLATFORM=offscreen`.

---

## 7) Výstupy

| Soubor | Co to je |
|---|---|
| `odm_project/strom.las` | **finální mračno** — 1,33 M bodů, barevné, UTM 33N (metry), LAS 1.4 |
| `odm_project/odm_georeferencing/odm_georeferenced_model.laz` | originální výstup ODM |
| `odm_project/*.png` | náhledy (shora/z boku, vegetace, crop stromu) |
| `viewer/cloud.ply`, `viewer/tree.ply` | data pro viewer (celá scéna / strom) |

Měřítko je z GPS dronu → **orientační (±desítky %)**. Kontrola: panelák ~12–15 m, strom ~3–4 m.

---

## 8) Struktura repozitáře

```
DJI_*.MP4 / DJI_*.SRT   vstup (4K/60fps ~19 s) + telemetrie
make_geo.py             .SRT → geo.txt
README.md               tento přehled
POSTUP.md               postup + průběžný stav (deník)
CLAUDE.md               příručka pro Claude Code (viz níže)
strom_colab.ipynb       GPU varianta na Colab
.venv/                  ffmpeg + laspy/pyproj/matplotlib   (necommitovat)
3d-forest/              klon VUKOZ-OEL/3d-forest           (necommitovat)
odm_project/            images/, odm_georeferencing/, strom.las, náhledy
viewer/                 Qt5 viewer (src/, CMakeLists.txt, run.sh, README.md)
```

---

## 9) Troubleshooting

- **`could not connect to display` / Qt spadne v Dockeru nebo na Colabu** → headless nástroj:
  `QT_QPA_PLATFORM=offscreen`. U vieweru přes SSH zapni X11-Forwarding a ověř `echo $DISPLAY`.
- **`fused.ply`/dense chybí, COLMAP „requires CUDA"** → apt-COLMAP nemá CUDA; použij ODM (CPU)
  nebo Colab GPU.
- **pip: „externally-managed-environment" (PEP 668)** → instaluj do `.venv/`, ne globálně.
- **CPU se přehřívá** → sniž `--max-concurrency`, drž medium kvalitu, nech běžet watchdog.
- **3D Forest nevidí `.laz`** → převeď na `.las` (krok 4.4); import bere `*.las`/`*.pcd`.
- **Viewer přes SSH je trhaný** → `./run.sh tree.ply --max 300000` (podvzorkování).

---

## 10) Dokumentační soubory

Tři soubory, tři účely — nepleť si je:

- **`README.md`** (tenhle) — pro **člověka**. Co projekt je, jak ho od nuly zreprodukovat,
  jak ho spustit. Stabilní referenční dokument.
- **`POSTUP.md`** — **deník/checklist** konkrétního průchodu (co je hotové, naměřené hodnoty,
  rozhodnutí). Vývoj v čase.
- **`CLAUDE.md`** — **příručka pro Claude Code**, kterou si AI asistent automaticky načte na
  začátku každé session v tomto adresáři. Obsahuje hlavně **omezení prostředí a konvence**
  (GUI softwarově, ne OpenGL; instalovat do `.venv`; hlídat teplotu; ODM ne COLMAP atd.),
  aby asistent neopakoval chyby a držel se zavedeného postupu. Je to **kontext uložený v repu**
  (verzovaný, sdílený), na rozdíl od osobní paměti asistenta mimo projekt.

  Stručně: *README = jak to spustit (člověk), POSTUP = co se udělalo (deník),
  CLAUDE.md = jak tu pracovat (pravidla pro AI).*
```
