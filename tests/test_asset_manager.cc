#include "test_helpers.h"
#include "common/asset_manager.hpp"

int runAssetManagerTests() {
    int failures = 0;

    AssetManager mgr;

    // Test 1: Load data.xml
    {
        std::string xml = mgr.loadTextFile("data.xml");
        if (xml.empty()) {
            std::cerr << "AssetManagerTest: Failed to load data.xml" << std::endl;
            failures++;
        }
        if (xml.find("<WavefrontFile") == std::string::npos) {
            std::cerr << "AssetManagerTest: data.xml content invalid" << std::endl;
            failures++;
        }
    }

    // Test 2: Non-existent file error handling
    {
        std::string missing = mgr.loadTextFile("non_existent_file_xyz.txt");
        if (!missing.empty()) {
            std::cerr << "AssetManagerTest: Non-existent file should return empty string" << std::endl;
            failures++;
        }
    }

    // Test 3: Load binary file
    {
        size_t len = 0;
        unsigned char* data = mgr.loadBinaryFile("debug.png", len);
        if (!data || len == 0) {
            std::cerr << "AssetManagerTest: Failed to load debug.png" << std::endl;
            failures++;
        } else {
            // PNG signature check (0x89 'P' 'N' 'G')
            if (data[0] != 0x89 || data[1] != 'P' || data[2] != 'N' || data[3] != 'G') {
                std::cerr << "AssetManagerTest: debug.png header invalid" << std::endl;
                failures++;
            }
            delete [] data;
        }
    }

    return failures;
}
