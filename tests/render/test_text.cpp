// Text and 2D UI: optional TTF fonts (#41, no GPU) and the 2D batch/renderer (#42).
#include "../gpu/GpuTestContext.h"
#include "RenderTest.h"

#include <doctest/doctest.h>
#include <mge/core/Log.h>
#include <mge/core/Profiler.h>
#include <examples/platformer2d/PlatformerView.h>
#include <mge/graphics/Batch2D.h>
#include <mge/graphics/DebugOverlay.h>
#include <mge/graphics/Renderer2D.h>
#include <mge/graphics/TtfFont.h>

#include <cstdlib>
#include <filesystem>
#include <functional>
#include <vector>

using namespace mge;
using mge_test::GpuTestContext;

namespace {
    // A system font for testing (the engine does not bundle one). MGE_TEST_TTF overrides it manually
    std::filesystem::path findSystemTtf() {
        if (const char* env = std::getenv("MGE_TEST_TTF")) return env;
        for (const char* candidate : {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                      "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
                                      "/usr/share/fonts/google-droid-sans-fonts/DroidSans.ttf",
                                      "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
                                      "/System/Library/Fonts/Supplemental/Arial.ttf"})
            if (std::filesystem::exists(candidate)) return candidate;
        return {};
    }

    struct QuietLog {
        QuietLog() {
            Log::setSink([](const LogRecord&) {});
        }
        ~QuietLog() { Log::setSink(nullptr); }
    };
}

TEST_CASE("System TTF font bakes into an atlas with consistent metrics") {
    const std::filesystem::path path = findSystemTtf();
    if (path.empty()) {
        MESSAGE("No known system TTF font found: skipping (set MGE_TEST_TTF=<path> to specify one)");
        return;
    }
    const Font font = loadTtfFontFile(path, 24.0f);
    REQUIRE(font.valid());
    CHECK_FALSE(font.isPixelFont());
    CHECK(font.lineHeight() >= 24.0f);
    CHECK(font.lineHeight() <= 40.0f);

    const Glyph& m = font.glyph(U'M');
    const Glyph& dot = font.glyph(U'.');
    const Glyph& space = font.glyph(U' ');
    CHECK(m.size.x > 0);
    CHECK(m.size.y > 10);
    CHECK(space.size.x == 0); // stb does not allocate pixels for space
    CHECK(space.advance > 0.0f);
    CHECK(m.advance > dot.advance); // Proportional, not monospace
    // 'M' sits on the baseline: its top edge is below the top of the line
    CHECK(m.offset.y >= 0.0f);
    CHECK(m.offset.y + float(m.size.y) <= font.lineHeight() + 1.0f);
    // 'g' descends below the baseline further than 'M'
    CHECK(font.glyph(U'g').offset.y + float(font.glyph(U'g').size.y) > m.offset.y + float(m.size.y));

    // There is coverage in the atlas where 'M' is placed
    float alpha = 0.0f;
    for (int y = 0; y < m.size.y; y++)
        for (int x = 0; x < m.size.x; x++) alpha += font.atlas().get(m.atlasPos.x + x, m.atlasPos.y + y).a;
    CHECK(alpha > 10.0f);

    CHECK(font.measure("MM").x == doctest::Approx(2.0f * m.advance));
}

TEST_CASE("Invalid or missing TTF returns an invalid font (and logs an error)") {
    QuietLog quiet;
    CHECK_FALSE(loadTtfFont({}, 16.0f).valid());
    CHECK_FALSE(loadTtfFont(std::vector<std::uint8_t>(64, 0xAB), 16.0f).valid());
    CHECK_FALSE(loadTtfFontFile("/no/existe/fuente.ttf", 16.0f).valid());
    CHECK_FALSE(loadTtfFontFile("no/existe/fuente.ttf", 16.0f).valid());
}

// --- Batch2D (#42): CPU-side 2D batch builder ---

TEST_CASE("Batch2D: each primitive emits 6-vertex quads in pixel space (top-left origin)") {
    Batch2D batch;
    batch.rect({10, 20}, {30, 40}, {1, 0, 0, 1});
    CHECK(batch.quadCount() == 1);
    REQUIRE(batch.vertices().size() == 6);
    glm::vec2 lo(1e9f), hi(-1e9f);
    for (const GpuVertex& v : batch.vertices()) {
        lo = glm::min(lo, glm::vec2(v.position));
        hi = glm::max(hi, glm::vec2(v.position));
        CHECK(v.color == glm::vec4(1, 0, 0, 1));
        CHECK(v.uv.x == doctest::Approx(Batch2D::kNoTexture)); // Untextured: white in shader
    }
    CHECK(lo == glm::vec2(10, 20));
    CHECK(hi == glm::vec2(40, 60));

    batch.rectOutline({0, 0}, {10, 10}, {1, 1, 1, 1}, 2.0f);
    CHECK(batch.quadCount() == 5); // 4 sides
    batch.line({0, 0}, {10, 0}, {1, 1, 1, 1}, 2.0f);
    CHECK(batch.quadCount() == 6);
    batch.rect({0, 0}, {0, 10}, {1, 1, 1, 1}); // Empty: nothing
    batch.line({5, 5}, {5, 5}, {1, 1, 1, 1});
    CHECK(batch.quadCount() == 6);
    CHECK(batch.commands().size() == 1);
    batch.clear();
    CHECK(batch.vertices().empty());
    CHECK(batch.commands().empty());
}

TEST_CASE("Batch2D: text emits one quad per visible glyph, with requested color and no quads for spaces") {
    const Font font = Font::builtin();
    const Texture2D atlas; // Only identifies the batch texture (no GPU needed)
    Batch2D batch;
    const glm::vec4 yellow(1.0f, 0.9f, 0.2f, 1.0f);
    const glm::vec2 size = batch.text(font, &atlas, {4, 6}, "FPS: 60", yellow, 2.0f);
    CHECK(size == font.measure("FPS: 60", 2.0f));
    CHECK(batch.quadCount() == 6);                                        // 7 characters, one space
    for (const GpuVertex& v : batch.vertices()) CHECK(v.color == yellow); // KI#19: color is preserved
    REQUIRE(batch.commands().size() == 1);
    CHECK(batch.commands()[0].texture == &atlas);
    CHECK(batch.commands()[0].nearest); // Pixel font

    // 'F' starts at the requested position and measures 8 px * scale
    glm::vec2 lo(1e9f), hi(-1e9f);
    for (std::size_t i = 0; i < 6; i++) {
        lo = glm::min(lo, glm::vec2(batch.vertices()[i].position));
        hi = glm::max(hi, glm::vec2(batch.vertices()[i].position));
    }
    CHECK(lo == glm::vec2(4, 6));
    CHECK(hi == glm::vec2(20, 22));

    batch.clear();
    batch.text(font, &atlas, {0, 0}, "a\nb", yellow);
    REQUIRE(batch.quadCount() == 2);
    CHECK(batch.vertices()[6].position.y >= font.lineHeight()); // 'b' on the second line
}

TEST_CASE("Batch2D: text renders upright (top row of glyph = higher v in Texture2D)") {
    // KI#18: text used to render upside down. Texture2D stores the image flipped (v = 1 at top), so the
    // top edge of the quad must sample the higher v
    const Font font = Font::builtin();
    const Texture2D atlas;
    Batch2D batch;
    batch.text(font, &atlas, {0, 0}, "A", {1, 1, 1, 1});
    REQUIRE(batch.vertices().size() == 6);
    const Glyph& g = font.glyph(U'A');
    const float vTop = 1.0f - float(g.atlasPos.y) / float(font.atlas().height());
    for (const GpuVertex& v : batch.vertices()) {
        if (v.position.y == 0.0f)
            CHECK(v.uv.y == doctest::Approx(vTop));
        else
            CHECK(v.uv.y < vTop);
    }
}

TEST_CASE("Batch2D: hundreds of elements in few draw calls (rects do not break text batch)") {
    const Font font = Font::builtin();
    const Texture2D atlas;
    Batch2D batch;
    for (int i = 0; i < 1000; i++) {
        batch.rect({float(i % 40) * 10.0f, float(i / 40) * 10.0f}, {8, 8}, {0.2f, 0.4f, 0.8f, 1.0f});
        if (i % 20 == 0) batch.text(font, &atlas, {0, float(i)}, "Linea de texto", {1, 1, 1, 1});
    }
    CHECK(batch.commands().size() == 1); // Interleaved: single batch using the font texture

    const Texture2D other;
    batch.image(&other, {0, 0}, {16, 16});
    CHECK(batch.commands().size() == 2); // Different texture: new batch
    batch.image(&other, {20, 0}, {16, 16});
    CHECK(batch.commands().size() == 2);
    batch.image(&other, {40, 0}, {16, 16}, {0, 0}, {1, 1}, glm::vec4(1.0f), /*nearest=*/true);
    CHECK(batch.commands().size() == 3); // Different filter
}

TEST_CASE("Batch2D: nested clips (intersection) split batches and fully clipped items emit no vertices") {
    Batch2D batch;
    batch.rect({0, 0}, {5, 5}, {1, 1, 1, 1});
    batch.pushClip({{10, 10}, {100, 50}});
    batch.rect({0, 0}, {5, 5}, {1, 1, 1, 1});
    batch.pushClip({{50, 0}, {200, 40}});
    batch.rect({0, 0}, {5, 5}, {1, 1, 1, 1});
    REQUIRE(batch.commands().size() == 3);
    CHECK_FALSE(batch.commands()[0].clip.has_value());
    CHECK(*batch.commands()[1].clip == Rect2D{{10, 10}, {100, 50}});
    CHECK(*batch.commands()[2].clip == Rect2D{{50, 10}, {60, 30}});

    batch.pushClip({{500, 500}, {10, 10}}); // Disjoint from previous: empty
    batch.rect({0, 0}, {5, 5}, {1, 1, 1, 1});
    CHECK(batch.quadCount() == 3);
    batch.popClip();
    batch.popClip();
    batch.popClip();
    batch.rect({0, 0}, {5, 5}, {1, 1, 1, 1});
    CHECK_FALSE(batch.commands().back().clip.has_value());
    batch.popClip(); // Extra pop: does not crash
}

// --- Renderer2D (#42): on the GPU ---

namespace {
    mge_test::Image drawUi(GpuDevice& device, std::uint32_t width, std::uint32_t height, float scale,
                           const std::function<void(Renderer2D&)>& build,
                           std::uint32_t* drawCalls = nullptr) {
        Renderer2D ui(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
        REQUIRE(ui.valid());
        GpuTexture target = device.createRenderTarget(width, height);
        CommandBuffer cmd = device.acquireCommandBuffer();
        {
            ColorTarget clear;
            clear.texture = target.get();
            clear.clearColor = {0.08f, 0.09f, 0.12f, 1.0f};
            RenderPass pass = cmd.beginRenderPass(clear); // Background "scene"
        }
        ui.begin(width, height, scale);
        build(ui);
        ui.flush(cmd, target.get());
        if (drawCalls) *drawCalls = ui.drawCalls();
        REQUIRE(cmd.submitAndWait());
        return {width, height, device.downloadTexture(target, width, height)};
    }
}

TEST_CASE("Render: pixel-space 2D UI (rects, outlines, lines, 1x/2x text, and clipping) (#42)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    std::uint32_t drawCalls = 0;
    const mge_test::Image image = drawUi(
        gpu.device(), 160, 120, 1.0f,
        [](Renderer2D& ui) {
            ui.rect({4, 4}, {72, 30}, {0.0f, 0.0f, 0.0f, 0.6f});
            ui.rectOutline({4, 4}, {72, 30}, {1.0f, 0.8f, 0.2f, 1.0f});
            ui.text({8, 8}, "FPS: 60", {1.0f, 0.9f, 0.3f, 1.0f});
            ui.text({8, 18}, "Hola mge!", {0.6f, 1.0f, 0.6f, 1.0f});
            ui.text({4, 40}, "2x", {1, 1, 1, 1}, 2.0f);
            ui.line({4, 64}, {150, 100}, {0.3f, 0.7f, 1.0f, 1.0f}, 2.0f);
            for (int i = 0; i < 12; i++)
                ui.rect({90.0f + float(i % 4) * 16.0f, 6.0f + float(i / 4) * 16.0f}, {12, 12},
                        {float(i) / 12.0f, 0.4f, 1.0f - float(i) / 12.0f, 1.0f});
            ui.pushClip({{40, 80}, {60, 10}}); // Only a horizontal strip of the text
            ui.text({36, 78}, "RECORTADO", {1.0f, 0.4f, 0.4f, 1.0f}, 2.0f);
            ui.popClip();
        },
        &drawCalls);
    CHECK(drawCalls == 2); // Everything except clipped text in a single batch

    // Top-left corner: yellow border is at the top (y = 4), not the bottom
    const std::uint8_t* border = image.pixel(30, 4);
    CHECK(border[0] > 200);
    CHECK(border[1] > 150);
    CHECK(border[2] < 100);
    // Outside the clip rect there is no red from the text (y = 95 is below the 80..90 strip)
    for (std::uint32_t x = 36; x < 150; x++) CHECK(image.pixel(x, 95)[0] < 150);

    const mge_test::GoldenResult result = mge_test::compareWithGolden("ui_batch", image);
    if (!result.message.empty()) MESSAGE(result.message);
    CHECK(result.passed);
}

TEST_CASE("Render: DPI scale enlarges UI in physical pixels without shifting origin (#42)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    const auto build = [](Renderer2D& ui) {
        ui.rect({2, 2}, {10, 10}, {1, 0, 0, 1});
        CHECK(ui.size() == glm::vec2(64.0f / ui.scale()));
    };
    const mge_test::Image x1 = drawUi(gpu.device(), 64, 64, 1.0f, build);
    const mge_test::Image x2 = drawUi(gpu.device(), 64, 64, 2.0f, build);
    CHECK(x1.pixel(10, 10)[0] > 200);
    CHECK(x1.pixel(15, 15)[0] < 100);
    CHECK(x2.pixel(20, 20)[0] > 200); // Rect from 4..24 physical px
    CHECK(x2.pixel(26, 26)[0] < 100);
    CHECK(x2.pixel(3, 3)[0] < 100);
}

// --- DebugOverlay (#43) ---

TEST_CASE("DebugOverlay: FPS over last ~0.5 s, average, max, and wait time from history") {
    DebugOverlay overlay;
    CHECK(overlay.fps() == doctest::Approx(0.0f));
    CHECK(overlay.averageMs() == doctest::Approx(0.0f));
    for (int i = 0; i < 100; i++) overlay.addFrame(10.0f, 4.0f); // 100 FPS
    CHECK(overlay.fps() == doctest::Approx(100.0f));
    for (int i = 0; i < 30; i++) overlay.addFrame(20.0f); // Last 0.5 s at 50 FPS
    CHECK(overlay.fps() == doctest::Approx(50.0f).epsilon(0.05));
    CHECK(overlay.maxMs() == doctest::Approx(20.0f));
    CHECK(overlay.averageMs() == doctest::Approx((100 * 10.0f + 30 * 20.0f) / 130.0f));
    CHECK(overlay.averageWaitMs() == doctest::Approx(100 * 4.0f / 130.0f));

    // Ring buffer history: only the last kHistory frames remain
    for (std::size_t i = 0; i < DebugOverlay::kHistory; i++) overlay.addFrame(5.0f, 50.0f);
    CHECK(overlay.sampleCount() == DebugOverlay::kHistory);
    CHECK(overlay.maxMs() == doctest::Approx(5.0f));
    CHECK(overlay.averageWaitMs() == doctest::Approx(5.0f)); // Wait time is clamped to frame time
}

TEST_CASE("DebugOverlay: counters preserve insertion order, update in place, and can be removed") {
    DebugOverlay overlay;
    overlay.setCounter("chunks", "10");
    overlay.setCounter("memoria", "3 MB");
    overlay.setCounter("chunks", "12");
    REQUIRE(overlay.counters().size() == 2);
    CHECK(overlay.counters()[0].first == "chunks");
    CHECK(overlay.counters()[0].second == "12");
    overlay.removeCounter("chunks");
    REQUIRE(overlay.counters().size() == 1);
    CHECK(overlay.counters()[0].first == "memoria");
}

TEST_CASE("DebugOverlay: panel is a single batch with one bar per frame in the graph") {
    const Font font = Font::builtin();
    const Texture2D atlas;
    const auto quads = [&](int frames, bool counter) {
        DebugOverlay overlay;
        for (int i = 0; i < frames; i++) overlay.addFrame(16.0f);
        if (counter) overlay.setCounter("chunks", "10");
        Batch2D batch;
        const glm::vec2 size = overlay.build(batch, font, &atlas, {8, 8});
        CHECK(size.x >= float(DebugOverlay::kHistory));
        CHECK(size.y > DebugOverlay::kGraphHeight + 2 * font.lineHeight());
        CHECK(batch.commands().size() == 1); // Everything in one draw call
        return batch.quadCount();
    };
    CHECK(quads(61, false) == quads(60, false) + 1); // One more bar (same text: 63 FPS, 16.0 ms)
    CHECK(quads(60, true) == quads(60, false) + 9);  // "chunks: 10" = 9 visible glyphs
}

TEST_CASE("DebugOverlay: setProfile copies entries and indents them in build()") {
    Profiler profiler;
    profiler.beginFrame();
    {
        MGE_PROFILE_SCOPE(profiler, "frame");
        { MGE_PROFILE_SCOPE(profiler, "update"); }
    }
    profiler.endFrame();

    DebugOverlay overlay;
    overlay.setProfile(profiler);

    REQUIRE(overlay.profileEntries().size() == 2);
    CHECK(overlay.profileEntries()[0].name == "frame");
    CHECK(overlay.profileEntries()[0].depth == 0);
    CHECK(overlay.profileEntries()[1].name == "update");
    CHECK(overlay.profileEntries()[1].depth == 1);

    const Font font = Font::builtin();
    Batch2D batchWithoutProfile;
    DebugOverlay overlayWithoutProfile;
    const glm::vec2 sizeWithout = overlayWithoutProfile.build(batchWithoutProfile, font, nullptr);

    Batch2D batchWithProfile;
    const glm::vec2 sizeWith = overlay.build(batchWithProfile, font, nullptr);

    CHECK(sizeWith.y > sizeWithout.y);
    CHECK(batchWithProfile.vertices().size() > batchWithoutProfile.vertices().size());
}

TEST_CASE("Render: performance overlay with fixed frame timings (#43)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    DebugOverlay overlay;
    // 60 FPS with occasional 25 ms (yellow) and 45 ms (red) spikes
    for (std::size_t i = 0; i < DebugOverlay::kHistory; i++)
        overlay.addFrame(i % 60 == 30 ? 45.0f : i % 20 == 10 ? 25.0f : 15.0f + float(i % 7) * 0.2f, 9.0f);
    overlay.setCounter("dibujo", "312 llamadas, 845k triangulos");
    overlay.setCounter("chunks", "248 cargados, 162 mallados");
    overlay.setCounter("", "F1 panel  F3 chunks");
    const mge_test::Image image =
        drawUi(gpu.device(), 320, 180, 1.0f, [&](Renderer2D& ui) { overlay.draw(ui); });

    const mge_test::GoldenResult result = mge_test::compareWithGolden("debug_overlay", image);
    if (!result.message.empty()) MESSAGE(result.message);
    CHECK(result.passed);
}

// --- #44: regression guard for old HUD bugs (invisible, upside-down, and colorless labels) ---

TEST_CASE("Render: HUD text renders in pixels, upright, in the requested color, regardless of winding "
          "(#44)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    constexpr float kScale = 2.0f;
    const glm::vec4 yellow(1.0f, 0.9f, 0.2f, 1.0f);
    const mge_test::Image image = drawUi(gpu.device(), 64, 40, 1.0f, [&](Renderer2D& ui) {
        ui.text({0, 0}, "F", yellow, kScale); // Top-left, in pixels (KI#1)
        ui.line({60, 36}, {20, 36}, yellow,
                2.0f); // Right-to-left: opposite triangle winding (KI#2)
    });
    // Every pixel of 'F' in the glyph table, scaled 2x, must be in place: upright (KI#18) and yellow
    // (KI#19); unlit pixels must match the background
    const unsigned char* bits = builtinFontGlyphBits(U'F');
    REQUIRE(bits != nullptr);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            CAPTURE(x);
            CAPTURE(y);
            const std::uint8_t* p = image.pixel(std::uint32_t(x * 2 + 1), std::uint32_t(y * 2 + 1));
            if (bits[y] >> x & 1u) {
                CHECK(p[0] > 230);
                CHECK(p[1] > 200);
                CHECK(p[2] < 80);
            } else {
                CHECK(p[0] < 60);
            }
        }
    const std::uint8_t* line = image.pixel(40, 36);
    CHECK(line[0] > 230);
}

// --- 2D Platformer (#45): full 2D pipeline (levels from assets/levels + Renderer2D) ---

TEST_CASE("Render: 2D platformer, level 4 (ice, snow, water, bridge) (#45)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    std::string error;
    auto level = platformer::loadLevel("levels/level4.txt", &error);
    REQUIRE_MESSAGE(level.has_value(), error);
    platformer::Platformer game(std::move(*level));
    // Walk right a bit and land on the ground: camera follows
    for (int i = 0; i < 90; i++) game.step({0.6f, false}, 1.0f / 60.0f);
    const mge_test::Image image =
        drawUi(gpu.device(), 960, 384, 1.0f, [&](Renderer2D& ui) { platformer::drawScene(ui, game, 4); });
    const mge_test::GoldenResult result = mge_test::compareWithGolden("platformer_level4", image);
    if (!result.message.empty()) MESSAGE(result.message);
    CHECK(result.passed);
}
