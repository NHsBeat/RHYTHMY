#!/bin/bash
# RHYTHMY — Build self-extracting installers for R36S
#
# Run this on Linux (or WSL) after cross-compiling:
#
#   Step 1:  bash build_linux.sh cross
#   Step 2:  bash packaging/make_installer.sh
#
# Result:
#   Install_RHYTHMY_SD1.sh  — audio on system card (SD1)
#   Install_RHYTHMY_SD2.sh  — audio on second card (SD2)
#
# Upload BOTH files to your website. User downloads the one they need.

set -e

VERSION="1.0"
BINARY="build_out/rhythmy"   # aarch64 binary produced by build_bootlin.sh
STAGING="_staging_tmp"

# ── Checks ────────────────────────────────────────────────────────────────────
if [ ! -f "$BINARY" ]; then
    echo "ERROR: $BINARY not found."
    echo ""
    echo "Cross-compile first:"
    echo "  bash build_linux.sh cross"
    exit 1
fi

command -v tar >/dev/null 2>&1 || { echo "ERROR: 'tar' not found."; exit 1; }

echo "Building self-extracting installers for RHYTHMY v${VERSION}..."
echo "  Binary: $BINARY ($(wc -c < "$BINARY") bytes)"
echo ""

# ── Prepare payload: stripped binary + controller images ──────────────────────
mkdir -p "$STAGING"
cp "$BINARY" "$STAGING/rhythmy"
aarch64-linux-gnu-strip "$STAGING/rhythmy" 2>/dev/null \
    || "$HOME/toolchains/aarch64--glibc--stable-2020.02-2/bin/aarch64-linux-strip" "$STAGING/rhythmy" 2>/dev/null \
    || echo "  (already stripped or strip unavailable)"
chmod +x "$STAGING/rhythmy"

# Bundle the controller images so the Controls screen shows them on-device
if [ -d assets ]; then
    mkdir -p "$STAGING/assets"
    cp assets/*.png "$STAGING/assets/" 2>/dev/null || true
fi

# Create the payload (same for both installers)
PAYLOAD="_payload.tar.gz"
tar czf "$PAYLOAD" -C "$STAGING" .
rm -rf "$STAGING"

# ── Build one installer ───────────────────────────────────────────────────────
build_installer() {
    local TARGET="$1"   # "sd1" or "sd2"
    local OUT="Install_RHYTHMY_${TARGET^^}.sh"

    sed "s/RHYTHMY_AUDIO_TARGET_PLACEHOLDER/$TARGET/" \
        packaging/installer_header.sh > "$OUT"
    cat "$PAYLOAD" >> "$OUT"
    chmod +x "$OUT"

    local BYTES
    BYTES=$(wc -c < "$OUT")
    echo "  $OUT  ($(( BYTES / 1024 )) KB)"
}

build_installer "sd1"
build_installer "sd2"

rm -f "$PAYLOAD"

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
echo "Done. Upload both files to your website:"
echo ""
echo "  Install_RHYTHMY_SD1.sh"
echo "    → Audio stored on the system SD card"
echo "    → For users with one SD card, or who prefer system card"
echo ""
echo "  Install_RHYTHMY_SD2.sh"
echo "    → Audio stored on the second SD card (/roms2)"
echo "    → Recommended for users with two SD cards"
echo ""
echo "How users install:"
echo "  1. Download the file that matches their setup"
echo "  2. Copy to /roms/ports/ on the SD card"
echo "  3. Run it from the Ports menu (press A)"
echo "  4. Wait ~5 seconds — done"
echo "  5. Reboot EmulationStation — RHYTHMY appears in Ports"
