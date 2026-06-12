#!/bin/bash
LOG="/roms/ports/RHYTHMY/launch.log"
echo "step1_started" > "$LOG"
echo "path: $0" >> "$LOG"
echo "user: $(whoami)" >> "$LOG"

cd /roms/ports/RHYTHMY
echo "step2_cd_done" >> "$LOG"

echo "--- ldd ---" >> "$LOG"
ldd ./rhythmy >> "$LOG" 2>&1

echo "--- SDL2 on device ---" >> "$LOG"
find /usr /lib /opt -name "libSDL2*" 2>/dev/null >> "$LOG"

echo "--- launching ---" >> "$LOG"
export SDL_AUDIODRIVER=alsa
export SDL_VIDEODRIVER=kmsdrm
./rhythmy >> "$LOG" 2>&1
echo "exit_code: $?" >> "$LOG"
