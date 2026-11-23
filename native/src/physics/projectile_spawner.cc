// Copyright (C) 2017-2026 Chris Liebert

#include "physics/projectile_spawner.h"
#include "physics/simulation.h"
#include "graphics/camera.h"

namespace droidgfxsim {

ProjectileSpawner::ProjectileSpawner(btDiscreteDynamicsWorld* dynamics_world, size_t max_capacity)
    : dynamics_world_(dynamics_world),
      max_capacity_(max_capacity),
      radius_(kDefaultRadius),
      mass_(kDefaultMass),
      speed_(kDefaultSpeed) {
    sphere_shape_ = new btSphereShape(radius_);
}

ProjectileSpawner::ProjectileSpawner(Simulation* simulation, size_t max_capacity)
    : ProjectileSpawner(simulation ? simulation->dynamics_world : nullptr, max_capacity) {}

ProjectileSpawner::~ProjectileSpawner() {
    reset();
    if (sphere_shape_) {
        delete sphere_shape_;
        sphere_shape_ = nullptr;
    }
}

ProjectileSpawner::ProjectileSpawner(ProjectileSpawner&& other) noexcept
    : dynamics_world_(other.dynamics_world_),
      sphere_shape_(other.sphere_shape_),
      pool_(std::move(other.pool_)),
      max_capacity_(other.max_capacity_),
      radius_(other.radius_),
      mass_(other.mass_),
      speed_(other.speed_) {
    other.dynamics_world_ = nullptr;
    other.sphere_shape_ = nullptr;
}

ProjectileSpawner& ProjectileSpawner::operator=(ProjectileSpawner&& other) noexcept {
    if (this != &other) {
        reset();
        if (sphere_shape_) {
            delete sphere_shape_;
        }

        dynamics_world_ = other.dynamics_world_;
        sphere_shape_ = other.sphere_shape_;
        pool_ = std::move(other.pool_);
        max_capacity_ = other.max_capacity_;
        radius_ = other.radius_;
        mass_ = other.mass_;
        speed_ = other.speed_;

        other.dynamics_world_ = nullptr;
        other.sphere_shape_ = nullptr;
    }
    return *this;
}

btRigidBody* ProjectileSpawner::spawn(const glm::vec3& position, const glm::vec3& direction,
                                     float speed) {
    if (!dynamics_world_ || !sphere_shape_) {
        return nullptr;
    }

    // Recycle oldest projectile if pool reaches max capacity
    if (pool_.size() >= max_capacity_) {
        ActiveProjectile oldest = pool_.front();
        pool_.pop_front();
        if (oldest.body) {
            dynamics_world_->removeRigidBody(oldest.body);
            delete oldest.body;
        }
        if (oldest.motion_state) {
            delete oldest.motion_state;
        }
    }

    btTransform start_transform;
    start_transform.setIdentity();
    start_transform.setOrigin(btVector3(position.x, position.y, position.z));

    btDefaultMotionState* motion_state = new btDefaultMotionState(start_transform);
    btVector3 local_inertia(0.0f, 0.0f, 0.0f);
    sphere_shape_->calculateLocalInertia(mass_, local_inertia);

    btRigidBody::btRigidBodyConstructionInfo rb_info(mass_, motion_state, sphere_shape_, local_inertia);
    rb_info.m_restitution = 0.6f;
    rb_info.m_friction = 0.7f;

    btRigidBody* body = new btRigidBody(rb_info);

    glm::vec3 norm_dir = (glm::length(direction) > 1e-6f)
                             ? glm::normalize(direction)
                             : glm::vec3(0.0f, 0.0f, -1.0f);

    body->setLinearVelocity(btVector3(norm_dir.x * speed, norm_dir.y * speed, norm_dir.z * speed));
    body->setCcdMotionThreshold(0.2f);
    body->setCcdSweptSphereRadius(0.2f);

    dynamics_world_->addRigidBody(body);
    pool_.push_back(ActiveProjectile{body, motion_state});

    return body;
}

btRigidBody* ProjectileSpawner::spawnFromCamera(const Camera* camera, float forward_offset,
                                               float speed) {
    if (!camera) {
        return nullptr;
    }

    glm::vec3 spawn_pos = camera->position + camera->direction * forward_offset;
    return spawn(spawn_pos, camera->direction, speed);
}

void ProjectileSpawner::reset() {
    for (auto& item : pool_) {
        if (item.body) {
            if (dynamics_world_) {
                dynamics_world_->removeRigidBody(item.body);
            }
            delete item.body;
        }
        if (item.motion_state) {
            delete item.motion_state;
        }
    }
    pool_.clear();
}

void ProjectileSpawner::setDynamicsWorld(btDiscreteDynamicsWorld* world) noexcept {
    if (dynamics_world_ != world) {
        reset();
        dynamics_world_ = world;
    }
}

} // namespace droidgfxsim
