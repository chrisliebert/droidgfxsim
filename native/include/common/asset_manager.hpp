// Copyright (C) 2017-2026 Chris Liebert

#ifndef _ASSET_MANAGER_HPP_
#define _ASSET_MANAGER_HPP_

#include "common/log.h"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

#if defined(__ANDROID__)
    #include <android/asset_manager.h>
    #include <android/asset_manager_jni.h>
    class AssetManager {
        AAssetManager* mgr;
    public:
        AssetManager(AAssetManager* _android_mgr) : mgr(_android_mgr) {}

        std::string loadTextFile(const char* path) {
            if (!mgr || !path) return "";
            AAsset* file = AAssetManager_open(mgr, path, AASSET_MODE_BUFFER);
            if (!file) {
                LOGE("Failed to open Android asset: %s", path);
                return "";
            }
            size_t file_length = AAsset_getLength(file);
            std::vector<char> buffer(file_length + 1, '\0');
            AAsset_read(file, buffer.data(), file_length);
            AAsset_close(file);
            return std::string(buffer.data());
        }

        char* loadTextChars(const char* path) {
            if (!mgr || !path) return nullptr;
            AAsset* file = AAssetManager_open(mgr, path, AASSET_MODE_BUFFER);
            if (!file) return nullptr;
            size_t file_length = AAsset_getLength(file);
            char* fileContent = new char[file_length + 1];
            AAsset_read(file, fileContent, file_length);
            AAsset_close(file);
            fileContent[file_length] = '\0';
            return fileContent;
        }

        unsigned char* loadBinaryFile(const char* filename, size_t& file_length) {
            file_length = 0;
            if (!mgr || !filename) return nullptr;
            AAsset* file = AAssetManager_open(mgr, filename, AASSET_MODE_BUFFER);
            if (!file) return nullptr;
            file_length = AAsset_getLength(file);
            if (file_length == 0) {
                AAsset_close(file);
                return nullptr;
            }
            unsigned char* file_bytes = new unsigned char[file_length];
            AAsset_read(file, file_bytes, file_length);
            AAsset_close(file);
            return file_bytes;
        }
    };

#elif defined(DESKTOP_APP) || !defined(__ANDROID__)
#include <fstream>
#include <iostream>
#include <sstream>

class AssetManager {
private:
    std::string resolvePath(const std::string& path) const {
        if (path.empty()) return path;
        // 1. Direct path check
        {
            std::ifstream f(path, std::ios::binary);
            if (f.good()) return path;
        }
        // 2. Relative fallbacks
        const std::string candidates[] = {
            "app/src/main/assets/" + path,
            "../app/src/main/assets/" + path,
            "../../app/src/main/assets/" + path,
            "assets/" + path,
            "../assets/" + path
        };
        for (const auto& c : candidates) {
            std::ifstream f(c, std::ios::binary);
            if (f.good()) return c;
        }
        return path;
    }

public:
    std::string loadTextFile(const char* filename) {
        if (!filename) return "";
        std::string resolved = resolvePath(filename);
        std::ifstream filestream(resolved, std::ios::in);
        if (!filestream.is_open()) {
            LOGE("Unable to open text file: %s (resolved: %s)", filename, resolved.c_str());
            return "";
        }
        std::stringstream ss;
        ss << filestream.rdbuf();
        return ss.str();
    }

    char* loadTextChars(const char* path) {
        if (!path) return nullptr;
        std::string resolved = resolvePath(path);
        std::ifstream filestream(resolved, std::ios::in);
        if (!filestream.is_open()) {
            LOGE("Unable to open text file: %s (resolved: %s)", path, resolved.c_str());
            return nullptr;
        }
        std::stringstream ss;
        ss << filestream.rdbuf();
        std::string content = ss.str();
        char* file_contents = new char[content.size() + 1];
        std::memcpy(file_contents, content.c_str(), content.size() + 1);
        return file_contents;
    }

    unsigned char* loadBinaryFile(const char* filename, size_t& file_length) {
        file_length = 0;
        if (!filename) return nullptr;
        std::string resolved = resolvePath(filename);
        std::ifstream filestream(resolved, std::ios::in | std::ios::binary);
        if (!filestream.is_open()) {
            LOGE("Unable to open binary file: %s (resolved: %s)", filename, resolved.c_str());
            return nullptr;
        }
        filestream.seekg(0, std::ios::end);
        file_length = (size_t)filestream.tellg();
        if (file_length == 0) return nullptr;
        filestream.seekg(0, std::ios::beg);
        unsigned char* file_contents = new unsigned char[file_length];
        filestream.read(reinterpret_cast<char*>(file_contents), file_length);
        filestream.close();
        return file_contents;
    }
};

#endif

#endif // _ASSET_MANAGER_HPP_
