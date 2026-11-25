// <mge/mge.h> linking only mge_core: must compile without SDL or GPU and not see modules
#include <doctest/doctest.h>
#include <mge/mge.h>

#if defined(MGE_HAS_PLATFORM) || defined(MGE_HAS_GPU) || defined(MGE_HAS_GRAPHICS) || defined(MGE_HAS_AUDIO)
#error "mge_core_tests does not link modules: <mge/mge.h> should not include them"
#endif

TEST_CASE("<mge/mge.h> brings in the core API") {
    mge::Registry registry;
    mge::Entity entity = registry.create();
    registry.add<mge::Transform>(entity);
    mge::Pcg32 rng(1);
    mge::InputState input;
    float smoothed = mge::damp(0.0f, 10.0f, 5.0f, 0.1f);
    float vel = 0.0f;
    float spring = mge::smoothDamp(0.0f, 10.0f, vel, 0.2f, 0.1f);
    CHECK(registry.has<mge::Transform>(entity));
    CHECK(rng.range(0, 10) < 10);
    CHECK_FALSE(input.isDown(mge::Key::A));
    CHECK(smoothed > 0.0f);
    CHECK(spring > 0.0f);
}
