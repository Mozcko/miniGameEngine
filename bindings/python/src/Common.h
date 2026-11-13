#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/operators.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/unique_ptr.h>
#include <nanobind/stl/unordered_map.h>
#include <nanobind/stl/vector.h>
#include <nanobind/trampoline.h>

#include <mge/audio/AudioModule.h>
#include <mge/core/Application.h>
#include <mge/core/Clock.h>
#include <mge/core/Log.h>
#include <mge/core/Time.h>
#include <mge/core/ecs/Camera.h>
#include <mge/core/ecs/Registry.h>
#include <mge/core/ecs/Transform.h>
#include <mge/core/ecs/World.h>
#include <mge/gpu/GpuDevice.h>
#include <mge/gpu/GpuMesh.h>
#include <mge/gpu/GpuModule.h>
#include <mge/gpu/GpuResourceTracker.h>
#include <mge/graphics/DayNight.h>
#include <mge/graphics/MeshRenderer.h>
#include <mge/graphics/Renderer2D.h>
#include <mge/graphics/Sky.h>
#include <mge/graphics/Texture2D.h>
#include <mge/physics/CharacterController.h>
#include <mge/physics/PhysicsWorld.h>
#include <mge/physics/RigidBody.h>
#include <mge/platform/PlatformModule.h>
#include <mge/procgen/MeshData.h>
#include <mge/procgen/Primitives.h>
#include <mge/synth/Mixer.h>
#include <mge/synth/Music.h>
#include <mge/synth/Offline.h>
#include <mge/synth/Sfx.h>
#include <mge/voxel/Block.h>
#include <mge/voxel/Chunk.h>
#include <mge/voxel/VoxelRaycast.h>
#include <mge/voxel/VoxelWorld.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stdexcept>
#include <string>

namespace mge::python {

    // Excepción personalizada del motor en Python
    class EngineError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    void initMath(nanobind::module_& m);
    void initEcs(nanobind::module_& m);
    void initApp(nanobind::module_& m);
    void initRender(nanobind::module_& m);
    void initAudio(nanobind::module_& m);
    void initVoxel(nanobind::module_& m);

} // namespace mge::python
