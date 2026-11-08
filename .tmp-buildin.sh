set -e
apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
  build-essential cmake git python3 python3-jinja2 \
  libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev \
  libxi-dev libxkbcommon-dev libwayland-dev libdecor-0-dev \
  libgl1-mesa-dev libgles2-mesa-dev libxss-dev libxfixes-dev \
  libxpresent-dev libxshmfence-dev libxtst-dev xvfb fonts-dejavu-core \
  libssl-dev > /dev/null
python3 -m pip install -q --upgrade pip cmake 2>/dev/null || pip3 install -q cmake
gcc --version | head -1; cmake --version | head -1; ldd --version | head -1
cd /src
cmake -B /tmp/b -DCMAKE_BUILD_TYPE=Release > /tmp/cfg.log 2>&1 || { tail -30 /tmp/cfg.log; exit 1; }
cmake --build /tmp/b -j"$(nproc)" --target ply_tutorial ply_companion ply_physics > /tmp/build.log 2>&1 || { grep -iE "error" /tmp/build.log | head -20; exit 1; }
ldd /tmp/b/ply_tutorial | head -3
objdump -T /tmp/b/ply_tutorial | grep -oE "GLIBC_[0-9.]+" | sort -uV | tail -2
