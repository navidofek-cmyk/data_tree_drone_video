#!/usr/bin/env bash
# Spustí point cloud viewer. Funguje lokálně i přes ssh -X / MobaXterm.
#
# Použití:
#   ./run.sh                  # cloud.ply
#   ./run.sh strom.ply        # jiný PLY
#   ./run.sh --max 300000     # podvzorkovat (pomalé spojení)
#   ./run.sh cloud.ply --max 300000
set -eo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

# ── rozdělení argumentů: [ply] [extra...] ──────────────────────────────
ARGS=("$@")
PLY="cloud.ply"
if [ "${#ARGS[@]}" -gt 0 ] && [[ "${ARGS[0]}" != --* ]]; then
  PLY="${ARGS[0]}"; ARGS=("${ARGS[@]:1}")
fi
if [ ! -f "$PLY" ]; then
  echo "PLY nenalezen: $PLY (jsi ve složce viewer/?)"; exit 1
fi

# ── build, pokud chybí ─────────────────────────────────────────────────
if [ ! -x build/viewer ]; then
  echo "» build vieweru…"
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build build -j"$(nproc)" >/dev/null
fi

# ── kontrola displeje ──────────────────────────────────────────────────
if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
  cat <<'MSG'
✗ Není nastavený DISPLAY ani WAYLAND_DISPLAY — okno nemá kam vyskočit.

Možnosti:
  • MobaXterm: v Session → Advanced SSH zapni „X11-Forwarding"
    (a v MobaXterm musí běžet X server – ikona „X" vpravo nahoře svítí),
    pak se znovu připoj. Po přihlášení bývá DISPLAY = localhost:10.0.
    Ověř:  echo $DISPLAY
  • z příkazové řádky:  ssh -X ivand@<stroj>
  • na monitoru OptiPlexu (Wayland/XWayland):  DISPLAY=:0 ./run.sh

Pro headless náhled bez okna:
  QT_QPA_PLATFORM=offscreen ./build/viewer "$PLY" --snapshot nahled.png
MSG
  exit 1
fi

echo "» DISPLAY=${DISPLAY:-(wayland)}  PLY=$PLY  ${ARGS[*]}"
exec ./build/viewer "$PLY" "${ARGS[@]}"
