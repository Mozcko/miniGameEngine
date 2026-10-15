#include <mge/audio/AudioModule.h>
#include <mge/audio/AudioManager.h>

namespace mge {

    bool AudioModule::onInit(Application& /*app*/) {
        AudioManager::init();
        return true;
    }

    void AudioModule::onShutdown(Application& /*app*/) {
        AudioManager::shutdown();
    }
}
