// NOLINTBEGIN(readability-identifier-naming)
#include <mge/c/mge_c_internal.h>

namespace {

    inline mge::Entity toEngineEntity(mge_entity e) {
        if (e == MGE_NULL_ENTITY || (e & 0xFFFFFFFFu) == 0xFFFFFFFFu) return mge::kNullEntity;
        return mge::Entity{static_cast<std::uint32_t>(e & 0xFFFFFFFFu), static_cast<std::uint32_t>(e >> 32)};
    }

    inline mge_entity fromEngineEntity(mge::Entity e) {
        if (e.isNull()) return MGE_NULL_ENTITY;
        return (static_cast<uint64_t>(e.generation) << 32) | static_cast<uint64_t>(e.index);
    }

} // namespace

extern "C" {

mge_entity mge_entity_create(mge_app* app) {
    if (!app || !app->world) return MGE_NULL_ENTITY;
    mge::Entity e = app->world->registry().create();
    return fromEngineEntity(e);
}

void mge_entity_destroy(mge_app* app, mge_entity entity) {
    if (!app || !app->world) return;
    app->world->registry().destroy(toEngineEntity(entity));
}

bool mge_entity_is_valid(mge_app* app, mge_entity entity) {
    if (!app || !app->world) return false;
    return app->world->registry().valid(toEngineEntity(entity));
}

/* --- Built-in: Transform --- */
mge_result mge_transform_set(mge_app* app, mge_entity entity, const mge_transform* transform) {
    if (!app || !app->world || !transform) return MGE_ERROR_INVALID_ARGUMENT;
    mge::Entity e = toEngineEntity(entity);
    if (!app->world->registry().valid(e)) return MGE_ERROR_INVALID_HANDLE;

    mge::Transform t;
    t.position = glm::vec3(transform->position[0], transform->position[1], transform->position[2]);
    t.rotation = glm::quat(transform->rotation[3], transform->rotation[0], transform->rotation[1],
                           transform->rotation[2]); // w, x, y, z
    t.scale = glm::vec3(transform->scale[0], transform->scale[1], transform->scale[2]);

    app->world->registry().add<mge::Transform>(e, t);
    return MGE_OK;
}

mge_result mge_transform_get(mge_app* app, mge_entity entity, mge_transform* out_transform) {
    if (!app || !app->world || !out_transform) return MGE_ERROR_INVALID_ARGUMENT;
    mge::Entity e = toEngineEntity(entity);
    if (!app->world->registry().has<mge::Transform>(e)) return MGE_ERROR_NOT_FOUND;

    const auto& t = app->world->registry().get<mge::Transform>(e);
    out_transform->position[0] = t.position.x;
    out_transform->position[1] = t.position.y;
    out_transform->position[2] = t.position.z;
    out_transform->rotation[0] = t.rotation.x;
    out_transform->rotation[1] = t.rotation.y;
    out_transform->rotation[2] = t.rotation.z;
    out_transform->rotation[3] = t.rotation.w;
    out_transform->scale[0] = t.scale.x;
    out_transform->scale[1] = t.scale.y;
    out_transform->scale[2] = t.scale.z;
    return MGE_OK;
}

bool mge_transform_has(mge_app* app, mge_entity entity) {
    if (!app || !app->world) return false;
    return app->world->registry().has<mge::Transform>(toEngineEntity(entity));
}

void mge_transform_remove(mge_app* app, mge_entity entity) {
    if (!app || !app->world) return;
    app->world->registry().remove<mge::Transform>(toEngineEntity(entity));
}

/* --- Built-in: Camera --- */
mge_result mge_camera_set(mge_app* app, mge_entity entity, const mge_camera* camera) {
    if (!app || !app->world || !camera) return MGE_ERROR_INVALID_ARGUMENT;
    mge::Entity e = toEngineEntity(entity);
    if (!app->world->registry().valid(e)) return MGE_ERROR_INVALID_HANDLE;

    mge::Camera c;
    c.projection =
        camera->is_orthographic ? mge::ProjectionType::Orthographic : mge::ProjectionType::Perspective;
    c.fovY = camera->fov_y;
    c.nearPlane = camera->near_plane;
    c.farPlane = camera->far_plane;
    c.orthoHeight = camera->ortho_size;

    app->world->registry().add<mge::Camera>(e, c);
    return MGE_OK;
}

mge_result mge_camera_get(mge_app* app, mge_entity entity, mge_camera* out_camera) {
    if (!app || !app->world || !out_camera) return MGE_ERROR_INVALID_ARGUMENT;
    mge::Entity e = toEngineEntity(entity);
    if (!app->world->registry().has<mge::Camera>(e)) return MGE_ERROR_NOT_FOUND;

    const auto& c = app->world->registry().get<mge::Camera>(e);
    out_camera->is_orthographic = (c.projection == mge::ProjectionType::Orthographic);
    out_camera->fov_y = c.fovY;
    out_camera->near_plane = c.nearPlane;
    out_camera->far_plane = c.farPlane;
    out_camera->ortho_size = c.orthoHeight;
    return MGE_OK;
}

bool mge_camera_has(mge_app* app, mge_entity entity) {
    if (!app || !app->world) return false;
    return app->world->registry().has<mge::Camera>(toEngineEntity(entity));
}

void mge_camera_remove(mge_app* app, mge_entity entity) {
    if (!app || !app->world) return;
    app->world->registry().remove<mge::Camera>(toEngineEntity(entity));
}

/* --- Built-in: CharacterController --- */
mge_result mge_character_controller_set(mge_app* app, mge_entity entity,
                                        const mge_character_controller* controller) {
    if (!app || !app->world || !controller) return MGE_ERROR_INVALID_ARGUMENT;
    mge::Entity e = toEngineEntity(entity);
    if (!app->world->registry().valid(e)) return MGE_ERROR_INVALID_HANDLE;

    mge::CharacterController cc;
    cc.mode = (controller->mode == 1) ? mge::CharacterMode::Fly : mge::CharacterMode::Walk;
    cc.walkSpeed = controller->speed;
    cc.jumpSpeed = controller->jump_speed;
    cc.gravity = controller->gravity;
    cc.grounded = controller->is_grounded;

    app->world->registry().add<mge::CharacterController>(e, cc);
    return MGE_OK;
}

mge_result mge_character_controller_get(mge_app* app, mge_entity entity,
                                        mge_character_controller* out_controller) {
    if (!app || !app->world || !out_controller) return MGE_ERROR_INVALID_ARGUMENT;
    mge::Entity e = toEngineEntity(entity);
    if (!app->world->registry().has<mge::CharacterController>(e)) return MGE_ERROR_NOT_FOUND;

    const auto& cc = app->world->registry().get<mge::CharacterController>(e);
    out_controller->mode = (cc.mode == mge::CharacterMode::Fly) ? 1 : 0;
    out_controller->speed = cc.walkSpeed;
    out_controller->jump_speed = cc.jumpSpeed;
    out_controller->gravity = cc.gravity;
    out_controller->is_grounded = cc.grounded;
    return MGE_OK;
}

bool mge_character_controller_has(mge_app* app, mge_entity entity) {
    if (!app || !app->world) return false;
    return app->world->registry().has<mge::CharacterController>(toEngineEntity(entity));
}

void mge_character_controller_remove(mge_app* app, mge_entity entity) {
    if (!app || !app->world) return;
    app->world->registry().remove<mge::CharacterController>(toEngineEntity(entity));
}

/* --- Built-in: RigidBody --- */
mge_result mge_rigid_body_set(mge_app* app, mge_entity entity, const mge_rigid_body* body) {
    if (!app || !app->world || !body) return MGE_ERROR_INVALID_ARGUMENT;
    mge::Entity e = toEngineEntity(entity);
    if (!app->world->registry().valid(e)) return MGE_ERROR_INVALID_HANDLE;

    mge::RigidBody rb;
    rb.velocity = glm::vec3(body->velocity[0], body->velocity[1], body->velocity[2]);
    rb.mass = body->mass;
    rb.restitution = body->restitution;
    rb.gravityScale = body->use_gravity ? body->gravity_scale : 0.0f;

    app->world->registry().add<mge::RigidBody>(e, rb);
    return MGE_OK;
}

mge_result mge_rigid_body_get(mge_app* app, mge_entity entity, mge_rigid_body* out_body) {
    if (!app || !app->world || !out_body) return MGE_ERROR_INVALID_ARGUMENT;
    mge::Entity e = toEngineEntity(entity);
    if (!app->world->registry().has<mge::RigidBody>(e)) return MGE_ERROR_NOT_FOUND;

    const auto& rb = app->world->registry().get<mge::RigidBody>(e);
    out_body->velocity[0] = rb.velocity.x;
    out_body->velocity[1] = rb.velocity.y;
    out_body->velocity[2] = rb.velocity.z;
    out_body->mass = rb.mass;
    out_body->restitution = rb.restitution;
    out_body->gravity_scale = rb.gravityScale;
    out_body->use_gravity = (rb.gravityScale > 0.0f);
    out_body->is_kinematic = (rb.mass <= 0.0f);
    return MGE_OK;
}

bool mge_rigid_body_has(mge_app* app, mge_entity entity) {
    if (!app || !app->world) return false;
    return app->world->registry().has<mge::RigidBody>(toEngineEntity(entity));
}

void mge_rigid_body_remove(mge_app* app, mge_entity entity) {
    if (!app || !app->world) return;
    app->world->registry().remove<mge::RigidBody>(toEngineEntity(entity));
}

/* --- Dynamic Runtime Components --- */
mge_component_id mge_component_register(mge_app* app, const char* name, size_t size, size_t align) {
    if (!app || !app->world || !name) {
        mge::c_impl::setLastError("mge_component_register: app o name es nulo");
        return MGE_INVALID_COMPONENT_ID;
    }
    const mge::ComponentTypeId id = app->world->registry().registerRuntimeComponent(name, size, align);
    if (id == mge::kInvalidComponentType) {
        mge::c_impl::setLastError("mge_component_register: parámetros inválidos o conflicto de layout");
        return MGE_INVALID_COMPONENT_ID;
    }
    return static_cast<mge_component_id>(id);
}

void* mge_component_add(mge_app* app, mge_entity entity, mge_component_id comp_id) {
    if (!app || !app->world) {
        mge::c_impl::setLastError("mge_component_add: app es nulo");
        return nullptr;
    }
    auto& reg = app->world->registry();
    mge::Entity e = toEngineEntity(entity);
    if (!reg.valid(e)) {
        mge::c_impl::setLastError("mge_component_add: entidad inválida");
        return nullptr;
    }
    if (!reg.isValidRuntimeComponent(comp_id)) {
        mge::c_impl::setLastError("mge_component_add: comp_id inválido");
        return nullptr;
    }
    void* ptr = reg.addRaw(e, comp_id);
    if (!ptr) {
        mge::c_impl::setLastError("mge_component_add: no se pudo agregar el componente");
    }
    return ptr;
}

void* mge_component_get(mge_app* app, mge_entity entity, mge_component_id comp_id) {
    if (!app || !app->world) {
        mge::c_impl::setLastError("mge_component_get: app es nulo");
        return nullptr;
    }
    auto& reg = app->world->registry();
    mge::Entity e = toEngineEntity(entity);
    if (!reg.valid(e)) {
        mge::c_impl::setLastError("mge_component_get: entidad inválida");
        return nullptr;
    }
    if (!reg.isValidRuntimeComponent(comp_id)) {
        mge::c_impl::setLastError("mge_component_get: comp_id inválido");
        return nullptr;
    }
    if (!reg.hasRaw(e, comp_id)) {
        mge::c_impl::setLastError("mge_component_get: la entidad no posee el componente");
        return nullptr;
    }
    return reg.getRaw(e, comp_id);
}

bool mge_component_has(mge_app* app, mge_entity entity, mge_component_id comp_id) {
    if (!app || !app->world) {
        mge::c_impl::setLastError("mge_component_has: app es nulo");
        return false;
    }
    const auto& reg = app->world->registry();
    mge::Entity e = toEngineEntity(entity);
    if (!reg.valid(e)) {
        mge::c_impl::setLastError("mge_component_has: entidad inválida");
        return false;
    }
    if (!reg.isValidRuntimeComponent(comp_id)) {
        mge::c_impl::setLastError("mge_component_has: comp_id inválido");
        return false;
    }
    return reg.hasRaw(e, comp_id);
}

void mge_component_remove(mge_app* app, mge_entity entity, mge_component_id comp_id) {
    if (!app || !app->world) {
        mge::c_impl::setLastError("mge_component_remove: app es nulo");
        return;
    }
    auto& reg = app->world->registry();
    mge::Entity e = toEngineEntity(entity);
    if (!reg.valid(e)) {
        mge::c_impl::setLastError("mge_component_remove: entidad inválida");
        return;
    }
    if (!reg.isValidRuntimeComponent(comp_id)) {
        mge::c_impl::setLastError("mge_component_remove: comp_id inválido");
        return;
    }
    reg.removeRaw(e, comp_id);
}

void mge_component_each(mge_app* app, const mge_component_id* comp_ids, size_t comp_count,
                        void (*callback)(mge_entity entity, void** components, void* user_data),
                        void* user_data) {
    if (!app || !app->world || !comp_ids || comp_count == 0 || !callback) {
        mge::c_impl::setLastError("mge_component_each: argumentos inválidos");
        return;
    }

    auto& reg = app->world->registry();
    std::vector<mge::ComponentTypeId> types(comp_count);
    for (size_t i = 0; i < comp_count; ++i) {
        if (!reg.isValidRuntimeComponent(comp_ids[i])) {
            mge::c_impl::setLastError("mge_component_each: comp_id inválido");
            return;
        }
        types[i] = comp_ids[i];
    }

    reg.eachRaw(types, [callback, user_data](mge::Entity e, void** ptrs) {
        callback(fromEngineEntity(e), ptrs, user_data);
    });
}

/* --- Systems --- */
mge_result mge_system_add(mge_app* app, mge_system_phase phase, const char* name,
                          void (*fn)(mge_app* app, float dt, void* user_data), void* user_data) {
    if (!app || !app->world || !fn) return MGE_ERROR_INVALID_ARGUMENT;

    mge::SystemPhase sp = mge::SystemPhase::Update;
    if (phase == MGE_PHASE_FIXED)
        sp = mge::SystemPhase::Fixed;
    else if (phase == MGE_PHASE_RENDER)
        sp = mge::SystemPhase::Render;

    std::string sysName = name ? name : "c_system";
    app->world->addSystem(sp, sysName,
                          [app, fn, user_data](mge::World&, float dt) { fn(app, dt, user_data); });

    return MGE_OK;
}

} // extern "C"
// NOLINTEND(readability-identifier-naming)
