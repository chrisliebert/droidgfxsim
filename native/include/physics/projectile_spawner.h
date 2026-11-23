// Copyright (C) 2017-2026 Chris Liebert
// Projectile Spawner for launching dynamic physics sphere cannonballs

#ifndef _DROIDGFXSIM_PROJECTILE_SPAWNER_H_
#define _DROIDGFXSIM_PROJECTILE_SPAWNER_H_

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <deque>
#include <vector>

#include "btBulletDynamicsCommon.h"

class Camera;
class Simulation;

namespace droidgfxsim {

/// @brief Tracks individual projectile rigid bodies and motion states in the active pool.
struct ActiveProjectile {
    btRigidBody* body{nullptr};
    btDefaultMotionState* motion_state{nullptr};
};

/// @brief Manages spawning and lifecycle of high-speed cannonball sphere rigid bodies.
class ProjectileSpawner {
public:
    static constexpr float kDefaultRadius = 0.4f;
    static constexpr float kDefaultMass = 5.0f;
    static constexpr float kDefaultSpeed = 35.0f; // 35 m/s
    static constexpr size_t kDefaultMaxCapacity = 32;

    explicit ProjectileSpawner(btDiscreteDynamicsWorld* dynamics_world = nullptr,
                               size_t max_capacity = kDefaultMaxCapacity);
    explicit ProjectileSpawner(Simulation* simulation,
                               size_t max_capacity = kDefaultMaxCapacity);
    ~ProjectileSpawner();

    // Non-copyable, movable
    ProjectileSpawner(const ProjectileSpawner&) = delete;
    ProjectileSpawner& operator=(const ProjectileSpawner&) = delete;
    ProjectileSpawner(ProjectileSpawner&& other) noexcept;
    ProjectileSpawner& operator=(ProjectileSpawner&& other) noexcept;

    /// @brief Spawn a dynamic sphere projectile launched along the given direction vector.
    /// @param position Starting world position.
    /// @param direction Normalized trajectory direction.
    /// @param speed Launch velocity magnitude in m/s (default 35.0 m/s).
    /// @return Pointer to the spawned btRigidBody or nullptr on failure.
    btRigidBody* spawn(const glm::vec3& position, const glm::vec3& direction,
                       float speed = kDefaultSpeed);

    /// @brief Spawn projectile from camera viewpoint.
    /// @param camera Camera providing position and direction.
    /// @param forward_offset Offset along direction to prevent clipping with camera plane.
    /// @param speed Launch velocity magnitude in m/s.
    /// @return Pointer to spawned btRigidBody or nullptr.
    btRigidBody* spawnFromCamera(const Camera* camera, float forward_offset = 1.0f,
                                 float speed = kDefaultSpeed);

    /// @brief Remove all active projectiles from the physics world and free resources.
    void reset();

    /// @brief Update active dynamics world.
    void setDynamicsWorld(btDiscreteDynamicsWorld* world) noexcept;

    /// @brief Number of active projectiles currently in the world.
    [[nodiscard]] size_t activeCount() const noexcept { return pool_.size(); }

    /// @brief Maximum active capacity of the projectile pool.
    [[nodiscard]] size_t maxCapacity() const noexcept { return max_capacity_; }

    /// @brief Get sphere radius in meters.
    [[nodiscard]] float getRadius() const noexcept { return radius_; }

    /// @brief Get projectile mass in kilograms.
    [[nodiscard]] float getMass() const noexcept { return mass_; }

    /// @brief Get default launch speed in m/s.
    [[nodiscard]] float getDefaultSpeed() const noexcept { return speed_; }

    /// @brief Access active projectile pool.
    [[nodiscard]] const std::deque<ActiveProjectile>& getPool() const noexcept { return pool_; }

private:
    btDiscreteDynamicsWorld* dynamics_world_{nullptr};
    btSphereShape* sphere_shape_{nullptr};
    std::deque<ActiveProjectile> pool_;
    size_t max_capacity_{kDefaultMaxCapacity};
    float radius_{kDefaultRadius};
    float mass_{kDefaultMass};
    float speed_{kDefaultSpeed};
};

} // namespace droidgfxsim

#endif // _DROIDGFXSIM_PROJECTILE_SPAWNER_H_
