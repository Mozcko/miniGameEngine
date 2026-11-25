// AudioModule (#46, #235) with SDL's "dummy" driver: no sound card required
#include <doctest/doctest.h>
#include <mge/audio/AudioModule.h>
#include <mge/audio/OggLoader.h>
#include <mge/core/Application.h>
#include <mge/core/Log.h>

#include <SDL3/SDL.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

using namespace mge;

namespace {

    // LSB-first bit writer matching Vorbis bitstream packing
    class BitWriter {
    public:
        void write(std::uint32_t value, int bits) {
            for (int i = 0; i < bits; i++) {
                if (m_bitPos == 0) m_bytes.push_back(0);
                if (((value >> i) & 1u) != 0u) m_bytes.back() |= static_cast<std::uint8_t>(1u << m_bitPos);
                m_bitPos = (m_bitPos + 1) & 7;
            }
        }

        void writeBytes(const char* text, std::size_t length) {
            for (std::size_t i = 0; i < length; i++) write(static_cast<std::uint8_t>(text[i]), 8);
        }

        std::vector<std::uint8_t> take() { return std::move(m_bytes); }

    private:
        std::vector<std::uint8_t> m_bytes;
        int m_bitPos = 0;
    };

    // Ogg CRC-32: non-reflected MSB-first, polynomial 0x04c11db7, init 0, no final XOR
    std::uint32_t oggCrc32(const std::uint8_t* data, std::size_t size) {
        std::uint32_t crc = 0;
        for (std::size_t i = 0; i < size; i++) {
            crc ^= static_cast<std::uint32_t>(data[i]) << 24;
            for (int bit = 0; bit < 8; bit++) {
                crc = (crc & 0x80000000u) != 0u ? ((crc << 1) ^ 0x04c11db7u) : (crc << 1);
            }
        }
        return crc;
    }

    void appendOggPage(std::vector<std::uint8_t>& out, std::uint8_t headerType, std::uint64_t granulePos,
                       std::uint32_t serial, std::uint32_t sequence,
                       const std::vector<std::vector<std::uint8_t>>& packets) {
        const std::size_t pageStart = out.size();
        out.insert(out.end(), {'O', 'g', 'g', 'S'});
        out.push_back(0); // Stream structure version
        out.push_back(headerType);
        for (int i = 0; i < 8; i++) out.push_back(static_cast<std::uint8_t>((granulePos >> (i * 8)) & 0xFFu));
        for (int i = 0; i < 4; i++) out.push_back(static_cast<std::uint8_t>((serial >> (i * 8)) & 0xFFu));
        for (int i = 0; i < 4; i++) out.push_back(static_cast<std::uint8_t>((sequence >> (i * 8)) & 0xFFu));
        const std::size_t crcOffset = out.size();
        out.insert(out.end(), {0, 0, 0, 0});

        out.push_back(static_cast<std::uint8_t>(packets.size()));
        for (const auto& pkt : packets) out.push_back(static_cast<std::uint8_t>(pkt.size()));
        for (const auto& pkt : packets) out.insert(out.end(), pkt.begin(), pkt.end());

        const std::uint32_t crc = oggCrc32(out.data() + pageStart, out.size() - pageStart);
        for (int i = 0; i < 4; i++) {
            const auto byte = static_cast<std::uint8_t>((crc >> (i * 8)) & 0xFFu);
            out[crcOffset + static_cast<std::size_t>(i)] = byte;
        }
    }

    // Builds a minimal valid Ogg Vorbis stream (blocksize 64 -> 32 frames per output block)
    std::vector<std::uint8_t> buildTestOggVorbis(int channels, int sampleRate, int outputBlocks) {
        constexpr std::uint32_t kSerial = 0x12345678u;
        constexpr int kFramesPerBlock = 32;

        // Packet 0: Identification header (30 bytes)
        BitWriter id;
        id.write(0x01u, 8);
        id.writeBytes("vorbis", 6);
        id.write(0u, 32); // vorbis_version
        id.write(static_cast<std::uint32_t>(channels), 8);
        id.write(static_cast<std::uint32_t>(sampleRate), 32);
        id.write(0u, 32);   // bitrate_max
        id.write(0u, 32);   // bitrate_nominal
        id.write(0u, 32);   // bitrate_min
        id.write(0x66u, 8); // blocksize_0 = 64, blocksize_1 = 64
        id.write(0x01u, 8); // framing_flag

        // Packet 1: Comment header (16 bytes)
        BitWriter comment;
        comment.write(0x03u, 8);
        comment.writeBytes("vorbis", 6);
        comment.write(0u, 32);   // vendor_length
        comment.write(0u, 32);   // comment_list_length
        comment.write(0x01u, 8); // framing_flag

        // Packet 2: Minimal setup header (1 codebook, 1 floor1, 1 residue0, 1 mapping0, 1 mode)
        BitWriter setup;
        setup.write(0x05u, 8);
        setup.writeBytes("vorbis", 6);
        // Codebooks: 1 codebook with 1 entry of length 1, lookup_type 0
        setup.write(0u, 8); // codebook_count - 1
        setup.write(0x42u, 8);
        setup.write(0x43u, 8);
        setup.write(0x56u, 8);
        setup.write(1u, 16); // dimensions
        setup.write(1u, 24); // entries
        setup.write(0u, 1);  // ordered
        setup.write(0u, 1);  // sparse
        setup.write(0u, 5);  // codeword length - 1
        setup.write(0u, 4);  // lookup_type
        // Time domain transfers
        setup.write(0u, 6);
        setup.write(0u, 16);
        // Floors: 1 floor1 with 0 partitions
        setup.write(0u, 6);
        setup.write(1u, 16); // floor_type = 1
        setup.write(0u, 5);  // partitions = 0
        setup.write(0u, 2);  // multiplier - 1 = 0
        setup.write(0u, 4);  // rangebits = 0
        // Residues: 1 residue0 with begin = end = 0, cascade = 0
        setup.write(0u, 6);
        setup.write(0u, 16); // residue_type = 0
        setup.write(0u, 24); // begin
        setup.write(0u, 24); // end
        setup.write(0u, 24); // partition_size - 1
        setup.write(0u, 6);  // classifications - 1
        setup.write(0u, 8);  // classbook
        setup.write(0u, 3);  // cascade[0] low_bits
        setup.write(0u, 1);  // cascade[0] has_high_bits
        // Mappings: 1 mapping0 with 1 submap, 0 coupling steps
        setup.write(0u, 6);
        setup.write(0u, 16); // mapping_type = 0
        setup.write(0u, 1);  // submaps_flag
        setup.write(0u, 1);  // coupling_flag
        setup.write(0u, 2);  // reserved
        setup.write(0u, 8);  // submap time
        setup.write(0u, 8);  // submap floor
        setup.write(0u, 8);  // submap residue
        // Modes: 1 mode with blockflag = 0
        setup.write(0u, 6);
        setup.write(0u, 1);  // blockflag
        setup.write(0u, 16); // windowtype
        setup.write(0u, 16); // transformtype
        setup.write(0u, 8);  // mapping
        setup.write(1u, 1);  // framing_flag

        // Audio packets: 1st packet is discarded by Vorbis overlap-add; each subsequent yields 32 frames
        std::vector<std::vector<std::uint8_t>> audioPackets(static_cast<std::size_t>(outputBlocks + 1),
                                                            std::vector<std::uint8_t>{0x00u});

        std::vector<std::uint8_t> ogg;
        appendOggPage(ogg, 0x02u, 0u, kSerial, 0u, {id.take()});
        appendOggPage(ogg, 0x00u, 0u, kSerial, 1u, {comment.take(), setup.take()});
        appendOggPage(ogg, 0x04u, static_cast<std::uint64_t>(outputBlocks * kFramesPerBlock), kSerial, 2u,
                      audioPackets);
        return ogg;
    }

    struct TempFile {
        std::filesystem::path path;

        TempFile(const std::string& filename, const std::vector<std::uint8_t>& bytes)
            : path(std::filesystem::temp_directory_path() / filename) {
            std::ofstream out(path, std::ios::binary);
            out.write(reinterpret_cast<const char*>(bytes.data()),
                      static_cast<std::streamsize>(bytes.size()));
        }

        ~TempFile() {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    };

} // namespace

TEST_CASE("AudioModule: opens dummy device, loads WAVs into mixer, and shuts down") {
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    Application app(ApplicationConfig{});
    AudioModule audio;
    REQUIRE(audio.onInit(app));
    CHECK(audio.hasDevice());

    const SampleId jump = audio.loadWav("sounds/jump.wav");
    REQUIRE(jump != kInvalidSample);
    const Sample* sample = audio.mixer().sample(jump);
    REQUIRE(sample != nullptr);
    CHECK(sample->channels == 1);
    // jump.wav lasts ~0.09 s; resampled to 48 kHz
    CHECK(sample->frameCount() > 2400);
    CHECK(sample->frameCount() < 48000);
    {
        Log::setSink([](const LogRecord&) {}); // Expected error should not pollute test output
        CHECK(audio.loadWav("sounds/no_existe.wav") == kInvalidSample);
        Log::setSink(nullptr);
    }

    CHECK(audio.play(jump));
    CHECK(audio.playTone({440.0f, 0.05f}));
    // SDL audio thread drains the queue
    for (int i = 0; i < 200 && audio.stats().activeVoices == 0; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    audio.setMuted(true);
    CHECK(audio.muted());
    audio.onShutdown(app);
    CHECK_FALSE(audio.hasDevice());
    SDL_ResetHint(SDL_HINT_AUDIO_DRIVER);
}

TEST_CASE("OggLoader: decodes mono, stereo, resamples, and rejects invalid inputs") {
    const auto mono48k = buildTestOggVorbis(1, 48000, 10);
    const Sample monoSample = loadOgg(mono48k, 48000);
    REQUIRE_FALSE(monoSample.empty());
    CHECK(monoSample.channels == 1);
    CHECK(monoSample.frameCount() == 320);

    const auto stereo48k = buildTestOggVorbis(2, 48000, 8);
    const Sample stereoSample = loadOgg(stereo48k, 48000);
    REQUIRE_FALSE(stereoSample.empty());
    CHECK(stereoSample.channels == 2);
    CHECK(stereoSample.frameCount() == 256);

    // 24 kHz source resampled to 48 kHz doubles frame count (~640 frames)
    const auto mono24k = buildTestOggVorbis(1, 24000, 10);
    const Sample resampled = loadOgg(mono24k, 48000);
    REQUIRE_FALSE(resampled.empty());
    CHECK(resampled.channels == 1);
    CHECK(resampled.frameCount() >= 600);
    CHECK(resampled.frameCount() <= 680);

    // Invalid inputs return empty Sample without crashing
    Log::setSink([](const LogRecord&) {});
    CHECK(loadOgg(nullptr, 0, 48000).empty());
    CHECK(loadOgg(mono48k, 0).empty());
    const std::vector<std::uint8_t> corrupt = {'n', 'o', 't', ' ', 'o', 'g', 'g'};
    CHECK(loadOgg(corrupt, 48000).empty());
    CHECK(loadOggFile("sounds/does_not_exist.ogg", 48000).empty());
    Log::setSink(nullptr);
}

TEST_CASE("AudioModule: loads OGG Vorbis sound effects from file and memory and dispatches loadSound") {
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    Application app(ApplicationConfig{});
    AudioModule audio;
    REQUIRE(audio.onInit(app));

    const auto oggBytes = buildTestOggVorbis(1, 48000, 12);
    const TempFile tempOgg("mge_test_sfx.ogg", oggBytes);

    const SampleId fromFile = audio.loadOgg(tempOgg.path);
    REQUIRE(fromFile != kInvalidSample);
    const Sample* fileSample = audio.mixer().sample(fromFile);
    REQUIRE(fileSample != nullptr);
    CHECK(fileSample->channels == 1);
    CHECK(fileSample->frameCount() == 384);
    CHECK(audio.play(fromFile));

    const SampleId fromMemory = audio.loadOggFromMemory(oggBytes.data(), oggBytes.size());
    REQUIRE(fromMemory != kInvalidSample);
    const Sample* memSample = audio.mixer().sample(fromMemory);
    REQUIRE(memSample != nullptr);
    CHECK(memSample->frameCount() == 384);

    // Extension-based dispatch
    CHECK(audio.loadSound(tempOgg.path) != kInvalidSample);
    CHECK(audio.loadSound("sounds/jump.wav") != kInvalidSample);

    Log::setSink([](const LogRecord&) {});
    CHECK(audio.loadOgg("sounds/does_not_exist.ogg") == kInvalidSample);
    CHECK(audio.loadSound("sounds/unsupported.mp3") == kInvalidSample);
    Log::setSink(nullptr);

    audio.onShutdown(app);
    SDL_ResetHint(SDL_HINT_AUDIO_DRIVER);
}
