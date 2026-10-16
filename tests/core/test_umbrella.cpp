// <mge/mge.h> enlazando solo mge_core: debe compilar sin SDL ni GPU y no ver los módulos
#include <doctest/doctest.h>
#include <mge/mge.h>

#if defined(MGE_HAS_PLATFORM) || defined(MGE_HAS_GPU) || defined(MGE_HAS_GRAPHICS) || defined(MGE_HAS_AUDIO)
#error "mge_core_tests no enlaza módulos: <mge/mge.h> no debería incluirlos"
#endif

TEST_CASE("<mge/mge.h> trae la API del core") {
    mge::Registry registry;
    mge::Entity entity = registry.create();
    registry.add<mge::Transform>(entity);
    mge::Pcg32 rng(1);
    mge::InputState input;
    CHECK(registry.has<mge::Transform>(entity));
    CHECK(rng.range(0, 10) < 10);
    CHECK_FALSE(input.isDown(mge::Key::A));
}
