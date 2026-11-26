// Copyright (C) 2017-2026 Chris Liebert

#ifndef _TERRAIN_H_
#define _TERRAIN_H_

#include <string>
#include <vector>

#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h"
#include "common/asset_manager.hpp"
#include "graphics/scene_graph.h"

namespace droidgfxsim {

class HeightmapTerrain {
public:
    HeightmapTerrain();
    ~HeightmapTerrain() = default;

    bool loadFromPNG(const std::string& filename,
                     AssetManager* asset_manager,
                     int width = 121,
                     int length = 121,
                     float scale_x = 0.75f,
                     float scale_z = 0.75f,
                     float min_height = -1.5f,
                     float max_height = 24.0f);

    void generateProcedural(int width = 121,
                            int length = 121,
                            float scale_x = 0.75f,
                            float scale_z = 0.75f,
                            float min_height = -1.5f,
                            float max_height = 24.0f);

    static float evalAnalyticalHeight(float world_x, float world_z);

    float getHeightAt(float world_x, float world_z) const;
    float getGridHeight(int x, int z) const;

    scenegraph::GeometryNode* buildGeometryNode(const std::string& name = "Arena_Terrain_Geometry") const;
    btHeightfieldTerrainShape* createCollisionShape() const;

    int getWidth() const { return width_; }
    int getLength() const { return length_; }
    float getScaleX() const { return scale_x_; }
    float getScaleZ() const { return scale_z_; }
    float getMinHeight() const { return min_height_; }
    float getMaxHeight() const { return max_height_; }
    const std::vector<float>& getHeightData() const { return height_data_; }

private:
    int width_;
    int length_;
    float scale_x_;
    float scale_z_;
    float min_height_;
    float max_height_;
    std::vector<float> height_data_;
};

} // namespace droidgfxsim

#endif // _TERRAIN_H_
