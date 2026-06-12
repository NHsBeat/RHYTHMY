#!/bin/bash
LOG="/roms/ports/RHYTHMY/launch.log"

# Write marker immediately + force flush to FAT32
echo "DBG3_STARTED $(date)" > "$LOG"
sync

printf "\n=== RHYTHMY DEBUG 3 ===\n" >> /dev/tty1 2>/dev/null
printf "Checking SDL2...\n" >> /dev/tty1 2>/dev/null

echo "--- whoami ---" >> "$LOG"
whoami >> "$LOG" 2>&1
sync

echo "--- binary ---" >> "$LOG"
ls -la /roms/ports/RHYTHMY/rhythmy >> "$LOG" 2>&1
sync

echo "--- ldd ---" >> "$LOG"
ldd /roms/ports/RHYTHMY/rhythmy >> "$LOG" 2>&1
sync

echo "--- SDL2 search ---" >> "$LOG"
find /usr /lib /opt -name "libSDL2*.so*" 2>/dev/null >> "$LOG"
sync

printf "Launching...\n" >> /dev/tty1 2>/dev/null
echo "--- launch kmsdrm ---" >> "$LOG"
export SDL_AUDIODRIVER=alsa
export SDL_VIDEODRIVER=kmsdrm
/roms/ports/RHYTHMY/rhythmy >> "$LOG" 2>&1
echo "exit_kmsdrm: $?" >> "$LOG"
sync

printf "kmsdrm done, trying fbdev...\n" >> /dev/tty1 2>/dev/null
echo "--- launch fbdev ---" >> "$LOG"
export SDL_VIDEODRIVER=fbdev
/roms/ports/RHYTHMY/rhythmy >> "$LOG" 2>&1
echo "exit_fbdev: $?" >> "$LOG"
sync

printf "Done. Check launch.log\n" >> /dev/tty1 2>/dev/null
sleep 2
