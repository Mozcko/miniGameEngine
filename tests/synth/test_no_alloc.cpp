// El hilo de audio no reserva memoria (#47): se reemplaza operator new en este programa de tests y se cuenta
// lo que se reserva mientras corre Mixer::render
#include <doctest/doctest.h>
#include <mge/synth/Mixer.h>

#include <atomic>
#include <cstdlib>
#include <new>
#include <vector>

namespace {
    thread_local bool tCounting = false;
    std::atomic<int> gAllocations{0};
}

void* operator new(std::size_t size) {
    if (tCounting) gAllocations.fetch_add(1);
    if (void* p = std::malloc(size == 0 ? 1 : size)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept {
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}

TEST_CASE("Mixer::render no reserva memoria (notas, samples, tonos, eco, robo de voces)") {
    using namespace mge;
    Mixer mixer(48000);
    Sample s;
    s.data.assign(2000, 0.25f);
    const SampleId id = mixer.addSample(std::move(s));
    Instrument lead;
    lead.filter = FilterMode::LowPass;
    lead.vibratoDepth = 0.3f;
    lead.delaySend = 0.5f;
    mixer.setDelay({0.25f, 0.4f, 0.5f});
    for (int i = 0; i < 40; i++) mixer.playNote(lead, 200.0f + float(i) * 10.0f, 1.0f, 0.1f);
    mixer.playSample(id);
    mixer.playTone({500.0f, 0.2f});
    std::vector<float> out(4096 * 2);

    gAllocations = 0;
    tCounting = true;
    for (int i = 0; i < 20; i++) mixer.render(out.data(), 4096);
    tCounting = false;
    CHECK(gAllocations.load() == 0);
}
