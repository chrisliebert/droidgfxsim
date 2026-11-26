// Copyright (C) 2017-2026 Chris Liebert

#include "graphics/vulkan_renderer.h"
#include "graphics/gl_code.h"
#include "common/log.h"
#include "graphics/scene_graph.h"
#include "graphics/camera.h"

#include <iostream>
#include <vector>
#include <cstring>
#include <set>
#include <algorithm>
#include <limits>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#if defined(DESKTOP_APP) && !defined(__ANDROID__)
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

namespace {

// Per-draw push constant block (model transform)
struct PushConstantBlock {
    glm::mat4 model;
};

// Per-frame uniform buffer object (std140)
struct FrameUBO {
    glm::mat4 proj;
    glm::mat4 view;
    glm::vec4 lightPos;
};

// Precompiled SPIR-V 1.0 vertex shader bytecode (compiled from GLSL 450 via glslc -O)
// Matches GL3SceneGraphRenderer world-space/view-space attributes + UBO + model push constant
const uint32_t kVertexShaderSpv[] = {
    0x07230203, 0x00010000, 0x000d000b, 0x0000007a, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x000f000f, 0x00000000, 0x00000004, 0x6e69616d, 0x00000000, 0x00000015, 0x0000002e, 0x00000039,
    0x00000045, 0x00000048, 0x00000054, 0x00000057, 0x00000063, 0x00000076, 0x00000078, 0x00030047,
    0x0000000b, 0x00000002, 0x00040048, 0x0000000b, 0x00000000, 0x00000005, 0x00050048, 0x0000000b,
    0x00000000, 0x00000007, 0x00000010, 0x00050048, 0x0000000b, 0x00000000, 0x00000023, 0x00000000,
    0x00040047, 0x00000015, 0x0000001e, 0x00000000, 0x00030047, 0x0000001f, 0x00000002, 0x00040048,
    0x0000001f, 0x00000000, 0x00000005, 0x00050048, 0x0000001f, 0x00000000, 0x00000007, 0x00000010,
    0x00050048, 0x0000001f, 0x00000000, 0x00000023, 0x00000000, 0x00040048, 0x0000001f, 0x00000001,
    0x00000005, 0x00050048, 0x0000001f, 0x00000001, 0x00000007, 0x00000010, 0x00050048, 0x0000001f,
    0x00000001, 0x00000023, 0x00000040, 0x00050048, 0x0000001f, 0x00000002, 0x00000023, 0x00000080,
    0x00040047, 0x00000021, 0x00000021, 0x00000000, 0x00040047, 0x00000021, 0x00000022, 0x00000000,
    0x00030047, 0x0000002c, 0x00000002, 0x00050048, 0x0000002c, 0x00000000, 0x0000000b, 0x00000000,
    0x00050048, 0x0000002c, 0x00000001, 0x0000000b, 0x00000001, 0x00050048, 0x0000002c, 0x00000002,
    0x0000000b, 0x00000003, 0x00050048, 0x0000002c, 0x00000003, 0x0000000b, 0x00000004, 0x00040047,
    0x00000039, 0x0000001e, 0x00000000, 0x00040047, 0x00000045, 0x0000001e, 0x00000002, 0x00040047,
    0x00000048, 0x0000001e, 0x00000001, 0x00040047, 0x00000054, 0x0000001e, 0x00000001, 0x00040047,
    0x00000057, 0x0000001e, 0x00000003, 0x00040047, 0x00000063, 0x0000001e, 0x00000005, 0x00040047,
    0x00000076, 0x0000001e, 0x00000004, 0x00040047, 0x00000078, 0x0000001e, 0x00000002, 0x00020013,
    0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017,
    0x00000007, 0x00000006, 0x00000004, 0x00040018, 0x0000000a, 0x00000007, 0x00000004, 0x0003001e,
    0x0000000b, 0x0000000a, 0x00040020, 0x0000000c, 0x00000009, 0x0000000b, 0x0004003b, 0x0000000c,
    0x0000000d, 0x00000009, 0x00040015, 0x0000000e, 0x00000020, 0x00000001, 0x0004002b, 0x0000000e,
    0x0000000f, 0x00000000, 0x00040020, 0x00000010, 0x00000009, 0x0000000a, 0x00040017, 0x00000013,
    0x00000006, 0x00000003, 0x00040020, 0x00000014, 0x00000001, 0x00000013, 0x0004003b, 0x00000014,
    0x00000015, 0x00000001, 0x0004002b, 0x00000006, 0x00000017, 0x3f800000, 0x0005001e, 0x0000001f,
    0x0000000a, 0x0000000a, 0x00000007, 0x00040020, 0x00000020, 0x00000002, 0x0000001f, 0x0004003b,
    0x00000020, 0x00000021, 0x00000002, 0x0004002b, 0x0000000e, 0x00000022, 0x00000001, 0x00040020,
    0x00000023, 0x00000002, 0x0000000a, 0x00040015, 0x00000029, 0x00000020, 0x00000000, 0x0004002b,
    0x00000029, 0x0000002a, 0x00000001, 0x0004001c, 0x0000002b, 0x00000006, 0x0000002a, 0x0006001e,
    0x0000002c, 0x00000007, 0x00000006, 0x0000002b, 0x0000002b, 0x00040020, 0x0000002d, 0x00000003,
    0x0000002c, 0x0004003b, 0x0000002d, 0x0000002e, 0x00000003, 0x00040020, 0x00000036, 0x00000003,
    0x00000007, 0x00040020, 0x00000038, 0x00000003, 0x00000013, 0x0004003b, 0x00000038, 0x00000039,
    0x00000003, 0x0004003b, 0x00000038, 0x00000045, 0x00000003, 0x0004003b, 0x00000038, 0x00000048,
    0x00000003, 0x00040018, 0x0000004c, 0x00000013, 0x00000003, 0x0004003b, 0x00000014, 0x00000054,
    0x00000001, 0x0004003b, 0x00000038, 0x00000057, 0x00000003, 0x0004003b, 0x00000038, 0x00000063,
    0x00000003, 0x0004002b, 0x0000000e, 0x00000066, 0x00000002, 0x00040020, 0x00000067, 0x00000002,
    0x00000007, 0x00040017, 0x00000074, 0x00000006, 0x00000002, 0x00040020, 0x00000075, 0x00000003,
    0x00000074, 0x0004003b, 0x00000075, 0x00000076, 0x00000003, 0x00040020, 0x00000077, 0x00000001,
    0x00000074, 0x0004003b, 0x00000077, 0x00000078, 0x00000001, 0x00050036, 0x00000002, 0x00000004,
    0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x00050041, 0x00000010, 0x00000011, 0x0000000d,
    0x0000000f, 0x0004003d, 0x0000000a, 0x00000012, 0x00000011, 0x0004003d, 0x00000013, 0x00000016,
    0x00000015, 0x00050051, 0x00000006, 0x00000018, 0x00000016, 0x00000000, 0x00050051, 0x00000006,
    0x00000019, 0x00000016, 0x00000001, 0x00050051, 0x00000006, 0x0000001a, 0x00000016, 0x00000002,
    0x00070050, 0x00000007, 0x0000001b, 0x00000018, 0x00000019, 0x0000001a, 0x00000017, 0x00050091,
    0x00000007, 0x0000001c, 0x00000012, 0x0000001b, 0x00050041, 0x00000023, 0x00000024, 0x00000021,
    0x00000022, 0x0004003d, 0x0000000a, 0x00000025, 0x00000024, 0x00050092, 0x0000000a, 0x00000028,
    0x00000025, 0x00000012, 0x00050041, 0x00000023, 0x0000002f, 0x00000021, 0x0000000f, 0x0004003d,
    0x0000000a, 0x00000030, 0x0000002f, 0x00050092, 0x0000000a, 0x00000033, 0x00000030, 0x00000025,
    0x00050091, 0x00000007, 0x00000035, 0x00000033, 0x0000001c, 0x00050041, 0x00000036, 0x00000037,
    0x0000002e, 0x0000000f, 0x0003003e, 0x00000037, 0x00000035, 0x00050091, 0x00000007, 0x00000040,
    0x00000028, 0x0000001b, 0x00050051, 0x00000006, 0x00000041, 0x00000040, 0x00000000, 0x00050051,
    0x00000006, 0x00000042, 0x00000040, 0x00000001, 0x00050051, 0x00000006, 0x00000043, 0x00000040,
    0x00000002, 0x00060050, 0x00000013, 0x00000044, 0x00000041, 0x00000042, 0x00000043, 0x0003003e,
    0x00000039, 0x00000044, 0x0008004f, 0x00000013, 0x00000047, 0x0000001c, 0x0000001c, 0x00000000,
    0x00000001, 0x00000002, 0x0003003e, 0x00000045, 0x00000047, 0x0006000c, 0x0000000a, 0x0000004a,
    0x00000001, 0x00000022, 0x00000028, 0x00040054, 0x0000000a, 0x0000004b, 0x0000004a, 0x00050051,
    0x00000007, 0x0000004d, 0x0000004b, 0x00000000, 0x0008004f, 0x00000013, 0x0000004e, 0x0000004d,
    0x0000004d, 0x00000000, 0x00000001, 0x00000002, 0x00050051, 0x00000007, 0x0000004f, 0x0000004b,
    0x00000001, 0x0008004f, 0x00000013, 0x00000050, 0x0000004f, 0x0000004f, 0x00000000, 0x00000001,
    0x00000002, 0x00050051, 0x00000007, 0x00000051, 0x0000004b, 0x00000002, 0x0008004f, 0x00000013,
    0x00000052, 0x00000051, 0x00000051, 0x00000000, 0x00000001, 0x00000002, 0x00060050, 0x0000004c,
    0x00000053, 0x0000004e, 0x00000050, 0x00000052, 0x0004003d, 0x00000013, 0x00000055, 0x00000054,
    0x00050091, 0x00000013, 0x00000056, 0x00000053, 0x00000055, 0x0003003e, 0x00000048, 0x00000056,
    0x00050051, 0x00000007, 0x0000005a, 0x00000012, 0x00000000, 0x0008004f, 0x00000013, 0x0000005b,
    0x0000005a, 0x0000005a, 0x00000000, 0x00000001, 0x00000002, 0x00050051, 0x00000007, 0x0000005c,
    0x00000012, 0x00000001, 0x0008004f, 0x00000013, 0x0000005d, 0x0000005c, 0x0000005c, 0x00000000,
    0x00000001, 0x00000002, 0x00050051, 0x00000007, 0x0000005e, 0x00000012, 0x00000002, 0x0008004f,
    0x00000013, 0x0000005f, 0x0000005e, 0x0000005e, 0x00000000, 0x00000001, 0x00000002, 0x00060050,
    0x0000004c, 0x00000060, 0x0000005b, 0x0000005d, 0x0000005f, 0x00050091, 0x00000013, 0x00000062,
    0x00000060, 0x00000055, 0x0003003e, 0x00000057, 0x00000062, 0x00050041, 0x00000067, 0x00000068,
    0x00000021, 0x00000066, 0x0004003d, 0x00000007, 0x00000069, 0x00000068, 0x00050051, 0x00000006,
    0x0000006b, 0x00000069, 0x00000000, 0x00050051, 0x00000006, 0x0000006c, 0x00000069, 0x00000001,
    0x00050051, 0x00000006, 0x0000006d, 0x00000069, 0x00000002, 0x00070050, 0x00000007, 0x0000006e,
    0x0000006b, 0x0000006c, 0x0000006d, 0x00000017, 0x00050091, 0x00000007, 0x0000006f, 0x00000025,
    0x0000006e, 0x00050051, 0x00000006, 0x00000070, 0x0000006f, 0x00000000, 0x00050051, 0x00000006,
    0x00000071, 0x0000006f, 0x00000001, 0x00050051, 0x00000006, 0x00000072, 0x0000006f, 0x00000002,
    0x00060050, 0x00000013, 0x00000073, 0x00000070, 0x00000071, 0x00000072, 0x0003003e, 0x00000063,
    0x00000073, 0x0004003d, 0x00000074, 0x00000079, 0x00000078, 0x0003003e, 0x00000076, 0x00000079,
    0x000100fd, 0x00010038
};

// Precompiled SPIR-V 1.0 fragment shader bytecode (compiled from GLSL 450 via glslc -O)
// Matches GL3SceneGraphRenderer: hemisphere sky/earth ambient, sun light, Blinn-Phong specular,
// sampled diffuseTexture, worldPos.xz micro-grain modulation, and Rayleigh horizon fog {0.55, 0.68, 0.82}
const uint32_t kFragmentShaderSpv[] = {
    0x07230203, 0x00010000, 0x000d000b, 0x00000233, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x000c000f, 0x00000004, 0x00000004, 0x6e69616d, 0x00000000, 0x00000067, 0x0000008e, 0x0000009c,
    0x0000009e, 0x000000d0, 0x000000d7, 0x00000110, 0x00030010, 0x00000004, 0x00000007, 0x00040047,
    0x00000067, 0x0000001e, 0x00000003, 0x00040047, 0x0000008e, 0x0000001e, 0x00000001, 0x00040047,
    0x0000009c, 0x0000001e, 0x00000005, 0x00040047, 0x0000009e, 0x0000001e, 0x00000000, 0x00040047,
    0x000000cd, 0x00000021, 0x00000001, 0x00040047, 0x000000cd, 0x00000022, 0x00000000, 0x00040047,
    0x000000d0, 0x0000001e, 0x00000004, 0x00040047, 0x000000d7, 0x0000001e, 0x00000002, 0x00040047,
    0x00000110, 0x0000001e, 0x00000000, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002,
    0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000002, 0x0004002b,
    0x00000006, 0x00000011, 0x42f6ae14, 0x0004002b, 0x00000006, 0x00000012, 0x43e41ae1, 0x0005002c,
    0x00000007, 0x00000013, 0x00000011, 0x00000012, 0x0004002b, 0x00000006, 0x00000018, 0x423547ae,
    0x0004002b, 0x00000006, 0x00000035, 0x40400000, 0x0004002b, 0x00000006, 0x00000036, 0x40000000,
    0x0004002b, 0x00000006, 0x00000042, 0x3f800000, 0x0004002b, 0x00000006, 0x00000043, 0x00000000,
    0x0005002c, 0x00000007, 0x00000044, 0x00000042, 0x00000043, 0x0005002c, 0x00000007, 0x0000004a,
    0x00000043, 0x00000042, 0x0005002c, 0x00000007, 0x00000050, 0x00000042, 0x00000042, 0x00040017,
    0x00000063, 0x00000006, 0x00000003, 0x00040020, 0x00000066, 0x00000001, 0x00000063, 0x0004003b,
    0x00000066, 0x00000067, 0x00000001, 0x0004002b, 0x00000006, 0x0000006b, 0x38d1b717, 0x00020014,
    0x0000006c, 0x0006002c, 0x00000063, 0x00000074, 0x00000043, 0x00000042, 0x00000043, 0x0004002b,
    0x00000006, 0x00000077, 0x3ea3d70a, 0x0004002b, 0x00000006, 0x00000078, 0x3e8f5c29, 0x0004002b,
    0x00000006, 0x00000079, 0x3e75c28f, 0x0006002c, 0x00000063, 0x0000007a, 0x00000077, 0x00000078,
    0x00000079, 0x0004002b, 0x00000006, 0x0000007b, 0x3f147ae1, 0x0004002b, 0x00000006, 0x0000007c,
    0x3f3851ec, 0x0004002b, 0x00000006, 0x0000007d, 0x3f6147ae, 0x0006002c, 0x00000063, 0x0000007e,
    0x0000007b, 0x0000007c, 0x0000007d, 0x0004002b, 0x00000006, 0x00000081, 0x3f000000, 0x0004002b,
    0x00000006, 0x00000087, 0x3ed70a3d, 0x0004002b, 0x00000006, 0x0000008a, 0x3f7851ec, 0x0004002b,
    0x00000006, 0x0000008b, 0x3f68f5c3, 0x0006002c, 0x00000063, 0x0000008c, 0x00000042, 0x0000008a,
    0x0000008b, 0x0004003b, 0x00000066, 0x0000008e, 0x00000001, 0x0006002c, 0x00000063, 0x00000099,
    0x00000043, 0x00000043, 0x00000042, 0x0004003b, 0x00000066, 0x0000009c, 0x00000001, 0x0004003b,
    0x00000066, 0x0000009e, 0x00000001, 0x0004002b, 0x00000006, 0x000000b9, 0x42000000, 0x0004002b,
    0x00000006, 0x000000bc, 0x3e3851ec, 0x00040017, 0x000000c7, 0x00000006, 0x00000004, 0x00090019,
    0x000000ca, 0x00000006, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x00000000,
    0x0003001b, 0x000000cb, 0x000000ca, 0x00040020, 0x000000cc, 0x00000000, 0x000000cb, 0x0004003b,
    0x000000cc, 0x000000cd, 0x00000000, 0x00040020, 0x000000cf, 0x00000001, 0x00000007, 0x0004003b,
    0x000000cf, 0x000000d0, 0x00000001, 0x0004003b, 0x00000066, 0x000000d7, 0x00000001, 0x0004002b,
    0x00000006, 0x000000e0, 0x410553f8, 0x0004002b, 0x00000006, 0x000000e2, 0x418a6666, 0x0004002b,
    0x00000006, 0x000000e3, 0x4111999a, 0x0005002c, 0x00000007, 0x000000e4, 0x000000e2, 0x000000e3,
    0x0004002b, 0x00000006, 0x000000e9, 0x3f19999a, 0x0004002b, 0x00000006, 0x000000ec, 0x3ecccccd,
    0x0004002b, 0x00000006, 0x000000f1, 0x41200000, 0x0004002b, 0x00000006, 0x000000f2, 0x42280000,
    0x0004002b, 0x00000006, 0x000000f7, 0x3f666666, 0x0004002b, 0x00000006, 0x000000f8, 0x3e4ccccd,
    0x0004002b, 0x00000006, 0x00000106, 0x42400000, 0x0004002b, 0x00000006, 0x00000107, 0x42f00000,
    0x0004002b, 0x00000006, 0x0000010b, 0x3f0ccccd, 0x0004002b, 0x00000006, 0x0000010c, 0x3f2e147b,
    0x0004002b, 0x00000006, 0x0000010d, 0x3f51eb85, 0x0006002c, 0x00000063, 0x0000010e, 0x0000010b,
    0x0000010c, 0x0000010d, 0x00040020, 0x0000010f, 0x00000003, 0x000000c7, 0x0004003b, 0x0000010f,
    0x00000110, 0x00000003, 0x0005002c, 0x00000007, 0x00000231, 0x00000035, 0x00000035, 0x0005002c,
    0x00000007, 0x00000232, 0x00000018, 0x00000018, 0x00050036, 0x00000002, 0x00000004, 0x00000000,
    0x00000003, 0x000200f8, 0x00000005, 0x0004003d, 0x00000063, 0x00000068, 0x00000067, 0x00050094,
    0x00000006, 0x0000006a, 0x00000068, 0x00000068, 0x000500ba, 0x0000006c, 0x0000006d, 0x0000006a,
    0x0000006b, 0x000300f7, 0x00000070, 0x00000000, 0x000400fa, 0x0000006d, 0x0000006f, 0x00000073,
    0x000200f8, 0x0000006f, 0x0006000c, 0x00000063, 0x00000072, 0x00000001, 0x00000045, 0x00000068,
    0x000200f9, 0x00000070, 0x000200f8, 0x00000073, 0x000200f9, 0x00000070, 0x000200f8, 0x00000070,
    0x000700f5, 0x00000063, 0x0000022f, 0x00000072, 0x0000006f, 0x00000074, 0x00000073, 0x00050051,
    0x00000006, 0x00000080, 0x0000022f, 0x00000001, 0x00050085, 0x00000006, 0x00000082, 0x00000080,
    0x00000081, 0x00050081, 0x00000006, 0x00000083, 0x00000082, 0x00000081, 0x0008000c, 0x00000006,
    0x00000084, 0x00000001, 0x0000002b, 0x00000083, 0x00000043, 0x00000042, 0x00060050, 0x00000063,
    0x00000085, 0x00000084, 0x00000084, 0x00000084, 0x0008000c, 0x00000063, 0x00000086, 0x00000001,
    0x0000002e, 0x0000007a, 0x0000007e, 0x00000085, 0x0005008e, 0x00000063, 0x00000088, 0x00000086,
    0x00000087, 0x0004003d, 0x00000063, 0x0000008f, 0x0000008e, 0x00050094, 0x00000006, 0x00000091,
    0x0000008f, 0x0000008f, 0x000500ba, 0x0000006c, 0x00000092, 0x00000091, 0x0000006b, 0x000300f7,
    0x00000095, 0x00000000, 0x000400fa, 0x00000092, 0x00000094, 0x00000098, 0x000200f8, 0x00000094,
    0x0006000c, 0x00000063, 0x00000097, 0x00000001, 0x00000045, 0x0000008f, 0x000200f9, 0x00000095,
    0x000200f8, 0x00000098, 0x000200f9, 0x00000095, 0x000200f8, 0x00000095, 0x000700f5, 0x00000063,
    0x00000230, 0x00000097, 0x00000094, 0x00000099, 0x00000098, 0x0004003d, 0x00000063, 0x0000009d,
    0x0000009c, 0x0004003d, 0x00000063, 0x0000009f, 0x0000009e, 0x00050083, 0x00000063, 0x000000a0,
    0x0000009d, 0x0000009f, 0x0006000c, 0x00000063, 0x000000a1, 0x00000001, 0x00000045, 0x000000a0,
    0x00050094, 0x00000006, 0x000000a5, 0x00000230, 0x000000a1, 0x0007000c, 0x00000006, 0x000000a6,
    0x00000001, 0x00000028, 0x000000a5, 0x00000043, 0x0005008e, 0x00000063, 0x000000aa, 0x0000008c,
    0x000000a6, 0x0004007f, 0x00000063, 0x000000ad, 0x0000009f, 0x0006000c, 0x00000063, 0x000000ae,
    0x00000001, 0x00000045, 0x000000ad, 0x00050081, 0x00000063, 0x000000b2, 0x000000a1, 0x000000ae,
    0x0006000c, 0x00000063, 0x000000b3, 0x00000001, 0x00000045, 0x000000b2, 0x00050094, 0x00000006,
    0x000000b7, 0x00000230, 0x000000b3, 0x0007000c, 0x00000006, 0x000000b8, 0x00000001, 0x00000028,
    0x000000b7, 0x00000043, 0x0007000c, 0x00000006, 0x000000ba, 0x00000001, 0x0000001a, 0x000000b8,
    0x000000b9, 0x00050085, 0x00000006, 0x000000be, 0x000000bc, 0x000000ba, 0x0005008e, 0x00000063,
    0x000000c0, 0x0000008c, 0x000000be, 0x00050081, 0x00000063, 0x000000c4, 0x00000088, 0x000000aa,
    0x00050081, 0x00000063, 0x000000c6, 0x000000c4, 0x000000c0, 0x0004003d, 0x000000cb, 0x000000ce,
    0x000000cd, 0x0004003d, 0x00000007, 0x000000d1, 0x000000d0, 0x00050057, 0x000000c7, 0x000000d2,
    0x000000ce, 0x000000d1, 0x0006000c, 0x00000006, 0x000000d5, 0x00000001, 0x00000042, 0x0000009f,
    0x0004003d, 0x00000063, 0x000000d8, 0x000000d7, 0x0007004f, 0x00000007, 0x000000d9, 0x000000d8,
    0x000000d8, 0x00000000, 0x00000002, 0x0005008e, 0x00000007, 0x000000da, 0x000000d9, 0x00000036,
    0x0006000c, 0x00000007, 0x0000012b, 0x00000001, 0x00000008, 0x000000da, 0x0006000c, 0x00000007,
    0x0000012d, 0x00000001, 0x0000000a, 0x000000da, 0x00050085, 0x00000007, 0x00000130, 0x0000012d,
    0x0000012d, 0x0005008e, 0x00000007, 0x00000132, 0x0000012d, 0x00000036, 0x00050083, 0x00000007,
    0x00000134, 0x00000231, 0x00000132, 0x00050085, 0x00000007, 0x00000135, 0x00000130, 0x00000134,
    0x00050085, 0x00000007, 0x00000151, 0x0000012b, 0x00000013, 0x0006000c, 0x00000007, 0x00000152,
    0x00000001, 0x0000000a, 0x00000151, 0x00050081, 0x00000007, 0x00000156, 0x00000152, 0x00000232,
    0x00050094, 0x00000006, 0x00000157, 0x00000152, 0x00000156, 0x00050050, 0x00000007, 0x00000159,
    0x00000157, 0x00000157, 0x00050081, 0x00000007, 0x0000015a, 0x00000152, 0x00000159, 0x00050051,
    0x00000006, 0x0000015c, 0x0000015a, 0x00000000, 0x00050051, 0x00000006, 0x0000015e, 0x0000015a,
    0x00000001, 0x00050085, 0x00000006, 0x0000015f, 0x0000015c, 0x0000015e, 0x0006000c, 0x00000006,
    0x00000160, 0x00000001, 0x0000000a, 0x0000015f, 0x00050081, 0x00000007, 0x00000139, 0x0000012b,
    0x00000044, 0x00050085, 0x00000007, 0x00000164, 0x00000139, 0x00000013, 0x0006000c, 0x00000007,
    0x00000165, 0x00000001, 0x0000000a, 0x00000164, 0x00050081, 0x00000007, 0x00000169, 0x00000165,
    0x00000232, 0x00050094, 0x00000006, 0x0000016a, 0x00000165, 0x00000169, 0x00050050, 0x00000007,
    0x0000016c, 0x0000016a, 0x0000016a, 0x00050081, 0x00000007, 0x0000016d, 0x00000165, 0x0000016c,
    0x00050051, 0x00000006, 0x0000016f, 0x0000016d, 0x00000000, 0x00050051, 0x00000006, 0x00000171,
    0x0000016d, 0x00000001, 0x00050085, 0x00000006, 0x00000172, 0x0000016f, 0x00000171, 0x0006000c,
    0x00000006, 0x00000173, 0x00000001, 0x0000000a, 0x00000172, 0x00050081, 0x00000007, 0x0000013c,
    0x0000012b, 0x0000004a, 0x00050085, 0x00000007, 0x00000177, 0x0000013c, 0x00000013, 0x0006000c,
    0x00000007, 0x00000178, 0x00000001, 0x0000000a, 0x00000177, 0x00050081, 0x00000007, 0x0000017c,
    0x00000178, 0x00000232, 0x00050094, 0x00000006, 0x0000017d, 0x00000178, 0x0000017c, 0x00050050,
    0x00000007, 0x0000017f, 0x0000017d, 0x0000017d, 0x00050081, 0x00000007, 0x00000180, 0x00000178,
    0x0000017f, 0x00050051, 0x00000006, 0x00000182, 0x00000180, 0x00000000, 0x00050051, 0x00000006,
    0x00000184, 0x00000180, 0x00000001, 0x00050085, 0x00000006, 0x00000185, 0x00000182, 0x00000184,
    0x0006000c, 0x00000006, 0x00000186, 0x00000001, 0x0000000a, 0x00000185, 0x00050081, 0x00000007,
    0x0000013f, 0x0000012b, 0x00000050, 0x00050085, 0x00000007, 0x0000018a, 0x0000013f, 0x00000013,
    0x0006000c, 0x00000007, 0x0000018b, 0x00000001, 0x0000000a, 0x0000018a, 0x00050081, 0x00000007,
    0x0000018f, 0x0000018b, 0x00000232, 0x00050094, 0x00000006, 0x00000190, 0x0000018b, 0x0000018f,
    0x00050050, 0x00000007, 0x00000192, 0x00000190, 0x00000190, 0x00050081, 0x00000007, 0x00000193,
    0x0000018b, 0x00000192, 0x00050051, 0x00000006, 0x00000195, 0x00000193, 0x00000000, 0x00050051,
    0x00000006, 0x00000197, 0x00000193, 0x00000001, 0x00050085, 0x00000006, 0x00000198, 0x00000195,
    0x00000197, 0x0006000c, 0x00000006, 0x00000199, 0x00000001, 0x0000000a, 0x00000198, 0x00050051,
    0x00000006, 0x00000144, 0x00000135, 0x00000000, 0x0008000c, 0x00000006, 0x00000145, 0x00000001,
    0x0000002e, 0x00000160, 0x00000173, 0x00000144, 0x0008000c, 0x00000006, 0x0000014a, 0x00000001,
    0x0000002e, 0x00000186, 0x00000199, 0x00000144, 0x00050051, 0x00000006, 0x0000014c, 0x00000135,
    0x00000001, 0x0008000c, 0x00000006, 0x0000014d, 0x00000001, 0x0000002e, 0x00000145, 0x0000014a,
    0x0000014c, 0x0005008e, 0x00000007, 0x000000e1, 0x000000d9, 0x000000e0, 0x00050081, 0x00000007,
    0x000000e5, 0x000000e1, 0x000000e4, 0x0006000c, 0x00000007, 0x000001a8, 0x00000001, 0x00000008,
    0x000000e5, 0x0006000c, 0x00000007, 0x000001aa, 0x00000001, 0x0000000a, 0x000000e5, 0x00050085,
    0x00000007, 0x000001ad, 0x000001aa, 0x000001aa, 0x0005008e, 0x00000007, 0x000001af, 0x000001aa,
    0x00000036, 0x00050083, 0x00000007, 0x000001b1, 0x00000231, 0x000001af, 0x00050085, 0x00000007,
    0x000001b2, 0x000001ad, 0x000001b1, 0x00050085, 0x00000007, 0x000001ce, 0x000001a8, 0x00000013,
    0x0006000c, 0x00000007, 0x000001cf, 0x00000001, 0x0000000a, 0x000001ce, 0x00050081, 0x00000007,
    0x000001d3, 0x000001cf, 0x00000232, 0x00050094, 0x00000006, 0x000001d4, 0x000001cf, 0x000001d3,
    0x00050050, 0x00000007, 0x000001d6, 0x000001d4, 0x000001d4, 0x00050081, 0x00000007, 0x000001d7,
    0x000001cf, 0x000001d6, 0x00050051, 0x00000006, 0x000001d9, 0x000001d7, 0x00000000, 0x00050051,
    0x00000006, 0x000001db, 0x000001d7, 0x00000001, 0x00050085, 0x00000006, 0x000001dc, 0x000001d9,
    0x000001db, 0x0006000c, 0x00000006, 0x000001dd, 0x00000001, 0x0000000a, 0x000001dc, 0x00050081,
    0x00000007, 0x000001b6, 0x000001a8, 0x00000044, 0x00050085, 0x00000007, 0x000001e1, 0x000001b6,
    0x00000013, 0x0006000c, 0x00000007, 0x000001e2, 0x00000001, 0x0000000a, 0x000001e1, 0x00050081,
    0x00000007, 0x000001e6, 0x000001e2, 0x00000232, 0x00050094, 0x00000006, 0x000001e7, 0x000001e2,
    0x000001e6, 0x00050050, 0x00000007, 0x000001e9, 0x000001e7, 0x000001e7, 0x00050081, 0x00000007,
    0x000001ea, 0x000001e2, 0x000001e9, 0x00050051, 0x00000006, 0x000001ec, 0x000001ea, 0x00000000,
    0x00050051, 0x00000006, 0x000001ee, 0x000001ea, 0x00000001, 0x00050085, 0x00000006, 0x000001ef,
    0x000001ec, 0x000001ee, 0x0006000c, 0x00000006, 0x000001f0, 0x00000001, 0x0000000a, 0x000001ef,
    0x00050081, 0x00000007, 0x000001b9, 0x000001a8, 0x0000004a, 0x00050085, 0x00000007, 0x000001f4,
    0x000001b9, 0x00000013, 0x0006000c, 0x00000007, 0x000001f5, 0x00000001, 0x0000000a, 0x000001f4,
    0x00050081, 0x00000007, 0x000001f9, 0x000001f5, 0x00000232, 0x00050094, 0x00000006, 0x000001fa,
    0x000001f5, 0x000001f9, 0x00050050, 0x00000007, 0x000001fc, 0x000001fa, 0x000001fa, 0x00050081,
    0x00000007, 0x000001fd, 0x000001f5, 0x000001fc, 0x00050051, 0x00000006, 0x000001ff, 0x000001fd,
    0x00000000, 0x00050051, 0x00000006, 0x00000201, 0x000001fd, 0x00000001, 0x00050085, 0x00000006,
    0x00000202, 0x000001ff, 0x00000201, 0x0006000c, 0x00000006, 0x00000203, 0x00000001, 0x0000000a,
    0x00000202, 0x00050081, 0x00000007, 0x000001bc, 0x000001a8, 0x00000050, 0x00050085, 0x00000007,
    0x00000207, 0x000001bc, 0x00000013, 0x0006000c, 0x00000007, 0x00000208, 0x00000001, 0x0000000a,
    0x00000207, 0x00050081, 0x00000007, 0x0000020c, 0x00000208, 0x00000232, 0x00050094, 0x00000006,
    0x0000020d, 0x00000208, 0x0000020c, 0x00050050, 0x00000007, 0x0000020f, 0x0000020d, 0x0000020d,
    0x00050081, 0x00000007, 0x00000210, 0x00000208, 0x0000020f, 0x00050051, 0x00000006, 0x00000212,
    0x00000210, 0x00000000, 0x00050051, 0x00000006, 0x00000214, 0x00000210, 0x00000001, 0x00050085,
    0x00000006, 0x00000215, 0x00000212, 0x00000214, 0x0006000c, 0x00000006, 0x00000216, 0x00000001,
    0x0000000a, 0x00000215, 0x00050051, 0x00000006, 0x000001c1, 0x000001b2, 0x00000000, 0x0008000c,
    0x00000006, 0x000001c2, 0x00000001, 0x0000002e, 0x000001dd, 0x000001f0, 0x000001c1, 0x0008000c,
    0x00000006, 0x000001c7, 0x00000001, 0x0000002e, 0x00000203, 0x00000216, 0x000001c1, 0x00050051,
    0x00000006, 0x000001c9, 0x000001b2, 0x00000001, 0x0008000c, 0x00000006, 0x000001ca, 0x00000001,
    0x0000002e, 0x000001c2, 0x000001c7, 0x000001c9, 0x00050085, 0x00000006, 0x000000eb, 0x000000e9,
    0x0000014d, 0x00050085, 0x00000006, 0x000000ee, 0x000000ec, 0x000001ca, 0x00050081, 0x00000006,
    0x000000ef, 0x000000eb, 0x000000ee, 0x0008000c, 0x00000006, 0x000000f4, 0x00000001, 0x00000031,
    0x000000f1, 0x000000f2, 0x000000d5, 0x00050083, 0x00000006, 0x000000f5, 0x00000042, 0x000000f4,
    0x00050085, 0x00000006, 0x000000fa, 0x000000f8, 0x000000ef, 0x00050081, 0x00000006, 0x000000fb,
    0x000000f7, 0x000000fa, 0x0008000c, 0x00000006, 0x000000fd, 0x00000001, 0x0000002e, 0x00000042,
    0x000000fb, 0x000000f5, 0x0008004f, 0x00000063, 0x00000100, 0x000000d2, 0x000000d2, 0x00000000,
    0x00000001, 0x00000002, 0x0005008e, 0x00000063, 0x00000102, 0x00000100, 0x000000fd, 0x00050085,
    0x00000063, 0x00000104, 0x00000102, 0x000000c6, 0x0008000c, 0x00000006, 0x00000109, 0x00000001,
    0x00000031, 0x00000106, 0x00000107, 0x000000d5, 0x00060050, 0x00000063, 0x00000114, 0x00000109,
    0x00000109, 0x00000109, 0x0008000c, 0x00000063, 0x00000115, 0x00000001, 0x0000002e, 0x00000104,
    0x0000010e, 0x00000114, 0x00050051, 0x00000006, 0x00000118, 0x000000d2, 0x00000003, 0x00050051,
    0x00000006, 0x00000119, 0x00000115, 0x00000000, 0x00050051, 0x00000006, 0x0000011a, 0x00000115,
    0x00000001, 0x00050051, 0x00000006, 0x0000011b, 0x00000115, 0x00000002, 0x00070050, 0x000000c7,
    0x0000011c, 0x00000119, 0x0000011a, 0x0000011b, 0x00000118, 0x0003003e, 0x00000110, 0x0000011c,
    0x000100fd, 0x00010038
};

} // namespace

VulkanRenderer::VulkanRenderer()
    : instance(VK_NULL_HANDLE)
    , physicalDevice(VK_NULL_HANDLE)
    , device(VK_NULL_HANDLE)
    , graphicsQueue(VK_NULL_HANDLE)
    , presentQueue(VK_NULL_HANDLE)
    , surface(VK_NULL_HANDLE)
    , swapChain(VK_NULL_HANDLE)
    , renderPass(VK_NULL_HANDLE)
    , pipelineLayout(VK_NULL_HANDLE)
    , graphicsPipeline(VK_NULL_HANDLE)
    , commandPool(VK_NULL_HANDLE)
    , imageAvailableSemaphore(VK_NULL_HANDLE)
    , renderFinishedSemaphore(VK_NULL_HANDLE)
    , inFlightFence(VK_NULL_HANDLE)
    , window(nullptr)
    , isHeadless(true)
    , initialized(false)
    , framebufferResized(false)
    , graphicsQueueFamilyIndex(0)
    , presentQueueFamilyIndex(0)
    , swapChainExtent{1200, 800}
#if defined(DROIDGFXSIM_HAS_VULKAN)
    , swapChainImageFormat(VK_FORMAT_B8G8R8A8_UNORM)
    , depthFormat(VK_FORMAT_D32_SFLOAT)
#else
    , swapChainImageFormat(0)
    , depthFormat(0)
#endif
    , depthImage(VK_NULL_HANDLE)
    , depthImageMemory(VK_NULL_HANDLE)
    , depthImageView(VK_NULL_HANDLE)
    , offscreenImage(VK_NULL_HANDLE)
    , offscreenImageMemory(VK_NULL_HANDLE)
    , offscreenImageView(VK_NULL_HANDLE)
    , descriptorSetLayout(VK_NULL_HANDLE)
    , descriptorPool(VK_NULL_HANDLE)
    , uniformBuffer(VK_NULL_HANDLE)
    , uniformBufferMemory(VK_NULL_HANDLE)
    , uniformBufferMapped(nullptr)
    , defaultTexture{}
    , overlay_(nullptr)
{
}

VulkanRenderer::~VulkanRenderer() {
    cleanup();
}

bool VulkanRenderer::isSupported() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    uint32_t extensionCount = 0;
    VkResult res = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    if (res != VK_SUCCESS) {
        LOGI("Vulkan probe: vkEnumerateInstanceExtensionProperties returned %d", res);
        return false;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VulkanSupportProbe";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "droidgfxsim";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkInstance testInstance = VK_NULL_HANDLE;
    res = vkCreateInstance(&createInfo, nullptr, &testInstance);
    if (res != VK_SUCCESS) {
        LOGI("Vulkan probe: instance creation returned %d", res);
        return false;
    }

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(testInstance, &deviceCount, nullptr);
    vkDestroyInstance(testInstance, nullptr);

    if (deviceCount == 0) {
        LOGI("Vulkan probe: 0 physical devices found.");
        return false;
    }

    LOGI("Vulkan is supported (%u physical devices found)", deviceCount);
    return true;
#else
    LOGI("Vulkan is disabled or headers are missing at compile time.");
    return false;
#endif
}

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, uint32_t properties) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    LOGE("Failed to find suitable memory type for Vulkan buffer/image (filter: 0x%x, props: 0x%x)", typeFilter, properties);
#else
    (void)typeFilter;
    (void)properties;
#endif
    return 0;
}

VkFormat VulkanRenderer::findDepthFormat() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    const VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    for (VkFormat candidate : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, candidate, &props);
        if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ==
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return candidate;
        }
    }
    return VK_FORMAT_D32_SFLOAT;
#else
    return 0;
#endif
}

void VulkanRenderer::createBuffer(uint64_t size, uint32_t usage, uint32_t properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult res = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create Vulkan buffer of size %llu (VkResult %d)", static_cast<unsigned long long>(size), res);
        buffer = VK_NULL_HANDLE;
        return;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    res = vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
    if (res != VK_SUCCESS) {
        LOGE("Failed to allocate buffer memory (VkResult %d)", res);
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        bufferMemory = VK_NULL_HANDLE;
        return;
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
#else
    (void)size; (void)usage; (void)properties; (void)buffer; (void)bufferMemory;
#endif
}

void VulkanRenderer::uploadBufferData(VkDeviceMemory memory, const void* data, uint64_t size) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (!memory || !data || size == 0) return;
    void* mapped = nullptr;
    VkResult res = vkMapMemory(device, memory, 0, size, 0, &mapped);
    if (res == VK_SUCCESS && mapped) {
        std::memcpy(mapped, data, static_cast<size_t>(size));
        vkUnmapMemory(device, memory);
    } else {
        LOGE("Failed to map memory for buffer upload (VkResult %d)", res);
    }
#else
    (void)memory; (void)data; (void)size;
#endif
}

void VulkanRenderer::createImage(uint32_t width, uint32_t height, uint32_t format, uint32_t tiling, uint32_t usage, uint32_t properties, VkImage& image, VkDeviceMemory& imageMemory) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = static_cast<VkFormat>(format);
    imageInfo.tiling = static_cast<VkImageTiling>(tiling);
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = static_cast<VkImageUsageFlags>(usage);
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VkResult res = vkCreateImage(device, &imageInfo, nullptr, &image);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create Vulkan image (%ux%u, VkResult %d)", width, height, res);
        image = VK_NULL_HANDLE;
        return;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    res = vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory);
    if (res != VK_SUCCESS) {
        LOGE("Failed to allocate Vulkan image memory (VkResult %d)", res);
        vkDestroyImage(device, image, nullptr);
        image = VK_NULL_HANDLE;
        imageMemory = VK_NULL_HANDLE;
        return;
    }

    vkBindImageMemory(device, image, imageMemory, 0);
#else
    (void)width; (void)height; (void)format; (void)tiling; (void)usage; (void)properties; (void)image; (void)imageMemory;
#endif
}

VkImageView VulkanRenderer::createImageView(VkImage image, uint32_t format, uint32_t aspectFlags) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = static_cast<VkFormat>(format);
    viewInfo.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(aspectFlags);
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView = VK_NULL_HANDLE;
    VkResult res = vkCreateImageView(device, &viewInfo, nullptr, &imageView);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create Vulkan image view (VkResult %d)", res);
        return VK_NULL_HANDLE;
    }
    return imageView;
#else
    (void)image; (void)format; (void)aspectFlags;
    return VK_NULL_HANDLE;
#endif
}

VkCommandBuffer VulkanRenderer::beginSingleTimeCommands() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
#else
    return VK_NULL_HANDLE;
#endif
}

void VulkanRenderer::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (commandBuffer == VK_NULL_HANDLE) return;
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
#else
    (void)commandBuffer;
#endif
}

void VulkanRenderer::transitionImageLayout(VkImage image, uint32_t format, uint32_t oldLayout, uint32_t newLayout) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    (void)format;
    VkCommandBuffer cmd = beginSingleTimeCommands();
    if (cmd == VK_NULL_HANDLE) return;

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = static_cast<VkImageLayout>(oldLayout);
    barrier.newLayout = static_cast<VkImageLayout>(newLayout);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(cmd);
#else
    (void)image; (void)format; (void)oldLayout; (void)newLayout;
#endif
}

void VulkanRenderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkCommandBuffer cmd = beginSingleTimeCommands();
    if (cmd == VK_NULL_HANDLE) return;

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endSingleTimeCommands(cmd);
#else
    (void)buffer; (void)image; (void)width; (void)height;
#endif
}

bool VulkanRenderer::createTextureFromRGBA(const unsigned char* rgbaPixels, uint32_t width, uint32_t height, TextureResource& outTex) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (!rgbaPixels || width == 0 || height == 0 || device == VK_NULL_HANDLE ||
        descriptorPool == VK_NULL_HANDLE || descriptorSetLayout == VK_NULL_HANDLE) {
        return false;
    }

    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4u;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    createBuffer(imageSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer,
                 stagingMemory);
    if (stagingBuffer == VK_NULL_HANDLE || stagingMemory == VK_NULL_HANDLE) {
        return false;
    }
    uploadBufferData(stagingMemory, rgbaPixels, imageSize);

    createImage(width,
                height,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                outTex.image,
                outTex.memory);

    if (outTex.image == VK_NULL_HANDLE) {
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        return false;
    }

    transitionImageLayout(outTex.image, VK_FORMAT_R8G8B8A8_UNORM,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, outTex.image, width, height);
    transitionImageLayout(outTex.image, VK_FORMAT_R8G8B8A8_UNORM,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    outTex.view = createImageView(outTex.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
    if (outTex.view == VK_NULL_HANDLE) {
        destroyTextureResource(outTex);
        return false;
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &outTex.sampler) != VK_SUCCESS) {
        LOGE("Failed to create Vulkan texture sampler");
        destroyTextureResource(outTex);
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &outTex.descriptorSet) != VK_SUCCESS) {
        LOGE("Failed to allocate Vulkan descriptor set for texture");
        destroyTextureResource(outTex);
        return false;
    }

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(FrameUBO);

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = outTex.view;
    imageInfo.sampler = outTex.sampler;

    VkWriteDescriptorSet descriptorWrites[2]{};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = outTex.descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = outTex.descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 2, descriptorWrites, 0, nullptr);
    return true;
#else
    (void)rgbaPixels; (void)width; (void)height; (void)outTex;
    return false;
#endif
}

void VulkanRenderer::destroyTextureResource(TextureResource& tex) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (device != VK_NULL_HANDLE) {
        if (tex.sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, tex.sampler, nullptr);
        }
        if (tex.view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, tex.view, nullptr);
        }
        if (tex.image != VK_NULL_HANDLE) {
            vkDestroyImage(device, tex.image, nullptr);
        }
        if (tex.memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, tex.memory, nullptr);
        }
    }
    tex = TextureResource{};
#else
    (void)tex;
#endif
}

void VulkanRenderer::loadTextures(std::map<std::string, Image*>& images) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }

    for (auto it = images.begin(); it != images.end(); ++it) {
        Image* img = it->second;
        if (initialized && device != VK_NULL_HANDLE &&
            textures.find(it->first) == textures.end() &&
            img != nullptr && img->data != nullptr && img->w > 0 && img->h > 0 && img->comp >= 1) {
            const size_t pixelCount = static_cast<size_t>(img->w) * static_cast<size_t>(img->h);
            std::vector<unsigned char> rgba(pixelCount * 4u, 255);
            if (img->comp == 1) {
                for (size_t i = 0; i < pixelCount; ++i) {
                    const unsigned char v = img->data[i];
                    rgba[i * 4 + 0] = v;
                    rgba[i * 4 + 1] = v;
                    rgba[i * 4 + 2] = v;
                    rgba[i * 4 + 3] = 255;
                }
            } else if (img->comp == 2) {
                for (size_t i = 0; i < pixelCount; ++i) {
                    const unsigned char v = img->data[i * 2 + 0];
                    rgba[i * 4 + 0] = v;
                    rgba[i * 4 + 1] = v;
                    rgba[i * 4 + 2] = v;
                    rgba[i * 4 + 3] = img->data[i * 2 + 1];
                }
            } else if (img->comp == 3) {
                for (size_t i = 0; i < pixelCount; ++i) {
                    rgba[i * 4 + 0] = img->data[i * 3 + 0];
                    rgba[i * 4 + 1] = img->data[i * 3 + 1];
                    rgba[i * 4 + 2] = img->data[i * 3 + 2];
                    rgba[i * 4 + 3] = 255;
                }
            } else {
                std::memcpy(rgba.data(), img->data, pixelCount * 4u);
            }

            TextureResource texRes{};
            if (createTextureFromRGBA(rgba.data(), static_cast<uint32_t>(img->w), static_cast<uint32_t>(img->h), texRes)) {
                textures.insert(std::make_pair(it->first, texRes));
            }
        }
        delete img;
        it->second = nullptr;
    }
    images.clear();
#else
    for (auto it = images.begin(); it != images.end(); ++it) {
        delete it->second;
        it->second = nullptr;
    }
    images.clear();
#endif
}

void VulkanRenderer::setOverlay(ImGuiOverlay* overlay) {
    overlay_ = overlay;
}

ImGuiVulkanInitInfo VulkanRenderer::getImGuiInitInfo() const {
    ImGuiVulkanInitInfo info{};
#if defined(DROIDGFXSIM_HAS_VULKAN)
    info.instance = static_cast<void*>(instance);
    info.physical_device = static_cast<void*>(physicalDevice);
    info.device = static_cast<void*>(device);
    info.queue_family = graphicsQueueFamilyIndex;
    info.queue = static_cast<void*>(graphicsQueue);
    info.render_pass = static_cast<void*>(renderPass);
    uint32_t count = static_cast<uint32_t>(swapChainImages.size());
    info.min_image_count = std::max(2u, count);
    info.image_count = std::max(2u, count);
#endif
    return info;
}

void VulkanRenderer::onResize(int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    framebufferResized = true;
}

void VulkanRenderer::clearMeshCache() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
        for (auto& pair : meshBuffers) {
            if (pair.second.vertexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, pair.second.vertexBuffer, nullptr);
            }
            if (pair.second.vertexBufferMemory != VK_NULL_HANDLE) {
                vkFreeMemory(device, pair.second.vertexBufferMemory, nullptr);
            }
            if (pair.second.indexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, pair.second.indexBuffer, nullptr);
            }
            if (pair.second.indexBufferMemory != VK_NULL_HANDLE) {
                vkFreeMemory(device, pair.second.indexBufferMemory, nullptr);
            }
        }
    }
    meshBuffers.clear();
#endif
}

void VulkanRenderer::ensureMeshBuffers(scenegraph::GeometryNode* geomNode) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (!geomNode || geomNode->vertex_data.empty()) return;
    auto it = meshBuffers.find(geomNode);
    if (it != meshBuffers.end()) {
        return;
    }

    MeshBuffer mb{};
    mb.vertexCount = static_cast<uint32_t>(geomNode->vertex_data.size());
    VkDeviceSize vertexBufferSize = sizeof(scenegraph::Vertex) * geomNode->vertex_data.size();

    createBuffer(vertexBufferSize,
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 mb.vertexBuffer,
                 mb.vertexBufferMemory);

    uploadBufferData(mb.vertexBufferMemory, geomNode->vertex_data.data(), vertexBufferSize);

    if (!geomNode->index_data.empty()) {
        mb.indexCount = static_cast<uint32_t>(geomNode->index_data.size());
        VkDeviceSize indexBufferSize = sizeof(uint32_t) * geomNode->index_data.size();

        createBuffer(indexBufferSize,
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     mb.indexBuffer,
                     mb.indexBufferMemory);

        uploadBufferData(mb.indexBufferMemory, geomNode->index_data.data(), indexBufferSize);
    }

    meshBuffers[geomNode] = mb;
#else
    (void)geomNode;
#endif
}

bool VulkanRenderer::createInstance() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "droidgfxsim";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "droidgfxsim_engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> requiredExtensions;
    if (!isHeadless && window != nullptr) {
#if defined(DESKTOP_APP) && !defined(__ANDROID__)
        uint32_t glfwExtCount = 0;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
        if (glfwExts && glfwExtCount > 0) {
            for (uint32_t i = 0; i < glfwExtCount; i++) {
                requiredExtensions.push_back(glfwExts[i]);
            }
        } else {
            LOGI("GLFW required instance extensions not available; falling back to headless Vulkan");
            isHeadless = true;
        }
#endif
    }

    uint32_t availableExtCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(availableExtCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtCount, availableExtensions.data());

    std::vector<const char*> validExtensions;
    for (const char* req : requiredExtensions) {
        bool found = false;
        for (const auto& ext : availableExtensions) {
            if (std::strcmp(req, ext.extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (found) {
            validExtensions.push_back(req);
        } else {
            LOGI("Instance extension %s not supported by loader", req);
        }
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(validExtensions.size());
    createInfo.ppEnabledExtensionNames = validExtensions.empty() ? nullptr : validExtensions.data();
    createInfo.enabledLayerCount = 0;

    VkResult res = vkCreateInstance(&createInfo, nullptr, &instance);
    if (res != VK_SUCCESS) {
        // Fallback to Vulkan 1.0 apiVersion if 1.3 unsupported by older loader
        appInfo.apiVersion = VK_API_VERSION_1_0;
        res = vkCreateInstance(&createInfo, nullptr, &instance);
    }

    if (res != VK_SUCCESS) {
        LOGE("Failed to create Vulkan instance (VkResult %d)", res);
        return false;
    }

    LOGI("Vulkan instance created successfully (API version 1.3 target)");

    if (!isHeadless && window != nullptr) {
#if defined(DESKTOP_APP) && !defined(__ANDROID__)
        res = glfwCreateWindowSurface(instance, window, nullptr, &surface);
        if (res != VK_SUCCESS) {
            LOGI("Failed to create GLFW window surface (%d); falling back to headless mode", res);
            surface = VK_NULL_HANDLE;
            isHeadless = true;
        } else {
            LOGI("GLFW Vulkan window surface created successfully");
        }
#else
        isHeadless = true;
#endif
    }

    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::pickPhysicalDevice() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOGE("Failed to find GPUs with Vulkan support");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    physicalDevice = VK_NULL_HANDLE;
    int bestScore = -1;

    for (const auto& deviceCandidate : devices) {
        VkPhysicalDeviceProperties props;
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceProperties(deviceCandidate, &props);
        vkGetPhysicalDeviceFeatures(deviceCandidate, &features);

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(deviceCandidate, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(deviceCandidate, &queueFamilyCount, queueFamilies.data());

        int gIndex = -1;
        int pIndex = -1;

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                gIndex = static_cast<int>(i);
            }
            if (!isHeadless && surface != VK_NULL_HANDLE) {
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(deviceCandidate, i, surface, &presentSupport);
                if (presentSupport) {
                    pIndex = static_cast<int>(i);
                }
            }
        }

        if (gIndex >= 0 && (isHeadless || pIndex >= 0)) {
            int score = 1;
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score = 4;
            else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score = 3;
            else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) score = 2;

            if (score > bestScore) {
                bestScore = score;
                physicalDevice = deviceCandidate;
                graphicsQueueFamilyIndex = static_cast<uint32_t>(gIndex);
                presentQueueFamilyIndex = (pIndex >= 0) ? static_cast<uint32_t>(pIndex) : graphicsQueueFamilyIndex;
                LOGI("Selected Vulkan physical device: %s (Type: %d)", props.deviceName, props.deviceType);
            }
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        if (!isHeadless) {
            LOGI("No device supported surface presentation; switching to headless offscreen rendering");
            isHeadless = true;
            if (surface != VK_NULL_HANDLE) {
                vkDestroySurfaceKHR(instance, surface, nullptr);
                surface = VK_NULL_HANDLE;
            }
            return pickPhysicalDevice();
        }
        LOGE("Failed to find a suitable Vulkan physical device");
        return false;
    }

    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createLogicalDevice() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { graphicsQueueFamilyIndex, presentQueueFamilyIndex };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    std::vector<const char*> deviceExtensions;
    if (!isHeadless && surface != VK_NULL_HANDLE) {
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.empty() ? nullptr : deviceExtensions.data();

    VkResult res = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create Vulkan logical device (VkResult %d)", res);
        return false;
    }

    vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);
    if (!isHeadless && surface != VK_NULL_HANDLE) {
        vkGetDeviceQueue(device, presentQueueFamilyIndex, 0, &presentQueue);
    } else {
        presentQueue = graphicsQueue;
    }

    LOGI("Vulkan logical device and queues initialized successfully");
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createSwapChain() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (isHeadless || surface == VK_NULL_HANDLE) {
        // Headless offscreen rendering mode setup
        swapChainExtent = { 1200, 800 };
        swapChainImageFormat = VK_FORMAT_R8G8B8A8_UNORM;

        createImage(swapChainExtent.width,
                    swapChainExtent.height,
                    swapChainImageFormat,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    offscreenImage,
                    offscreenImageMemory);
        if (offscreenImage == VK_NULL_HANDLE) {
            return false;
        }

        offscreenImageView = createImageView(offscreenImage, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        if (offscreenImageView == VK_NULL_HANDLE) {
            return false;
        }

        swapChainImages.clear();
        swapChainImages.push_back(offscreenImage);
        swapChainImageViews.clear();
        swapChainImageViews.push_back(offscreenImageView);
        LOGI("Headless Vulkan offscreen framebuffer target created (%ux%u)", swapChainExtent.width, swapChainExtent.height);
        return true;
    }

    // Windowed presentation mode setup
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount > 0) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
    }

    VkSurfaceFormatKHR surfaceFormat = formats.empty() ?
        VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR } : formats[0];
    for (const auto& availableFormat : formats) {
        if ((availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM ||
             availableFormat.format == VK_FORMAT_R8G8B8A8_UNORM) &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = availableFormat;
            break;
        }
    }
    swapChainImageFormat = surfaceFormat.format;

    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() && capabilities.currentExtent.width > 0) {
        swapChainExtent = capabilities.currentExtent;
    } else {
        int width = 1200, height = 800;
#if defined(DESKTOP_APP) && !defined(__ANDROID__)
        if (window) {
            glfwGetFramebufferSize(window, &width, &height);
        }
#endif
        swapChainExtent.width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        swapChainExtent.height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapChainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = { graphicsQueueFamilyIndex, presentQueueFamilyIndex };
    if (graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;

    VkResult res = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create Vulkan swap chain (%d); falling back to headless mode", res);
        isHeadless = true;
        return createSwapChain();
    }

    uint32_t actualImageCount = 0;
    vkGetSwapchainImagesKHR(device, swapChain, &actualImageCount, nullptr);
    swapChainImages.resize(actualImageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &actualImageCount, swapChainImages.data());

    swapChainImageViews.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        if (swapChainImageViews[i] == VK_NULL_HANDLE) {
            LOGE("Failed to create swap chain image view %zu", i);
            return false;
        }
    }

    LOGI("Vulkan swapchain initialized (%zu images, extent: %ux%u)", swapChainImages.size(), swapChainExtent.width, swapChainExtent.height);
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createDepthResources() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    depthFormat = findDepthFormat();
    createImage(swapChainExtent.width,
                swapChainExtent.height,
                depthFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                depthImage,
                depthImageMemory);
    if (depthImage == VK_NULL_HANDLE) {
        LOGE("Failed to create depth image");
        return false;
    }
    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    if (depthImageView == VK_NULL_HANDLE) {
        LOGE("Failed to create depth image view");
        return false;
    }
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createRenderPass() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = (!isHeadless && surface != VK_NULL_HANDLE) ?
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkResult res = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create Vulkan render pass (VkResult %d)", res);
        return false;
    }

    LOGI("Vulkan render pass created successfully");
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createDescriptorSetLayout() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[2] = { uboLayoutBinding, samplerLayoutBinding };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;

    VkResult res = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create descriptor set layout (VkResult %d)", res);
        return false;
    }
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createGraphicsPipeline() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkShaderModuleCreateInfo vertModuleInfo{};
    vertModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertModuleInfo.codeSize = sizeof(kVertexShaderSpv);
    vertModuleInfo.pCode = kVertexShaderSpv;

    VkShaderModule vertShaderModule = VK_NULL_HANDLE;
    VkResult res = vkCreateShaderModule(device, &vertModuleInfo, nullptr, &vertShaderModule);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create vertex shader module (VkResult %d)", res);
        return false;
    }

    VkShaderModuleCreateInfo fragModuleInfo{};
    fragModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragModuleInfo.codeSize = sizeof(kFragmentShaderSpv);
    fragModuleInfo.pCode = kFragmentShaderSpv;

    VkShaderModule fragShaderModule = VK_NULL_HANDLE;
    res = vkCreateShaderModule(device, &fragModuleInfo, nullptr, &fragShaderModule);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create fragment shader module (VkResult %d)", res);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo vertStageInfo{};
    vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertShaderModule;
    vertStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragShaderModule;
    fragStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertStageInfo, fragStageInfo };

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(scenegraph::Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescriptions[3]{};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = static_cast<uint32_t>(offsetof(scenegraph::Vertex, position));

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = static_cast<uint32_t>(offsetof(scenegraph::Vertex, normal));

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = static_cast<uint32_t>(offsetof(scenegraph::Vertex, texcoord));

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 3;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateInfo.pDynamicStates = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstantBlock);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    res = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create pipeline layout (VkResult %d)", res);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);

    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);

    if (res != VK_SUCCESS) {
        LOGE("Failed to create graphics pipeline (VkResult %d)", res);
        return false;
    }

    LOGI("Vulkan graphics pipeline created successfully");
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createFramebuffers() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        VkImageView attachments[2] = {
            swapChainImageViews[i],
            depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 2;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        VkResult res = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]);
        if (res != VK_SUCCESS) {
            LOGE("Failed to create framebuffer %zu (VkResult %d)", i, res);
            return false;
        }
    }

    LOGI("Created %zu Vulkan framebuffers", swapChainFramebuffers.size());
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createCommandPool() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;

    VkResult res = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create Vulkan command pool (VkResult %d)", res);
        return false;
    }

    LOGI("Vulkan command pool created successfully");
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createUniformBuffer() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    createBuffer(sizeof(FrameUBO),
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 uniformBuffer,
                 uniformBufferMemory);
    if (uniformBuffer == VK_NULL_HANDLE || uniformBufferMemory == VK_NULL_HANDLE) {
        return false;
    }

    VkResult res = vkMapMemory(device, uniformBufferMemory, 0, sizeof(FrameUBO), 0, &uniformBufferMapped);
    if (res != VK_SUCCESS || uniformBufferMapped == nullptr) {
        LOGE("Failed to persistently map uniform buffer memory (VkResult %d)", res);
        return false;
    }

    FrameUBO initialUbo{};
    initialUbo.proj = glm::mat4(1.0f);
    initialUbo.view = glm::mat4(1.0f);
    initialUbo.lightPos = glm::vec4(22.0f, 44.0f, 28.0f, 1.0f);
    std::memcpy(uniformBufferMapped, &initialUbo, sizeof(FrameUBO));
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createDescriptorPool() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 128;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 128;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 128;

    VkResult res = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create Vulkan descriptor pool (VkResult %d)", res);
        return false;
    }
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createDefaultTexture() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    const unsigned char whitePixel[4] = { 255, 255, 255, 255 };
    return createTextureFromRGBA(whitePixel, 1, 1, defaultTexture);
#else
    return false;
#endif
}

bool VulkanRenderer::createCommandBuffers() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    commandBuffers.resize(swapChainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    VkResult res = vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data());
    if (res != VK_SUCCESS) {
        LOGE("Failed to allocate command buffers (VkResult %d)", res);
        return false;
    }

    LOGI("Allocated %zu Vulkan command buffers", commandBuffers.size());
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createSyncObjects() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkResult r1 = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore);
    VkResult r2 = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore);
    VkResult r3 = vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence);

    if (r1 != VK_SUCCESS || r2 != VK_SUCCESS || r3 != VK_SUCCESS) {
        LOGE("Failed to create Vulkan sync objects (%d, %d, %d)", r1, r2, r3);
        return false;
    }

    LOGI("Vulkan synchronization objects created successfully");
    return true;
#else
    return false;
#endif
}

void VulkanRenderer::cleanupSwapChain() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (device == VK_NULL_HANDLE) return;

    if (commandPool != VK_NULL_HANDLE && !commandBuffers.empty()) {
        vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
        commandBuffers.clear();
    }

    for (auto fb : swapChainFramebuffers) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
    }
    swapChainFramebuffers.clear();

    if (depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, depthImageView, nullptr);
        depthImageView = VK_NULL_HANDLE;
    }
    if (depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, depthImage, nullptr);
        depthImage = VK_NULL_HANDLE;
    }
    if (depthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, depthImageMemory, nullptr);
        depthImageMemory = VK_NULL_HANDLE;
    }

    for (auto iv : swapChainImageViews) {
        if (iv != VK_NULL_HANDLE && iv != offscreenImageView) {
            vkDestroyImageView(device, iv, nullptr);
        }
    }
    swapChainImageViews.clear();

    if (swapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        swapChain = VK_NULL_HANDLE;
    }
    swapChainImages.clear();
#endif
}

bool VulkanRenderer::recreateSwapChain() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (isHeadless || device == VK_NULL_HANDLE || surface == VK_NULL_HANDLE) {
        return false;
    }

    int width = 0;
    int height = 0;
#if defined(DESKTOP_APP) && !defined(__ANDROID__)
    if (window) {
        glfwGetFramebufferSize(window, &width, &height);
    }
#endif
    if (width == 0 || height == 0) {
        framebufferResized = true;
        return false;
    }

    vkDeviceWaitIdle(device);
    cleanupSwapChain();

    if (!createSwapChain() || !createDepthResources() || !createFramebuffers() || !createCommandBuffers()) {
        LOGE("Failed to recreate Vulkan swap chain");
        return false;
    }

    if (overlay_) {
        overlay_->setVulkanMinImageCount(static_cast<uint32_t>(std::max<size_t>(2, swapChainImages.size())));
    }
    framebufferResized = false;
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::init(GLFWwindow* win) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    cleanup();
    this->window = win;
    this->isHeadless = (win == nullptr);
    this->framebufferResized = false;

    LOGI("Initializing Vulkan 1.3 IRenderer backend (%s mode)...", isHeadless ? "Headless Offscreen" : "Windowed");

    if (!createInstance()) {
        LOGE("Vulkan initialization failed at createInstance");
        cleanup();
        return false;
    }
    if (!pickPhysicalDevice()) {
        LOGE("Vulkan initialization failed at pickPhysicalDevice");
        cleanup();
        return false;
    }
    if (!createLogicalDevice()) {
        LOGE("Vulkan initialization failed at createLogicalDevice");
        cleanup();
        return false;
    }
    if (!createSwapChain()) {
        LOGE("Vulkan initialization failed at createSwapChain");
        cleanup();
        return false;
    }
    if (!createDepthResources()) {
        LOGE("Vulkan initialization failed at createDepthResources");
        cleanup();
        return false;
    }
    if (!createRenderPass()) {
        LOGE("Vulkan initialization failed at createRenderPass");
        cleanup();
        return false;
    }
    if (!createDescriptorSetLayout()) {
        LOGE("Vulkan initialization failed at createDescriptorSetLayout");
        cleanup();
        return false;
    }
    if (!createGraphicsPipeline()) {
        LOGE("Vulkan initialization failed at createGraphicsPipeline");
        cleanup();
        return false;
    }
    if (!createFramebuffers()) {
        LOGE("Vulkan initialization failed at createFramebuffers");
        cleanup();
        return false;
    }
    if (!createCommandPool()) {
        LOGE("Vulkan initialization failed at createCommandPool");
        cleanup();
        return false;
    }
    if (!createUniformBuffer()) {
        LOGE("Vulkan initialization failed at createUniformBuffer");
        cleanup();
        return false;
    }
    if (!createDescriptorPool()) {
        LOGE("Vulkan initialization failed at createDescriptorPool");
        cleanup();
        return false;
    }
    if (!createDefaultTexture()) {
        LOGE("Vulkan initialization failed at createDefaultTexture");
        cleanup();
        return false;
    }
    if (!createCommandBuffers()) {
        LOGE("Vulkan initialization failed at createCommandBuffers");
        cleanup();
        return false;
    }
    if (!createSyncObjects()) {
        LOGE("Vulkan initialization failed at createSyncObjects");
        cleanup();
        return false;
    }

    initialized = true;
    LOGI("Vulkan 1.3 IRenderer successfully initialized and ready for draw passes");
    return true;
#else
    (void)win;
    LOGE("Cannot initialize VulkanRenderer: Vulkan is disabled or headers are missing");
    return false;
#endif
}

void VulkanRenderer::cleanup() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);

        clearMeshCache();

        for (auto& pair : textures) {
            destroyTextureResource(pair.second);
        }
        textures.clear();
        destroyTextureResource(defaultTexture);

        if (uniformBufferMapped != nullptr && uniformBufferMemory != VK_NULL_HANDLE) {
            vkUnmapMemory(device, uniformBufferMemory);
            uniformBufferMapped = nullptr;
        }
        if (uniformBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, uniformBuffer, nullptr);
            uniformBuffer = VK_NULL_HANDLE;
        }
        if (uniformBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, uniformBufferMemory, nullptr);
            uniformBufferMemory = VK_NULL_HANDLE;
        }

        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            descriptorPool = VK_NULL_HANDLE;
        }

        if (imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
            imageAvailableSemaphore = VK_NULL_HANDLE;
        }
        if (renderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
            renderFinishedSemaphore = VK_NULL_HANDLE;
        }
        if (inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(device, inFlightFence, nullptr);
            inFlightFence = VK_NULL_HANDLE;
        }

        cleanupSwapChain();

        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }

        if (graphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, graphicsPipeline, nullptr);
            graphicsPipeline = VK_NULL_HANDLE;
        }
        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }
        if (descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
            descriptorSetLayout = VK_NULL_HANDLE;
        }
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }

        if (offscreenImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, offscreenImageView, nullptr);
            offscreenImageView = VK_NULL_HANDLE;
        }
        if (offscreenImage != VK_NULL_HANDLE) {
            vkDestroyImage(device, offscreenImage, nullptr);
            offscreenImage = VK_NULL_HANDLE;
        }
        if (offscreenImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, offscreenImageMemory, nullptr);
            offscreenImageMemory = VK_NULL_HANDLE;
        }

        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }

    if (instance != VK_NULL_HANDLE) {
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }

    swapChainImages.clear();
    physicalDevice = VK_NULL_HANDLE;
    graphicsQueue = VK_NULL_HANDLE;
    presentQueue = VK_NULL_HANDLE;
    window = nullptr;
    overlay_ = nullptr;
    framebufferResized = false;
    initialized = false;
#endif
}

void VulkanRenderer::recordNodeDraw(VkCommandBuffer cmd, scenegraph::Node* node, const void* parentMatrix, VkDescriptorSet currentDescriptorSet, VkDescriptorSet& boundDescriptorSet) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (!node) return;

    const glm::mat4& parentMat = *static_cast<const glm::mat4*>(parentMatrix);
    glm::mat4 currentModel = parentMat;
    VkDescriptorSet activeDesc = currentDescriptorSet;

    if (node->type == scenegraph::NodeType::Material) {
        auto* mn = static_cast<scenegraph::MaterialNode*>(node);
        activeDesc = defaultTexture.descriptorSet;
        if (!mn->diffuse_texture.empty()) {
            auto texIt = textures.find(mn->diffuse_texture);
            if (texIt != textures.end() && texIt->second.descriptorSet != VK_NULL_HANDLE) {
                activeDesc = texIt->second.descriptorSet;
            }
        }
    } else if (node->type == scenegraph::NodeType::Transform) {
        auto* tn = static_cast<scenegraph::TransformNode*>(node);
        currentModel = parentMat * tn->matrix;
    } else if (node->type == scenegraph::NodeType::Geometry) {
        auto* gn = static_cast<scenegraph::GeometryNode*>(node);
        if (gn && !gn->vertex_data.empty()) {
            ensureMeshBuffers(gn);
            auto it = meshBuffers.find(gn);
            if (it != meshBuffers.end() && it->second.vertexBuffer != VK_NULL_HANDLE) {
                if (activeDesc != VK_NULL_HANDLE && activeDesc != boundDescriptorSet) {
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                                            0, 1, &activeDesc, 0, nullptr);
                    boundDescriptorSet = activeDesc;
                }

                PushConstantBlock pc{};
                pc.model = currentModel;
                vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantBlock), &pc);

                VkBuffer vertexBuffers[] = { it->second.vertexBuffer };
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

                if (it->second.indexBuffer != VK_NULL_HANDLE && it->second.indexCount > 0) {
                    vkCmdBindIndexBuffer(cmd, it->second.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(cmd, it->second.indexCount, 1, 0, 0, 0);
                } else {
                    vkCmdDraw(cmd, it->second.vertexCount, 1, 0, 0);
                }
            }
        }
    }

    for (scenegraph::Node* child : node->children) {
        recordNodeDraw(cmd, child, &currentModel, activeDesc, boundDescriptorSet);
    }
#else
    (void)cmd; (void)node; (void)parentMatrix; (void)currentDescriptorSet; (void)boundDescriptorSet;
#endif
}

void VulkanRenderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, scenegraph::Node* rootNode, Camera* camera) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (uniformBufferMapped != nullptr) {
        FrameUBO ubo{};
        const glm::mat4 glToVkClip(
            glm::vec4(1.0f,  0.0f, 0.0f, 0.0f),
            glm::vec4(0.0f, -1.0f, 0.0f, 0.0f),
            glm::vec4(0.0f,  0.0f, 0.5f, 0.0f),
            glm::vec4(0.0f,  0.0f, 0.5f, 1.0f)
        );
        if (camera != nullptr) {
            ubo.proj = glToVkClip * camera->projection_matrix;
            ubo.view = camera->modelview_matrix;
        } else {
            ubo.proj = glToVkClip;
            ubo.view = glm::mat4(1.0f);
        }
        ubo.lightPos = glm::vec4(22.0f, 44.0f, 28.0f, 1.0f);
        std::memcpy(uniformBufferMapped, &ubo, sizeof(FrameUBO));
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        LOGE("Failed to begin recording Vulkan command buffer");
        return;
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    VkClearValue clearValues[2]{};
    clearValues[0].color = {{0.55f, 0.68f, 0.82f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkDescriptorSet boundDescriptorSet = VK_NULL_HANDLE;
    if (defaultTexture.descriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                                0, 1, &defaultTexture.descriptorSet, 0, nullptr);
        boundDescriptorSet = defaultTexture.descriptorSet;
    }

    glm::mat4 identityModel = glm::mat4(1.0f);
    if (rootNode != nullptr) {
        recordNodeDraw(cmd, rootNode, &identityModel, defaultTexture.descriptorSet, boundDescriptorSet);
    }

    if (!isHeadless && overlay_ != nullptr) {
        overlay_->renderVulkanDrawData(static_cast<void*>(cmd));
    }

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        LOGE("Failed to finalize recording Vulkan command buffer");
    }
#else
    (void)cmd; (void)imageIndex; (void)rootNode; (void)camera;
#endif
}

void VulkanRenderer::render(scenegraph::Node* node, Camera* camera) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (!initialized || device == VK_NULL_HANDLE) {
        return;
    }

    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

    if (!isHeadless && swapChain != VK_NULL_HANDLE) {
        if (framebufferResized) {
            recreateSwapChain();
            return;
        }

        uint32_t imageIndex = 0;
        VkResult res = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        if (res == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
            LOGE("Failed to acquire next Vulkan swap chain image (VkResult %d)", res);
            return;
        }

        vkResetFences(device, 1, &inFlightFence);
        vkResetCommandBuffer(commandBuffers[imageIndex], 0);
        recordCommandBuffer(commandBuffers[imageIndex], imageIndex, node, camera);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

        VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VkResult submitRes = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);
        if (submitRes != VK_SUCCESS) {
            LOGE("Failed to submit draw command buffer to graphics queue (VkResult %d)", submitRes);
            return;
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { swapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        VkResult presentRes = vkQueuePresentKHR(presentQueue, &presentInfo);
        if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR ||
            res == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
        } else if (presentRes != VK_SUCCESS) {
            LOGE("Failed to present Vulkan swap chain image (VkResult %d)", presentRes);
        }
    } else {
        // Headless offscreen rendering mode pass
        vkResetFences(device, 1, &inFlightFence);
        uint32_t imageIndex = 0;
        vkResetCommandBuffer(commandBuffers[imageIndex], 0);
        recordCommandBuffer(commandBuffers[imageIndex], imageIndex, node, camera);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

        VkResult res = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);
        if (res != VK_SUCCESS) {
            LOGE("Failed to submit headless command buffer (VkResult %d)", res);
            return;
        }

        vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    }
#else
    (void)node; (void)camera;
#endif
}
