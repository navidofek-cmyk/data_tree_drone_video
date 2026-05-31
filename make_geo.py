import re, glob, os
srt = open("DJI_20260529123804_0130_D.SRT", encoding="utf-8", errors="ignore").read()
# map FrameCnt -> (lat, lon, abs_alt)
data = {}
for blk in re.split(r"\n\s*\n", srt):
    fc = re.search(r"FrameCnt:\s*(\d+)", blk)
    lat = re.search(r"latitude:\s*([-\d.]+)", blk)
    lon = re.search(r"longitude:\s*([-\d.]+)", blk)
    alt = re.search(r"abs_alt:\s*([-\d.]+)", blk)
    if fc and lat and lon and alt:
        data[int(fc.group(1))] = (float(lat.group(1)), float(lon.group(1)), float(alt.group(1)))
imgs = sorted(glob.glob("odm_project/images/frame_*.jpg"))
lines = ["EPSG:4326"]
missing = 0
for img in imgs:
    k = int(re.search(r"frame_(\d+)", img).group(1))
    framecnt = 12*(k-1) + 1          # vystup #k -> zdrojovy frame -> SRT FrameCnt
    if framecnt not in data:
        missing += 1; continue
    lat, lon, alt = data[framecnt]
    lines.append(f"{os.path.basename(img)} {lon:.8f} {lat:.8f} {alt:.3f}")
open("odm_project/images/geo.txt","w").write("\n".join(lines)+"\n")
print(f"Zaznamu SRT: {len(data)}, snimku: {len(imgs)}, bez GPS: {missing}")
print("geo.txt ma radku (vc. hlavicky):", len(lines))
