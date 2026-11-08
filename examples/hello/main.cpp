#include "plyEngine/Input.hpp"
#include "plyEngine/OrthoCamera.hpp"
#include "plyEngine/SpriteAtlas.hpp"
#include "plyEngine/SpriteBatch.hpp"
#include "plyEngine/Window.hpp"

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

// Writes an uncompressed 24-bit BMP (bottom-up, BGR — as the format requires).
void writeBMP(const char* path, int w, int h, const unsigned char* rgb) {
    std::FILE* f = std::fopen(path, "wb");
    if (!f) {
        std::fprintf(stderr, "could not open %s for writing\n", path);
        return;
    }

    const int rowSize = (w * 3 + 3) & ~3;
    const uint32_t dataSize = static_cast<uint32_t>(rowSize) * static_cast<uint32_t>(h);
    const uint32_t fileSize = 54 + dataSize;

    auto put16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    auto put32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };

    std::fwrite("BM", 2, 1, f);
    put32(fileSize);
    put16(0); put16(0);
    put32(54);   // pixel data offset
    put32(40);   // BITMAPINFOHEADER size
    put32(static_cast<uint32_t>(w));
    put32(static_cast<uint32_t>(h));
    put16(1);    // planes
    put16(24);   // bits per pixel
    put32(0);    // BI_RGB
    put32(dataSize);
    put32(0); put32(0);
    put32(0); put32(0);

    std::vector<unsigned char> row(static_cast<size_t>(rowSize), 0);
    for (int y = 0; y < h; ++y) {
        const unsigned char* src = rgb + static_cast<size_t>(y) * w * 3;
        for (int x = 0; x < w; ++x) {
            row[static_cast<size_t>(x) * 3 + 0] = src[static_cast<size_t>(x) * 3 + 2];  // B
            row[static_cast<size_t>(x) * 3 + 1] = src[static_cast<size_t>(x) * 3 + 1];  // G
            row[static_cast<size_t>(x) * 3 + 2] = src[static_cast<size_t>(x) * 3 + 0];  // R
        }
        std::fwrite(row.data(), 1, row.size(), f);
    }
    std::fclose(f);
}

struct BBox {
    int minx = 1 << 30, miny = 1 << 30, maxx = -1, maxy = -1;
    bool empty() const { return maxx < 0; }
};

// Bounding box of pixels that differ from the clear colour at (0,0).
BBox contentBBox(const std::vector<unsigned char>& rgb, int w, int h) {
    BBox b;
    const unsigned char* ref = &rgb[0];
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const unsigned char* p = &rgb[(static_cast<size_t>(y) * w + x) * 3];
            if (p[0] != ref[0] || p[1] != ref[1] || p[2] != ref[2]) {
                if (x < b.minx) b.minx = x;
                if (x > b.maxx) b.maxx = x;
                if (y < b.miny) b.miny = y;
                if (y > b.maxy) b.maxy = y;
            }
        }
    }
    return b;
}

}  // namespace

int main(int argc, char** argv) {
    int maxFrames = 0;  // 0 = run until closed
    const char* screenshotPath = nullptr;
    bool verify = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            maxFrames = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshotPath = argv[++i];
        } else if (std::strcmp(argv[i], "--verify") == 0) {
            verify = true;
        }
    }

    ply::WindowConfig cfg;
    cfg.title = "plyEngine - renderer";
    cfg.width = 1280;
    cfg.height = 720;

    ply::Window window(cfg);
    if (!window.valid()) {
        std::fprintf(stderr, "failed to create window\n");
        return 1;
    }

    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(ply::Window::glGetProcAddress))) {
        std::fprintf(stderr, "failed to load OpenGL via glad\n");
        return 1;
    }

    glClearColor(0.10f, 0.10f, 0.15f, 1.0f);

    // Build a 2x2 atlas (128x128) of four two-tone cells with black borders.
    const int cellSize = 64;
    const int atlasSize = 128;
    std::vector<unsigned char> pixels(static_cast<size_t>(atlasSize) * atlasSize * 4);

    auto fillCell = [&](int cx, int cy, int r, int g, int b) {
        for (int py = 0; py < cellSize; ++py) {
            for (int px = 0; px < cellSize; ++px) {
                const int x = cx * cellSize + px;
                const int y = cy * cellSize + py;
                const int i = (y * atlasSize + x) * 4;
                const bool border = px < 2 || py < 2 || px >= cellSize - 2 || py >= cellSize - 2;
                const bool dark = py >= cellSize / 2;  // bottom half darker (orientation check)
                if (border) {
                    pixels[i + 0] = pixels[i + 1] = pixels[i + 2] = 0;
                } else {
                    pixels[i + 0] = static_cast<unsigned char>(dark ? r / 2 : r);
                    pixels[i + 1] = static_cast<unsigned char>(dark ? g / 2 : g);
                    pixels[i + 2] = static_cast<unsigned char>(dark ? b / 2 : b);
                }
                pixels[i + 3] = 255;
            }
        }
    };

    fillCell(0, 0, 220, 60, 60);    // red    (top-left)
    fillCell(1, 0, 60, 220, 60);    // green  (top-right)
    fillCell(0, 1, 60, 90, 220);    // blue   (bottom-left)
    fillCell(1, 1, 220, 200, 60);   // yellow (bottom-right)

    ply::SpriteAtlas atlas(atlasSize, atlasSize, pixels.data());
    atlas.addRegion("red", 0, 0, cellSize, cellSize);
    atlas.addRegion("green", cellSize, 0, cellSize, cellSize);
    atlas.addRegion("blue", 0, cellSize, cellSize, cellSize);
    atlas.addRegion("yellow", cellSize, cellSize, cellSize, cellSize);

    const char* names[4] = {"red", "green", "blue", "yellow"};

    ply::OrthoCamera camera;
    ply::SpriteBatch batch;
    ply::Input input;

    const int grid = 8;
    const float spriteSize = 48.0f;
    const float spacing = 64.0f;

    // Center-anchored grid: positions are sprite CENTERS, so the outermost
    // sprite centers sit at +/-(grid-1)*spacing/2 and the grid is centred on
    // the camera origin.
    const float start = -(grid - 1) * spacing * 0.5f;
    const float moveSpeed = 6.0f;

    auto renderFrame = [&]() {
        glViewport(0, 0, window.width(), window.height());
        glClear(GL_COLOR_BUFFER_BIT);

        batch.begin(atlas.texture(), camera.viewProj());
        for (int r = 0; r < grid; ++r) {
            for (int c = 0; c < grid; ++c) {
                const ply::TextureRegion& region = atlas.region(names[(r + c) % 4]);
                const glm::vec2 center(start + c * spacing, start + r * spacing);
                batch.draw(center, {spriteSize, spriteSize}, region);
            }
        }
        batch.end();
    };

    int frames = 0;
    while (!input.quitRequested()) {
        input.update();
        camera.setViewport(window.width(), window.height());

        glm::vec2 pos = camera.position();
        if (input.isKeyDown(ply::Key::Left)) pos.x -= moveSpeed;
        if (input.isKeyDown(ply::Key::Right)) pos.x += moveSpeed;
        if (input.isKeyDown(ply::Key::Up)) pos.y += moveSpeed;
        if (input.isKeyDown(ply::Key::Down)) pos.y -= moveSpeed;
        camera.setPosition(pos);

        if (input.isKeyPressed(ply::Key::Equals)) camera.setZoom(camera.zoom() * 1.1f);
        if (input.isKeyPressed(ply::Key::Minus)) camera.setZoom(camera.zoom() / 1.1f);

        renderFrame();
        window.swapBuffers();

        if (maxFrames > 0 && ++frames >= maxFrames) {
            break;
        }
    }

    // Read back the last rendered frame once, for the screenshot and/or checks.
    int exitCode = 0;
    if (screenshotPath || verify) {
        renderFrame();
        const int w = window.width();
        const int h = window.height();
        std::vector<unsigned char> rgb(static_cast<size_t>(w) * h * 3);
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());

        if (screenshotPath) {
            writeBMP(screenshotPath, w, h, rgb.data());
            std::printf("wrote screenshot to %s\n", screenshotPath);
        }

        if (verify) {
            const BBox b = contentBBox(rgb, w, h);
            if (b.empty()) {
                std::fprintf(stderr, "VERIFY FAIL: nothing rendered\n");
                exitCode = 1;
            } else {
                const double cx = (b.minx + b.maxx) / 2.0;
                const double cy = (b.miny + b.maxy) / 2.0;
                const double dx = cx - w / 2.0;
                const double dy = cy - h / 2.0;
                const int bw = b.maxx - b.minx + 1;
                const int bh = b.maxy - b.miny + 1;

                std::printf("verify: content %dx%d at centre (%.1f, %.1f), "
                            "offset from screen centre (%+.1f, %+.1f)\n",
                            bw, bh, cx, cy, dx, dy);

                // Expected extent: (grid-1)*spacing + spriteSize, in both axes.
                const int expected = static_cast<int>((grid - 1) * spacing + spriteSize);
                if (bw != expected || bh != expected) {
                    std::fprintf(stderr,
                                 "VERIFY FAIL: content %dx%d, expected %dx%d\n",
                                 bw, bh, expected, expected);
                    exitCode = 1;
                }
                if (std::abs(dx) > 1.0 || std::abs(dy) > 1.0) {
                    std::fprintf(stderr,
                                 "VERIFY FAIL: grid off-centre by (%+.1f, %+.1f)\n",
                                 dx, dy);
                    exitCode = 1;
                }
                if (exitCode == 0) {
                    std::printf("verify: PASS\n");
                }
            }
        }
    }

    std::printf("ply_hello rendered %d frames (%zu quads, 1 batch)\n",
                frames, batch.quadCount());
    return exitCode;
}
