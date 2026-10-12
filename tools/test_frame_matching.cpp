// Exercises production GameFrame::set/match and TransformProcessor without a GPU.
#include "hle/rt64_workload_queue.h"
#include "hle/rt64_ts2_capture.h"
#include "hle/rt64_state.h"
#include "hle/rt64_task_memory.h"
#include "hle/rt64_ts2_camera.h"
#include <cmath>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <json/json.hpp>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

// The queue's renderer integration owns this callback; these tests never render.
extern "C" void ts2_set_render_aspect(float) {}
extern "C" void ts2_write_emitter_history(const char*) {}
extern "C" uint32_t ts2_emitter_for_dl(uint32_t) { return 0; }
extern "C" void ts2_set_display_list_task(uint32_t) {}
extern "C" int32_t ts2_model_for_dl(uint32_t) { return -1; }
extern "C" int32_t ts2_object_for_vertices(uint32_t) { return -1; }
extern "C" void ts2_set_vi_period_scale(double) {}

using namespace RT64;
static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static void scene(Workload& work, float camera = 0) {
    work.reset();
    auto& d = work.drawData;
    d.transformGroups[0].positionInterpolation = G_EX_COMPONENT_INTERPOLATE;
    d.transformGroups[0].tileInterpolation = G_EX_COMPONENT_SKIP;
    auto view = hlslpp::float4x4::identity();
    view[3][0] = camera;
    d.viewTransforms.push_back(view);
    d.projTransforms.push_back(hlslpp::float4x4::identity());
    d.viewProjTransforms.push_back(view);
    d.viewProjTransformGroups.push_back(0);
    work.fbPairCount = 1;
    auto& pair = work.fbPairs[0];
    pair.colorImage.address = 0x100000;
    pair.colorImage.width = 320;
    pair.colorImage.siz = 2;
    pair.projectionCount = 1;
    pair.projections.resize(1);
    auto& projection = pair.projections[0];
    projection.reset();
    projection.type = Projection::Type::Perspective;
    projection.transformsIndex = 1;
}

// A culled subset keeps the same local coordinates/UVs, just fewer vertices.
static uint32_t model(Workload& work, int first, int count, float x, int identity = 0) {
    auto& d = work.drawData;
    const uint32_t index = uint32_t(d.worldTransforms.size());
    const uint32_t start = d.vertexCount();
    auto matrix = hlslpp::float4x4::identity();
    matrix[3][0] = x;
    d.worldTransforms.push_back(matrix);
    d.worldTransformGroups.push_back(0);
    d.worldTransformPhysicalAddresses.push_back(0x4000 + index * 64);
    d.worldTransformSegmentedAddresses.push_back(0x4000 + index * 64);
    d.worldTransformVertexIndices.push_back(start);
    for (int v = first; v < first + count; ++v) {
        d.posFloats.insert(d.posFloats.end(), {float(v * 3 + identity * 1000), float(v * v), float(v % 7)});
        d.tcFloats.insert(d.tcFloats.end(), {float(v % 3), float(v % 5)});
        d.normColBytes.insert(d.normColBytes.end(), {255, 255, 255, 255});
        d.worldIndices.push_back(uint16_t(index));
        d.viewProjIndices.push_back(1);
        d.velFloats.insert(d.velFloats.end(), {0, 0, 0});
    }
    GameCall call{};
    call.callDesc.minWorldMatrix = index;
    call.callDesc.maxWorldMatrix = index;
    call.callDesc.triangleCount = count / 3;
    call.meshDesc.faceIndicesStart = uint32_t(d.faceIndices.size());
    for (uint32_t v = 0; v < call.callDesc.triangleCount * 3; ++v) d.faceIndices.push_back(start + v);
    work.fbPairs[0].projections[0].addGameCall(call);
    return index;
}

static GameFrame frame(WorkloadQueue& queue, std::initializer_list<uint32_t> indices) {
    GameFrame result;
    result.set(queue, indices.begin(), uint32_t(indices.size()));
    return result;
}

static void match(WorkloadQueue& queue, GameFrame& current, const GameFrame& previous) {
    bool uploaded = false, tiles = false, lookAt = false;
    current.match(nullptr, queue, previous, nullptr, uploaded, tiles, lookAt);
    require(!uploaded, "fixture unexpectedly needed GPU velocity uploads");
}

static void subsetTest() {
    auto queue = std::make_unique<WorkloadQueue>();
    scene(queue->workloads[0]);
    scene(queue->workloads[1]);
    model(queue->workloads[0], 0, 30, 0);
    model(queue->workloads[1], 10, 20, 2);
    auto previous = frame(*queue, {0});
    auto current = frame(*queue, {1});
    match(*queue, current, previous);
    const auto& map = current.frameMap.workloads[1];
    require(map.transforms[1].mapped, "culled model lost its frame match when triangle count changed");
    require(map.transforms[1].prevTransformIndex == 1, "culled model matched wrong transform");
    TransformProcessor::ProcessParams params{};
    params.workloadQueue = queue.get();
    params.curFrame = &current;
    params.prevFrame = &previous;
    params.curFrameWeight = 0.5f;
    params.prevFrameWeight = 0.0f;
    queue->transformProcessor.process(params);
    require(std::abs(float(queue->workloads[1].drawData.lerpWorldTransforms[1][3][0]) - 1.0f) < 0.001f,
        "culled model snapped to current pose instead of the midpoint");
}

// Camera-space CPU scenery: culled vertex arrays under a camera-to-world
// matrix. The matrix must stay current (the common camera interpolates);
// an actor whose camera-relative transform moved still interpolates.
static void cameraSpaceSceneryTest() {
    // 0: scenery matrix (held), 1: moving actor, 2: camera-locked reticle-style
    // geometry on another matrix (both keep matrix interpolation).
    // 3: scenery matrix but labelled camera-locked (first-person reticle) -> not held.
    for (int kind : {0, 1, 2, 3}) {
        const bool actor = kind != 0;
        auto queue = std::make_unique<WorkloadQueue>();
        scene(queue->workloads[0]);
        scene(queue->workloads[1]);
        model(queue->workloads[0], 0, 30, 0);
        model(queue->workloads[1], 10, 20, 2);
        for (int w = 0; w < 2; ++w) {
            auto relative = hlslpp::float4x4::identity();
            relative[3][2] = (kind == 1 && w == 1) ? 5.0f : 0.0f;
            queue->workloads[w].drawData.ts2CameraRelativeTransforms.push_back(relative);
            if (kind == 0 || kind == 3) queue->workloads[w].drawData.worldTransformPhysicalAddresses[1] = 0x1ED830;
            if (kind == 3) queue->workloads[w].drawData.ts2WorldCameraLocked.assign(2, 1);
        }
        auto previous = frame(*queue, {0});
        auto current = frame(*queue, {1});
        match(*queue, current, previous);
        const auto& map = current.frameMap.workloads[1];
        require(map.transforms[1].mapped, "scenery lost its frame match");
        require(map.transforms[1].holdCurrent == !actor, "camera-space scenery classification wrong");
        TransformProcessor::ProcessParams params{};
        params.workloadQueue = queue.get();
        params.curFrame = &current;
        params.prevFrame = &previous;
        params.curFrameWeight = 0.5f;
        queue->transformProcessor.process(params);
        const float x = float(queue->workloads[1].drawData.lerpWorldTransforms[1][3][0]);
        require(std::abs(x - (actor ? 1.0f : 2.0f)) < 0.001f,
            actor ? "moving actor lost interpolation" : "camera-space scenery matrix interpolated against the camera");
    }
}

// Camera turned: a world-fixed point moves to 'predicted'; a camera-locked
// body stays put (small bob); ambiguous or tiny motion is left unknown.
static void lockedObjectTest() {
    const hlslpp::float3 prev(70, -75, -150), predicted(40, -75, -160);
    require(ts2ClassifyObjectMotion(prev, hlslpp::float3(71, -74, -151), predicted) == 1, "camera-locked body not detected");
    require(ts2ClassifyObjectMotion(prev, hlslpp::float3(41, -75, -159), predicted) == -1, "world-fixed object marked locked");
    require(ts2ClassifyObjectMotion(prev, hlslpp::float3(55, -75, -155), predicted) == 0, "ambiguous motion classified");
    require(ts2ClassifyObjectMotion(prev, prev, prev + hlslpp::float3(0.5f, 0, 0)) == 0, "static camera classified");
}

static void identityTest() {
    auto queue = std::make_unique<WorkloadQueue>();
    scene(queue->workloads[0]);
    scene(queue->workloads[1]);
    model(queue->workloads[0], 0, 30, 0, 0);
    model(queue->workloads[0], 0, 30, 10, 1);
    // Swap draw order and put each model nearer the other model's old position.
    model(queue->workloads[1], 10, 20, 1, 1);
    model(queue->workloads[1], 10, 20, 9, 0);
    model(queue->workloads[1], 10, 20, 0, 2);
    auto previous = frame(*queue, {0});
    auto current = frame(*queue, {1});
    match(*queue, current, previous);
    const auto& transforms = current.frameMap.workloads[1].transforms;
    require(transforms[1].mapped && transforms[1].prevTransformIndex == 2, "nearest wrong geometry stole first match");
    require(transforms[2].mapped && transforms[2].prevTransformIndex == 1, "reordered model lost identity");
    require(!transforms[3].mapped, "unrelated model acquired another model's history");
}

static void partitionTest() {
    auto queue = std::make_unique<WorkloadQueue>();
    scene(queue->workloads[0]);
    scene(queue->workloads[1]);
    model(queue->workloads[0], 0, 30, 0);
    model(queue->workloads[1], 0, 30, 100);
    const auto current = frame(*queue, {0, 1});
    require(current.perspectiveScenes.size() == 2, "scene merged local transform indices from separate workloads");
}

static void ownershipTest() {
    auto queue = std::make_unique<WorkloadQueue>();
    // Camera distances deliberately favor the WRONG workload's old scene.
    scene(queue->workloads[0], 0);
    scene(queue->workloads[1], 100);
    scene(queue->workloads[2], 99);
    scene(queue->workloads[3], 1);
    model(queue->workloads[0], 0, 30, 10);
    model(queue->workloads[1], 0, 30, 100);
    model(queue->workloads[2], 0, 30, 12);
    model(queue->workloads[3], 0, 30, 102);
    auto previous = frame(*queue, {0, 1});
    auto current = frame(*queue, {2, 3});
    match(*queue, current, previous);
    for (uint32_t i = 0; i < 2; ++i) {
        const auto& map = current.frameMap.workloads[i + 2];
        require(map.mapped && map.prevWorkloadIndex == i, "workload history ownership changed");
        require(map.transforms[1].mapped, "owned model was not matched");
        require(std::abs(float(map.transforms[1].rigidBody.linearVelocity[0])) < 0.001f,
            "fixture forced linear interpolation must not carry guessed velocity");
        const auto& prev = queue->workloads[i].drawData.worldTransforms[1];
        const auto& cur = queue->workloads[i + 2].drawData.worldTransforms[1];
        const auto midpoint = map.transforms[1].rigidBody.lerp(0.5f, prev, cur, true);
        require(std::abs(float(midpoint[3][0]) - (i == 0 ? 11.0f : 101.0f)) < 0.001f,
            "model decomposition came from another workload");
    }
}

static void addedBatchTest() {
    auto queue = std::make_unique<WorkloadQueue>();
    for (uint32_t i = 0; i < 3; ++i) {
        scene(queue->workloads[i]);
        model(queue->workloads[i], 0, 30, float(i));
    }
    auto previous = frame(*queue, {0});
    auto current = frame(*queue, {1, 2});
    match(*queue, current, previous);
    require(!current.frameMap.workloads[2].mapped, "new workload inherited nonexistent history");
    require(!current.frameMap.workloads[1].mapped, "changed batch count retained ambiguous ordinal history");
    previous = frame(*queue, {0, 1});
    current = frame(*queue, {2});
    match(*queue, current, previous);
    require(!current.frameMap.workloads[2].mapped, "removed batch retained ambiguous ordinal history");
}

static void tileSafetyTest() {
    auto queue = std::make_unique<WorkloadQueue>();
    for (uint32_t i = 0; i < 2; ++i) {
        auto& work = queue->workloads[i];
        scene(work);
        model(work, 0, 30, float(i));
        work.drawData.transformGroups[0].tileInterpolation = G_EX_COMPONENT_AUTO;
        work.drawData.rdpTiles.push_back(interop::RDPTile{});
        work.drawData.callTiles.push_back(DrawCallTile{});
        auto& call = work.fbPairs[0].projections[0].gameCalls[0];
        call.callDesc.tileIndex = 0;
        call.callDesc.tileCount = 1;
    }
    // Identical vertex arrays, but a different set of faces is submitted.
    queue->workloads[1].fbPairs[0].projections[0].gameCalls[0].callDesc.triangleCount = 9;
    auto previous = frame(*queue, {0});
    auto current = frame(*queue, {1});
    match(*queue, current, previous);
    require(current.frameMap.workloads[1].transforms[1].mapped, "changed face count blocked matrix match");
    require(!current.frameMap.workloads[1].tiles[0].mapped, "relaxed matrix key also relaxed texture history");
    // A control with identical faces should retain eligible tile matching.
    queue->workloads[1].fbPairs[0].projections[0].gameCalls[0].callDesc.triangleCount = 10;
    current = frame(*queue, {1});
    match(*queue, current, previous);
    require(current.frameMap.workloads[1].tiles[0].mapped, "exact-face tile matching was accidentally disabled");
}

static void taggedSafetyTest() {
    auto queue = std::make_unique<WorkloadQueue>();
    for (uint32_t i = 0; i < 2; ++i) {
        auto& work = queue->workloads[i];
        scene(work);
        model(work, 0, i == 0 ? 30 : 20, float(i));
        work.drawData.transformGroups[0].matrixId = 42;
    }
    auto previous = frame(*queue, {0});
    auto current = frame(*queue, {1});
    match(*queue, current, previous);
    require(!current.frameMap.workloads[1].transforms[1].mapped,
        "culling-tolerant candidate key leaked into explicitly tagged calls");
}

static void batchTimingTest() {
    auto queue = std::make_unique<WorkloadQueue>();
    scene(queue->workloads[0]);
    scene(queue->workloads[1]);
    constexpr uint32_t models = 160;
    for (uint32_t i = 0; i < models; ++i) {
        model(queue->workloads[0], 0, 30, float(i), i);
        model(queue->workloads[1], 10, 20, float(i + 1), i);
    }
    auto previous = frame(*queue, {0});
    auto current = frame(*queue, {1});
    const auto start = std::chrono::steady_clock::now();
    match(*queue, current, previous);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    for (uint32_t i = 1; i <= models; ++i) {
        const auto& entry = current.frameMap.workloads[1].transforms[i];
        require(entry.mapped && entry.prevTransformIndex == i, "dense same-material batch matched incorrectly");
    }
    // Informational synthetic timing, not a gameplay-performance guarantee.
    std::cout << "160-model same-material fixture matching: " << ms << " ms\n";
}

static void taskMemoryTest() {
    uint8_t taskBytes[16]{}, guestBytes[16]{};
    int consumed = 0;
    writeTaskFramebuffer(taskBytes, guestBytes, 4, 4, [&](uint8_t* dst) {
        require(++consumed == 1, "framebuffer readback consumed twice");
        dst[4] = 42; dst[7] = 91;
    });
    require(consumed == 1 && guestBytes[4] == 42 && guestBytes[7] == 91,
        "readback was not mirrored to live guest memory");
    require(guestBytes[3] == 0 && guestBytes[8] == 0, "mirror exceeded written word range");
    consumed = 0;
    writeTaskFramebuffer(guestBytes, guestBytes, 0, 4, [&](uint8_t* dst) { ++consumed; dst[0] = 7; });
    require(consumed == 1 && guestBytes[0] == 7, "live-memory readback consumed twice");
    std::vector<uint8_t> live(0x800000, 0), snapshot(0x800000, 0);
    live[0x100] = 91; snapshot[0x100] = 17;
    uint32_t interrupt = 0;
    State state(live.data(), &interrupt, [](){});
    {
        TaskMemoryScope memory(state.RDRAM, snapshot.data());
        require(state.fromRDRAM(0x100)[0] == 17, "indirect task reads still see live RAM");
        live[0x100] = 92;
        require(state.fromRDRAM(0x100)[0] == 17, "task changed when the guest advanced");
        require(state.RDRAMWrite == live.data(), "framebuffer writeback lost its guest destination");
        try { TaskMemoryScope nested(state.RDRAM, live.data()); throw 1; } catch (int) {}
        require(state.fromRDRAM(0x100)[0] == 17, "unwinding lost the task memory binding");
    }
    require(state.fromRDRAM(0x100)[0] == 92, "task left a dangling snapshot binding");
    { TaskMemoryScope fallback(state.RDRAM, nullptr);
      require(state.RDRAM == live.data(), "null snapshot did not retain live memory"); }
}

static void simplePartsTest() {
    auto queue = std::make_unique<WorkloadQueue>();
    scene(queue->workloads[0]);
    scene(queue->workloads[1]);
    model(queue->workloads[0], 0, 30, 0, 0);
    model(queue->workloads[0], 0, 30, 10, 1);
    model(queue->workloads[1], 0, 30, 8, 0);
    model(queue->workloads[1], 0, 30, 18, 1);
    model(queue->workloads[1], 0, 30, 99, 2);
    auto previous = frame(*queue, {0});
    auto current = frame(*queue, {1});
    match(*queue, current, previous);
    auto& map = current.frameMap.workloads[1];
    require(map.transforms[1].mapped && map.transforms[2].mapped, "multipart identities did not match");
    // Reproduce conflicting component decisions independently of identity.
    map.transforms[1].rigidBody.lerpTranslation = false;
    map.transforms[2].rigidBody.lerpTranslation = true;
    TransformProcessor::ProcessParams p{};
    p.workloadQueue = queue.get(); p.curFrame = &current; p.prevFrame = &previous;
    p.curFrameWeight = 0.5f;
    queue->transformProcessor.process(p);
    auto& data = queue->workloads[1].drawData;
    require(std::abs(float(data.lerpWorldTransforms[2][3][0] - data.lerpWorldTransforms[1][3][0]) - 10) > 1,
        "fixture did not reproduce inconsistent part spacing");
    p.simpleAutoModels = true;
    for (float weight : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
        p.curFrameWeight = weight;
        queue->transformProcessor.process(p);
        require(std::abs(float(data.lerpWorldTransforms[1][3][0]) - 8*weight) < 0.001f,
            "simple model used the wrong interpolation weight");
        require(std::abs(float(data.lerpWorldTransforms[2][3][0] - data.lerpWorldTransforms[1][3][0]) - 10) < 0.001f,
            "simple interpolation separated the parts");
        require(float(data.lerpWorldTransforms[3][3][0]) == 99, "unmatched geometry borrowed history");
    }
    // A deliberately skipped component in an explicit group must be honored.
    auto explicitGroup = data.transformGroups[0]; explicitGroup.matrixId = 123;
    data.transformGroups.push_back(explicitGroup);
    data.worldTransformGroups[1] = uint32_t(data.transformGroups.size()-1);
    p.curFrameWeight = 0.5f;
    queue->transformProcessor.process(p);
    require(float(data.lerpWorldTransforms[1][3][0]) == 8, "simple AUTO policy overrode an explicit group");
}

static void articulatedCameraSpaceTest() {
    // Enemies and the first-person mesh use the room's camera-space matrix,
    // but their individual vertices must retain animation velocities.
    for (int32_t key : {0x1E4B90, 0x02012340, 0x01087EA0, 0x02056780}) {
      for (float amplitude : {0.25f, 10.0f}) {
        auto queue = std::make_unique<WorkloadQueue>();
        for (int i = 0; i < 2; ++i) {
            auto& work = queue->workloads[i];
            scene(work);
            model(work, 0, 30, 0);
            auto& d = work.drawData;
            d.worldTransformPhysicalAddresses[1] = 0x1ED830;
            d.ts2VertexObjects.assign(30, key);
            d.ts2WorldCameraLocked.assign(2, 0);
            if (key == 0x02012340) d.ts2WorldCameraLocked[1] = 2;
            // Small movement used to hit the dead zone, larger articulation
            // failed the rigid-motion consensus. Both must interpolate.
            if (i) for (int v = 0; v < 30; ++v) d.posFloats[v * 3] += (v % 2 ? amplitude : -amplitude);
        }
        auto previous = frame(*queue, {0});
        auto current = frame(*queue, {1});
        match(*queue, current, previous);
        const auto& velocities = queue->workloads[1].drawData.velFloats;
        for (int v = 0; v < 30; ++v) {
            const bool animated = key == 0x1E4B90 || key == 0x02012340;
            const float expected = animated ? (v % 2 ? amplitude : -amplitude) : 0.0f;
            require(std::abs(velocities[v * 3] - expected) < 0.001f,
                "camera-space actor animation was treated as rigid scenery");
        }
      }
    }
}

static void translucentFirstPersonSurfaceTest() {
    auto queue = std::make_unique<WorkloadQueue>();
    for (int i = 0; i < 2; ++i) {
        auto& work = queue->workloads[i];
        scene(work);
        model(work, 0, 270, 0);
        auto& d = work.drawData;
        d.ts2VertexObjects.assign(270, 0x020D4F10);
        d.ts2WorldCameraLocked.assign(2, 0);
        d.ts2WorldCameraLocked[1] = 2;
        for (int v = 0; v < 270; ++v) d.normColBytes[v * 4 + 3] = 64;
        if (i) for (int v = 0; v < 270; ++v) d.posFloats[v * 3] += 2.0f;
    }
    auto previous = frame(*queue, {0});
    auto current = frame(*queue, {1});
    match(*queue, current, previous);
    for (float velocity : queue->workloads[1].drawData.velFloats)
        require(velocity == 0.0f, "translucent first-person reflection gained unstable vertex history");
}

// A single guest emitter submits several G_MODIFYVTX quads. The real
// 8998->8999 capture has two independent screen-space motions under that
// emitter; neither has the 60% support needed by an emitter-wide vote.
static uint32_t screenQuad(Workload& work, uint32_t emitter, float x, float y,
                           float u, uint32_t commandAddress = 0) {
    auto& d = work.drawData;
    const uint32_t start = d.vertexCount();
    for (int i = 0; i < 4; ++i) {
        const float px = x + ((i == 1 || i == 2) ? 8.0f : 0.0f);
        const float py = y + ((i >= 2) ? 4.0f : 0.0f);
        d.posFloats.insert(d.posFloats.end(), {float(i), 0.0f, 0.0f});
        d.tcFloats.insert(d.tcFloats.end(), {u + ((i == 1 || i == 2) ? 1.0f : 0.0f), (i >= 2) ? 1.0f : 0.0f});
        d.normColBytes.insert(d.normColBytes.end(), {255, 255, 255, 255});
        d.worldIndices.push_back(1);
        d.viewProjIndices.push_back(1);
        d.velFloats.insert(d.velFloats.end(), {0.0f, 0.0f, 0.0f});
        d.modifyPosUints.push_back((start + i) << 1);
        d.modifyPosUints.push_back((uint32_t(uint16_t(int16_t(std::lround(px * 4.0f)))) << 16) |
                                   uint16_t(int16_t(std::lround(py * 4.0f))));
    }
    GameCall call{};
    call.ts2Emitter = emitter;
    call.ts2CommandAddress = commandAddress;
    call.callDesc.minWorldMatrix = call.callDesc.maxWorldMatrix = 1;
    call.callDesc.triangleCount = 2;
    call.callDesc.textureOn = 1;
    call.callDesc.tileIndex = uint32_t(d.callTiles.size());
    call.callDesc.tileCount = 1;
    DrawCallTile tile{};
    tile.tmemHashOrID = uint64_t(u + 1.0f);
    d.callTiles.push_back(tile);
    d.rdpTiles.push_back(interop::RDPTile{});
    call.meshDesc.faceIndicesStart = uint32_t(d.faceIndices.size());
    d.faceIndices.insert(d.faceIndices.end(), {start, start + 1, start + 2, start, start + 2, start + 3});
    work.fbPairs[0].projections[0].addGameCall(call);
    return start;
}

static void independentScreenQuadTest() {
    auto queue = std::make_unique<WorkloadQueue>();
    std::array<uint32_t, 6> currentVertices{};
    uint32_t staticVertex = 0;
    for (int w = 0; w < 2; ++w) {
        auto& work = queue->workloads[w];
        scene(work);
        model(work, 0, 30, 0);
        for (int q = 0; q < 6; ++q) {
            // The same emitter moves three quads vertically and three
            // horizontally. Distinct UVs ensure that neither motion gets a
            // majority from accidental cross-quad correspondences.
            const float x = 40.0f + q * 20.0f - (w && q >= 3 ? 2.0f : 0.0f);
            const float y = 50.0f + q * 12.0f - (w && q < 3 ? 5.0f : 0.0f);
            const uint32_t address = (w ? 0x480000 : 0x400000) + q * 0x78;
            const uint32_t v = screenQuad(work, 0x8005D060, x, y, float(q * 4), address);
            if (w) currentVertices[q] = v;
        }
        staticVertex = screenQuad(work, 0x80085A80, 10.0f, 10.0f, 99.0f,
                                  (w ? 0x480000 : 0x400000) + 0x1000);
    }
    auto previous = frame(*queue, {0});
    auto current = frame(*queue, {1});
    match(*queue, current, previous);
    require(current.frameMap.workloads[1].mapped, "screen quad fixture did not match its workload");
    const auto& velocity = queue->workloads[1].drawData.velFloats;
    for (int q = 0; q < 6; ++q) {
        const float x = q >= 3 ? -2.0f : 0.0f;
        const float y = q < 3 ? -5.0f : 0.0f;
        for (int i = 0; i < 4; ++i) {
            const uint32_t v = currentVertices[q] + i;
            require(velocity[v * 3] == x && velocity[v * 3 + 1] == y,
                "independently moving screen quad lost its own velocity");
        }
        // RSPModifyCS presents current XY minus (1-weight)*velocity. At
        // weight 0.5 each quad must land at its actual native midpoint.
        const uint32_t packed = queue->workloads[1].drawData.modifyPosUints[q * 8 + 1];
        const float nativeX = float(int16_t(packed >> 16)) / 4.0f;
        const float nativeY = float(int16_t(packed & 0xFFFF)) / 4.0f;
        const float expectedX = 40.0f + q * 20.0f - (q >= 3 ? 1.0f : 0.0f);
        const float expectedY = 50.0f + q * 12.0f - (q < 3 ? 2.5f : 0.0f);
        require(nativeX - x * 0.5f == expectedX && nativeY - y * 0.5f == expectedY,
            "screen quad did not reach its native midpoint at half weight");
    }
    for (int i = 0; i < 4; ++i)
        require(velocity[(staticVertex + i) * 3] == 0.0f && velocity[(staticVertex + i) * 3 + 1] == 0.0f,
            "static HUD acquired a moving quad's history");

    // Same emitter and ordinal are insufficient when a different sprite
    // occupies that draw call. Changed UVs must leave it unblended.
    auto& changed = queue->workloads[1].drawData;
    changed.tcFloats[currentVertices[2] * 2] += 0.5f;
    current = frame(*queue, {1});
    match(*queue, current, previous);
    require(current.frameMap.workloads[1].mapped, "changed sprite fixture lost its workload match");
    for (int i = 0; i < 4; ++i) {
        const uint32_t v = currentVertices[2] + i;
        require(changed.velFloats[v * 3] == 0.0f && changed.velFloats[v * 3 + 1] == 0.0f,
            "changed sprite borrowed the previous quad's velocity");
    }
    changed.tcFloats[currentVertices[2] * 2] -= 0.5f;
    auto& changedCall = queue->workloads[1].fbPairs[0].projections[0].gameCalls[3];
    changedCall.ts2CommandAddress += 4;
    current = frame(*queue, {1});
    match(*queue, current, previous);
    require(changed.velFloats[currentVertices[2] * 3 + 1] == 0.0f,
        "quad with changed relative DL location borrowed history");
    changedCall.ts2CommandAddress -= 4;
    changed.callTiles[2].tmemHashOrID += 1;
    current = frame(*queue, {1});
    match(*queue, current, previous);
    require(changed.velFloats[currentVertices[2] * 3 + 1] == 0.0f,
        "quad with changed texture borrowed history");
    changed.callTiles[2].tmemHashOrID -= 1;
    changed.rdpTiles[2].uls = 1.0f;
    current = frame(*queue, {1});
    match(*queue, current, previous);
    require(changed.velFloats[currentVertices[2] * 3 + 1] == 0.0f,
        "quad with changed atlas bounds borrowed history");
    changed.rdpTiles[2].uls = 0.0f;
    const size_t packedIndex = 2u * 8u + 1u * 2u + 1u;
    changed.modifyPosUints[packedIndex] += 1u << 16;
    current = frame(*queue, {1});
    match(*queue, current, previous);
    require(changed.velFloats[currentVertices[2] * 3 + 1] == 0.0f,
        "quad with a quarter-pixel vertex mismatch borrowed history");
}

static void captureTest() {
#ifdef _WIN32
    _putenv_s("TS2_FRAME_CAPTURE", "1");
#else
    setenv("TS2_FRAME_CAPTURE", "1", 1);
#endif
    const auto original = std::filesystem::current_path();
    struct RestoreDirectory {
        std::filesystem::path path;
        ~RestoreDirectory() { std::filesystem::current_path(path); }
    } restore{original};
    const auto temp = std::filesystem::temp_directory_path() /
        ("SpaceRanger-capture-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(temp);
    std::filesystem::current_path(temp);
    auto queue = std::make_unique<WorkloadQueue>();
    scene(queue->workloads[0]);
    scene(queue->workloads[1]);
    model(queue->workloads[0], 0, 30, 0);
    model(queue->workloads[1], 10, 20, 2);
    auto previous = frame(*queue, {0});
    auto current = frame(*queue, {1});
    match(*queue, current, previous);
    TransformProcessor::ProcessParams params{};
    params.workloadQueue = queue.get();
    params.curFrame = &current;
    params.prevFrame = &previous;
    params.curFrameWeight = 0.5f;
    queue->transformProcessor.process(params);
    require(ts2_request_frame_capture(), "capture request rejected");
    require(!ts2_request_frame_capture(), "active capture accepted another request");
    for (int i = 0; i < 15; ++i)
        ts2CaptureDisplayFrame(*queue, current, 0.5f, 1.0f, true, false);
    // Capture serialization runs on a detached writer. Wait for its files
    // before checking them or restoring this test's temporary directory.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    for (;;) {
        size_t ready = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(temp)) {
            if (entry.path().filename().string().rfind("display-", 0) == 0 &&
                entry.path().extension() == ".json" && entry.file_size() > 0) ++ready;
        }
        if (ready == 12 || std::chrono::steady_clock::now() >= deadline) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    size_t files = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(temp)) {
        if (entry.path().filename().string().rfind("display-", 0) != 0 || entry.path().extension() != ".json") continue;
        ++files;
        std::ifstream stream(entry.path());
        const auto capture = nlohmann::json::parse(stream);
        const auto& work = capture.at("workloads").at(0);
        require(capture.at("format") == 1 && capture.at("weight") == 0.5f, "capture metadata did not round-trip");
        require(work.at("positions").size() == 60 && work.at("faces").size() == 18, "capture lost submitted geometry");
        require(work.at("world_native").at(1).at(12) == 2.0f, "capture lost native transform");
        require(std::abs(work.at("world_display").at(1).at(12).get<float>() - 1.0f) < 0.001f,
            "capture did not retain actual interpolated transform");
        require(work.at("calls").at(0).at("triangles") == 6, "capture lost ordered draw calls");
    }
    require(files == 12, "capture did not stop after its frame limit");
    std::cout << "Capture round-trip files: " << temp.string() << '\n';
}

int main(int argc, char** argv) {
    const std::string selection = argc > 1 ? argv[1] : "all";
    int failed = 0;
    for (const auto& test : {std::pair{"subset", subsetTest}, {"scenery", cameraSpaceSceneryTest}, {"locked", lockedObjectTest}, {"identity", identityTest},
             {"partition", partitionTest}, {"ownership", ownershipTest}, {"added", addedBatchTest},
             {"tiles", tileSafetyTest}, {"tagged", taggedSafetyTest}, {"batch", batchTimingTest}, {"memory", taskMemoryTest}, {"simple", simplePartsTest}, {"articulated", articulatedCameraSpaceTest}, {"visor", translucentFirstPersonSurfaceTest}, {"screen_quad", independentScreenQuadTest}, {"capture", captureTest}}) {
        if (selection != "all" && selection != test.first) continue;
        try { test.second(); std::cout << "PASS: " << test.first << '\n'; }
        catch (const std::exception& e) { ++failed; std::cerr << "FAIL: " << test.first << ": " << e.what() << '\n'; }
    }
    return failed ? 1 : 0;
}
