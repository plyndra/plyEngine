// Headless spritesheet-parser tests: no GL context required.

#include "plyEngine/Spritesheet.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "  FAIL: %s\n", what);
        ++failures;
    }
}

void checkRect(const ply::NamedRect& r, const char* name, int x, int y, int w, int h,
               const char* what) {
    if (r.name != name || r.x != x || r.y != y || r.w != w || r.h != h) {
        std::fprintf(stderr,
                     "  FAIL: %s (got '%s' %d,%d %dx%d; expected '%s' %d,%d %dx%d)\n",
                     what, r.name.c_str(), r.x, r.y, r.w, r.h, name, x, y, w, h);
        ++failures;
    }
}

const char* kGenericJson = R"({
  "frames": [
    { "name": "idle_0", "x": 0,   "y": 0,  "w": 32, "h": 32 },
    { "name": "idle_1", "x": 32,  "y": 0,  "w": 32, "h": 32 },
    { "name": "run_0",  "x": 0,   "y": 32, "w": 48, "h": 32 }
  ]
})";

const char* kAsepriteHash = R"({
  "frames": {
    "walk 0.aseprite": {
      "frame": { "x": 0, "y": 0, "w": 16, "h": 16 },
      "rotated": false, "trimmed": true,
      "duration": 100
    },
    "walk 1.aseprite": {
      "frame": { "x": 16, "y": 0, "w": 16, "h": 16 },
      "rotated": false, "trimmed": false,
      "duration": 100
    }
  },
  "meta": {
    "frameTags": [ { "name": "walk", "from": 0, "to": 1 } ]
  }
})";

const char* kAsepriteArray = R"({
  "frames": [
    { "filename": "idle 0", "frame": { "x": 0, "y": 0, "w": 8, "h": 8 }, "rotated": true },
    { "filename": "idle 1", "frame": { "x": 8, "y": 0, "w": 8, "h": 8 }, "rotated": false }
  ]
})";

const char* kTexturePacker = R"(sheet.png
size: 256,256
format: RGBA8888
filter: Linear,Linear
repeat: none
hero_idle_0
  rotate: false
  xy: 0, 0
  size: 32, 32
  orig: 32, 32
  offset: 0, 0
  index: -1
hero_idle_1
  rotate: false
  xy: 32, 0
  size: 32, 32
  orig: 32, 32
  offset: 0, 0
  index: -1
gem
  rotate: true
  xy: 64, 48
  size: 16, 24
  orig: 16, 24
  offset: 0, 0
  index: -1
)";

}  // namespace

int main() {
    // --- generic JSON -------------------------------------------------------
    {
        std::vector<ply::NamedRect> rects;
        std::string error;
        check(ply::parseGenericSpritesheetJson(kGenericJson, rects, error),
              "generic JSON parses");
        check(rects.size() == 3, "generic JSON frame count");
        if (rects.size() == 3) {
            checkRect(rects[0], "idle_0", 0, 0, 32, 32, "generic frame 0");
            checkRect(rects[2], "run_0", 0, 32, 48, 32, "generic frame 2");
        }
    }

    // --- generic JSON rejects malformed input ------------------------------
    {
        std::vector<ply::NamedRect> rects;
        std::string error;
        check(!ply::parseGenericSpritesheetJson("{ not json", rects, error),
              "malformed JSON rejected");
        check(!error.empty(), "malformed JSON sets an error message");

        check(!ply::parseGenericSpritesheetJson(
                  R"({"frames":[{"name":"a","x":0,"y":0}]})", rects, error),
              "missing w/h rejected");
    }

    // --- Aseprite hash form ------------------------------------------------
    {
        std::vector<ply::NamedRect> rects;
        std::vector<std::string> tags;
        std::string error;
        check(ply::parseAsepriteJson(kAsepriteHash, rects, error, &tags),
              "Aseprite hash form parses");
        check(rects.size() == 2, "Aseprite hash frame count");
        if (rects.size() == 2) {
            checkRect(rects[0], "walk 0", 0, 0, 16, 16, "aseprite frame 0");
            checkRect(rects[1], "walk 1", 16, 0, 16, 16, "aseprite frame 1");
            check(rects[0].trimmed, "trimmed flag read");
            check(!rects[1].trimmed, "untrimmed flag read");
        }
        check(tags.size() == 1 && tags[0] == "walk", "frameTags read");
    }

    // --- Aseprite array form ----------------------------------------------
    {
        std::vector<ply::NamedRect> rects;
        std::string error;
        check(ply::parseAsepriteJson(kAsepriteArray, rects, error),
              "Aseprite array form parses");
        check(rects.size() == 2, "Aseprite array frame count");
        if (rects.size() == 2) {
            checkRect(rects[0], "idle 0", 0, 0, 8, 8, "aseprite array frame 0");
            check(rects[0].rotated, "rotated flag read");
            check(!rects[1].rotated, "non-rotated flag read");
        }
    }

    // --- TexturePacker atlas ----------------------------------------------
    {
        std::vector<ply::NamedRect> rects;
        std::string error;
        check(ply::parseTexturePackerAtlas(kTexturePacker, rects, error),
              "TexturePacker atlas parses");
        check(rects.size() == 3, "TexturePacker sprite count");
        if (rects.size() == 3) {
            checkRect(rects[0], "hero_idle_0", 0, 0, 32, 32, "TP sprite 0");
            checkRect(rects[1], "hero_idle_1", 32, 0, 32, 32, "TP sprite 1");
            checkRect(rects[2], "gem", 64, 48, 16, 24, "TP sprite 2");
            check(rects[2].rotated, "TP rotate flag read");
            check(!rects[0].rotated, "TP non-rotated flag read");
        }
    }

    // --- TexturePacker handles CRLF and reports empty input ---------------
    {
        std::vector<ply::NamedRect> rects;
        std::string error;
        const std::string crlf =
            "sheet.png\r\nsize: 64,64\r\n\r\nsprite_a\r\n  xy: 4, 8\r\n  size: 16, 16\r\n";
        check(ply::parseTexturePackerAtlas(crlf, rects, error), "CRLF atlas parses");
        check(rects.size() == 1, "CRLF sprite count");
        if (rects.size() == 1) {
            checkRect(rects[0], "sprite_a", 4, 8, 16, 16, "CRLF sprite");
        }

        std::vector<ply::NamedRect> none;
        check(!ply::parseTexturePackerAtlas("", none, error),
              "empty atlas rejected");
    }

    // --- TexturePacker rejects a sprite with no size ----------------------
    {
        std::vector<ply::NamedRect> rects;
        std::string error;
        const std::string broken = "sheet.png\nsize: 64,64\n\nbroken_sprite\n  xy: 0, 0\n";
        check(!ply::parseTexturePackerAtlas(broken, rects, error),
              "sprite without size rejected");
        check(!error.empty(), "rejection sets an error message");
    }

    if (failures == 0) {
        std::printf("spritesheet tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "spritesheet tests: %d failure(s)\n", failures);
    return 1;
}
