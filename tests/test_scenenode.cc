#include "test_helpers.h"
#include "graphics/scene_graph.h"
#include "common/concepts.hpp"
#include "common/memory_arena.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

using droidgfxsim::ArenaScope;
using droidgfxsim::LinearMemoryResource;
using droidgfxsim::MemoryArena;

int runSceneNodeTests() {
    int failures = 0;

    // Test 1: Node creation and hierarchy
    {
        scenegraph::Node* root = new scenegraph::Node();
        root->name = "Root";

        scenegraph::TransformNode* trans = new scenegraph::TransformNode();
        trans->name = "PlayerTransform";

        scenegraph::GeometryNode* geom = new scenegraph::GeometryNode();
        geom->name = "PlayerMesh";

        scenegraph::Vertex v1{{-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}};
        scenegraph::Vertex v2{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}};
        geom->vertex_data.push_back(v1);
        geom->vertex_data.push_back(v2);
        geom->computeWelzlBoundingSphere();

        trans->children.push_back(geom);
        root->children.push_back(trans);

        // Find by name
        scenegraph::Node* found = root->find("PlayerTransform");
        if (!found || found != trans) {
            std::cerr << "SceneNodeTest: Failed to find PlayerTransform" << std::endl;
            failures++;
        }

        // Find by name and type
        scenegraph::TransformNode* foundTrans = scenegraph::find_transform_node("PlayerTransform", root);
        if (!foundTrans || foundTrans != trans) {
            std::cerr << "SceneNodeTest: find_transform_node mismatch" << std::endl;
            failures++;
        }

        // Verify Welzl sphere was computed
        if (geom->radius <= 0.0f) {
            std::cerr << "SceneNodeTest: Geometry bounding sphere radius invalid: " << geom->radius << std::endl;
            failures++;
        }

        // Clean destruction via virtual destructors
        scenegraph::destroy(root);
    }

    // Test 2: LinearMemoryResource C++20 Concept Enforcement
    {
        static_assert(LinearMemoryResource<MemoryArena>, "MemoryArena must fulfill LinearMemoryResource concept");
        static_assert(!LinearMemoryResource<int>, "Primitive types must not fulfill LinearMemoryResource");
        static_assert(!LinearMemoryResource<std::string>, "std::string must not fulfill LinearMemoryResource");
    }

    // Test 3: MemoryArena 64-byte Cache Alignment and Base Buffer
    {
        MemoryArena arena(4096);
        ASSERT_TRUE(arena.capacity() == 4096);
        ASSERT_TRUE(arena.used_bytes() == 0);
        ASSERT_TRUE(arena.data() != nullptr);

        // Verify base buffer is 64-byte cache-line aligned
        uintptr_t base_addr = reinterpret_cast<uintptr_t>(arena.data());
        ASSERT_TRUE(base_addr % MemoryArena::kDefaultCacheAlignment == 0);
        ASSERT_TRUE(base_addr % 64 == 0);

        // Allocate with 64-byte alignment
        void* ptr64_1 = arena.allocate(128, 64);
        ASSERT_TRUE(ptr64_1 != nullptr);
        ASSERT_TRUE(reinterpret_cast<uintptr_t>(ptr64_1) % 64 == 0);

        // Allocate with 32-byte alignment
        void* ptr32 = arena.allocate(32, 32);
        ASSERT_TRUE(ptr32 != nullptr);
        ASSERT_TRUE(reinterpret_cast<uintptr_t>(ptr32) % 32 == 0);

        // Allocate another with 64-byte alignment (forces alignment padding after 32 bytes)
        void* ptr64_2 = arena.allocate(64, 64);
        ASSERT_TRUE(ptr64_2 != nullptr);
        ASSERT_TRUE(reinterpret_cast<uintptr_t>(ptr64_2) % 64 == 0);
        ASSERT_TRUE(reinterpret_cast<uintptr_t>(ptr64_2) >= reinterpret_cast<uintptr_t>(ptr32) + 32);

        // Verify used_bytes is tracked accurately
        ASSERT_TRUE(arena.used_bytes() <= arena.capacity());
        ASSERT_TRUE(arena.used_bytes() >= 128 + 32 + 64);
    }

    // Test 4: Bump Allocation, Watermarks, and Capacity Boundaries
    {
        constexpr size_t kArenaSize = 1024;
        MemoryArena arena(kArenaSize);

        size_t initial_marker = arena.get_marker();
        ASSERT_TRUE(initial_marker == 0);

        // Sequential bump allocation
        void* p1 = arena.allocate(128, 8);
        ASSERT_TRUE(p1 != nullptr);
        size_t m1 = arena.get_marker();
        ASSERT_TRUE(m1 >= 128);

        void* p2 = arena.allocate(256, 8);
        ASSERT_TRUE(p2 != nullptr);
        ASSERT_TRUE(reinterpret_cast<uintptr_t>(p2) >= reinterpret_cast<uintptr_t>(p1) + 128);
        size_t m2 = arena.get_marker();
        ASSERT_TRUE(m2 >= m1 + 256);

        // Test marker rewind
        arena.reset_to_marker(m1);
        ASSERT_TRUE(arena.used_bytes() == m1);
        ASSERT_TRUE(arena.get_marker() == m1);

        // Re-allocate over rewound region
        void* p3 = arena.allocate(256, 8);
        ASSERT_TRUE(p3 == p2); // Bump allocator returns same memory location when rewound to m1

        // Full reset
        arena.reset();
        ASSERT_TRUE(arena.used_bytes() == 0);
        ASSERT_TRUE(arena.get_marker() == 0);

        // Test boundary exhaustion
        void* p_large = arena.allocate(kArenaSize, 64);
        ASSERT_TRUE(p_large != nullptr);
        // Next allocation exceeds capacity
        void* p_overflow = arena.allocate(16, 8);
        ASSERT_TRUE(p_overflow == nullptr);
    }

    // Test 5: allocate_span<float> and Placement Construction
    {
        MemoryArena arena(2048);

        // Zero count returns empty span
        auto empty_span = arena.allocate_span<float>(0);
        ASSERT_TRUE(empty_span.empty());
        ASSERT_TRUE(empty_span.size() == 0);

        // Allocate 16 floats
        constexpr size_t kCount = 16;
        std::span<float> float_span = arena.allocate_span<float>(kCount);
        ASSERT_TRUE(!float_span.empty());
        ASSERT_TRUE(float_span.size() == kCount);
        ASSERT_TRUE(float_span.data() != nullptr);
        ASSERT_TRUE(reinterpret_cast<uintptr_t>(float_span.data()) % alignof(float) == 0);

        // Write and read values through span
        for (size_t i = 0; i < kCount; ++i) {
            float_span[i] = static_cast<float>(i) * 3.14159f;
        }

        for (size_t i = 0; i < kCount; ++i) {
            ASSERT_NEAR(float_span[i], static_cast<float>(i) * 3.14159f, 1e-4f);
        }

        // Object creation via create<T>
        struct SampleData {
            int x;
            float y;
            int z;
            SampleData(int x_in, float y_in, int z_in) : x(x_in), y(y_in), z(z_in) {}
        };
        SampleData* obj = arena.create<SampleData>(42, 2.718f, 100);
        ASSERT_TRUE(obj != nullptr);
        ASSERT_TRUE(obj->x == 42);
        ASSERT_NEAR(obj->y, 2.718f, 1e-4f);
        ASSERT_TRUE(obj->z == 100);
    }

    // Test 6: ArenaScope Move Semantics and Watermark Transfer
    {
        MemoryArena arena(4096);
        arena.allocate(64, 16);
        const size_t baseline = arena.used_bytes();

        // Subtest 6A: Standard RAII unwinding
        {
            ArenaScope scope(arena);
            ASSERT_TRUE(scope.is_active());
            ASSERT_TRUE(scope.saved_marker() == baseline);
            arena.allocate(256, 16);
            ASSERT_TRUE(arena.used_bytes() > baseline);
        }
        ASSERT_TRUE(arena.used_bytes() == baseline);

        // Subtest 6B: Explicit move construction transfers ownership
        {
            ArenaScope scope1(arena);
            ASSERT_TRUE(scope1.is_active());
            ASSERT_TRUE(scope1.saved_marker() == baseline);
            arena.allocate(128, 16);
            const size_t post_alloc = arena.used_bytes();
            ASSERT_TRUE(post_alloc > baseline);

            // Move-construct scope2 from scope1
            ArenaScope scope2(std::move(scope1));
            // scope1 must be disarmed
            ASSERT_FALSE(scope1.is_active());
            // scope2 must be active and own baseline marker
            ASSERT_TRUE(scope2.is_active());
            ASSERT_TRUE(scope2.saved_marker() == baseline);

            arena.allocate(128, 16);
            ASSERT_TRUE(arena.used_bytes() > post_alloc);
            // When scope1 destructs at end of block, it must do nothing (no assertion failure)
        }
        // At block exit, scope2 destructed and rewound arena to baseline
        ASSERT_TRUE(arena.used_bytes() == baseline);

        // Subtest 6C: Helper function returning ArenaScope (RVO / move across call stack)
        auto create_scoped_allocator = [](MemoryArena& a) -> ArenaScope {
            ArenaScope s(a);
            a.allocate(200, 16);
            return s; // transfers cleanup ownership to caller
        };

        const size_t before_call = arena.used_bytes();
        {
            ArenaScope transferred_scope = create_scoped_allocator(arena);
            ASSERT_TRUE(transferred_scope.is_active());
            ASSERT_TRUE(transferred_scope.saved_marker() == before_call);
            ASSERT_TRUE(arena.used_bytes() >= before_call + 200);

            arena.allocate(100, 16);
        }
        // Upon transferred_scope destruction, arena is rewound cleanly to before_call
        ASSERT_TRUE(arena.used_bytes() == before_call);

        // Subtest 6D: Nested scopes where inner is moved into outer std::optional
        {
            std::optional<ArenaScope> outer_scope;
            {
                ArenaScope inner_scope(arena);
                arena.allocate(300, 16);
                outer_scope.emplace(std::move(inner_scope));
                ASSERT_FALSE(inner_scope.is_active());
                // inner_scope destructs here: must NOT rewind or assert
            }
            // Memory allocated in inner scope remains valid and arena used_bytes unchanged
            ASSERT_TRUE(arena.used_bytes() >= before_call + 300);
            ASSERT_TRUE(outer_scope.has_value());
            ASSERT_TRUE(outer_scope->is_active());
            ASSERT_TRUE(outer_scope->saved_marker() == before_call);
        }
        // outer_scope destructs here: rewinds to before_call
        ASSERT_TRUE(arena.used_bytes() == before_call);
    }

    return failures;
}
