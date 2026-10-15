#pragma once

// Header "paraguas" del motor: #include <mge/mge.h> trae la API común.
// El core siempre está incluido. Los módulos se incluyen solo si el programa los enlaza:
// cada target de módulo define MGE_HAS_<MÓDULO> para quien lo usa (ver los CMakeLists de
// src/mge/*). Así un programa headless nunca necesita los headers de SDL ni de OpenGL.

// --- Core (siempre) ---
#include <mge/core/Application.h>
#include <mge/core/Assets.h>
#include <mge/core/Clock.h>
#include <mge/core/Input.h>
#include <mge/core/Log.h>
#include <mge/core/Module.h>
#include <mge/core/Random.h>
#include <mge/core/Time.h>
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
#include <mge/voxel/VoxelWorld.h>
#endif

// --- Plataforma: ventana, contexto OpenGL, input de SDL ---
#ifdef MGE_HAS_PLATFORM
#include <mge/platform/PlatformModule.h>
#include <mge/platform/Window.h>
#endif

// --- Render (OpenGL legacy hasta #40) ---
#ifdef MGE_HAS_RENDER
#include <mge/render/3d/Mesh.h>
#include <mge/render/camera/Camera2D.h>
#include <mge/render/camera/Camera3D.h>
#include <mge/render/camera/FirstPersonCamera.h>
#include <mge/render/core/GLSLProgram.h>
#include <mge/render/core/Texture.h>
#include <mge/render/core/Vertex.h>
#endif

// --- Audio ---
#ifdef MGE_HAS_AUDIO
#include <mge/audio/AudioManager.h>
#include <mge/audio/AudioModule.h>
#endif
