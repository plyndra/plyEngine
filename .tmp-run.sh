set -e
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq xvfb libgl1-mesa-dri libglx-mesa0 fonts-dejavu-core >/dev/null 2>&1
cd /src && python3 -m pip install -q cmake 2>/dev/null || true
cmake -B /tmp/b -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1
cmake --build /tmp/b -j"$(nproc)" --target ply_tutorial ply_companion >/dev/null 2>&1
export LIBGL_ALWAYS_SOFTWARE=1
xvfb-run -a /tmp/b/ply_tutorial --verify | tail -2
xvfb-run -a /tmp/b/ply_companion --verify --no-mdns --web /src/companion/dist | grep -E "outcome|verify"
