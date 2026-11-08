// End-to-end asset pipeline tests: real PNG + real TTF + real fixture files.
// No GL context required (Image, Font and the parsers are all CPU-side).

#include "plyEngine/Font.hpp"
#include "plyEngine/Image.hpp"
#include "plyEngine/Spritesheet.hpp"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

int failures = 0;
int skipped = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "  FAIL: %s\n", what);
        ++failures;
    }
}

std::string dataDir() { return std::string(PLY_TEST_DATA_DIR); }

std::string readText(const std::string& path) {
    std::ifstream file(path);
    return std::string(std::istreambuf_iterator<char>(file),
                       std::istreambuf_iterator<char>());
}

struct RGBA {
    int r, g, b, a;
};

RGBA pixelAt(const ply::Image& image, int x, int y) {
    const unsigned char* p = image.pixels() + (static_cast<size_t>(y) * image.width() + x) * 4;
    return RGBA{p[0], p[1], p[2], p[3]};
}

bool colorNear(const RGBA& c, int r, int g, int b) {
    auto near = [](int a, int b) { return a - b <= 2 && b - a <= 2; };
    return near(c.r, r) && near(c.g, g) && near(c.b, b);
}

}  // namespace

int main() {
    // --- PNG decoding -------------------------------------------------------
    {
        const ply::Image image = ply::Image::loadFromFile(dataDir() + "/test_sheet.png");
        check(image.valid(), "PNG loads");
        if (image.valid()) {
            check(image.width() == 128 && image.height() == 64, "PNG dimensions");
            check(image.error().empty(), "no error on a successful load");

            // Fixture is a 4x2 grid of 32px cells, row 0 = top.
            check(colorNear(pixelAt(image, 16, 16), 255, 0, 0), "cell 0 is red");
            check(colorNear(pixelAt(image, 48, 16), 0, 255, 0), "cell 1 is green");
            check(colorNear(pixelAt(image, 80, 16), 0, 0, 255), "cell 2 is blue");
            check(colorNear(pixelAt(image, 112, 16), 255, 255, 0), "cell 3 is yellow");
            check(colorNear(pixelAt(image, 16, 48), 0, 255, 255), "cell 4 is cyan");
            check(colorNear(pixelAt(image, 112, 48), 255, 255, 255), "cell 7 is white");
            check(pixelAt(image, 16, 16).a == 255, "PNG is opaque");
        }
    }

    // --- a missing file fails cleanly --------------------------------------
    {
        const ply::Image image = ply::Image::loadFromFile(dataDir() + "/nope.png");
        check(!image.valid(), "missing file yields an invalid image");
        check(!image.error().empty(), "missing file sets an error");
    }

    // --- a non-image file fails cleanly ------------------------------------
    {
        const ply::Image image = ply::Image::loadFromFile(dataDir() + "/test_sheet.json");
        check(!image.valid(), "non-image file yields an invalid image");
        check(!image.error().empty(), "non-image file sets an error");
    }

    // --- the three spritesheet formats agree on the same sheet -------------
    {
        std::vector<ply::NamedRect> fromGeneric;
        std::vector<ply::NamedRect> fromAseprite;
        std::vector<ply::NamedRect> fromAtlas;
        std::string error;

        check(ply::parseGenericSpritesheetJson(
                  readText(dataDir() + "/test_sheet.json"), fromGeneric, error),
              "fixture parses as generic JSON");
        check(ply::parseAsepriteJson(
                  readText(dataDir() + "/test_sheet.aseprite.json"), fromAseprite, error),
              "fixture parses as Aseprite JSON");
        check(ply::parseTexturePackerAtlas(
                  readText(dataDir() + "/test_sheet.atlas"), fromAtlas, error),
              "fixture parses as a TexturePacker atlas");

        check(fromGeneric.size() == 8, "generic: 8 frames");
        check(fromAseprite.size() == 8, "aseprite: 8 frames");
        check(fromAtlas.size() == 8, "atlas: 8 sprites");

        // Every format must describe the same rectangles. Aseprite's hash form
        // yields frames in key order, so compare by NAME rather than index.
        if (fromGeneric.size() == 8 && fromAseprite.size() == 8 && fromAtlas.size() == 8) {
            bool allAgree = true;
            for (size_t i = 0; i < 8; ++i) {
                if (fromGeneric[i].x != fromAtlas[i].x ||
                    fromGeneric[i].y != fromAtlas[i].y ||
                    fromGeneric[i].w != fromAtlas[i].w ||
                    fromGeneric[i].h != fromAtlas[i].h) {
                    allAgree = false;
                }
            }
            check(allAgree, "generic and atlas agree index-by-index");

            // Aseprite names carry a frame suffix in the fixture ("red 0").
            bool asepriteAgrees = true;
            for (const ply::NamedRect& r : fromGeneric) {
                bool found = false;
                for (const ply::NamedRect& a : fromAseprite) {
                    if (a.name == r.name + " 0") {
                        found = (a.x == r.x && a.y == r.y && a.w == r.w && a.h == r.h);
                        break;
                    }
                }
                if (!found) {
                    asepriteAgrees = false;
                    break;
                }
            }
            check(asepriteAgrees, "aseprite agrees with generic, matched by name");
        }

        // Spot-check the last cell against the fixture layout.
        if (fromGeneric.size() == 8) {
            const ply::NamedRect& last = fromGeneric[7];
            check(last.name == "white" && last.x == 96 && last.y == 32,
                  "last frame rect matches the sheet layout");
        }
    }

    // --- TTF baking --------------------------------------------------------
    {
        // Best-effort: CI images do not always ship fonts.
        const char* candidates[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        };

        ply::Font font;
        bool loaded = false;
        for (const char* path : candidates) {
            if (font.loadFromFile(path, 32)) {
                loaded = true;
                break;
            }
        }

        if (!loaded) {
            std::printf("  SKIP: no system TTF found; font baking not exercised\n");
            ++skipped;
        } else {
            check(font.valid(), "font bakes");
            check(font.atlasWidth() > 0 && font.atlasHeight() > 0, "atlas has size");
            check(font.atlasPixels() != nullptr, "atlas pixels present");
            check(font.lineHeight() > 0.0f, "positive line height");
            check(font.measure("") == 0.0f, "empty string measures 0");
            check(font.measure("abc") > 0.0f, "text measures positive width");
            check(font.measure("abcdef") > font.measure("abc"),
                  "longer text measures wider");

            const ply::Font::Glyph* a = font.glyph('A');
            check(a != nullptr, "'A' has metrics");
            if (a) {
                check(a->w > 0 && a->h > 0, "'A' has a non-empty glyph rect");
                check(a->xadvance > 0.0f, "'A' advances");
                check(a->x >= 0 && a->y >= 0, "glyph rect inside the atlas");
                check(a->x + a->w <= font.atlasWidth(), "glyph fits atlas width");
                check(a->y + a->h <= font.atlasHeight(), "glyph fits atlas height");
            }

            // Space is blank but must still advance the pen.
            const ply::Font::Glyph* space = font.glyph(' ');
            check(space != nullptr, "space has metrics");
            if (space) {
                check(space->w == 0 || space->h == 0, "space has no opaque rect");
                check(space->xadvance > 0.0f, "space advances");
            }

            // Out-of-range codepoints are rejected, not read out of bounds.
            check(font.glyph(1) == nullptr, "control char rejected");
            check(font.glyph(9999) == nullptr, "high codepoint rejected");

            // The baked atlas must actually contain ink.
            long opaque = 0;
            for (size_t i = 3; i < static_cast<size_t>(font.atlasWidth()) *
                                       font.atlasHeight() * 4;
                 i += 4) {
                if (font.atlasPixels()[i] > 0) {
                    ++opaque;
                }
            }
            check(opaque > 0, "baked atlas contains glyph coverage");
        }
    }

    if (failures == 0) {
        std::printf("image/font tests: PASS (%d skipped)\n", skipped);
        return 0;
    }
    std::fprintf(stderr, "image/font tests: %d failure(s)\n", failures);
    return 1;
}
