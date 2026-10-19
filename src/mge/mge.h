#pragma once

// Header "paraguas" del motor: #include <mge/mge.h> trae la API común.
// El core siempre está incluido. Los módulos se incluyen solo si el programa los enlaza:
// cada target de módulo define MGE_HAS_<MÓDULO> para quien lo usa (ver los CMakeLists de
// src/mge/*). Así un programa headless nunca necesita los headers de SDL ni de la GPU.

// --- Core (siempre) ---
#include <mge/core/Application.h>
#include <mge/core/Assets.h>
#include <mge/core/Clock.h>
#include <mge/core/FlyController.h>
#include <mge/core/Input.h>
#include <mge/core/Log.h>
#include <mge/core/Module.h>
#include <mge/core/Random.h>
#include <mge/core/Time.h>
#include <mge/core/ecs/Camera.h>
#include <mge/core/ecs/Entity.h>
#include <mge/core/ecs/Registry.h>
#include <mge/core/ecs/Transform.h>
#include <mge/core/ecs/World.h>
#include <mge/core/math/Geometry.h>

// --- Generación procedural (headless) ---
#ifdef MGE_HAS_PROCGEN
#include <mge/procgen/MeshData.h>
#include <mge/procgen/MeshOps.h>
#include <mge/procgen/Noise.h>
#include <mge/procgen/Primitives.h>
#include <mge/procgen/Sdf.h>
#endif

// --- Mundos voxel (headless) ---
#ifdef MGE_HAS_VOXEL
#include <mge/voxel/Block.h>
#include <mge/voxel/Chunk.h>
#include <mge/voxel/ChunkMesher.h>
#include <mge/voxel/ChunkStreamer.h>
#include <mge/voxel/VoxelWorld.h>
#endif

// --- Plataforma: ventana e input de SDL ---
#ifdef MGE_HAS_PLATFORM
#include <mge/platform/PlatformModule.h>
#include <mge/platform/Window.h>
#endif

// --- GPU (SDL3 GPU: Vulkan/Metal/D3D12) ---
#ifdef MGE_HAS_GPU
#include <mge/gpu/CommandBuffer.h>
#include <mge/gpu/GpuDevice.h>
#include <mge/gpu/GpuMesh.h>
#include <mge/gpu/GpuModule.h>
#include <mge/gpu/Handles.h>
#include <mge/gpu/Pipeline.h>
#include <mge/gpu/Shader.h>
#include <mge/gpu/Swapchain.h>
#endif

// --- Graphics: renderer sobre SDL3 GPU ---
#ifdef MGE_HAS_GRAPHICS
#include <mge/graphics/Lighting.h>
#include <mge/graphics/MeshRenderer.h>
#include <mge/graphics/RenderQueue.h>
#include <mge/graphics/Texture2D.h>
#endif

// --- Audio ---
#ifdef MGE_HAS_AUDIO
#include <mge/audio/AudioManager.h>
#include <mge/audio/AudioModule.h>
#endif
