#ifndef _ASSET_MANAGER_HPP_
#define _ASSET_MANAGER_HPP_

#include "common/log.h"
#include <cassert>
#include <cstring>
#include <string>

#if defined(__ANDROID__)
    #include <android/asset_manager.h>
    #include <android/asset_manager_jni.h>
    class AssetManager {
        AAssetManager* mgr;
    public:
        AssetManager(AAssetManager* _android_mgr) {
            mgr = _android_mgr;
        }

        std::string loadTextFile(const char* path) {
            assert(mgr);
            AAsset* file = AAssetManager_open(mgr, path, AASSET_MODE_BUFFER);
            // Get the file length
            size_t fileLength = AAsset_getLength(file);

            // Allocate memory to read file
            char* fileContent = new char[fileLength+1];
            assert(fileContent);

            AAsset_read(file, fileContent, fileLength);
            AAsset_close(file);
            fileContent[fileLength] = '\0';

            std::string str(fileContent);
            delete [] fileContent;
            return str;
        }

        char* loadTextChars(const char* path) {
            AAsset* file = AAssetManager_open(mgr, path, AASSET_MODE_BUFFER);
            // Get the file length
            size_t fileLength = AAsset_getLength(file);
            // Allocate memory to read file
            char* fileContent = new char[fileLength+1];
            assert(fileContent);
            AAsset_read(file, fileContent, fileLength);
            AAsset_close(file);
            fileContent[fileLength] = '\0';
            return fileContent;
        }

        unsigned char* loadBinaryFile(const char* filename, size_t& file_length) {
            AAsset* file = AAssetManager_open(mgr, filename, AASSET_MODE_BUFFER);
            file_length = AAsset_getLength(file);
            if(file_length == 0) return 0;
            unsigned char* file_bytes = new unsigned char[file_length];
            assert(file_bytes);
            AAsset_read(file, file_bytes, file_length);
            AAsset_close(file);
            return file_bytes;
        }
    };

#elif defined(DESKTOP_APP)
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

class AssetManager {
public:
    std::string loadTextFile(const char* filename) {
    	std::ifstream filestream(filename);
		std::vector<char> buffer((std::istreambuf_iterator<char>(filestream)), std::istreambuf_iterator<char>());
		filestream.close();
		buffer.push_back('\0');
		return std::string(buffer.data());
	}

	char* loadTextChars(const char* path) {
    	std::ifstream filestream(path);
		std::vector<char> buffer((std::istreambuf_iterator<char>(filestream)), std::istreambuf_iterator<char>());
		filestream.close();
		buffer.push_back('\0');
		char* file_contents = new char[buffer.size()];
		assert(file_contents);
		memcpy(file_contents, buffer.data(), buffer.size());
		return file_contents;
    }

    unsigned char* loadBinaryFile(const char* filename, size_t& file_length) {
       std::ifstream filestream(filename, std::ios::in | std::ios::binary);
       file_length = 0;
       if (!filestream.is_open()) {
    	   LOGE("Unable to open %s", filename);
    	   return 0;
       }
       filestream.seekg(0, std::ios::end);
       file_length = filestream.tellg();
       if(file_length == 0) return 0;
       filestream.seekg(0, std::ios::beg);
       unsigned char* file_contents = new unsigned char[file_length];
       assert(file_contents);
       filestream.seekg(0, std::ios::beg);
       filestream.read((char*)file_contents, file_length);
       filestream.close();
       return file_contents;
    }
};

#endif

#endif // _ASSET_MANAGER_HPP_
