#!/bin/bash
# RHYTHMY v1.0 - Portable Music Workstation
# Self-extracting installer for R36S / ArkOS / ArcOS
# Copyright (C) 2026 Haz. All rights reserved.
#
# USAGE: Place this file in /roms/ports/ and run it from the Ports menu.
#        It will install RHYTHMY and then remove itself.
#
# RHYTHMY_AUDIO_TARGET is set by make_installer.sh:
#   "sd1" → audio in the game folder (system card)
#   "sd2" → audio on the second SD card (/roms2 or similar)

SELF="$(readlink -f "$0")"
PORTS_DIR="$(dirname "$SELF")"
INSTALL_DIR="$PORTS_DIR/RHYTHMY"
LAUNCHER="$PORTS_DIR/RHYTHMY.sh"
AUDIO_TARGET="RHYTHMY_AUDIO_TARGET_PLACEHOLDER"

clear
echo "================================"
echo "  RHYTHMY v1.0"
echo "  Portable Music Workstation"
echo "================================"
echo ""

# ── Resolve audio base path ───────────────────────────────────────────────────
if [ "$AUDIO_TARGET" = "sd2" ]; then
    # Find second SD card
    SD2_PATH=""
    for CANDIDATE in /roms2 /mnt/mmc /storage/roms2 /run/media/mmcblk1p1; do
        if [ -d "$CANDIDATE" ] && [ -w "$CANDIDATE" ]; then
            SD2_PATH="$CANDIDATE"
            break
        fi
    done

    if [ -z "$SD2_PATH" ]; then
        echo "ERROR: Second SD card not found."
        echo "Make sure it is inserted and try again."
        echo "Or use the SD1 version of the installer."
        sleep 5
        exit 1
    fi

    AUDIO_BASE="$SD2_PATH/RHYTHMY/Audio"
    echo "Audio storage: SD Card 2  ($SD2_PATH)"
else
    AUDIO_BASE="$INSTALL_DIR/Audio"
    echo "Audio storage: SD Card 1  (system card)"
fi

echo "Installing to: $INSTALL_DIR"
echo ""

# ── Verify embedded archive ───────────────────────────────────────────────────
SKIP=$(awk '/^__ARCHIVE__$/{print NR+1; exit}' "$SELF")
if [ -z "$SKIP" ]; then
    echo "ERROR: Archive missing — re-download the installer."
    sleep 5
    exit 1
fi

# ── Create all folders ────────────────────────────────────────────────────────
mkdir -p "$INSTALL_DIR"
mkdir -p "$AUDIO_BASE/PUT YOUR SAMPLES HERE"
mkdir -p "$AUDIO_BASE/wavs"
mkdir -p "$AUDIO_BASE/wavs/bounced"
mkdir -p "$INSTALL_DIR/projects"

# Write audio path so the app finds the right folder at runtime
echo "$AUDIO_BASE" > "$INSTALL_DIR/audio_dir.cfg"

# ── Extract binary ────────────────────────────────────────────────────────────
echo "Extracting files..."
tail -n +"$SKIP" "$SELF" | tar xzf - -C "$INSTALL_DIR"
RESULT=$?

if [ $RESULT -ne 0 ]; then
    echo ""
    echo "ERROR: Extraction failed (code $RESULT)."
    echo "The installer may be corrupted — re-download it."
    sleep 5
    exit 1
fi

chmod +x "$INSTALL_DIR/rhythmy"

# ── Create permanent launcher ─────────────────────────────────────────────────
cat > "$LAUNCHER" << 'LAUNCHER_EOF'
#!/bin/bash
# RHYTHMY v1.0 - Portable Music Workstation
# Copyright (C) 2026 Haz. All rights reserved.
GAMEDIR="$(dirname "$(readlink -f "$0")")/RHYTHMY"
cd "$GAMEDIR"
export HOME="$GAMEDIR"

# Use the device's gamepad mapping if one is present
for f in /opt/inttools/gamecontrollerdb.txt \
         /storage/.config/SDL-GameControllerDB/gamecontrollerdb.txt \
         /usr/share/SDL2/gamecontrollerdb.txt; do
    [ -f "$f" ] && export SDL_GAMECONTROLLERCONFIG_FILE="$f" && break
done

export SDL_AUDIODRIVER=alsa
export SDL_VIDEODRIVER=kmsdrm
./rhythmy > "$GAMEDIR/run.log" 2>&1
LAUNCHER_EOF
chmod +x "$LAUNCHER"

# ── Create uninstaller (keeps audio + projects) ───────────────────────────────
cat > "$PORTS_DIR/Uninstall_RHYTHMY.sh" << 'UNINSTALL_EOF'
#!/bin/bash
# RHYTHMY — Uninstaller (keeps your audio files and projects)
SELF="$(readlink -f "$0")"
PORTS_DIR="$(dirname "$SELF")"
INSTALL_DIR="$PORTS_DIR/RHYTHMY"

clear
echo "================================"
echo "  RHYTHMY - Uninstall"
echo "  (audio files will be kept)"
echo "================================"
echo ""

# Remove game binary and config
rm -f "$INSTALL_DIR/rhythmy"
rm -f "$INSTALL_DIR/audio_dir.cfg"

# Remove launcher and both uninstallers
rm -f "$PORTS_DIR/RHYTHMY.sh"
rm -f "$PORTS_DIR/Uninstall_RHYTHMY_Full.sh"

echo "RHYTHMY removed."
echo ""
echo "Your files are still here:"
echo "  $INSTALL_DIR/Audio/"
echo "  $INSTALL_DIR/projects/"
echo ""
echo "You can delete them manually if you want."
sleep 4

rm -- "$SELF"
exit 0
UNINSTALL_EOF
chmod +x "$PORTS_DIR/Uninstall_RHYTHMY.sh"

# ── Create full uninstaller (removes everything) ──────────────────────────────
cat > "$PORTS_DIR/Uninstall_RHYTHMY_Full.sh" << 'FULL_EOF'
#!/bin/bash
# RHYTHMY — Full Uninstaller (removes EVERYTHING including audio and projects)
SELF="$(readlink -f "$0")"
PORTS_DIR="$(dirname "$SELF")"
INSTALL_DIR="$PORTS_DIR/RHYTHMY"

clear
echo "================================"
echo "  RHYTHMY - Full Uninstall"
echo "  ALL files will be deleted!"
echo "================================"
echo ""
echo "Removing in 5 seconds..."
sleep 5

# Read audio location from config before deleting it
AUDIO_BASE=""
if [ -f "$INSTALL_DIR/audio_dir.cfg" ]; then
    AUDIO_BASE=$(cat "$INSTALL_DIR/audio_dir.cfg")
fi

# Remove entire game folder
rm -rf "$INSTALL_DIR"

# Remove audio if it was stored outside the game folder (SD2)
if [ -n "$AUDIO_BASE" ] && [ "$AUDIO_BASE" != "$INSTALL_DIR/Audio" ]; then
    rm -rf "$AUDIO_BASE"
    # Remove parent RHYTHMY folder on SD2 if now empty
    rmdir "$(dirname "$AUDIO_BASE")" 2>/dev/null || true
fi

# Remove launcher and soft uninstaller
rm -f "$PORTS_DIR/RHYTHMY.sh"
rm -f "$PORTS_DIR/Uninstall_RHYTHMY.sh"

echo ""
echo "RHYTHMY fully removed."
sleep 3

rm -- "$SELF"
exit 0
FULL_EOF
chmod +x "$PORTS_DIR/Uninstall_RHYTHMY_Full.sh"

# ── Done ──────────────────────────────────────────────────────────────────────
echo ""
echo "Installation complete!"
echo ""
echo "  Samples:  $AUDIO_BASE/PUT YOUR SAMPLES HERE/"
echo "  Renders:  $AUDIO_BASE/wavs/"
echo "  Bounces:  $AUDIO_BASE/wavs/bounced/"
echo "  Projects: $INSTALL_DIR/projects/"
echo ""
echo "Removing installer..."
sleep 3

rm -- "$SELF"
exit 0

# Everything below this line is binary data — do not edit
__ARCHIVE__
