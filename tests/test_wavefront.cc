#include "test_helpers.h"
#include "common/asset_manager.hpp"
#include "graphics/wavefront_factory.h"

int runWavefrontTests() {
    int failures = 0;

    AssetManager mgr;
    WavefrontSceneGraphFactory factory;

    bool loaded = factory.addWavefront("test.obj", glm::mat4(1.0f), &mgr);
    if (!loaded) {
        std::cerr << "WavefrontTest: Failed to load test.obj" << std::endl;
        failures++;
        return failures;
    }

    scenegraph::Node* root = factory.build();
    if (!root) {
        std::cerr << "WavefrontTest: Factory build returned null" << std::endl;
        failures++;
        return failures;
    }

    // Verify root has children
    if (root->children.empty()) {
        std::cerr << "WavefrontTest: Root node has no children" << std::endl;
        failures++;
    }

    scenegraph::destroy(root);
    return failures;
}
