// NOLINTBEGIN(readability-identifier-naming)
#include <mge/c/mge_c_internal.h>

#include <cstring>

namespace {

    mge_mesh* registerMesh(mge_app* app, mge::MeshData meshData) {
        auto* mesh = new mge_mesh();
        mesh->app = app;
        mesh->cpuMesh = std::move(meshData);
        mesh->alive = false;

        if (app && app->gpu) {
            mesh->gpuMesh = app->gpu->device().uploadMesh(mesh->cpuMesh);
            mesh->alive = !mesh->gpuMesh.empty();
            if (mesh->alive) {
                mesh->trackerId = app->gpu->device().tracker().registerResource("GpuMesh", [mesh]() {
                    mesh->gpuMesh = mge::GpuMesh{};
                    mesh->alive = false;
                    mesh->app = nullptr;
                });
            }
        }
        return mesh;
    }

} // namespace

extern "C" {

mge_mesh* mge_mesh_create_cube(mge_app* app, float size) {
    return registerMesh(app, mge::primitives::box(glm::vec3(size)));
}

mge_mesh* mge_mesh_create_sphere(mge_app* app, float radius, int rings, int sectors) {
    (void)rings;
    (void)sectors;
    return registerMesh(app, mge::primitives::icosphere(radius, 2));
}

mge_mesh* mge_mesh_create_plane(mge_app* app, float width, float depth) {
    return registerMesh(app, mge::primitives::plane(width, depth));
}

mge_mesh* mge_mesh_create_cylinder(mge_app* app, float radius, float height, int sectors) {
    return registerMesh(app, mge::primitives::cylinder(radius, height, sectors > 2 ? sectors : 16));
}

mge_mesh* mge_mesh_upload(mge_app* app, const float* vertices, size_t vertex_count, const uint32_t* indices,
                          size_t index_count) {
    mge::MeshData data;
    if (vertices && vertex_count > 0) {
        data.positions.reserve(vertex_count);
        for (size_t i = 0; i < vertex_count; ++i) {
            data.positions.emplace_back(vertices[i * 3], vertices[i * 3 + 1], vertices[i * 3 + 2]);
        }
    }
    if (indices && index_count > 0) {
        data.indices.assign(indices, indices + index_count);
    }
    return registerMesh(app, std::move(data));
}

void mge_mesh_destroy(mge_mesh* mesh) {
    if (!mesh) return;
    if (mesh->alive && mesh->app) {
        if (mesh->app->gpu) {
            mesh->app->gpu->device().tracker().unregisterResource(mesh->trackerId);
        }
    }
    mesh->gpuMesh = mge::GpuMesh{};
    mesh->alive = false;
    mesh->app = nullptr;
    delete mesh;
}

void mge_renderer_draw_mesh(mge_app* app, mge_mesh* mesh, const float model[16], const float color[4]) {
    if (!app || !mesh || !mesh->alive || !app->gpu || !app->meshRenderer) return;

    const auto* frame = app->gpu->frame();
    if (!frame) return;

    glm::mat4 modelMat = model ? glm::make_mat4(model) : glm::mat4(1.0f);

    if (!app->inRenderPass) {
        glm::vec4 clearColor(0.08f, 0.12f, 0.18f, 1.0f);
        clearColor = glm::vec4(app->dayNight.sky().zenithColor, 1.0f);
        if (!app->meshRenderer->begin(*frame->cmd, frame->swapchain, frame->width, frame->height,
                                      clearColor)) {
            return;
        }

        glm::mat4 viewProj = app->projMatrix * app->viewMatrix;
        glm::vec3 camPos(0.0f);
        if (app->customCamera) {
            glm::mat4 invView = glm::inverse(app->viewMatrix);
            camPos = glm::vec3(invView[3]);
        }
        app->meshRenderer->setCamera(viewProj, camPos);
        app->meshRenderer->setLighting(app->currentLighting);

        if (app->skyRenderer) {
            app->skyRenderer->draw(*app->meshRenderer, glm::inverse(viewProj), camPos, app->dayNight.sky());
        }

        app->inRenderPass = true;
    }

    (void)color;
    mge::MeshMaterial mat;
    app->meshRenderer->draw(mesh->gpuMesh, modelMat, nullptr, mge::MeshPass::Opaque, mat);
}

void mge_renderer_set_lighting(mge_app* app, const mge_lighting* lighting) {
    if (!app || !lighting) return;
    app->currentLighting.sunDirection =
        glm::normalize(glm::vec3(lighting->sun_dir[0], lighting->sun_dir[1], lighting->sun_dir[2]));
    app->currentLighting.sunColor =
        glm::vec3(lighting->sun_color[0], lighting->sun_color[1], lighting->sun_color[2]) *
        lighting->sun_intensity;
    app->currentLighting.skyAmbient =
        glm::vec3(lighting->sky_ambient[0], lighting->sky_ambient[1], lighting->sky_ambient[2]);
    app->currentLighting.groundAmbient =
        glm::vec3(lighting->ground_ambient[0], lighting->ground_ambient[1], lighting->ground_ambient[2]);
    app->currentLighting.fogColor =
        glm::vec3(lighting->fog_color[0], lighting->fog_color[1], lighting->fog_color[2]);
    app->currentLighting.fog = (lighting->fog_density > 0.0f);
    if (lighting->fog_density > 0.0f) {
        app->currentLighting.fogEnd = 1.0f / lighting->fog_density;
        app->currentLighting.fogStart = app->currentLighting.fogEnd * 0.4f;
    }

    if (app->meshRenderer) {
        app->meshRenderer->setLighting(app->currentLighting);
    }
}

void mge_renderer_set_camera(mge_app* app, const float view[16], const float proj[16]) {
    if (!app) return;
    if (view) app->viewMatrix = glm::make_mat4(view);
    if (proj) app->projMatrix = glm::make_mat4(proj);
    app->customCamera = true;
}

void mge_sky_set_time_of_day(mge_app* app, float hour) {
    if (!app) return;
    app->dayNight.hour = hour;
}

void mge_sky_advance_time(mge_app* app, float hours) {
    if (!app) return;
    app->dayNight.advance(hours);
}

/* --- 2D UI & Text --- */
void mge_ui_rect(mge_app* app, float x, float y, float w, float h, const float color[4]) {
    if (!app || !app->uiRenderer) return;
    glm::vec4 col = color ? glm::make_vec4(color) : glm::vec4(1.0f);
    app->uiRenderer->rect(glm::vec2(x, y), glm::vec2(w, h), col);
}

void mge_ui_rect_outline(mge_app* app, float x, float y, float w, float h, const float color[4],
                         float thickness) {
    if (!app || !app->uiRenderer) return;
    glm::vec4 col = color ? glm::make_vec4(color) : glm::vec4(1.0f);
    app->uiRenderer->rectOutline(glm::vec2(x, y), glm::vec2(w, h), col, thickness > 0.0f ? thickness : 1.0f);
}

void mge_ui_line(mge_app* app, float x0, float y0, float x1, float y1, const float color[4],
                 float thickness) {
    if (!app || !app->uiRenderer) return;
    glm::vec4 col = color ? glm::make_vec4(color) : glm::vec4(1.0f);
    app->uiRenderer->line(glm::vec2(x0, y0), glm::vec2(x1, y1), col, thickness > 0.0f ? thickness : 1.0f);
}

void mge_ui_text(mge_app* app, float x, float y, const char* text, float scale, const float color[4]) {
    if (!app || !app->uiRenderer || !text) return;
    glm::vec4 col = color ? glm::make_vec4(color) : glm::vec4(1.0f);
    app->uiRenderer->text(glm::vec2(x, y), text, col, scale > 0.0f ? scale : 1.0f);
}

mge_texture* mge_texture_create_rgba(mge_app* app, int width, int height, const uint8_t* pixels) {
    if (!app || !app->gpu || width <= 0 || height <= 0 || !pixels) return nullptr;

    auto* tex = new mge_texture();
    tex->app = app;
    tex->texture = mge::Texture2D::fromPixels(app->gpu->device(), static_cast<uint32_t>(width),
                                              static_cast<uint32_t>(height), pixels);
    tex->alive = static_cast<bool>(tex->texture);

    if (tex->alive) {
        tex->trackerId = app->gpu->device().tracker().registerResource("Texture2D", [tex]() {
            tex->texture = mge::Texture2D{};
            tex->alive = false;
            tex->app = nullptr;
        });
    }
    return tex;
}

void mge_texture_destroy(mge_texture* texture) {
    if (!texture) return;
    if (texture->alive && texture->app) {
        if (texture->app->gpu) {
            texture->app->gpu->device().tracker().unregisterResource(texture->trackerId);
        }
    }
    texture->texture = mge::Texture2D{};
    texture->alive = false;
    texture->app = nullptr;
    delete texture;
}

void mge_ui_image(mge_app* app, mge_texture* texture, float x, float y, float w, float h,
                  const float color[4]) {
    if (!app || !app->uiRenderer || !texture || !texture->alive) return;
    glm::vec4 col = color ? glm::make_vec4(color) : glm::vec4(1.0f);
    app->uiRenderer->image(&texture->texture, glm::vec2(x, y), glm::vec2(w, h), col);
}

} // extern "C"
// NOLINTEND(readability-identifier-naming)
