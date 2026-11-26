// Copyright (C) 2017-2026 Chris Liebert
// Modern C++20 Concepts Architecture for droidgfxsim

#ifndef _DROIDGFXSIM_CONCEPTS_HPP_
#define _DROIDGFXSIM_CONCEPTS_HPP_

#include <concepts>
#include <cstddef>
#include <span>
#include <string>
#include <type_traits>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "LinearMath/btVector3.h"

namespace scenegraph {
    class Node;
}
class Camera;

namespace droidgfxsim {

/// @brief Concept constraining rendering backend implementations.
/// Requires a render method accepting a scenegraph node hierarchy and a camera view.
template <typename T>
concept RenderBackend = requires(T& renderer, scenegraph::Node* node, Camera* camera) {
    { renderer.render(node, camera) } -> std::same_as<void>;
};

/// @brief Concept constraining spatial entities possessing a transformation matrix.
template <typename T>
concept SpatialEntity = requires(const T& entity) {
    { entity.matrix } -> std::convertible_to<glm::mat4>;
} || requires(const T& entity) {
    { entity.getTransform() } -> std::convertible_to<glm::mat4>;
};

/// @brief Concept constraining geometric bounding volumes with a center point and radial boundary.
template <typename T>
concept BoundingVolume = requires(const T& volume) {
    { volume.radius } -> std::convertible_to<float>;
} && (
    requires(const T& volume) {
        { volume.center } -> std::convertible_to<glm::vec3>;
    } ||
    requires(const T& volume) {
        { volume.center[0] } -> std::convertible_to<float>;
        { volume.center[1] } -> std::convertible_to<float>;
        { volume.center[2] } -> std::convertible_to<float>;
    }
);

/// @brief Concept constraining physics objects or simulation managers that can receive collision impulses or forces.
template <typename T>
concept CollisionReceiver = requires(T& receiver, const btVector3& force, const btVector3& rel_pos) {
    { receiver.applyForce(force, rel_pos) } -> std::same_as<void>;
} || requires(T& receiver, const std::string& name, const btVector3& force, const btVector3& rel_pos) {
    { receiver.applyForce(name, force, rel_pos) } -> std::same_as<void>;
};

/// @brief Concept constraining high-performance linear bump-allocation memory resources.
template <typename T>
concept LinearMemoryResource = requires(T& res, const T& const_res, size_t bytes, size_t alignment) {
    { res.allocate(bytes, alignment) } -> std::convertible_to<void*>;
    { res.reset() } noexcept -> std::same_as<void>;
    { const_res.used_bytes() } noexcept -> std::convertible_to<size_t>;
    { const_res.capacity() } noexcept -> std::convertible_to<size_t>;
};

} // namespace droidgfxsim

#endif // _DROIDGFXSIM_CONCEPTS_HPP_
