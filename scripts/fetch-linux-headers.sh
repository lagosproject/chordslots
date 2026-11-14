#!/usr/bin/env bash
# Fallback for Debian/Ubuntu machines where you cannot `sudo apt install` the
# JUCE build dependencies. Downloads the -dev packages (no root needed) and
# unpacks just their headers into third_party/sysroot, which CMakeLists.txt
# picks up automatically.
#
# If you do have root, skip this script and run instead:
#   sudo apt install libasound2-dev libx11-dev libxext-dev libxrandr-dev \
#        libxinerama-dev libxcursor-dev libxrender-dev libxcomposite-dev libxi-dev \
#        libfreetype-dev libfontconfig1-dev
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
sysroot="$root/third_party/sysroot"
mkdir -p "$sysroot/debs" "$sysroot/pkgconfig"

cd "$sysroot/debs"
apt download libxrandr-dev libxinerama-dev libxcursor-dev libxrender-dev \
    libxcomposite-dev libxfixes-dev libxi-dev libasound2-dev
for deb in ./*.deb; do
    dpkg-deb -x "$deb" "$sysroot"
done

# The runtime libraries are already installed system-wide; only the unversioned
# linker name for ALSA is missing (the X11 extensions are dlopen'ed by JUCE).
multiarch="$(gcc -print-multiarch)"
syslib="/usr/lib/$multiarch"
mkdir -p "$sysroot/lib"
ln -sf "$syslib/libasound.so.2" "$sysroot/lib/libasound.so"

cat > "$sysroot/pkgconfig/alsa.pc" <<EOF
Name: alsa
Description: ALSA (headers from the local sysroot, library from the system)
Version: 1.2
Libs: -L$sysroot/lib -lasound
Cflags: -I$sysroot/usr/include
EOF

echo "Headers unpacked into $sysroot"
