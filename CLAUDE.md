# CLAUDE.md — data_tree_drone_video

Příručka projektu pro Claude Code. Čti celé před prací; respektuj omezení prostředí níže.

## Co projekt dělá

Z **videa obletu stromu z DJI dronu** (DJI Neo) vyrobit **mračno bodů ve formátu `.las`**
(stejný formát jako lidar) přes **fotogrammetrii (SfM + MVS)** a prohlížet/analyzovat ho
ve vlastním **Qt C++ vieweru** a v **3D Forest**.

Pipeline:
```
video.MP4 + .SRT(telemetrie)
  → ffmpeg: snímky          (odm_project/images/frame_*.jpg)
  → make_geo.py: geo.txt    (GPS na snímek, z .SRT)
  → OpenDroneMap (Docker)   → odm_project/odm_georeferencing/odm_georeferenced_model.laz
  → laspy: .laz → .las      (odm_project/strom.las)
  → viewer/ (Qt5) nebo 3D Forest
```

Kontext: učební/pohovorový projekt v `cpp_pohovor/` (C++). Důraz na to, aby věci šly
spustit i přes vzdálené X11.

---

## ⚠️ Omezení prostředí (DŮLEŽITÉ — tohle určuje rozhodnutí)

- **Stroj:** Dell OptiPlex, 12 jader, 32 GB RAM, **Intel UHD 630 — žádná NVIDIA/CUDA**, **slabé chlazení**.
  Záložní stroj: i7, 16 GB RAM, lépe chlazený. K dispozici i Google Colab (T4/A100).
- **Přístup přes SSH z Windows (MobaXterm).** Lokální plocha je Wayland.
  → **GUI musí jet přes X11 forwarding.** Moderní OpenGL (core profile/shadery) přes
  indirect-GLX **selže** → 3D náhledy psát **softwarově (QPainter), bez OpenGL/GLX**.
- **Bez NVIDIA:** COLMAP/Meshroom husté MVS (CUDA) lokálně nejde.
  → fotogrammetrie přes **OpenDroneMap (CPU, Docker)**; GPU varianta jen na Colabu (viz `strom_colab.ipynb`).
- **`sudo` chce heslo** → neinstaluj systémové balíky bez domluvy. **ffmpeg** je statický ve `venv`.
- **Python má PEP 668** → instaluj jen do **`.venv/`**, nikdy globálně (`pip install --break-system-packages` nepoužívat).
- **Docker funguje bez sudo.** Běží i cizí kontejnery (`3d-forest-build`, vlastní služby) — **nevypínej je**.
- **Dlouhé CPU joby = riziko přehřátí.** Vždy omez jádra a **hlídej teplotu** (viz níže).

---

## Struktura

```
DJI_*.MP4 / DJI_*.SRT      vstup: 4K/60fps video (~19 s) + telemetrie po snímcích (GPS, alt, expozice)
make_geo.py                generuje geo.txt z .SRT (lon lat alt na snímek)
POSTUP.md                  kompletní postup krok za krokem + stav
strom_colab.ipynb          GPU varianta (COLMAP) na Google Colab
.venv/                     statický ffmpeg + laspy/pyproj/matplotlib (NEcommitovat)
3d-forest/                 klon https://github.com/VUKOZ-OEL/3d-forest (čte .las/.pcd)
odm_project/
  images/                  snímky (frame_*.jpg) + geo.txt
  odm_georeferencing/odm_georeferenced_model.laz   výstup ODM (georef, metry)
  strom.las                finální mračno pro 3D Forest/viewer
viewer/                    vlastní Qt5 prohlížeč mračna (viz viewer/README.md)
```

---

## Časté úkoly a přesné příkazy

Pracovní adresář: `/home/ivand/projects/learning_cpp/cpp_pohovor/data_tree_drone_video`

### ffmpeg (statický, bez sudo)
```bash
python3 -m venv .venv && .venv/bin/pip install imageio-ffmpeg
FF=$(.venv/bin/python -c "import imageio_ffmpeg,sys; sys.stdout.write(imageio_ffmpeg.get_ffmpeg_exe())")
```

### Extrakce snímků (krátké, omez vlákna)
```bash
"$FF" -hide_banner -loglevel error -threads 2 -i DJI_*.MP4 \
  -vf "select=not(mod(n\,12)),scale=2560:1440" -vsync 0 -q:v 3 \
  -y odm_project/images/frame_%04d.jpg
```
Mapování na telemetrii: výstup `frame_<k>` ↔ `FrameCnt = 12*(k-1)+1`. Pak `.venv/bin/python make_geo.py`.

### ODM rekonstrukce (TEPELNĚ NÁROČNÉ — pусť na pozadí + watchdog)
```bash
docker pull opendronemap/odm
docker run --rm -v "$PWD":/datasets opendronemap/odm \
  --project-path /datasets odm_project \
  --geo /datasets/odm_project/images/geo.txt \
  --feature-quality medium --pc-quality medium --max-concurrency 4
```
- `--max-concurrency` = hlavní páka na teplotu (4 bezpečné). `high` kvalita radši na chlazeném i7.
- Pozn.: OpenMVS si v špičce vezme víc jader než `--max-concurrency` (má vlastní threading).

### Teplotní watchdog (povinné u dlouhých CPU jobů)
```bash
# x86_pkg_temp = teplota CPU package; zabij kontejner při >90 °C
while cid=$(docker ps -q --filter ancestor=opendronemap/odm); [ -n "$cid" ]; do
  t=$(($(cat /sys/class/thermal/thermal_zone3/temp)/1000)); echo "${t}C"
  [ "$t" -gt 90 ] && { docker stop "$cid"; break; }; sleep 15
done
```

### .laz → .las (3D Forest čte jen .las/.pcd, ne .laz)
```bash
.venv/bin/pip install "laspy[lazrs]"
.venv/bin/python -c "import laspy; las=laspy.read('odm_project/odm_georeferencing/odm_georeferenced_model.laz'); las.write('odm_project/strom.las')"
```

### Viewer (Qt5, softwarový) — build a běh
```bash
cd viewer && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"$(nproc)"
./run.sh tree.ply         # přes MobaXterm s X11-Forwarding; ověř echo $DISPLAY
# headless render do PNG (bez okna):
QT_QPA_PLATFORM=offscreen ./build/viewer tree.ply --snapshot nahled.png 1100 800
```

---

## Konvence a poznámky pro Claude

- **GUI vždy softwarově (QPainter), ne OpenGL.** Vzor: `../cfd_simple_cube_qt/DomainView3D`.
  Hotová implementace: `viewer/src/PointCloudView.cpp` (z-buffer, Blender-styl gizmo, persp/ortho,
  řezací roviny). CMake linkuje jen `Qt5::Widgets`.
- **PLY pro viewer:** `binary_little_endian`, `float x,y,z` + `uchar red,green,blue`,
  souřadnice **vycentrované** (offset v hlavičce jako komentář) — jinak float32 ztratí přesnost
  na UTM souřadnicích (~623000, ~5.4e6).
- **Komentáře a UI texty česky** (jak je zvykem v `cpp_pohovor/`). C++17, CMake, Qt5.
- **Měřítko mračna:** georeferencováno z GPS dronu (UTM 33N, metry), ale GPS je hrubé →
  absolutní rozměry **orientační (±desítky %)**. Kontrola: panelák ~12–15 m, cílový strom ~3–4 m.
- **Headless test čehokoli Qt:** `QT_QPA_PLATFORM=offscreen` + `--snapshot` (PNG si pak přečti).
- Náhledy/analýzy mračna lokálně: `.venv` má `laspy`, `pyproj` (UTM), `matplotlib`.
- **Necommitovat** `.venv/`, `odm_project/` (velké), `viewer/build/`, `3d-forest/`.

## TODO / nápady
- viewer: přímé čtení `.las`, obarvení podle výšky, měření vzdáleností, octree/LOD
- 3D Forest: segmentace stromu a výpočet atributů na `strom.las`
- lepší mračno: víc snímků / `high` kvalita na i7; pro přesné měřítko GCP / referenční objekt
```
