// Copyright (C) 2017-2026 Chris Liebert
// Interactive Mouse Raycast Picking and btPoint2PointConstraint dragging

#ifndef _DROIDGFXSIM_RAYCAST_PICKER_H_
#define _DROIDGFXSIM_RAYCAST_PICKER_H_

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "btBulletDynamicsCommon.h"

class Camera;

namespace droidgfxsim {

/// @brief World-space mathematical ray with origin and normalized direction.
struct Ray {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
};

/// @brief Handles unprojection from screen coordinates and dynamic rigid body dragging via Bullet constraints.
class RaycastPicker {
public:
    explicit RaycastPicker(btDiscreteDynamicsWorld* dynamics_world = nullptr);
    ~RaycastPicker();

    // Non-copyable, movable
    RaycastPicker(const RaycastPicker&) = delete;
    RaycastPicker& operator=(const RaycastPicker&) = delete;
    RaycastPicker(RaycastPicker&& other) noexcept;
    RaycastPicker& operator=(RaycastPicker&& other) noexcept;

    /// @brief Convert screen pixel coordinates into a normalized world-space Ray using inverse view-projection matrix.
    /// @param screen_x Mouse X coordinate in window space (0 to window_w).
    /// @param screen_y Mouse Y coordinate in window space (0 to window_h). Origin at top-left.
    /// @param window_w Window width in pixels.
    /// @param window_h Window height in pixels.
    /// @param inv_view_proj Inverse of combined view-projection matrix (P * V)^(-1).
    static Ray unproject(float screen_x, float screen_y, int window_w, int window_h,
                         const glm::mat4& inv_view_proj);

    /// @brief Overload computing unprojection directly from Camera view & projection matrices.
    static Ray unproject(float screen_x, float screen_y, int window_w, int window_h,
                         const Camera* camera);

    /// @brief Cast a ray into the physics world and attach a point-to-point constraint if a dynamic body is hit.
    /// @param ray World-space ray.
    /// @param max_distance Maximum ray test range in world units.
    /// @return True if a dynamic body was successfully picked and constraint attached.
    bool pick(const Ray& ray, float max_distance = 1000.0f);

    /// @brief Overload performing unprojection and pick in a single call.
    bool pick(float screen_x, float screen_y, int window_w, int window_h,
              const glm::mat4& inv_view_proj, float max_distance = 1000.0f);

    /// @brief Overload performing unprojection and pick from camera.
    bool pick(float screen_x, float screen_y, int window_w, int window_h,
              const Camera* camera, float max_distance = 1000.0f);

    /// @brief Update constraint pivot target along the camera ray preserving picked hit distance.
    void drag(const Ray& ray);

    /// @brief Overload updating drag position from screen coordinates and inverse view-projection matrix.
    void drag(float screen_x, float screen_y, int window_w, int window_h,
              const glm::mat4& inv_view_proj);

    /// @brief Overload updating drag position from screen coordinates and Camera.
    void drag(float screen_x, float screen_y, int window_w, int window_h,
              const Camera* camera);

    /// @brief Release picked body, remove constraint from dynamics world, and reactivate body.
    void release();

    /// @brief Check if an object is currently being dragged.
    [[nodiscard]] bool isDragging() const noexcept { return is_dragging_; }

    /// @brief Get the currently picked rigid body (or nullptr).
    [[nodiscard]] btRigidBody* getPickedBody() const noexcept { return picked_body_; }

    /// @brief Get the active point-to-point constraint (or nullptr).
    [[nodiscard]] btPoint2PointConstraint* getConstraint() const noexcept { return constraint_; }

    /// @brief Get distance from ray origin to the initial hit point.
    [[nodiscard]] float getHitDistance() const noexcept { return hit_distance_; }

    /// @brief Get the local pivot in body coordinates.
    [[nodiscard]] const btVector3& getLocalPivot() const noexcept { return local_pivot_; }

    /// @brief Update or set dynamics world reference.
    void setDynamicsWorld(btDiscreteDynamicsWorld* world) noexcept { dynamics_world_ = world; }

private:
    btDiscreteDynamicsWorld* dynamics_world_{nullptr};
    btRigidBody* picked_body_{nullptr};
    btPoint2PointConstraint* constraint_{nullptr};
    btVector3 local_pivot_{0.0f, 0.0f, 0.0f};
    float hit_distance_{0.0f};
    bool is_dragging_{false};
};

} // namespace droidgfxsim

#endif // _DROIDGFXSIM_RAYCAST_PICKER_H_
