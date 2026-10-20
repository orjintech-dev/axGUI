// ============================================================
// TILE-BASED BATCH RENDERING SYSTEM
// High-performance rendering for 1000s of shapes
// With full support for all effects (2.5D, shadows, stroke styles, etc.)
// ============================================================
//
// Usage:
//   sdf.beginTileBatch();
//   for (...) {
//       sdf.addShape(...);  // add any shape
//   }
//   sdf.endTileBatch();     // split into tiles and render
//
// ============================================================

#pragma once

#include <vector>
#include <algorithm>

// ============================================================
// Configuration
// ============================================================

#define MAX_TOTAL_SHAPES 65536           // Toplam max shape
#define BATCH_TILE_SIZE 256              // Tile size for spatial grouping

// ============================================================
// Extended Batch Item - supports all shape types and effects
// ============================================================

struct SDFShapeItem {
    // Transform (world space)
    vec2 position;
    vec2 size;
    float rotation;

    // Shape
    int type;
    float params[8];      // params1 (4) + params2 (4)

    // Style - Basic
    cColor fillColor;
    cColor strokeColor;
    float strokeWidth;
    float strokeSoftness;
    int strokeMode;

    // Stroke Style (Tapered, Sketchy, Double, etc.)
    int strokeStyle;
    float strokeTaper;
    float strokeWobble;
    float strokeWobbleFreq;
    cColor strokeColor2;
    cColor strokeColor3;
    vec2 strokeOffset2;
    float strokeSpacing;
    float strokeNoise;
    float strokePressure;
    float brushAngle;
    float brushRatio;
    float animTime;

    // Dashed stroke
    bool dashEnabled;
    float dashLength;
    float dashGap;
    int dashCapStyle;

    // Glow
    cColor glowColor;
    float glowSize;
    float glowIntensity;
    int glowMode;

    // Shadow
    cColor shadowColor;
    vec2 shadowOffset;
    float shadowBlur;

    // Gradient
    int gradientType;
    cColor gradientColor1;
    cColor gradientColor2;
    vec2 gradientStart;
    vec2 gradientEnd;

    // Deform
    int deformType;
    vec4 deformParams;

    // 2.5D Lighting
    bool enable25D;
    float extrudeRadius;
    vec3 lightDir;
    float ambientStrength;
    float diffuseStrength;
    float specularStrength;
    float specularPower;
    float fresnelStrength;
    float fresnelIOR;

    // Bounding box (WORLD space - converted to screen at render time)
    float worldMinX, worldMinY, worldMaxX, worldMaxY;
};

// ============================================================
// Batch System (simplified - no tile complexity)
// ============================================================

class TileBatchSystem {
public:
    // Shape storage
    std::vector<SDFShapeItem> shapes;

    // Reference to renderer
    SdfRenderer2D* renderer = nullptr;

    // Stats
    int totalDrawCalls = 0;
    int shapesRendered = 0;
    int shapesCulled = 0;

    void init(SdfRenderer2D* sdf) {
        renderer = sdf;
        shapes.reserve(4096);
    }

    void begin() {
        shapes.clear();
        totalDrawCalls = 0;
        shapesRendered = 0;
        shapesCulled = 0;
    }

    // ========================================================
    // Shape add functions - record ALL effects
    // ========================================================

    void addShape(SDFShape type, float x, float y, float w, float h,
                  float p1 = 0, float p2 = 0, float p3 = 0, float p4 = 0,
                  float p5 = 0, float p6 = 0, float p7 = 0, float p8 = 0)
    {
        if (shapes.size() >= MAX_TOTAL_SHAPES) return;

        SDFShapeItem item;
        item.size = {w, h};
        item.rotation = renderer->rotation;

        if (renderer->useMatrix) {
            // With a matrix, take the position from the matrix translation
            // and transform the local coordinates (x, y) by the matrix
            float m0 = renderer->transformMatrix.m0;
            float m1 = renderer->transformMatrix.m1;
            float m4 = renderer->transformMatrix.m4;
            float m5 = renderer->transformMatrix.m5;
            float m12 = renderer->transformMatrix.m12;
            float m13 = renderer->transformMatrix.m13;

            // Local point (x, y) -> World point
            float worldX = m0 * x + m4 * y + m12;
            float worldY = m1 * x + m5 * y + m13;

            item.position = {worldX, worldY};

            // Extract the matrix rotation part (without scale)
            float scaleX = sqrtf(m0 * m0 + m1 * m1);
            float scaleY = sqrtf(m4 * m4 + m5 * m5);
            if (scaleX > 0.001f) {
                item.rotation = atan2f(m1, m0);
            }

            // Scale'i size'a uygula
            item.size.x *= scaleX;
            item.size.y *= scaleY;
        } else {
            item.position = {x, y};
        }

        item.type = (int)type;
        item.params[0] = p1; item.params[1] = p2;
        item.params[2] = p3; item.params[3] = p4;
        item.params[4] = p5; item.params[5] = p6;
        item.params[6] = p7; item.params[7] = p8;

        // ========== Basic Style ==========
        item.fillColor = renderer->fillColor;
        item.strokeColor = renderer->strokeColor;
        item.strokeWidth = renderer->strokeWidth;
        item.strokeSoftness = renderer->strokeSoftness;
        item.strokeMode = renderer->strokeMode;

        // ========== Stroke Style ==========
        item.strokeStyle = (int)renderer->strokeStyle;
        item.strokeTaper = renderer->strokeTaper;
        item.strokeWobble = renderer->strokeWobble;
        item.strokeWobbleFreq = renderer->strokeWobbleFreq;
        item.strokeColor2 = renderer->strokeColor2;
        item.strokeColor3 = renderer->strokeColor3;
        item.strokeOffset2 = renderer->strokeOffset2;
        item.strokeSpacing = renderer->strokeSpacing;
        item.strokeNoise = renderer->strokeNoise;
        item.strokePressure = renderer->strokePressure;
        item.brushAngle = renderer->brushAngle;
        item.brushRatio = renderer->brushRatio;
        item.animTime = renderer->animTime;

        // ========== Dashed Stroke ==========
        item.dashEnabled = renderer->dashEnabled;
        item.dashLength = renderer->dashLength;
        item.dashGap = renderer->dashGap;
        item.dashCapStyle = renderer->dashCapStyle;

        // ========== Glow ==========
        item.glowColor = renderer->glowColor;
        item.glowSize = renderer->glowSize;
        item.glowIntensity = renderer->glowIntensity;
        item.glowMode = (int)renderer->glowMode;

        // ========== Shadow ==========
        item.shadowColor = renderer->shadowColor;
        item.shadowOffset = renderer->shadowOffset;
        item.shadowBlur = renderer->shadowBlur;

        // ========== Gradient ==========
        item.gradientType = renderer->gradientType;
        item.gradientColor1 = renderer->gradientColor1;
        item.gradientColor2 = renderer->gradientColor2;
        item.gradientStart = renderer->gradientStart;
        item.gradientEnd = renderer->gradientEnd;

        // ========== Deform ==========
        item.deformType = renderer->deformType;
        item.deformParams = {renderer->deformParams.x, renderer->deformParams.y,
                             renderer->deformParams.z, renderer->deformParams.w};

        // ========== 2.5D Lighting ==========
        item.enable25D = renderer->enable25D;
        item.extrudeRadius = renderer->extrudeRadius;
        item.lightDir = {renderer->lightDir.x, renderer->lightDir.y, renderer->lightDir.z};
        item.ambientStrength = renderer->ambientStrength;
        item.diffuseStrength = renderer->diffuseStrength;
        item.specularStrength = renderer->specularStrength;
        item.specularPower = renderer->specularPower;
        item.fresnelStrength = renderer->fresnelStrength;
        item.fresnelIOR = renderer->fresnelIOR;

        // Compute world space bounding box
        calculateWorldBounds(item);

        shapes.push_back(item);
    }

    // Convenience functions
    void addCircle(float x, float y, float r) {
        addShape(SDF_CIRCLE, x, y, r*2, r*2, r);
    }

    void addBox(float x, float y, float w, float h) {
        addShape(SDF_BOX, x + w/2, y + h/2, w, h, w/2, h/2);
    }

    void addRoundedBox(float x, float y, float w, float h, float r) {
        addShape(SDF_ROUNDED_BOX, x + w/2, y + h/2, w, h, r);
    }

    void addLineWithCaps(vec2 from, vec2 to, float thickness,
                         LineCap capA, LineCap capB, float arrowSize = 0)
    {
        float cx = (from.x + to.x) * 0.5f;
        float cy = (from.y + to.y) * 0.5f;

        float minX = fminf(from.x, to.x);
        float minY = fminf(from.y, to.y);
        float maxX = fmaxf(from.x, to.x);
        float maxY = fmaxf(from.y, to.y);

        float pad = fmaxf(thickness, arrowSize) + 5.0f;
        float w = maxX - minX + pad * 2;
        float h = maxY - minY + pad * 2;

        vec2 relA = {from.x - cx, from.y - cy};
        vec2 relB = {to.x - cx, to.y - cy};
        float aSize = (arrowSize > 0.0f) ? arrowSize : thickness * 3.0f;

        addShape(SDF_ARROW, cx, cy, w, h,
                 relA.x, relA.y, relB.x, relB.y,
                 thickness, (float)capA, (float)capB, aSize);
    }

    void addHexagon(float x, float y, float r) {
        addShape(SDF_HEXAGON, x, y, r*2, r*2, r);
    }

    void addStar5(float x, float y, float r, float rf = 0.5f) {
        addShape(SDF_STAR5, x, y, r*2, r*2, r, rf);
    }

    void addSegment(float x1, float y1, float x2, float y2, float thickness) {
        float cx = (x1 + x2) * 0.5f;
        float cy = (y1 + y2) * 0.5f;
        float len = sqrtf((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
        addShape(SDF_SEGMENT, cx, cy, len, thickness,
                 x1 - cx, y1 - cy, x2 - cx, y2 - cy, thickness);
    }

    // ========================================================
    // Effect Key - for grouping shapes with the same effects
    // ========================================================

    struct EffectKey {
        // Shadow
        uint32_t shadowColorHash;
        int16_t shadowOffsetX, shadowOffsetY;
        int16_t shadowBlur;

        // Stroke Style
        int8_t strokeStyle;
        int8_t strokeMode;
        int16_t strokeTaper;
        int16_t strokeWobble;
        int16_t strokeSoftness;

        // Dashed
        int8_t dashEnabled;
        int16_t dashLength;
        int16_t dashGap;

        // Gradient
        int8_t gradientType;

        // Deform
        int8_t deformType;

        // 2.5D
        int8_t enable25D;
        int16_t extrudeRadius;

        bool operator==(const EffectKey& other) const {
            return shadowColorHash == other.shadowColorHash &&
                   shadowOffsetX == other.shadowOffsetX &&
                   shadowOffsetY == other.shadowOffsetY &&
                   shadowBlur == other.shadowBlur &&
                   strokeStyle == other.strokeStyle &&
                   strokeMode == other.strokeMode &&
                   strokeSoftness == other.strokeSoftness &&
                   dashEnabled == other.dashEnabled &&
                   dashLength == other.dashLength &&
                   dashGap == other.dashGap &&
                   gradientType == other.gradientType &&
                   deformType == other.deformType &&
                   enable25D == other.enable25D;
        }

        static EffectKey fromItem(const SDFShapeItem& item) {
            EffectKey key;
            // Manual color hash: pack RGBA into uint32
            key.shadowColorHash = ((uint32_t)item.shadowColor.r << 24) |
                                  ((uint32_t)item.shadowColor.g << 16) |
                                  ((uint32_t)item.shadowColor.b << 8) |
                                  (uint32_t)item.shadowColor.a;
            key.shadowOffsetX = (int16_t)(item.shadowOffset.x * 10);
            key.shadowOffsetY = (int16_t)(item.shadowOffset.y * 10);
            key.shadowBlur = (int16_t)(item.shadowBlur * 10);
            key.strokeStyle = (int8_t)item.strokeStyle;
            key.strokeMode = (int8_t)item.strokeMode;
            key.strokeTaper = (int16_t)(item.strokeTaper * 100);
            key.strokeWobble = (int16_t)(item.strokeWobble * 10);
            key.strokeSoftness = (int16_t)(item.strokeSoftness * 10);
            key.dashEnabled = item.dashEnabled ? 1 : 0;
            key.dashLength = (int16_t)(item.dashLength * 10);
            key.dashGap = (int16_t)(item.dashGap * 10);
            key.gradientType = (int8_t)item.gradientType;
            key.deformType = (int8_t)item.deformType;
            key.enable25D = item.enable25D ? 1 : 0;
            key.extrudeRadius = (int16_t)(item.extrudeRadius * 10);
            return key;
        }
    };

    // ========================================================
    // Rendering - build effect-compatible batches while preserving the original order
    // ========================================================

    void end() {
        if (shapes.empty()) return;

        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();

        vec2 pan = renderer->pan;
        float zoom = renderer->zoom;

        // Frustum culling - collect visible shapes (in original order)
        std::vector<int> visibleIndices;
        visibleIndices.reserve(shapes.size());

        for (int i = 0; i < (int)shapes.size(); i++) {
            const SDFShapeItem& item = shapes[i];

            // Convert world bounds to screen
            float sMinX = item.worldMinX * zoom + pan.x;
            float sMinY = item.worldMinY * zoom + pan.y;
            float sMaxX = item.worldMaxX * zoom + pan.x;
            float sMaxY = item.worldMaxY * zoom + pan.y;

            // Screen bounds check
            if (sMaxX >= 0 && sMinX <= screenW && sMaxY >= 0 && sMinY <= screenH) {
                visibleIndices.push_back(i);
            } else {
                shapesCulled++;
            }
        }

        if (visibleIndices.empty()) return;

        // ========================================================
        // RENDER IN ORDER - preserve the original z-order
        // Batch consecutive shapes with the same effects
        // ========================================================

        int totalVisible = (int)visibleIndices.size();
        int currentStart = 0;

        while (currentStart < totalVisible) {
            // Find the range of consecutive shapes with compatible effects
            EffectKey currentKey = EffectKey::fromItem(shapes[visibleIndices[currentStart]]);
            int currentEnd = currentStart + 1;

            // Group only consecutive shapes with identical effects (max SDF_MAX_BATCH)
            while (currentEnd < totalVisible &&
                   currentEnd - currentStart < SDF_MAX_BATCH) {
                EffectKey nextKey = EffectKey::fromItem(shapes[visibleIndices[currentEnd]]);
                if (!(nextKey == currentKey)) break;
                currentEnd++;
            }

            int count = currentEnd - currentStart;

            // Render this batch
            renderBatch(visibleIndices.data() + currentStart, count, screenW, screenH, pan, zoom);

            currentStart = currentEnd;
        }

        int mode = 0;
        SetShaderValue(renderer->shader, renderer->batchModeLoc, &mode, SHADER_UNIFORM_INT);
    }

    // ========================================================
    // Render a single batch of shapes with same effect params
    // ========================================================

    void renderBatch(const int* indices, int count, int screenW, int screenH, vec2 pan, float zoom) {
        if (count <= 0) return;

        // Batch data arrays
        vec4 posSize[SDF_MAX_BATCH];
        vec4 params[SDF_MAX_BATCH];
        vec4 params2[SDF_MAX_BATCH];
        vec4 fillColors[SDF_MAX_BATCH];
        vec4 strokeColors[SDF_MAX_BATCH];
        vec4 glowColors[SDF_MAX_BATCH];
        vec4 effects[SDF_MAX_BATCH];
        vec4 effects2[SDF_MAX_BATCH];
        float typeRot[SDF_MAX_BATCH * 2];

        float batchMinX = 1e10f, batchMinY = 1e10f;
        float batchMaxX = -1e10f, batchMaxY = -1e10f;

        // First item provides effect uniforms for entire batch
        const SDFShapeItem& firstItem = shapes[indices[0]];

        for (int i = 0; i < count; i++) {
            const SDFShapeItem& item = shapes[indices[i]];

            posSize[i] = {item.position.x, item.position.y, item.size.x, item.size.y};
            params[i] = {item.params[0], item.params[1], item.params[2], item.params[3]};
            params2[i] = {item.params[4], item.params[5], item.params[6], item.params[7]};
            fillColors[i] = item.fillColor.toVec4();
            strokeColors[i] = item.strokeColor.toVec4();
            glowColors[i] = item.glowColor.toVec4();
            effects[i] = {item.strokeWidth, item.glowSize, item.glowIntensity, (float)item.glowMode};
            effects2[i] = {item.strokeSoftness, (float)item.strokeMode, 0.0f, 0.0f};
            typeRot[i * 2] = (float)item.type;
            typeRot[i * 2 + 1] = item.rotation;

            float sMinX = item.worldMinX * zoom + pan.x;
            float sMinY = item.worldMinY * zoom + pan.y;
            float sMaxX = item.worldMaxX * zoom + pan.x;
            float sMaxY = item.worldMaxY * zoom + pan.y;

            batchMinX = fminf(batchMinX, sMinX);
            batchMinY = fminf(batchMinY, sMinY);
            batchMaxX = fmaxf(batchMaxX, sMaxX);
            batchMaxY = fmaxf(batchMaxY, sMaxY);
        }

        batchMinX = fmaxf(0, batchMinX);
        batchMinY = fmaxf(0, batchMinY);
        batchMaxX = fminf((float)screenW, batchMaxX);
        batchMaxY = fminf((float)screenH, batchMaxY);

        if (batchMaxX <= batchMinX || batchMaxY <= batchMinY) return;

        Shader& shader = renderer->shader;
        float screenH_f = (float)screenH;
        SetShaderValue(shader, renderer->screenHeightLoc, &screenH_f, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, renderer->panLoc, &renderer->pan, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, renderer->zoomLoc, &renderer->zoom, SHADER_UNIFORM_FLOAT);

        int mode = 2, pathMode = 0;
        SetShaderValue(shader, renderer->pathModeLoc, &pathMode, SHADER_UNIFORM_INT);
        SetShaderValue(shader, renderer->batchModeLoc, &mode, SHADER_UNIFORM_INT);
        SetShaderValue(shader, renderer->batchCountLoc, &count, SHADER_UNIFORM_INT);

        // ========== Shadow uniforms (from first item) ==========
        vec4 shadowCol = firstItem.shadowColor.toVec4();
        Vector2 shadowOff = {firstItem.shadowOffset.x, firstItem.shadowOffset.y};
        SetShaderValue(shader, renderer->shadowColorLoc, &shadowCol, SHADER_UNIFORM_VEC4);
        SetShaderValue(shader, renderer->shadowOffsetLoc, &shadowOff, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, renderer->shadowBlurLoc, &firstItem.shadowBlur, SHADER_UNIFORM_FLOAT);

        // ========== Deform uniforms (from first item) ==========
        Vector4 deformP = {firstItem.deformParams.x, firstItem.deformParams.y,
                           firstItem.deformParams.z, firstItem.deformParams.w};
        SetShaderValue(shader, renderer->deformTypeLoc, &firstItem.deformType, SHADER_UNIFORM_INT);
        SetShaderValue(shader, renderer->deformParamsLoc, &deformP, SHADER_UNIFORM_VEC4);

        // ========== Stroke style uniforms (from first item) ==========
        SetShaderValue(shader, renderer->strokeStyleLoc, &firstItem.strokeStyle, SHADER_UNIFORM_INT);
        SetShaderValue(shader, renderer->strokeTaperLoc, &firstItem.strokeTaper, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, renderer->strokeWobbleLoc, &firstItem.strokeWobble, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, renderer->strokeWobbleFreqLoc, &firstItem.strokeWobbleFreq, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, renderer->strokeModeLoc, &firstItem.strokeMode, SHADER_UNIFORM_INT);
        SetShaderValue(shader, renderer->strokeSoftnessLoc, &firstItem.strokeSoftness, SHADER_UNIFORM_FLOAT);

        // Additional stroke uniforms
        vec4 strokeCol2 = firstItem.strokeColor2.toVec4();
        vec4 strokeCol3 = firstItem.strokeColor3.toVec4();
        Vector2 strokeOff2 = {firstItem.strokeOffset2.x, firstItem.strokeOffset2.y};
        SetShaderValue(shader, renderer->strokeColor2Loc, &strokeCol2, SHADER_UNIFORM_VEC4);
        SetShaderValue(shader, renderer->strokeColor3Loc, &strokeCol3, SHADER_UNIFORM_VEC4);
        SetShaderValue(shader, renderer->strokeOffset2Loc, &strokeOff2, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, renderer->strokeSpacingLoc, &firstItem.strokeSpacing, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, renderer->strokeNoiseLoc, &firstItem.strokeNoise, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, renderer->strokePressureLoc, &firstItem.strokePressure, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, renderer->brushAngleLoc, &firstItem.brushAngle, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, renderer->brushRatioLoc, &firstItem.brushRatio, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, renderer->animTimeLoc, &firstItem.animTime, SHADER_UNIFORM_FLOAT);

        // ========== Dashed stroke uniforms (from first item) ==========
        // dashParams: x=length, y=gap, z=offset, w=enabled
        float dashParams[4] = {firstItem.dashLength, firstItem.dashGap, 0.0f, firstItem.dashEnabled ? 1.0f : 0.0f};
        SetShaderValue(shader, renderer->dashParamsLoc, dashParams, SHADER_UNIFORM_VEC4);
        // dashParams2: x=capStyle, y=customEnabled, z=reserved, w=reserved
        float dashParams2[4] = {(float)firstItem.dashCapStyle, 0.0f, 0.0f, 0.0f};
        SetShaderValue(shader, renderer->dashParams2Loc, dashParams2, SHADER_UNIFORM_VEC4);

        // ========== Gradient uniforms (from first item) ==========
        SetShaderValue(shader, renderer->gradientTypeLoc, &firstItem.gradientType, SHADER_UNIFORM_INT);
        if (firstItem.gradientType > 0) {
            vec4 gc1 = firstItem.gradientColor1.toVec4();
            vec4 gc2 = firstItem.gradientColor2.toVec4();
            Vector2 gradStart = {firstItem.gradientStart.x, firstItem.gradientStart.y};
            Vector2 gradEnd = {firstItem.gradientEnd.x, firstItem.gradientEnd.y};
            SetShaderValue(shader, renderer->gradientColor1Loc, &gc1, SHADER_UNIFORM_VEC4);
            SetShaderValue(shader, renderer->gradientColor2Loc, &gc2, SHADER_UNIFORM_VEC4);
            SetShaderValue(shader, renderer->gradientStartLoc, &gradStart, SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, renderer->gradientEndLoc, &gradEnd, SHADER_UNIFORM_VEC2);
        }

        // ========== 2.5D Lighting uniforms (from first item) ==========
        int enable25DInt = firstItem.enable25D ? 1 : 0;
        SetShaderValue(shader, renderer->enable25DLoc, &enable25DInt, SHADER_UNIFORM_INT);
        if (firstItem.enable25D) {
            Vector3 lightD = {firstItem.lightDir.x, firstItem.lightDir.y, firstItem.lightDir.z};
            SetShaderValue(shader, renderer->extrudeRadiusLoc, &firstItem.extrudeRadius, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, renderer->lightDirLoc, &lightD, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, renderer->ambientStrengthLoc, &firstItem.ambientStrength, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, renderer->diffuseStrengthLoc, &firstItem.diffuseStrength, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, renderer->specularStrengthLoc, &firstItem.specularStrength, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, renderer->specularPowerLoc, &firstItem.specularPower, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, renderer->fresnelStrengthLoc, &firstItem.fresnelStrength, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, renderer->fresnelIORLoc, &firstItem.fresnelIOR, SHADER_UNIFORM_FLOAT);
        }

        // Batch arrays
        SetShaderValueV(shader, renderer->batchPosSizeLoc, posSize, SHADER_UNIFORM_VEC4, count);
        SetShaderValueV(shader, renderer->batchParamsLoc, params, SHADER_UNIFORM_VEC4, count);
        SetShaderValueV(shader, renderer->batchParams2Loc, params2, SHADER_UNIFORM_VEC4, count);
        SetShaderValueV(shader, renderer->batchFillColorLoc, fillColors, SHADER_UNIFORM_VEC4, count);
        SetShaderValueV(shader, renderer->batchStrokeColorLoc, strokeColors, SHADER_UNIFORM_VEC4, count);
        SetShaderValueV(shader, renderer->batchGlowColorLoc, glowColors, SHADER_UNIFORM_VEC4, count);
        SetShaderValueV(shader, renderer->batchEffectsLoc, effects, SHADER_UNIFORM_VEC4, count);
        SetShaderValueV(shader, renderer->batchEffects2Loc, effects2, SHADER_UNIFORM_VEC4, count);
        SetShaderValueV(shader, renderer->batchTypeRotLoc, typeRot, SHADER_UNIFORM_VEC2, count);

        Rectangle screenBounds = {batchMinX, batchMinY, batchMaxX - batchMinX, batchMaxY - batchMinY};

        BeginShaderMode(shader);
        rlSetBlendFactors(RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_ONE);
        rlSetBlendMode(BLEND_CUSTOM);
        DrawRectangleRec(screenBounds, WHITE);
        EndShaderMode();
        rlSetBlendMode(BLEND_ALPHA);

        totalDrawCalls++;
        shapesRendered += count;
    }

    // ========================================================
    // World bounds computation
    // ========================================================

    void calculateWorldBounds(SDFShapeItem& item) {
        // Very wide padding - for debugging
        float basePad = 50.0f;  // Minimum padding
        float effectPad = item.glowSize + item.strokeWidth + item.strokeSoftness;

        // Extra padding for shadow
        if (item.shadowColor.a > 0) {
            effectPad += fabsf(item.shadowOffset.x) + fabsf(item.shadowOffset.y) + item.shadowBlur;
        }

        // Extra padding for 2.5D
        if (item.enable25D) {
            effectPad += item.extrudeRadius;
        }

        // For arrows, also add the line length
        float shapePad = 0.0f;
        if (item.type == SDF_ARROW) {
            // Line endpoints: params[0,1] = relA, params[2,3] = relB
            float lineExtent = fmaxf(
                fmaxf(fabsf(item.params[0]), fabsf(item.params[1])),
                fmaxf(fabsf(item.params[2]), fabsf(item.params[3]))
                );
            shapePad = lineExtent + item.params[4] + item.params[7] + 20.0f;  // + thickness + arrowSize
        }

        float pad = basePad + effectPad + shapePad;

        // World space bounds (zoom/pan independent)
        float halfW = item.size.x / 2.0f + pad;
        float halfH = item.size.y / 2.0f + pad;

        // Always use a circular bound (safer)
        float r = sqrtf(halfW * halfW + halfH * halfH);

        item.worldMinX = item.position.x - r;
        item.worldMinY = item.position.y - r;
        item.worldMaxX = item.position.x + r;
        item.worldMaxY = item.position.y + r;
    }
};

// ============================================================
// code to be added to SdfRenderer2D
// ============================================================

/*
// Add to the SdfRenderer2D class:

private:
    TileBatchSystem tileBatch;

public:
    void initTileBatch() {
        tileBatch.init(this);
    }

    void beginTileBatch() {
        tileBatch.begin();
    }

    TileBatchSystem& getTileBatch() {
        return tileBatch;
    }

    void endTileBatch() {
        tileBatch.end();
    }

    // Stats
    int getTileBatchDrawCalls() { return tileBatch.totalDrawCalls; }
    int getTileBatchShapesRendered() { return tileBatch.shapesRendered; }
    int getTileBatchShapesCulled() { return tileBatch.shapesCulled; }
*/
