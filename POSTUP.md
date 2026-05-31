# Převod videa stromu z DJI dronu → mračno bodů (.las) → 3D Forest

Postup, jak z videa obletu stromu (DJI Neo) vyrobit **mračno bodů ve formátu `.las`**
(stejný formát, jaký používá lidar) a načíst ho do **3D Forest**.

---

## 0. Pojmy

- **LiDAR** = laserový skener; měří vzdálenost časem návratu pulzu. Výstup = **mračno bodů**
  (X, Y, Z + intenzita, klasifikace…). Standardní formát: **`.las`** / `.laz`.
- **3D Forest** (VUKOZ) = software pro analýzu lidarových dat z lesa.
  Čte **`.las`** a **`.pcd`**, exportuje **`.las`** a `.csv`.
  Nástroje: klasifikace země, segmentace stromů, výška nad zemí, atributy stromu (DBH…).
- **Naše situace:** dron má jen RGB kameru, žádný laser → "lidar" nevyrobíme doslova,
  ale stejný typ dat (mračno bodů) uděláme **fotogrammetrií (SfM + MVS)**.

## Pipeline

```
video.MP4  →  snímky (ffmpeg)  →  geo.txt (z telemetrie .SRT)
           →  ODM: SfM + husté mračno  →  .laz/.las  →  3D Forest
```

---

## Hardware a omezení

- OptiPlex: **12 jader, 31 GB RAM, Intel UHD 630 (žádná NVIDIA/CUDA)** → slabé chlazení.
- Bez NVIDIA → COLMAP/Meshroom husté MVS odpadá; **OpenDroneMap (ODM) počítá na CPU** → vhovuje.
- Tepelně riziko = **jen krok ODM** (dlouhý 100% load). Řešení: méně jader + střední kvalita + hlídání teploty.

## Vstupní data

| Soubor | Popis |
|---|---|
| `DJI_20260529123804_0130_D.MP4` | 4K (3840×2160), HEVC, 59.94 fps, 18.87 s, ~1131 framů |
| `DJI_20260529123804_0130_D.SRT` | telemetrie po snímcích: `latitude`, `longitude`, `rel_alt`, `abs_alt`, iso/shutter/fnum |

**Pozorování ze snímků:** dron scénu opravdu obletěl (vícepohledové pokrytí). Cíl je
**mladý řídký stromek s tenkými větvemi** (pro fotogrammetrii nejtěžší → čekej šum/díry).
Scéna je široká (budovy, keře) — ODM zrekonstruuje celou scénu, stromek pak vyřízneme v 3D Forestu.

**⚠️ Měřítko:** GPS se za celý oblet mění jen o ~1–2 m (≈ úroveň šumu GPS).
Tvar mračna bude OK, ale **absolutní měřítko z GPS jen orientační (±desítky %)**.
Pro přesnou výšku by chtělo referenční předmět známé velikosti ve scéně.

---

## Krok 0 — ffmpeg (HOTOVO ✅)

Systém má chráněný Python (PEP 668) a `sudo` chce heslo → statický ffmpeg ve **venv**, nic do systému.

```bash
cd /home/ivand/projects/learning_cpp/cpp_pohovor/data_tree_drone_video
python3 -m venv .venv
.venv/bin/pip install imageio-ffmpeg
# cesta k binárce:
FF=$(.venv/bin/python -c "import imageio_ffmpeg,sys; sys.stdout.write(imageio_ffmpeg.get_ffmpeg_exe())")
```

## Krok 1 — extrakce snímků (HOTOVO ✅)

Každý 12. frame → ~5 sn./s → **95 snímků** ve 4K (`odm_project/images/`, ~156 MB).
Omezeno na 2 vlákna (krátká, chladná operace).

```bash
mkdir -p odm_project/images
"$FF" -hide_banner -loglevel error -threads 2 \
  -i DJI_20260529123804_0130_D.MP4 \
  -vf "select=not(mod(n\,12))" -vsync 0 -q:v 2 \
  odm_project/images/frame_%04d.jpg
```

Mapování výstupu na telemetrii: výstup `frame_%04d` číslo `k` ↔ zdrojový frame ↔ `FrameCnt = 12·(k−1)+1`.

## Krok 1b — geo.txt z telemetrie (HOTOVO ✅)

Skript `make_geo.py` přečte `.SRT` a vyrobí `odm_project/images/geo.txt`
(formát ODM: `EPSG:4326` + `jméno lon lat alt`). Všech 95 snímků dostalo GPS.

```bash
.venv/bin/python make_geo.py
# -> odm_project/images/geo.txt
```

## Krok 2 — ODM image (Docker)

```bash
docker pull opendronemap/odm     # ~3–4 GB, jen download, žádné teplo
```

## Krok 3 — rekonstrukce mračna (TEPELNĚ NÁROČNÉ ⚠️ — ZBÝVÁ)

```bash
docker run --rm \
  -v /home/ivand/projects/learning_cpp/cpp_pohovor/data_tree_drone_video:/datasets \
  opendronemap/odm \
  --project-path /datasets odm_project \
  --geo /datasets/odm_project/images/geo.txt \
  --feature-quality medium \
  --pc-quality medium \
  --max-concurrency 4 \
  --no-gpu
```

- `--max-concurrency` = hlavní páka na teplotu (**4 = bezpečné**, 6 = kompromis, 8 = rychlejší a teplejší).
- `medium` kvalita = méně tepla, pro stromek stačí.
- Odhad ~30–90 min, většina tepla ve fázi MVS (husté mračno).

**Hlídání teploty během běhu** (paralelně, abort při ~90 °C):

```bash
watch -n 5 'for z in /sys/class/thermal/thermal_zone*/temp; do \
  echo "$(cat ${z%temp}type): $(($(cat $z)/1000)) C"; done'
```

## Krok 4 — výstup .las

ODM uloží mračno do:
```
odm_project/odm_georeferencing/odm_georeferenced_model.laz
```
Případný převod `.laz → .las` (lehké). 3D Forest čte `.las` (i `.pcd`).

## Krok 5 — 3D Forest

Načíst `.las`, vyříznout okolí stromku, vyzkoušet klasifikaci země a segmentaci stromu.
3D Forest už běží v Dockeru (`3d-forest-build:latest`).

---

## Stav

- [x] Krok 0 — ffmpeg (venv)
- [x] Krok 1 — extrakce 95 snímků (zmenšeno na 2560×1440)
- [x] Krok 1b — geo.txt z telemetrie
- [x] Krok 2 — `docker pull opendronemap/odm`
- [x] Krok 3 — ODM rekonstrukce (4 jádra, medium, watchdog hlídal teplotu — max 73 °C)
- [x] Krok 4 — `.laz` → `odm_project/strom.las` (1,33 M bodů, LAS 1.4, RGB, UTM 33N, metry)
- [ ] Krok 5 — 3D Forest (segmentace stromu)
- [x] Bonus — vlastní Qt5 viewer (`viewer/`, softwarový QPainter, SSH/MobaXterm-friendly)

## Výsledek

- **`odm_project/strom.las`** — 1 332 526 bodů, barevné, georeferencované (WGS84/UTM 33N, v metrech),
  rozsah scény ~50,7 × 36,5 × 18,1 m.
- Náhledy: `odm_project/nahled2.png` (shora + z boku, barvy + výška).
- Kontrola měřítka: panelák ~12–15 m → reálný stromek 3–4 m do scény zapadá (georeference OK).
- Běh ODM hlídán `/tmp/odm_watchdog.sh` + `/tmp/odm_mon2.sh` (log `/tmp/odm_monitor.log`).
