// Copyright (C) 2017-2026 Chris Liebert

#include "physics/raycast_picker.h"
#include "graphics/camera.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <cmath>

namespace droidgfxsim {

RaycastPicker::RaycastPicker(btDiscreteDynamicsWorld* dynamics_world)
    : dynamics_world_(dynamics_world) {}

RaycastPicker::~RaycastPicker() {
    release();
}

RaycastPicker::RaycastPicker(RaycastPicker&& other) noexcept
    : dynamics_world_(other.dynamics_world_),
      picked_body_(other.picked_body_),
      constraint_(other.constraint_),
      local_pivot_(other.local_pivot_),
      hit_distance_(other.hit_distance_),
      is_dragging_(other.is_dragging_) {
    other.dynamics_world_ = nullptr;
    other.picked_body_ = nullptr;
    other.constraint_ = nullptr;
    other.is_dragging_ = false;
}

RaycastPicker& RaycastPicker::operator=(RaycastPicker&& other) noexcept {
    if (this != &other) {
        release();
        dynamics_world_ = other.dynamics_world_;
        picked_body_ = other.picked_body_;
        constraint_ = other.constraint_;
        local_pivot_ = other.local_pivot_;
        hit_distance_ = other.hit_distance_;
        is_dragging_ = other.is_dragging_;

        other.dynamics_world_ = nullptr;
        other.picked_body_ = nullptr;
        other.constraint_ = nullptr;
        other.is_dragging_ = false;
    }
    return *this;
}

Ray RaycastPicker::unproject(float screen_x, float screen_y, int window_w, int window_h,
                             const glm::mat4& inv_view_proj) {
    if (window_w <= 0 || window_h <= 0) {
        return Ray{glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f)};
    }

    // Convert screen coordinates (top-left 0,0) to Normalized Device Coordinates [-1, 1]
    float x_ndc = (2.0f * screen_x) / static_cast<float>(window_w) - 1.0f;
    float y_ndc = 1.0f - (2.0f * screen_y) / static_cast<float>(window_h);

    // Near and far points in clip space
    glm::vec4 clip_near(x_ndc, y_ndc, -1.0f, 1.0f);
    glm::vec4 clip_far(x_ndc, y_ndc, 1.0f, 1.0f);

    // Transform by inverse view-projection matrix
    glm::vec4 world_near = inv_view_proj * clip_near;
    glm::vec4 world_far = inv_view_proj * clip_far;

    // Perspective divide
    if (std::abs(world_near.w) > 1e-7f) {
        world_near /= world_near.w;
    }
    if (std::abs(world_far.w) > 1e-7f) {
        world_far /= world_far.w;
    }

    Ray ray;
    ray.origin = glm::vec3(world_near);
    glm::vec3 delta = glm::vec3(world_far - world_near);
    float len = glm::length(delta);
    if (len > 1e-6f) {
        ray.direction = delta / len;
    } else {
        ray.direction = glm::vec3(0.0f, 0.0f, -1.0f);
    }
    return ray;
}

Ray RaycastPicker::unproject(float screen_x, float screen_y, int window_w, int window_h,
                             const Camera* camera) {
    if (!camera) {
        return Ray{glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f)};
    }
    glm::mat4 view_proj = camera->projection_matrix * camera->modelview_matrix;
    glm::mat4 inv_view_proj = glm::inverse(view_proj);
    return unproject(screen_x, screen_y, window_w, window_h, inv_view_proj);
}

bool RaycastPicker::pick(const Ray& ray, float max_distance) {
    if (!dynamics_world_) {
        return false;
    }

    // Release any previously held constraint
    if (is_dragging_) {
        release();
    }

    btVector3 ray_from(ray.origin.x, ray.origin.y, ray.origin.z);
    btVector3 ray_to = ray_from + btVector3(ray.direction.x, ray.direction.y, ray.direction.z) * max_distance;

    btCollisionWorld::ClosestRayResultCallback ray_callback(ray_from, ray_to);
    dynamics_world_->rayTest(ray_from, ray_to, ray_callback);

    if (ray_callback.hasHit()) {
        btRigidBody* body = const_cast<btRigidBody*>(btRigidBody::upcast(ray_callback.m_collisionObject));
        if (body && !body->isStaticOrKinematicObject() && body->getInvMass() > 0.0f) {
            picked_body_ = body;
            body->setActivationState(DISABLE_DEACTIVATION);

            btVector3 hit_point_world = ray_callback.m_hitPointWorld;
            local_pivot_ = body->getCenterOfMassTransform().inverse() * hit_point_world;

            constraint_ = new btPoint2PointConstraint(*body, local_pivot_);
            constraint_->m_setting.m_impulseClamp = 30.0f;
            constraint_->m_setting.m_tau = 0.001f;

            dynamics_world_->addConstraint(constraint_, true);

            hit_distance_ = (hit_point_world - ray_from).length();
            is_dragging_ = true;
            return true;
        }
    }

    return false;
}

bool RaycastPicker::pick(float screen_x, float screen_y, int window_w, int window_h,
                         const glm::mat4& inv_view_proj, float max_distance) {
    Ray ray = unproject(screen_x, screen_y, window_w, window_h, inv_view_proj);
    return pick(ray, max_distance);
}

bool RaycastPicker::pick(float screen_x, float screen_y, int window_w, int window_h,
                         const Camera* camera, float max_distance) {
    Ray ray = unproject(screen_x, screen_y, window_w, window_h, camera);
    return pick(ray, max_distance);
}

void RaycastPicker::drag(const Ray& ray) {
    if (!is_dragging_ || !constraint_) {
        return;
    }

    glm::vec3 target = ray.origin + ray.direction * hit_distance_;
    constraint_->setPivotB(btVector3(target.x, target.y, target.z));

    if (picked_body_) {
        picked_body_->activate(true);
    }
}

void RaycastPicker::drag(float screen_x, float screen_y, int window_w, int window_h,
                         const glm::mat4& inv_view_proj) {
    Ray ray = unproject(screen_x, screen_y, window_w, window_h, inv_view_proj);
    drag(ray);
}

void RaycastPicker::drag(float screen_x, float screen_y, int window_w, int window_h,
                         const Camera* camera) {
    Ray ray = unproject(screen_x, screen_y, window_w, window_h, camera);
    drag(ray);
}

void RaycastPicker::release() {
    if (constraint_) {
        if (dynamics_world_) {
            dynamics_world_->removeConstraint(constraint_);
        }
        delete constraint_;
        constraint_ = nullptr;
    }

    if (picked_body_) {
        picked_body_->forceActivationState(ACTIVE_TAG);
        picked_body_->setDeactivationTime(0.0f);
        picked_body_ = nullptr;
    }

    is_dragging_ = false;
    hit_distance_ = 0.0f;
}

} // namespace droidgfxsim
