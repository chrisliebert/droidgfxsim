// Copyright (C) 2017-2026 Chris Liebert

#include "graphics/scene_presets.h"
#include "application/application.h"
#include "graphics/camera.h"
#include "physics/simulation.h"
#include "common/asset_manager.hpp"
#include "common/log.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace droidgfxsim {

namespace {

const PresetMetadata kPresets[ScenePresets::kPresetCount] = {
    {
        0,
        "Jenga Tower",
        "High-aspect rectangular block tower testing contact solver stability",
        "scenes/jenga.xml",
        glm::vec3(0.0f, 15.0f, 32.0f),
        0.0,
        -0.2
    },
    {
        1,
        "Wrecking Ball",
        "Suspended heavy pendulum swinging into structural target wall",
        "scenes/wrecking_ball.xml",
        glm::vec3(-18.0f, 12.0f, 26.0f),
        0.5,
        -0.2
    },
    {
        2,
        "Compound Shapes",
        "Rolling spheres, tumbling cylinders, and bouncing boxes on incline planes",
        "scenes/compound.xml",
        glm::vec3(0.0f, 16.0f, 28.0f),
        0.0,
        -0.3
    },
    {
        3,
        "Stress Test (500 Bodies)",
        "500 dynamic rigid bodies falling simultaneously into hopper",
        "scenes/stress_test.xml",
        glm::vec3(0.0f, 24.0f, 36.0f),
        0.0,
        -0.45
    }
};

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

} // namespace

const PresetMetadata& ScenePresets::getPresetInfo(int index) {
    if (index < 0 || index >= kPresetCount) {
        return kPresets[0];
    }
    return kPresets[index];
}

const char* ScenePresets::getPresetName(int index) {
    return getPresetInfo(index).name;
}

const char* ScenePresets::getPresetXmlPath(int index) {
    return getPresetInfo(index).xml_file;
}

void ScenePresets::configureCamera(int preset_index, Camera* camera) {
    if (!camera) return;
    const PresetMetadata& info = getPresetInfo(preset_index);
    camera->position = info.camera_pos;
    camera->horizontal_angle = info.camera_yaw;
    camera->vertical_angle = info.camera_pitch;
    camera->update();
}

bool ScenePresets::loadPreset(int preset_index, Application* app, Simulation* sim, Camera* camera) {
    if (preset_index < 0 || preset_index >= kPresetCount) {
        LOGE("ScenePresets::loadPreset: Invalid preset index %d", preset_index);
        return false;
    }

    const PresetMetadata& info = getPresetInfo(preset_index);
    LOGI("ScenePresets: Loading preset [%d] '%s' from %s", preset_index, info.name, info.xml_file);

    Simulation* target_sim = sim;
    Camera* target_camera = camera;

    if (app) {
        // 1. Cleanly destroy existing scenegraph nodes
        if (app->scenegraph_root) {
            scenegraph::destroy(app->scenegraph_root);
            app->scenegraph_root = nullptr;
        }

        // 2. Teardown and re-instantiate simulation to prevent memory bleed
        if (app->simulation) {
            delete app->simulation;
            app->simulation = new Simulation();
            target_sim = app->simulation;
        }

        if (!target_camera) {
            target_camera = app->camera;
        }

        // 3. Load preset XML definition
        app->scenegraph_root = app->loadXML(info.xml_file);
        if (!app->scenegraph_root) {
            // Try loading without directory prefix
            std::string filename_only = info.xml_file;
            size_t slash = filename_only.find_last_of("/\\");
            if (slash != std::string::npos) {
                filename_only = filename_only.substr(slash + 1);
            }
            app->scenegraph_root = app->loadXML(filename_only.c_str());
        }

        if (!app->scenegraph_root) {
            LOGE("ScenePresets: Failed to load XML %s for preset '%s'", info.xml_file, info.name);
            return false;
        }
    } else if (target_sim) {
        // Isolated simulation mode (e.g. headless unit testing)
        AssetManager asset_mgr;
        std::string xml_text = asset_mgr.loadTextFile(info.xml_file);
        if (xml_text.empty()) {
            std::string filename_only = info.xml_file;
            size_t slash = filename_only.find_last_of("/\\");
            if (slash != std::string::npos) {
                filename_only = filename_only.substr(slash + 1);
            }
            xml_text = asset_mgr.loadTextFile(filename_only.c_str());
        }

        if (!xml_text.empty()) {
            rapidxml::xml_document<> doc;
            std::vector<char> xml_buf(xml_text.begin(), xml_text.end());
            xml_buf.push_back('\0');
            doc.parse<0>(xml_buf.data());
            target_sim->parseXMLNode(doc.first_node(), nullptr);
        }
    }

    // 4. Configure camera view and orientation
    if (target_camera) {
        configureCamera(preset_index, target_camera);
    }

    // 5. Preset-specific physics setup
    if (target_sim && target_sim->dynamics_world) {
        if (preset_index == static_cast<int>(PresetIndex::WreckingBall)) {
            // Attach suspension constraint to the wrecking ball head
            auto it = target_sim->rigid_bodies.find("Ball_00");
            if (it != target_sim->rigid_bodies.end() && it->second) {
                btRigidBody* ball = it->second;
                btVector3 pivot_in_ball(0.0f, 0.0f, 0.0f);
                btPoint2PointConstraint* pendulum = new btPoint2PointConstraint(*ball, pivot_in_ball);
                pendulum->setPivotB(btVector3(0.0f, 16.0f, 0.0f));
                target_sim->dynamics_world->addConstraint(pendulum, true);

                // Initial swing impulse toward the target wall
                ball->setLinearVelocity(btVector3(0.0f, 0.0f, -14.0f));
                ball->activate(true);
            }
        } else if (preset_index == static_cast<int>(PresetIndex::StressTest)) {
            // Instantiate dynamic rigid bodies to reach benchmark load (500 dynamic bodies)
            int current_count = target_sim->dynamics_world->getNumCollisionObjects();
            int needed = 500 - current_count;
            if (needed > 0) {
                btSphereShape* stress_sphere = new btSphereShape(0.35f);
                btBoxShape* stress_box = new btBoxShape(btVector3(0.35f, 0.35f, 0.35f));
                target_sim->collision_shapes.push_back(stress_sphere);
                target_sim->collision_shapes.push_back(stress_box);

                int grid_x = 10;
                int grid_z = 10;
                int grid_y = (needed + (grid_x * grid_z - 1)) / (grid_x * grid_z);

                int spawned = 0;
                for (int y = 0; y < grid_y && spawned < needed; ++y) {
                    for (int x = 0; x < grid_x && spawned < needed; ++x) {
                        for (int z = 0; z < grid_z && spawned < needed; ++z) {
                            float px = (x - grid_x / 2.0f) * 0.9f;
                            float py = 8.0f + y * 1.0f;
                            float pz = (z - grid_z / 2.0f) * 0.9f;

                            btCollisionShape* shape = (spawned % 2 == 0)
                                ? static_cast<btCollisionShape*>(stress_sphere)
                                : static_cast<btCollisionShape*>(stress_box);
                            float mass = 1.0f;
                            btVector3 inertia(0, 0, 0);
                            shape->calculateLocalInertia(mass, inertia);

                            btTransform trans;
                            trans.setIdentity();
                            trans.setOrigin(btVector3(px, py, pz));

                            btDefaultMotionState* ms = new btDefaultMotionState(trans);
                            btRigidBody::btRigidBodyConstructionInfo rb_info(mass, ms, shape, inertia);
                            rb_info.m_restitution = 0.5f;
                            rb_info.m_friction = 0.6f;
                            btRigidBody* body = new btRigidBody(rb_info);
                            target_sim->dynamics_world->addRigidBody(body);

                            if (app && app->scenegraph_root) {
                                TransformNode* tn = new TransformNode();
                                tn->name = "StressBody_" + std::to_string(spawned);
                                tn->matrix = glm::translate(glm::mat4(1.0f), glm::vec3(px, py, pz));
                                app->scenegraph_root->children.push_back(tn);

                                PhysicsNode* pn = new PhysicsNode();
                                pn->body = body;
                                pn->collision_shape = shape;
                                pn->mass = mass;
                                pn->transform_node = tn;

                                int obj_idx = target_sim->dynamics_world->getNumCollisionObjects() - 1;
                                target_sim->collision_node_index.insert(std::make_pair(obj_idx, pn));
                            }

                            spawned++;
                        }
                    }
                }
                LOGI("ScenePresets: Benchmark spawned %d dynamic bodies (total in world: %d)",
                     spawned, target_sim->dynamics_world->getNumCollisionObjects());
            }
        }
    }

    return true;
}

bool ScenePresets::loadPreset(const std::string& name, Application* app, Simulation* sim,
                              Camera* camera) {
    std::string needle = toLower(name);
    for (int i = 0; i < kPresetCount; ++i) {
        std::string preset_name = toLower(kPresets[i].name);
        std::string xml_file = toLower(kPresets[i].xml_file);
        if (preset_name.find(needle) != std::string::npos ||
            xml_file.find(needle) != std::string::npos) {
            return loadPreset(i, app, sim, camera);
        }
    }
    LOGE("ScenePresets: No preset found matching '%s'", name.c_str());
    return false;
}

} // namespace droidgfxsim
