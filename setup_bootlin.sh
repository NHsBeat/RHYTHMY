#!/bin/bash
# Download + extract the Bootlin aarch64 toolchain (GCC 8.4, glibc 2.30).
# This bundles a matching glibc/libstdc++/crt so the binary needs GLIBC <= 2.30.
set -e

TCDIR="$HOME/toolchains"
NAME="aarch64--glibc--stable-2020.02-2"
URL="https://toolchains.bootlin.com/downloads/releases/toolchains/aarch64/tarballs/${NAME}.tar.bz2"

mkdir -p "$TCDIR"
cd "$TCDIR"

if [ ! -d "$TCDIR/$NAME" ]; then
    echo "=== Downloading Bootlin toolchain (~150 MB) ==="
    wget -O tc.tar.bz2 "$URL"
    echo "=== Extracting ==="
    tar xjf tc.tar.bz2
    rm -f tc.tar.bz2
else
    echo "=== Toolchain already present ==="
fi

echo ""
echo "=== Toolchain layout ==="
echo "ROOT:    $TCDIR/$NAME"
echo "--- compilers in bin/ ---"
ls "$TCDIR/$NAME/bin/" | grep -E 'gcc$|g\+\+$|strip$'
echo "--- sysroot ---"
find "$TCDIR/$NAME" -maxdepth 2 -name sysroot -type d
echo ""
echo "=== gcc version ==="
"$TCDIR/$NAME/bin/aarch64-linux-gcc" --version 2>/dev/null | head -1 || \
"$TCDIR/$NAME/bin/aarch64-buildroot-linux-gnu-gcc" --version 2>/dev/null | head -1 || \
ls "$TCDIR/$NAME/bin/" | grep -E 'gcc$'
echo "DONE"
