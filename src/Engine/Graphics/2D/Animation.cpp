#include "Animation.h"

namespace MiniGameEngine {

    Animation::Animation(float speed, bool loop) 
        : _currentFrameIndex(0), _speed(speed), _timeElapsed(0.0f), _loop(loop)
    {}

    void Animation::addFrame(int tileIndex) {
        _frames.push_back(tileIndex);
    }

    void Animation::update(float deltaTime) {
        // Sin frames no hay nada que animar (evita underflow de size() - 1)
        if (_frames.empty()) return;

        _timeElapsed += deltaTime;
        const int frameCount = static_cast<int>(_frames.size());

        // Si pasó el tiempo suficiente, avanzamos al siguiente frame
        if (_timeElapsed >= _speed) {
            _timeElapsed -= _speed; // Restamos en lugar de poner 0 para no perder tiempo sobrante

            // Solo avanzamos si estamos en loop O si no hemos llegado al final
            if (_loop || _currentFrameIndex < frameCount - 1) {
                _currentFrameIndex++;
            }

            // Si llegamos al final, volvemos al principio (Loop)
            if (_currentFrameIndex >= frameCount) {
                if (_loop) _currentFrameIndex = 0;
                else _currentFrameIndex = frameCount - 1;
            }
        }
    }

    int Animation::getCurrentTile() const {
        if (_frames.empty()) return 0;
        return _frames[_currentFrameIndex];
    }
}