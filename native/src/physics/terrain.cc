// Copyright (C) 2017-2026 Chris Liebert

#include "physics/terrain.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

#include "common/MathUtil.h"
#include "common/log.h"
#include "graphics/gl_code.h"

namespace droidgfxsim {

namespace {

inline float smoothstep(float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float boxDistance2D(float x, float z, float x0, float x1, float z0, float z1) {
    float dx = std::max({x0 - x, 0.0f, x - x1});
    float dz = std::max({z0 - z, 0.0f, z - z1});
    return std::sqrt(dx * dx + dz * dz);
}

} // namespace

HeightmapTerrain::HeightmapTerrain()
    : width_(81),
      length_(81),
      scale_x_(1.0f),
      scale_z_(1.0f),
      min_height_(-1.6f),
      max_height_(6.9f) {}

float HeightmapTerrain::evalAnalyticalHeight(float world_x, float world_z) {
    float d1 = boxDistance2D(world_x, world_z, -12.5f, 12.5f, -16.5f, -7.5f);
    float d2 = boxDistance2D(world_x, world_z, -10.5f, -5.5f, -8.0f, 17.0f);
    float d3 = boxDistance2D(world_x, world_z, 5.5f, 10.5f, -8.0f, 17.0f);
    float d4 = boxDistance2D(world_x, world_z, -3.2f, 3.2f, 11.8f, 17.6f);

    float min_pad = std::min({d1, d2, d3, d4});
    float pad_weight = smoothstep(0.0f, 3.0f, min_pad);

    float h_hills = 0.85f * std::sin(0.16f * world_x) * std::cos(0.14f * world_z)
                  + 0.55f * std::cos(0.27f * world_x + 0.21f * world_z)
                  + 0.35f * std::sin(0.42f * world_x - 0.33f * world_z + 0.7f);

    float h_wings = 2.6f * smoothstep(11.0f, 26.0f, std::abs(world_x))
                  * (0.7f + 0.3f * std::sin(0.18f * world_z));

    float r_rim = std::max({std::abs(world_x) - 25.0f, std::abs(world_z) - 25.0f, 0.0f});
    float h_rim = 4.2f * smoothstep(0.0f, 14.0f, r_rim);

    return pad_weight * (h_hills + h_wings + h_rim);
}

void HeightmapTerrain::generateProcedural(int width, int length,
                                          float scale_x, float scale_z,
                                          float min_height, float max_height) {
    width_ = std::max(2, width);
    length_ = std::max(2, length);
    scale_x_ = (scale_x > 1e-5f) ? scale_x : 1.0f;
    scale_z_ = (scale_z > 1e-5f) ? scale_z : 1.0f;
    min_height_ = min_height;
    max_height_ = (max_height >= min_height) ? max_height : min_height;

    height_data_.resize(static_cast<size_t>(width_) * static_cast<size_t>(length_));
    const float half_w = 0.5f * static_cast<float>(width_ - 1);
    const float half_l = 0.5f * static_cast<float>(length_ - 1);

    for (int j = 0; j < length_; ++j) {
        float wz = (-half_l + static_cast<float>(j)) * scale_z_;
        for (int x = 0; x < width_; ++x) {
            float wx = (-half_w + static_cast<float>(x)) * scale_x_;
            float h = std::clamp(evalAnalyticalHeight(wx, wz), min_height_, max_height_);
            height_data_[static_cast<size_t>(j) * static_cast<size_t>(width_) + static_cast<size_t>(x)] = h;
        }
    }
}

bool HeightmapTerrain::loadFromPNG(const std::string& filename,
                                   AssetManager* asset_manager,
                                   int width, int length,
                                   float scale_x, float scale_z,
                                   float min_height, float max_height) {
    scale_x_ = (scale_x > 1e-5f) ? scale_x : 1.0f;
    scale_z_ = (scale_z > 1e-5f) ? scale_z : 1.0f;
    min_height_ = min_height;
    max_height_ = (max_height >= min_height) ? max_height : min_height;

    if (!asset_manager || filename.empty()) {
        LOGI("HeightmapTerrain: AssetManager or filename unavailable, generating procedural heightmap");
        generateProcedural(width, length, scale_x_, scale_z_, min_height_, max_height_);
        return false;
    }

    Image img(filename.c_str(), asset_manager);
    if (!img.data || img.w < 2 || img.h < 2 || img.comp < 1) {
        LOGI("HeightmapTerrain: Failed to load '%s', falling back to procedural heightmap", filename.c_str());
        generateProcedural(width, length, scale_x_, scale_z_, min_height_, max_height_);
        return false;
    }

    width_ = (width > 1) ? width : img.w;
    length_ = (length > 1) ? length : img.h;
    height_data_.resize(static_cast<size_t>(width_) * static_cast<size_t>(length_));

    const float height_span = max_height_ - min_height_;
    if (img.w == width_ && img.h == length_) {
        for (int j = 0; j < length_; ++j) {
            for (int x = 0; x < width_; ++x) {
                size_t pixel_idx = (static_cast<size_t>(j) * static_cast<size_t>(img.w) + static_cast<size_t>(x))
                                 * static_cast<size_t>(img.comp);
                unsigned char val = img.data[pixel_idx];
                float h = min_height_ + (static_cast<float>(val) / 255.0f) * height_span;
                height_data_[static_cast<size_t>(j) * static_cast<size_t>(width_) + static_cast<size_t>(x)] =
                    std::clamp(h, min_height_, max_height_);
            }
        }
    } else {
        for (int j = 0; j < length_; ++j) {
            float v = static_cast<float>(j) / static_cast<float>(length_ - 1);
            float fy = v * static_cast<float>(img.h - 1);
            int y0 = std::clamp(static_cast<int>(std::floor(fy)), 0, img.h - 1);
            int y1 = std::min(y0 + 1, img.h - 1);
            float ty = fy - static_cast<float>(y0);
            for (int x = 0; x < width_; ++x) {
                float u = static_cast<float>(x) / static_cast<float>(width_ - 1);
                float fx = u * static_cast<float>(img.w - 1);
                int x0 = std::clamp(static_cast<int>(std::floor(fx)), 0, img.w - 1);
                int x1 = std::min(x0 + 1, img.w - 1);
                float tx = fx - static_cast<float>(x0);

                auto sample = [&](int sx, int sy) -> float {
                    size_t idx = (static_cast<size_t>(sy) * static_cast<size_t>(img.w) + static_cast<size_t>(sx))
                               * static_cast<size_t>(img.comp);
                    return static_cast<float>(img.data[idx]);
                };

                float c00 = sample(x0, y0);
                float c10 = sample(x1, y0);
                float c01 = sample(x0, y1);
                float c11 = sample(x1, y1);
                float val = (1.0f - ty) * ((1.0f - tx) * c00 + tx * c10)
                          + ty * ((1.0f - tx) * c01 + tx * c11);
                float h = min_height_ + (val / 255.0f) * height_span;
                height_data_[static_cast<size_t>(j) * static_cast<size_t>(width_) + static_cast<size_t>(x)] =
                    std::clamp(h, min_height_, max_height_);
            }
        }
    }

    LOGI("HeightmapTerrain: Loaded %dx%d heightmap from '%s' (range [%.2f, %.2f])",
         width_, length_, filename.c_str(), min_height_, max_height_);
    return true;
}

float HeightmapTerrain::getGridHeight(int x, int z) const {
    if (height_data_.empty() || width_ < 1 || length_ < 1) {
        return 0.0f;
    }
    int cx = std::clamp(x, 0, width_ - 1);
    int cz = std::clamp(z, 0, length_ - 1);
    return height_data_[static_cast<size_t>(cz) * static_cast<size_t>(width_) + static_cast<size_t>(cx)];
}

float HeightmapTerrain::getHeightAt(float world_x, float world_z) const {
    if (height_data_.empty() || width_ < 2 || length_ < 2) {
        return 0.0f;
    }

    const float gx = std::clamp(world_x / scale_x_ + 0.5f * static_cast<float>(width_ - 1),
                                0.0f, static_cast<float>(width_ - 1));
    const float gz = std::clamp(world_z / scale_z_ + 0.5f * static_cast<float>(length_ - 1),
                                0.0f, static_cast<float>(length_ - 1));

    const int x0 = std::clamp(static_cast<int>(std::floor(gx)), 0, width_ - 2);
    const int z0 = std::clamp(static_cast<int>(std::floor(gz)), 0, length_ - 2);

    const float u = gx - static_cast<float>(x0);
    const float v = gz - static_cast<float>(z0);

    const float h00 = getGridHeight(x0, z0);
    const float h10 = getGridHeight(x0 + 1, z0);
    const float h01 = getGridHeight(x0, z0 + 1);
    const float h11 = getGridHeight(x0 + 1, z0 + 1);

    // Match Bullet btHeightfieldTerrainShape (upAxis=1, flipQuadEdges=false) diagonal u + v = 1:
    // Tri 1: (x, j), (x, j+1), (x+1, j)
    // Tri 2: (x+1, j), (x, j+1), (x+1, j+1)
    if (u + v <= 1.0f) {
        return h00 + u * (h10 - h00) + v * (h01 - h00);
    }
    return h11 + (1.0f - u) * (h01 - h11) + (1.0f - v) * (h10 - h11);
}

scenegraph::GeometryNode* HeightmapTerrain::buildGeometryNode(const std::string& name) const {
    scenegraph::GeometryNode* geom = new scenegraph::GeometryNode();
    geom->name = name;

    if (width_ < 2 || length_ < 2 || height_data_.empty()) {
        return geom;
    }

    const size_t num_verts = static_cast<size_t>(width_) * static_cast<size_t>(length_);
    const size_t num_indices = static_cast<size_t>(width_ - 1) * static_cast<size_t>(length_ - 1) * 6;
    geom->vertex_data.reserve(num_verts);
    geom->index_data.reserve(num_indices);

    const float half_w = 0.5f * static_cast<float>(width_ - 1);
    const float half_l = 0.5f * static_cast<float>(length_ - 1);
    const float inv_w = 1.0f / static_cast<float>(width_ - 1);
    const float inv_l = 1.0f / static_cast<float>(length_ - 1);

    for (int j = 0; j < length_; ++j) {
        float wz = (-half_l + static_cast<float>(j)) * scale_z_;
        float tv = static_cast<float>(j) * inv_l;
        int zm = std::max(0, j - 1);
        int zp = std::min(length_ - 1, j + 1);
        float dz = static_cast<float>(zp - zm) * scale_z_;

        for (int x = 0; x < width_; ++x) {
            float wx = (-half_w + static_cast<float>(x)) * scale_x_;
            float wy = getGridHeight(x, j);
            float tu = static_cast<float>(x) * inv_w;

            int xm = std::max(0, x - 1);
            int xp = std::min(width_ - 1, x + 1);
            float dx = static_cast<float>(xp - xm) * scale_x_;

            float dhdx = (dx > 1e-6f) ? (getGridHeight(xp, j) - getGridHeight(xm, j)) / dx : 0.0f;
            float dhdz = (dz > 1e-6f) ? (getGridHeight(x, zp) - getGridHeight(x, zm)) / dz : 0.0f;
            glm::vec3 n = glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));

            scenegraph::Vertex v{};
            v.position[0] = wx;
            v.position[1] = wy;
            v.position[2] = wz;
            v.normal[0] = n.x;
            v.normal[1] = n.y;
            v.normal[2] = n.z;
            v.texcoord[0] = tu;
            v.texcoord[1] = tv;
            geom->vertex_data.push_back(v);
        }
    }

    for (int j = 0; j < length_ - 1; ++j) {
        for (int x = 0; x < width_ - 1; ++x) {
            GLuint i00 = static_cast<GLuint>(j * width_ + x);
            GLuint i10 = i00 + 1;
            GLuint i01 = static_cast<GLuint>((j + 1) * width_ + x);
            GLuint i11 = i01 + 1;

            // Tri 1: (x, j), (x, j+1), (x+1, j)
            geom->index_data.push_back(i00);
            geom->index_data.push_back(i01);
            geom->index_data.push_back(i10);

            // Tri 2: (x+1, j), (x, j+1), (x+1, j+1)
            geom->index_data.push_back(i10);
            geom->index_data.push_back(i01);
            geom->index_data.push_back(i11);
        }
    }

    // Compute exact minimal enclosing Welzl bounding sphere without deep 6,561-frame recursion
    // (preserves vertex positions unchanged and remains safe on 1 MB default Windows/Android stacks).
    std::vector<glm::vec3> extremal;
    extremal.reserve(32);
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dz = -1; dz <= 1; ++dz) {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                glm::vec3 dir(static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz));
                float best_proj = -1e30f;
                glm::vec3 best_pt(0.0f);
                for (const auto& v : geom->vertex_data) {
                    glm::vec3 p(v.position[0], v.position[1], v.position[2]);
                    float proj = glm::dot(p, dir);
                    if (proj > best_proj) {
                        best_proj = proj;
                        best_pt = p;
                    }
                }
                bool dup = false;
                for (const auto& e : extremal) {
                    if (glm::dot(e - best_pt, e - best_pt) < 1e-10f) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) {
                    extremal.push_back(best_pt);
                }
            }
        }
    }

    Math::Sphere sphere = Math::calculateBoundingSphere(extremal.data(), extremal.size());
    for (const auto& v : geom->vertex_data) {
        glm::vec3 p(v.position[0], v.position[1], v.position[2]);
        float tol = std::max(1e-5f, sphere.radius * 1e-5f);
        float r_tol = sphere.radius + tol;
        if (glm::dot(p - sphere.center, p - sphere.center) > r_tol * r_tol) {
            extremal.push_back(p);
            if (extremal.size() <= 128) {
                sphere = Math::calculateBoundingSphere(extremal.data(), extremal.size());
            }
        }
    }

    geom->center[0] = sphere.center.x;
    geom->center[1] = sphere.center.y;
    geom->center[2] = sphere.center.z;
    geom->radius = sphere.radius;
    return geom;
}

btHeightfieldTerrainShape* HeightmapTerrain::createCollisionShape() const {
    if (height_data_.empty() || width_ < 2 || length_ < 2) {
        return nullptr;
    }
    btHeightfieldTerrainShape* shape = new btHeightfieldTerrainShape(
        width_,
        length_,
        height_data_.data(),
        btScalar(min_height_),
        btScalar(max_height_),
        1,
        false
    );
    shape->setLocalScaling(btVector3(btScalar(scale_x_), btScalar(1.0f), btScalar(scale_z_)));
    shape->buildAccelerator(16);
    return shape;
}

} // namespace droidgfxsim
