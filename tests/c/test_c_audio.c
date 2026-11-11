#include "test_c.h"

static bool on_init_audio(mge_app* app, void* user_data) {
    int* result = (int*)user_data;

    // 1. Render offline de preset SFX (JUMP)
    mge_audio_sample* sfxJump = mge_audio_sfx_create_preset(app, MGE_SFX_JUMP, 1234);
    if (!sfxJump) {
        *result = 1;
        return false;
    }

    size_t frames = mge_audio_sample_frame_count(sfxJump);
    if (frames == 0) {
        *result = 2;
        return false;
    }

    // Comprobar que no es silencio absoluto (RMS > 0)
    float rms = mge_audio_sample_rms(sfxJump);
    if (rms <= 0.001f) {
        *result = 3;
        return false;
    }

    // 2. Render de preset SFX COIN
    mge_audio_sample* sfxCoin = mge_audio_sfx_create_preset(app, MGE_SFX_COIN, 42);
    if (!sfxCoin || mge_audio_sample_rms(sfxCoin) <= 0.001f) {
        *result = 4;
        return false;
    }

    // 3. Reproducción (en modo headless sin device real no falla)
    mge_audio_play(app, sfxJump, 1.0f, 0.0f);

    // 4. Canciones y control de volumen
    mge_audio_song_play(app, "platformer", 0.0f);
    mge_audio_song_stop(app, 0.0f);

    mge_audio_set_master_volume(app, 0.8f);
    mge_audio_set_sfx_volume(app, 0.7f);
    mge_audio_set_music_volume(app, 0.6f);
    mge_audio_set_mute(app, true);

    mge_audio_sample_destroy(sfxJump);
    mge_audio_sample_destroy(sfxCoin);

    mge_app_request_stop(app);
    *result = 0;
    return true;
}

int test_c_audio(void) {
    mge_app_config config;
    memset(&config, 0, sizeof(config));
    config.struct_size = sizeof(mge_app_config);
    config.headless = true;
    config.max_frames = 1;

    int testResult = -1;
    mge_app_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.struct_size = sizeof(mge_app_callbacks);
    callbacks.on_init = on_init_audio;
    callbacks.user_data = &testResult;

    mge_app_run(&config, &callbacks);
    TEST_ASSERT_EQ(testResult, 0);
    return 0;
}
