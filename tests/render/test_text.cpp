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
                                      "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf"})
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

TEST_CASE("Batch2D: sprite default placement, image equivalence, and scaling around pivot (#233)") {
    const Texture2D tex;

    // Default pivot (0.5, 0.5) centers the sprite on pos
    Batch2D batch;
    SpriteDesc desc;
    desc.pos = {100.0f, 50.0f};
    desc.size = {20.0f, 10.0f};
    desc.tint = {0.2f, 0.4f, 0.6f, 0.8f};
    batch.sprite(&tex, desc);

    REQUIRE(batch.quadCount() == 1);
    REQUIRE(batch.vertices().size() == 6);
    REQUIRE(batch.commands().size() == 1);
    CHECK(batch.commands()[0].texture == &tex);
    CHECK_FALSE(batch.commands()[0].nearest);

    glm::vec2 lo(1e9f), hi(-1e9f);
    for (const GpuVertex& v : batch.vertices()) {
        lo = glm::min(lo, glm::vec2(v.position));
        hi = glm::max(hi, glm::vec2(v.position));
        CHECK(v.color == desc.tint);
    }
    CHECK(lo == glm::vec2(90.0f, 45.0f));
    CHECK(hi == glm::vec2(110.0f, 55.0f));

    // With top-left pivot (0, 0), unrotated/unscaled/unflipped sprite matches image() identically
    Batch2D imageBatch;
    imageBatch.image(&tex, {12.0f, 24.0f}, {32.0f, 16.0f}, {0.25f, 0.5f}, {0.75f, 1.0f}, {1, 0.5f, 0.25f, 1},
                     true);
    Batch2D spriteBatch;
    SpriteDesc equiv;
    equiv.pos = {12.0f, 24.0f};
    equiv.size = {32.0f, 16.0f};
    equiv.pivot = {0.0f, 0.0f};
    equiv.uvMin = {0.25f, 0.5f};
    equiv.uvMax = {0.75f, 1.0f};
    equiv.tint = {1.0f, 0.5f, 0.25f, 1.0f};
    equiv.nearest = true;
    spriteBatch.sprite(&tex, equiv);

    REQUIRE(spriteBatch.vertices().size() == imageBatch.vertices().size());
    for (std::size_t i = 0; i < imageBatch.vertices().size(); ++i) {
        CHECK(spriteBatch.vertices()[i].position == imageBatch.vertices()[i].position);
        CHECK(spriteBatch.vertices()[i].uv == imageBatch.vertices()[i].uv);
        CHECK(spriteBatch.vertices()[i].color == imageBatch.vertices()[i].color);
    }

    // Non-uniform scaling around custom bottom-center pivot (0.5, 1.0)
    batch.clear();
    SpriteDesc scaled;
    scaled.pos = {50.0f, 80.0f};
    scaled.size = {20.0f, 10.0f};
    scaled.scale = {2.0f, 3.0f};
    scaled.pivot = {0.5f, 1.0f};
    batch.sprite(&tex, scaled);

    REQUIRE(batch.vertices().size() == 6);
    lo = glm::vec2(1e9f);
    hi = glm::vec2(-1e9f);
    for (const GpuVertex& v : batch.vertices()) {
        lo = glm::min(lo, glm::vec2(v.position));
        hi = glm::max(hi, glm::vec2(v.position));
    }
    CHECK(lo.x == doctest::Approx(30.0f));
    CHECK(hi.x == doctest::Approx(70.0f));
    CHECK(lo.y == doctest::Approx(50.0f));
    CHECK(hi.y == doctest::Approx(80.0f)); // Bottom edge stays pinned at pivot y = 80
}

TEST_CASE("Batch2D: sprite rotation around pivot and scale-before-rotate order (#233)") {
    constexpr float kHalfPi = 1.5707963267948966f;
    constexpr float kQuarterPi = 0.7853981633974483f;
    const Texture2D tex;

    // 90 deg clockwise in screen space (+Y down): (20x10) scaled by (2, 1) -> (40x10) -> rotated to (10x40)
    Batch2D batch;
    SpriteDesc desc;
    desc.pos = {100.0f, 100.0f};
    desc.size = {20.0f, 10.0f};
    desc.scale = {2.0f, 1.0f};
    desc.rotation = kHalfPi;
    batch.sprite(&tex, desc);

    REQUIRE(batch.vertices().size() == 6);
    // Quad vertices order: {0, 1, 2, 0, 2, 3} -> indices 0=TL, 1=TR, 2=BR, 5=BL
    const GpuVertex& v0 = batch.vertices()[0];
    const GpuVertex& v1 = batch.vertices()[1];
    const GpuVertex& v2 = batch.vertices()[2];
    const GpuVertex& v3 = batch.vertices()[5];

    // Original TL (-20, -5) rotates CW 90 deg to (+5, -20) -> (105, 80) with UV (0, 1)
    CHECK(v0.position.x == doctest::Approx(105.0f).epsilon(1e-4f));
    CHECK(v0.position.y == doctest::Approx(80.0f).epsilon(1e-4f));
    CHECK(v0.uv == glm::vec2(0.0f, 1.0f));

    // Original TR (+20, -5) rotates CW 90 deg to (+5, +20) -> (105, 120) with UV (1, 1)
    CHECK(v1.position.x == doctest::Approx(105.0f).epsilon(1e-4f));
    CHECK(v1.position.y == doctest::Approx(120.0f).epsilon(1e-4f));
    CHECK(v1.uv == glm::vec2(1.0f, 1.0f));

    // Original BR (+20, +5) rotates CW 90 deg to (-5, +20) -> (95, 120) with UV (1, 0)
    CHECK(v2.position.x == doctest::Approx(95.0f).epsilon(1e-4f));
    CHECK(v2.position.y == doctest::Approx(120.0f).epsilon(1e-4f));
    CHECK(v2.uv == glm::vec2(1.0f, 0.0f));

    // Original BL (-20, +5) rotates CW 90 deg to (-5, -20) -> (95, 80) with UV (0, 0)
    CHECK(v3.position.x == doctest::Approx(95.0f).epsilon(1e-4f));
    CHECK(v3.position.y == doctest::Approx(80.0f).epsilon(1e-4f));
    CHECK(v3.uv == glm::vec2(0.0f, 0.0f));

    // Custom pivot (0, 0) stays fixed at pos under 45 deg rotation, and edge lengths are preserved
    batch.clear();
    SpriteDesc pivoted;
    pivoted.pos = {20.0f, 30.0f};
    pivoted.size = {16.0f, 8.0f};
    pivoted.pivot = {0.0f, 0.0f};
    pivoted.rotation = kQuarterPi;
    batch.sprite(&tex, pivoted);

    REQUIRE(batch.vertices().size() == 6);
    const glm::vec2 c0 = glm::vec2(batch.vertices()[0].position);
    const glm::vec2 c1 = glm::vec2(batch.vertices()[1].position);
    const glm::vec2 c2 = glm::vec2(batch.vertices()[2].position);
    const glm::vec2 c3 = glm::vec2(batch.vertices()[5].position);
    CHECK(c0.x == doctest::Approx(20.0f).epsilon(1e-4f));
    CHECK(c0.y == doctest::Approx(30.0f).epsilon(1e-4f));
    CHECK(glm::distance(c0, c1) == doctest::Approx(16.0f).epsilon(1e-4f));
    CHECK(glm::distance(c1, c2) == doctest::Approx(8.0f).epsilon(1e-4f));
    CHECK(glm::distance(c2, c3) == doctest::Approx(16.0f).epsilon(1e-4f));
    CHECK(glm::distance(c3, c0) == doctest::Approx(8.0f).epsilon(1e-4f));
}

TEST_CASE("Batch2D: sprite horizontal and vertical flip swap UVs within sub-rect without moving corners "
          "(#233)") {
    const Texture2D tex;
    SpriteDesc base;
    base.pos = {40.0f, 40.0f};
    base.size = {20.0f, 10.0f};
    base.pivot = {0.25f, 0.5f};
    base.uvMin = {0.25f, 0.5f};
    base.uvMax = {0.5f, 0.75f};

    Batch2D normalBatch;
    normalBatch.sprite(&tex, base);

    SpriteDesc flippedX = base;
    flippedX.flipX = true;
    Batch2D flipXBatch;
    flipXBatch.sprite(&tex, flippedX);

    SpriteDesc flippedY = base;
    flippedY.flipY = true;
    Batch2D flipYBatch;
    flipYBatch.sprite(&tex, flippedY);

    SpriteDesc flippedXY = base;
    flippedXY.flipX = true;
    flippedXY.flipY = true;
    Batch2D flipXYBatch;
    flipXYBatch.sprite(&tex, flippedXY);

    REQUIRE(normalBatch.vertices().size() == 6);
    REQUIRE(flipXBatch.vertices().size() == 6);
    REQUIRE(flipYBatch.vertices().size() == 6);
    REQUIRE(flipXYBatch.vertices().size() == 6);

    // Positions are identical regardless of flip flags
    for (std::size_t i = 0; i < 6; ++i) {
        CHECK(flipXBatch.vertices()[i].position == normalBatch.vertices()[i].position);
        CHECK(flipYBatch.vertices()[i].position == normalBatch.vertices()[i].position);
        CHECK(flipXYBatch.vertices()[i].position == normalBatch.vertices()[i].position);
    }

    // Indices 0=TL, 1=TR, 2=BR, 5=BL
    CHECK(flipXBatch.vertices()[0].uv == glm::vec2(0.5f, 0.75f));
    CHECK(flipXBatch.vertices()[1].uv == glm::vec2(0.25f, 0.75f));
    CHECK(flipXBatch.vertices()[2].uv == glm::vec2(0.25f, 0.5f));
    CHECK(flipXBatch.vertices()[5].uv == glm::vec2(0.5f, 0.5f));

    CHECK(flipYBatch.vertices()[0].uv == glm::vec2(0.25f, 0.5f));
    CHECK(flipYBatch.vertices()[1].uv == glm::vec2(0.5f, 0.5f));
    CHECK(flipYBatch.vertices()[2].uv == glm::vec2(0.5f, 0.75f));
    CHECK(flipYBatch.vertices()[5].uv == glm::vec2(0.25f, 0.75f));

    CHECK(flipXYBatch.vertices()[0].uv == glm::vec2(0.5f, 0.5f));
    CHECK(flipXYBatch.vertices()[1].uv == glm::vec2(0.25f, 0.5f));
    CHECK(flipXYBatch.vertices()[2].uv == glm::vec2(0.25f, 0.75f));
    CHECK(flipXYBatch.vertices()[5].uv == glm::vec2(0.5f, 0.75f));
}

TEST_CASE("Batch2D: sprite batching, untextured sprites, clipping, and degenerate inputs (#233)") {
    const Texture2D texA;
    const Texture2D texB;
    Batch2D batch;

    SpriteDesc desc;
    desc.pos = {10.0f, 10.0f};
    desc.size = {16.0f, 16.0f};
    desc.nearest = true;

    // Same texture and filter share one draw call; untextured sprite does not break the batch
    batch.sprite(&texA, desc);
    batch.sprite(nullptr, desc);
    batch.sprite(&texA, desc);
    CHECK(batch.quadCount() == 3);
    CHECK(batch.commands().size() == 1);
    CHECK(batch.vertices()[6].uv.x == doctest::Approx(Batch2D::kNoTexture));

    // Changing filter or texture starts a new command
    desc.nearest = false;
    batch.sprite(&texA, desc);
    CHECK(batch.commands().size() == 2);
    batch.sprite(&texB, desc);
    CHECK(batch.commands().size() == 3);

    // Degenerate size or zero scale emits nothing
    const std::size_t before = batch.quadCount();
    SpriteDesc zeroSize = desc;
    zeroSize.size = {0.0f, 16.0f};
    batch.sprite(&texB, zeroSize);
    SpriteDesc negSize = desc;
    negSize.size = {16.0f, -4.0f};
    batch.sprite(&texB, negSize);
    SpriteDesc zeroScale = desc;
    zeroScale.scale = {1.0f, 0.0f};
    batch.sprite(&texB, zeroScale);
    CHECK(batch.quadCount() == before);

    // Empty clip rect suppresses emission
    batch.pushClip({{0.0f, 0.0f}, {0.0f, 0.0f}});
    batch.sprite(&texB, desc);
    batch.popClip();
    CHECK(batch.quadCount() == before);
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

TEST_CASE("Render: rotated, scaled, and flipped sprites in a single batch (#233)") {
    GpuTestContext gpu;
    if (!gpu.available()) return;
    GpuDevice& device = gpu.device();

    // 2x2 texture, row 0 at top: Red, Green / Blue, White
    const std::uint8_t pixels[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    const Texture2D texture = Texture2D::fromPixels(device, 2, 2, pixels);
    REQUIRE(texture);

    constexpr float kHalfPi = 1.5707963267948966f;
    std::uint32_t drawCalls = 0;
    const mge_test::Image image = drawUi(
        device, 112, 96, 1.0f,
        [&](Renderer2D& ui) {
            SpriteDesc base;
            base.size = {16.0f, 16.0f};
            base.nearest = true;

            // 1. Unflipped at (20, 20): TL=Red, TR=Green, BL=Blue, BR=White
            SpriteDesc normal = base;
            normal.pos = {20.0f, 20.0f};
            ui.sprite(&texture, normal);

            // 2. Horizontal flip at (52, 20): TL=Green, TR=Red, BL=White, BR=Blue
            SpriteDesc flipX = base;
            flipX.pos = {52.0f, 20.0f};
            flipX.flipX = true;
            ui.sprite(&texture, flipX);

            // 3. Vertical flip at (84, 20): TL=Blue, TR=White, BL=Red, BR=Green
            SpriteDesc flipY = base;
            flipY.pos = {84.0f, 20.0f};
            flipY.flipY = true;
            ui.sprite(&texture, flipY);

            // 4. Scaled (2, 1) then rotated 90 deg CW at (48, 64) -> 16 wide x 32 tall
            //    TL=Blue, TR=Red, BR=Green, BL=White
            SpriteDesc rotScaled = base;
            rotScaled.pos = {48.0f, 64.0f};
            rotScaled.scale = {2.0f, 1.0f};
            rotScaled.rotation = kHalfPi;
            ui.sprite(&texture, rotScaled);
        },
        &drawCalls);

    CHECK(drawCalls == 1);

    auto isRed = [](const std::uint8_t* p) { return p[0] > 200 && p[1] < 50 && p[2] < 50; };
    auto isGreen = [](const std::uint8_t* p) { return p[0] < 50 && p[1] > 200 && p[2] < 50; };
    auto isBlue = [](const std::uint8_t* p) { return p[0] < 50 && p[1] < 50 && p[2] > 200; };
    auto isWhite = [](const std::uint8_t* p) { return p[0] > 200 && p[1] > 200 && p[2] > 200; };
    auto isDark = [](const std::uint8_t* p) { return p[0] < 60 && p[1] < 60 && p[2] < 60; };

    // 1. Unflipped
    CHECK(isRed(image.pixel(16, 16)));
    CHECK(isGreen(image.pixel(24, 16)));
    CHECK(isBlue(image.pixel(16, 24)));
    CHECK(isWhite(image.pixel(24, 24)));

    // 2. Horizontally flipped
    CHECK(isGreen(image.pixel(48, 16)));
    CHECK(isRed(image.pixel(56, 16)));
    CHECK(isWhite(image.pixel(48, 24)));
    CHECK(isBlue(image.pixel(56, 24)));

    // 3. Vertically flipped
    CHECK(isBlue(image.pixel(80, 16)));
    CHECK(isWhite(image.pixel(88, 16)));
    CHECK(isRed(image.pixel(80, 24)));
    CHECK(isGreen(image.pixel(88, 24)));

    // 4. Scaled (2, 1) and rotated 90 deg CW: occupies x in [40, 56], y in [48, 80]
    CHECK(isBlue(image.pixel(44, 56)));
    CHECK(isRed(image.pixel(52, 56)));
    CHECK(isGreen(image.pixel(52, 72)));
    CHECK(isWhite(image.pixel(44, 72)));
    CHECK(isDark(image.pixel(32, 64)));
    CHECK(isDark(image.pixel(64, 64)));
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
