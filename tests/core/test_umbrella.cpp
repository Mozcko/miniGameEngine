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
    mge::Timer timer(1.0f);
    mge::TweenManager tweens;
    mge::CoroutineRunner coroutines;
    mge::EventBus events;
    CHECK(registry.has<mge::Transform>(entity));
    CHECK(rng.range(0, 10) < 10);
    CHECK_FALSE(input.isDown(mge::Key::A));
    CHECK(mge::ease::outQuad(1.0f) == doctest::Approx(1.0f));
    CHECK_FALSE(timer.isFinished());
    CHECK(tweens.activeCount() == 0);
    CHECK(coroutines.runningCount() == 0);
    CHECK(events.queuedCount() == 0);
}
