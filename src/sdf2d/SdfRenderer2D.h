// ===== SDF2D.h - Optimized with Full-Featured Batch + 2.5D Lighting + SVG Paths =====
#pragma once
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>
#include <cstdlib>
// #include "axDefs.h"
#include "sdfShaders.h"
#include "parameter.h"

#ifndef PI
#define PI 3.14159265358979323846f
#endif

#define SDF_MAX_BATCH 32
#define SDF_MAX_PATH_POINTS 512   // Maximum path points (vec4s)


// Forward declaration for batch system
class TileBatchSystem;
class SdfRenderer2D;

// Path command types
enum PathCommand {
    PATH_MOVE = 'M',      // M: Move to (x, y)
    PATH_LINE = 'L',      // L: Line to (x, y)
    PATH_CUBIC = 'C',     // C: Cubic bezier (c1x, c1y, c2x, c2y, x, y) - uses 3 vec2s
    PATH_QUAD = 'Q',      // Q: Quadratic bezier (cx, cy, x, y) - uses 2 vec2s
    PATH_CLOSE = 'Z',     // Z: Close path
    PATH_END = 255       // End of path data
};

// Fill rules
enum PathFillRule {    FILL_NONZERO = 0,    FILL_EVENODD = 1 };


// Glow blend modes
enum GlowMode {    GLOW_ADDITIVE = 0,    GLOW_SCREEN,    GLOW_SOFT,    GLOW_CLASSIC  };

// SDF shape types
enum SDFShape {
    SDF_CIRCLE = 0,
    SDF_BOX,
    SDF_ROUNDED_BOX,
    SDF_SEGMENT,
    SDF_RHOMBUS,
    SDF_TRAPEZOID,
    SDF_PARALLELOGRAM,
    SDF_EQUILATERAL_TRIANGLE,
    SDF_TRIANGLE,
    SDF_PENTAGON,
    SDF_HEXAGON,
    SDF_OCTAGON,
    SDF_HEXAGRAM,
    SDF_STAR5,
    SDF_STAR,
    SDF_PIE,
    SDF_CUT_DISK,
    SDF_ARC,
    SDF_VESICA,
    SDF_MOON,
    SDF_EGG,
    SDF_HEART,
    SDF_CROSS,
    SDF_ROUNDED_X,
    SDF_ELLIPSE,
    SDF_ROUNDED_CROSS,
    SDF_COOL_S,
    SDF_BLOBBY_CROSS,
    SDF_TUNNEL,
    SDF_BEZIER,
    SDF_ARROW,               // Line with configurable end caps
    SDF_PATH = 50,           // SVG Path
    SDF_LINE = 99,
    SDF_POLYLINE = 100
};

static const std::vector<IdStrPair> &sdfTypes = {
    {SDF_CIRCLE, "Circle"},    {SDF_ROUNDED_BOX, "Rounded Box"},    {SDF_SEGMENT, "Segment"},
    {SDF_RHOMBUS, "Rhombus"},    {SDF_TRAPEZOID, "Trapezoid"},    {SDF_PARALLELOGRAM, "Parallelogram"},
    {SDF_TRIANGLE, "Triangle"},    {SDF_EQUILATERAL_TRIANGLE, "Equilateral Triangle"},
    {SDF_PENTAGON, "Pentagon"},    {SDF_HEXAGON, "Hexagon"},    {SDF_OCTAGON, "Octagon"},
    {SDF_HEXAGRAM, "Hexagram"},    {SDF_STAR5, "Star5"},    {SDF_STAR, "Star"},
    {SDF_PIE, "Pie"},    {SDF_CUT_DISK, "Cut Disk"},    {SDF_ARC, "Arc"},    {SDF_VESICA, "Vesica"},
    {SDF_MOON, "Moon"},    {SDF_EGG, "Egg"},    {SDF_HEART, "Heart"},    {SDF_CROSS, "Cross"},
    {SDF_ROUNDED_X, "Rounded X"},    {SDF_ELLIPSE, "Ellipse"},    {SDF_ROUNDED_CROSS, "Rounded Cross"},
    {SDF_COOL_S, "Cool S"},    {SDF_BLOBBY_CROSS, "Blobby Cross"},    {SDF_TUNNEL, "Tunnel"},    {SDF_BEZIER, "Bezier"} };


// Line cap types for SDF_ARROW
enum LineCap {
    CAP_BUTT = 0,    // Flat/square end
    CAP_ROUND = 1,   // Round end
    CAP_ARROW = 2    // Arrow head
};

enum StrokeStyle {
    STROKE_SOLID = 0,       // Normal solid stroke
    STROKE_TAPERED,         // Tapered at start/end
    STROKE_SKETCHY,         // Hand-drawn wobble
    STROKE_DOUBLE,          // Double outline
    STROKE_TRIPLE,          // Triple outline
    STROKE_OFFSET_SHADOW,   // Offset shadow
    STROKE_VARIABLE,        // Variable width
    STROKE_INK_SPREAD,      // Ink spread
    STROKE_BRUSH,           // Brush effect
    STROKE_COMIC,           // Comic book style
    STROKE_NEON_OUTLINE,    // Neon double layer
    STROKE_HALFTONE         // Halftone comic
};

class SdfRenderer2D;

void transferSDFparams(ParamContainer &bc, SdfRenderer2D &sdf);

struct SDF2Dnode
{
    int type;
    vec2 pos, size;
    float param1=0, param2=0, param3=0, param4=0;
    float rotation=0;
    ParamContainer bc;

    int deformType = 0;
    float deformParams[4] = {0, 1, 0, 0};
    bool animateDeform = false;

    bool enable25D = false;
    float extrudeRadius = 20.0f;       // multiplier for shape size
    vec3 lightDir = {-0.5f, -0.5f, 1.0f};
    float ambientStrength = 0.3f;
    float diffuseStrength = 0.7f;
    float specularStrength = 0.5f;
    float specularPower = 32.0f;
    float fresnelStrength = 0.3f;
    float fresnelIOR = 1.5f;

    SDF2Dnode() {}
    SDF2Dnode(SDFShape t, vec2 _pos, vec2 sz, float p1, float p2, float p3, float p4, float rot, cColor fill, cColor stroke=cWhite,
              int strokeM=0, float strokeW=0, float strokeS=0);

    void init();
    void fromJson(json &j);

    void toJson(json &j);
    void copyStyleFrom(SDF2Dnode &other);
    rect bbox();
    bool checkHit(vec2 &pt);
    void draw(SdfRenderer2D &sdf, bool selected=false);
};


// ============================================================
// C++ Path Builder
// ============================================================

struct SDFPath {
    std::vector<vec4> data;
    vec2 currentPos = {0, 0};
    vec2 startPos = {0, 0};
    Rectangle bounds = {1e10f, 1e10f, -1e10f, -1e10f};
    cColor fillColor = cColor(0, 0, 0, 255); // fill color
    cColor strokeColor = cColor(0, 0, 0, 0); // stroke color
    float strokeWidth = 0.0f; // stroke width

    void clear() {
        data.clear();
        currentPos = {0, 0};
        startPos = {0, 0};
        bounds = {1e10f, 1e10f, -1e10f, -1e10f};
    }

    void updateBounds(float x, float y) {
        bounds.x = fminf(bounds.x, x);
        bounds.y = fminf(bounds.y, y);
        bounds.width = fmaxf(bounds.width, x);
        bounds.height = fmaxf(bounds.height, y);
    }

    // M - Move to
    void moveTo(float x, float y) {
        data.push_back({(float)PATH_MOVE, 0, x, y});
        currentPos = {x, y};
        startPos = {x, y};
        updateBounds(x, y);
    }

    // L - Line to
    void lineTo(float x, float y) {
        data.push_back({(float)PATH_LINE, 0, x, y});
        currentPos = {x, y};
        updateBounds(x, y);
    }

    // C - Cubic bezier
    void cubicTo(float c1x, float c1y, float c2x, float c2y, float x, float y) {
        data.push_back({(float)PATH_CUBIC, 0, c1x, c1y});
        data.push_back({c2x, c2y, x, y});
        currentPos = {x, y};
        updateBounds(c1x, c1y);
        updateBounds(c2x, c2y);
        updateBounds(x, y);
    }

    // Q - Quadratic bezier
    void quadTo(float cx, float cy, float x, float y) {
        data.push_back({(float)PATH_QUAD, 0, cx, cy});
        data.push_back({x, y, 0, 0});
        currentPos = {x, y};
        updateBounds(cx, cy);
        updateBounds(x, y);
    }

    // A - Arc (SVG elliptical arc, converted to cubic beziers)
    void arcTo(float rx, float ry, float rotationDeg, int largeArc, int sweep, float x, float y) {
        float x1 = currentPos.x;
        float y1 = currentPos.y;
        float x2 = x;
        float y2 = y;

        // Degenerate cases
        if(rx == 0 || ry == 0 || (x1 == x2 && y1 == y2)) {
            lineTo(x2, y2);
            return;
        }

        rx = fabsf(rx);
        ry = fabsf(ry);

        float phi = rotationDeg * PI / 180.0f;
        float cosPhi = cosf(phi);
        float sinPhi = sinf(phi);

        // Step 1: Compute (x1', y1')
        float dx = (x1 - x2) / 2.0f;
        float dy = (y1 - y2) / 2.0f;
        float x1p = cosPhi * dx + sinPhi * dy;
        float y1p = -sinPhi * dx + cosPhi * dy;

        // Correct radii if too small
        float lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
        if(lambda > 1.0f) {
            float sqrtLambda = sqrtf(lambda);
            rx *= sqrtLambda;
            ry *= sqrtLambda;
        }

        // Step 2: Compute (cx', cy')
        float rx2 = rx * rx;
        float ry2 = ry * ry;
        float x1p2 = x1p * x1p;
        float y1p2 = y1p * y1p;

        float num = rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2;
        float den = rx2 * y1p2 + ry2 * x1p2;
        float sq = (num > 0 && den > 0) ? sqrtf(num / den) : 0;
        if(largeArc == sweep) sq = -sq;

        float cxp = sq * rx * y1p / ry;
        float cyp = -sq * ry * x1p / rx;

        // Step 3: Compute (cx, cy)
        float cx = cosPhi * cxp - sinPhi * cyp + (x1 + x2) / 2.0f;
        float cy = sinPhi * cxp + cosPhi * cyp + (y1 + y2) / 2.0f;

        // Step 4: Compute angles
        auto vecAngle = [](float ux, float uy, float vx, float vy) -> float {
            float dot = ux * vx + uy * vy;
            float len = sqrtf(ux*ux + uy*uy) * sqrtf(vx*vx + vy*vy);
            if(len < 1e-10f) return 0;
            float ang = acosf(fmaxf(-1.0f, fminf(1.0f, dot / len)));
            if(ux * vy - uy * vx < 0) ang = -ang;
            return ang;
        };

        float theta1 = vecAngle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
        float dtheta = vecAngle((x1p - cxp) / rx, (y1p - cyp) / ry,
                                (-x1p - cxp) / rx, (-y1p - cyp) / ry);

        // Adjust dtheta based on sweep flag
        if(sweep == 0 && dtheta > 0) dtheta -= 2 * PI;
        if(sweep == 1 && dtheta < 0) dtheta += 2 * PI;

        // Split arc into cubic bezier segments (max 90 degrees each)
        int segments = (int)ceilf(fabsf(dtheta) / (PI / 2.0f));
        if(segments < 1) segments = 1;
        float delta = dtheta / segments;
        float t = tanf(delta / 4.0f) * 4.0f / 3.0f;

        float prevX = x1, prevY = y1;

        for(int i = 0; i < segments; i++) {
            float a1 = theta1 + i * delta;
            float a2 = a1 + delta;

            float cos1 = cosf(a1), sin1 = sinf(a1);
            float cos2 = cosf(a2), sin2 = sinf(a2);

            // End point of this segment
            float p2x = cx + rx * cos2 * cosPhi - ry * sin2 * sinPhi;
            float p2y = cy + rx * cos2 * sinPhi + ry * sin2 * cosPhi;

            // Control points
            float c1x = prevX + t * (-rx * sin1 * cosPhi - ry * cos1 * sinPhi);
            float c1y = prevY + t * (-rx * sin1 * sinPhi + ry * cos1 * cosPhi);
            float c2x = p2x - t * (-rx * sin2 * cosPhi - ry * cos2 * sinPhi);
            float c2y = p2y - t * (-rx * sin2 * sinPhi + ry * cos2 * cosPhi);

            cubicTo(c1x, c1y, c2x, c2y, p2x, p2y);

            prevX = p2x;
            prevY = p2y;
        }
    }

    // Z - Close path
    void close() {
        data.push_back({(float)PATH_CLOSE, 0, 0, 0});
        currentPos = startPos;
    }

    // Relative versions
    void moveToRel(float dx, float dy) { moveTo(currentPos.x + dx, currentPos.y + dy); }
    void lineToRel(float dx, float dy) { lineTo(currentPos.x + dx, currentPos.y + dy); }
    void cubicToRel(float dc1x, float dc1y, float dc2x, float dc2y, float dx, float dy) {
        cubicTo(currentPos.x + dc1x, currentPos.y + dc1y,
                currentPos.x + dc2x, currentPos.y + dc2y,
                currentPos.x + dx, currentPos.y + dy);
    }
    void quadToRel(float dcx, float dcy, float dx, float dy) {
        quadTo(currentPos.x + dcx, currentPos.y + dcy,
               currentPos.x + dx, currentPos.y + dy);
    }
    void arcToRel(float rx, float ry, float rotationDeg, int largeArc, int sweep, float dx, float dy) {
        arcTo(rx, ry, rotationDeg, largeArc, sweep, currentPos.x + dx, currentPos.y + dy);
    }

    // H/V shortcuts
    void hLineTo(float x) { lineTo(x, currentPos.y); }
    void vLineTo(float y) { lineTo(currentPos.x, y); }
    void hLineToRel(float dx) { lineTo(currentPos.x + dx, currentPos.y); }
    void vLineToRel(float dy) { lineTo(currentPos.x, currentPos.y + dy); }

    // End marker
    void end() {
        data.push_back({(float)PATH_END, 0, 0, 0});
    }

    // Get normalized bounds (min/max to width/height)
    Rectangle getBounds() const {
        return {bounds.x, bounds.y, bounds.width - bounds.x, bounds.height - bounds.y};
    }

    int getPointCount() const { return (int)data.size(); }
    const vec4* getData() const { return data.data(); }

    // Append another path's data translated by (ox, oy).
    // Strips the source END marker. Call end() on merged path when done.
    // Returns false if the glyph doesn't fit (caller should flush and retry or draw individually).
    bool appendOffset(const SDFPath& src, float ox, float oy, int maxPoints = SDF_MAX_PATH_POINTS) {
        const vec4* s = src.data.data();
        int n = (int)src.data.size();

        // Count source points (excluding END)
        int srcPts = n;
        if (srcPts > 0 && (int)s[srcPts - 1].x == PATH_END) srcPts--;

        // Check if it fits (leave room for END marker)
        if ((int)data.size() + srcPts + 1 > maxPoints) return false;

        for (int i = 0; i < n; i++) {
            int cmd = (int)s[i].x;
            if (cmd == PATH_END) break;

            if (cmd == PATH_MOVE || cmd == PATH_LINE) {
                // {cmd, 0, x, y} → offset x,y
                data.push_back({s[i].x, s[i].y, s[i].z + ox, s[i].w + oy});
                updateBounds(s[i].z + ox, s[i].w + oy);
            }
            else if (cmd == PATH_CUBIC) {
                // vec4[0]: {C, 0, c1x, c1y}
                // vec4[1]: {c2x, c2y, x, y}
                data.push_back({s[i].x, s[i].y, s[i].z + ox, s[i].w + oy});
                updateBounds(s[i].z + ox, s[i].w + oy);
                i++;
                if (i < n) {
                    data.push_back({s[i].x + ox, s[i].y + oy, s[i].z + ox, s[i].w + oy});
                    updateBounds(s[i].x + ox, s[i].y + oy);
                    updateBounds(s[i].z + ox, s[i].w + oy);
                }
            }
            else if (cmd == PATH_QUAD) {
                // vec4[0]: {Q, 0, cx, cy}
                // vec4[1]: {x, y, 0, 0}  (z,w are unused padding)
                data.push_back({s[i].x, s[i].y, s[i].z + ox, s[i].w + oy});
                updateBounds(s[i].z + ox, s[i].w + oy);
                i++;
                if (i < n) {
                    data.push_back({s[i].x + ox, s[i].y + oy, s[i].z, s[i].w});
                    updateBounds(s[i].x + ox, s[i].y + oy);
                }
            }
            else if (cmd == PATH_CLOSE) {
                data.push_back(s[i]);
            }
            else {
                data.push_back(s[i]);
            }
        }
        return true;
    }
};

// ============================================================
// C++ Wrapper Class
// ============================================================

class SdfRenderer2D {
public:
    Shader shader = {0};

    // Uniform locations
    int panLoc, zoomLoc, screenHeightLoc;
    int shapePosLoc, shapeSizeLoc, shapeRotationLoc;
    int shapeTypeLoc, shapeParamsLoc, shapeParams2Loc;
    int fillColorLoc, strokeColorLoc, strokeWidthLoc, strokeSoftnessLoc, strokeModeLoc;
    int glowColorLoc, glowSizeLoc, glowIntensityLoc, glowModeLoc;
    int shadowColorLoc, shadowOffsetLoc, shadowBlurLoc;
    int gradientTypeLoc, gradientColor1Loc, gradientColor2Loc;
    int gradientStartLoc, gradientEndLoc;
    int deformTypeLoc, deformParamsLoc;

    // 2.5D uniform locations
    int enable25DLoc, extrudeRadiusLoc, lightDirLoc;
    int ambientStrengthLoc, diffuseStrengthLoc;
    int specularStrengthLoc, specularPowerLoc;
    int fresnelStrengthLoc, fresnelIORLoc;

    int batchModeLoc, batchCountLoc;
    int batchParams2Loc;
    int batchTypeRotLoc;
    int batchPosSizeLoc, batchParamsLoc;
    int batchFillColorLoc, batchStrokeColorLoc;
    int batchGlowColorLoc, batchEffectsLoc, batchEffects2Loc;

    // Path uniform locations
    int pathModeLoc, pathPointCountLoc, pathFillRuleLoc, pathDataLoc, pathBoundsLoc;

    // Matrix transform uniform locations
    int useMatrixLoc;
    int transformCol0Loc, transformCol1Loc, transformCol2Loc;
    int inverseCol0Loc, inverseCol1Loc, inverseCol2Loc;

    // State
    vec2 position = {0, 0}, size = {100, 100};
    float rotation = 0;
    cColor fillColor = cWhite, strokeColor = cBlack;
    float strokeWidth = 0, strokeSoftness = 0;
    int strokeMode = 0;

    // Matrix transform state
    bool useMatrix = false;
    Matrix transformMatrix = MatrixIdentity();  // Raylib Matrix (4x4, we use 3x3 part)
    Matrix inverseMatrix = MatrixIdentity();

    // Cartoon Stroke state
    StrokeStyle strokeStyle = STROKE_SOLID;
    float strokeTaper = 0.5f, strokeWobble = 3.0f, strokeWobbleFreq = 0.1f;
    cColor strokeColor2 = {100, 100, 100, 255}, strokeColor3 = {50, 50, 50, 255};
    vec2 strokeOffset2 = {3, 3};
    float strokeSpacing = 4.0f, strokeNoise = 0.3f, strokePressure = 0.5f;
    float brushAngle = 0.0f, brushRatio = 2.0f;
    float animTime = 0.0f;

    // Dash stroke state
    bool dashEnabled = false;
    int dashCapStyle = 0;  // 0=butt, 1=round, 2=square
    float dashLength = 20.0f;
    float dashGap = 10.0f;
    float dashOffset = 0.0f;
    bool dashCustomEnabled = false;
    float dashPatternData[4] = {0, 0, 0, 0};  // custom pattern: dash1, gap1, dash2, gap2

    // Uniform locations - Stroke
    int strokeStyleLoc, strokeTaperLoc, strokeWobbleLoc, strokeWobbleFreqLoc;
    int strokeColor2Loc, strokeColor3Loc, strokeOffset2Loc;
    int strokeSpacingLoc, strokeNoiseLoc, strokePressureLoc;
    int brushAngleLoc, brushRatioLoc, animTimeLoc;

    // Uniform locations - Dash
    int dashParamsLoc, dashParams2Loc, dashPatternLoc;

    cColor glowColor = {255, 255, 0, 0};
    float glowSize = 0, glowIntensity = 1.0f;
    GlowMode glowMode = GLOW_ADDITIVE;
    cColor shadowColor = {0, 0, 0, 0};
    vec2 shadowOffset = {5, 5};
    float shadowBlur = 10;
    int gradientType = 0;
    cColor gradientColor1 = cWhite, gradientColor2 = cBlack;
    vec2 gradientStart = {0, 0}, gradientEnd = {100, 0};

    // Deformation
    int deformType = 0;
    Vector4 deformParams = {0, 1, 0, 0};

    // 2.5D Lighting
    bool enable25D = false;
    float extrudeRadius = 1.0f;
    Vector3 lightDir = {-0.5f, -0.5f, 1.0f};
    float ambientStrength = 0.3f;
    float diffuseStrength = 0.7f;
    float specularStrength = 0.5f;
    float specularPower = 32.0f;
    float fresnelStrength = 0.3f;
    float fresnelIOR = 1.5f;

    vec2 pan = {0, 0};
    float zoom = 1.0f;

    // Path state
    PathFillRule pathFillRule = FILL_NONZERO;

    void init()
    {
        if (shader.id != 0) return;
        float tm=GetTime();
        shader = LoadShaderFromMemory(nullptr, sdfShaderFS);

        panLoc = GetShaderLocation(shader, "pan");
        zoomLoc = GetShaderLocation(shader, "zoom");
        screenHeightLoc = GetShaderLocation(shader, "screenHeight");
        shapePosLoc = GetShaderLocation(shader, "shapePos");
        shapeSizeLoc = GetShaderLocation(shader, "shapeSize");
        shapeRotationLoc = GetShaderLocation(shader, "shapeRotation");
        shapeTypeLoc = GetShaderLocation(shader, "shapeType");
        shapeParamsLoc = GetShaderLocation(shader, "shapeParams");
        shapeParams2Loc = GetShaderLocation(shader, "shapeParams2");
        fillColorLoc = GetShaderLocation(shader, "fillColor");
        strokeColorLoc = GetShaderLocation(shader, "strokeColor");
        strokeWidthLoc = GetShaderLocation(shader, "strokeWidth");
        strokeSoftnessLoc = GetShaderLocation(shader, "strokeSoftness");
        strokeModeLoc = GetShaderLocation(shader, "strokeMode");
        glowColorLoc = GetShaderLocation(shader, "glowColor");
        glowSizeLoc = GetShaderLocation(shader, "glowSize");
        glowIntensityLoc = GetShaderLocation(shader, "glowIntensity");
        glowModeLoc = GetShaderLocation(shader, "glowMode");
        shadowColorLoc = GetShaderLocation(shader, "shadowColor");
        shadowOffsetLoc = GetShaderLocation(shader, "shadowOffset");
        shadowBlurLoc = GetShaderLocation(shader, "shadowBlur");
        gradientTypeLoc = GetShaderLocation(shader, "gradientType");
        gradientColor1Loc = GetShaderLocation(shader, "gradientColor1");
        gradientColor2Loc = GetShaderLocation(shader, "gradientColor2");
        gradientStartLoc = GetShaderLocation(shader, "gradientStart");
        gradientEndLoc = GetShaderLocation(shader, "gradientEnd");
        deformTypeLoc = GetShaderLocation(shader, "deformType");
        deformParamsLoc = GetShaderLocation(shader, "deformParams");

        // 2.5D
        enable25DLoc = GetShaderLocation(shader, "enable25D");
        extrudeRadiusLoc = GetShaderLocation(shader, "extrudeRadius");
        lightDirLoc = GetShaderLocation(shader, "lightDir");
        ambientStrengthLoc = GetShaderLocation(shader, "ambientStrength");
        diffuseStrengthLoc = GetShaderLocation(shader, "diffuseStrength");
        specularStrengthLoc = GetShaderLocation(shader, "specularStrength");
        specularPowerLoc = GetShaderLocation(shader, "specularPower");
        fresnelStrengthLoc = GetShaderLocation(shader, "fresnelStrength");
        fresnelIORLoc = GetShaderLocation(shader, "fresnelIOR");

        batchModeLoc = GetShaderLocation(shader, "batchMode");
        batchCountLoc = GetShaderLocation(shader, "batchCount");
        batchPosSizeLoc = GetShaderLocation(shader, "batchPosSize");
        batchParamsLoc = GetShaderLocation(shader, "batchParams");
        batchParams2Loc = GetShaderLocation(shader, "batchParams2");
        batchTypeRotLoc = GetShaderLocation(shader, "batchTypeRot");
        batchFillColorLoc = GetShaderLocation(shader, "batchFillColor");
        batchStrokeColorLoc = GetShaderLocation(shader, "batchStrokeColor");
        batchGlowColorLoc = GetShaderLocation(shader, "batchGlowColor");
        batchEffectsLoc = GetShaderLocation(shader, "batchEffects");
        batchEffects2Loc = GetShaderLocation(shader, "batchEffects2");

        // Path
        pathModeLoc = GetShaderLocation(shader, "pathMode");
        pathPointCountLoc = GetShaderLocation(shader, "pathPointCount");
        pathFillRuleLoc = GetShaderLocation(shader, "pathFillRule");
        pathDataLoc = GetShaderLocation(shader, "pathData");
        pathBoundsLoc = GetShaderLocation(shader, "pathBounds");

        // Cartoon stroke locations
        strokeStyleLoc = GetShaderLocation(shader, "strokeStyle");
        strokeTaperLoc = GetShaderLocation(shader, "strokeTaper");
        strokeWobbleLoc = GetShaderLocation(shader, "strokeWobble");
        strokeWobbleFreqLoc = GetShaderLocation(shader, "strokeWobbleFreq");
        strokeColor2Loc = GetShaderLocation(shader, "strokeColor2");
        strokeColor3Loc = GetShaderLocation(shader, "strokeColor3");
        strokeOffset2Loc = GetShaderLocation(shader, "strokeOffset2");
        strokeSpacingLoc = GetShaderLocation(shader, "strokeSpacing");
        strokeNoiseLoc = GetShaderLocation(shader, "strokeNoise");
        strokePressureLoc = GetShaderLocation(shader, "strokePressure");
        brushAngleLoc = GetShaderLocation(shader, "brushAngle");
        brushRatioLoc = GetShaderLocation(shader, "brushRatio");
        animTimeLoc = GetShaderLocation(shader, "animTime");

        // Dash uniform locations
        dashParamsLoc = GetShaderLocation(shader, "dashParams");
        dashParams2Loc = GetShaderLocation(shader, "dashParams2");
        dashPatternLoc = GetShaderLocation(shader, "dashPattern");

        // Matrix transform locations
        useMatrixLoc = GetShaderLocation(shader, "useMatrix");
        transformCol0Loc = GetShaderLocation(shader, "transformCol0");
        transformCol1Loc = GetShaderLocation(shader, "transformCol1");
        transformCol2Loc = GetShaderLocation(shader, "transformCol2");
        inverseCol0Loc = GetShaderLocation(shader, "inverseCol0");
        inverseCol1Loc = GetShaderLocation(shader, "inverseCol1");
        inverseCol2Loc = GetShaderLocation(shader, "inverseCol2");

        TraceLog(LOG_INFO, "SDF2D: Shader loaded with 2.5D and SVG Path support");
        axLog("compile time: %.2f",GetTime()-tm);
    }

    void unload() {
        if (shader.id != 0) { UnloadShader(shader); shader.id = 0; }
        unloadMask();
        unloadBlur();
    }

    // ========== MATRIX TRANSFORM API ==========

    // Reset to identity (no transform)
    void resetMatrix() {
        useMatrix = false;
        transformMatrix = MatrixIdentity();
        inverseMatrix = MatrixIdentity();
    }

    // Set transform from position and rotation (legacy mode)
    void setPositionRotation(float x, float y, float rot) {
        position = {x, y};
        rotation = rot;
        useMatrix = false;
    }

    // Set full matrix transform
    void setMatrix(const Matrix& mat) {
        useMatrix = true;
        transformMatrix = mat;
        inverseMatrix = MatrixInvert(mat);
    }

    // Build matrix from components
    void setMatrixTRS(float tx, float ty, float rotRad, float sx, float sy) {
        useMatrix = true;
        // Build: Translate * Rotate * Scale
        Matrix mT = MatrixTranslate(tx, ty, 0);
        Matrix mR = MatrixRotateZ(rotRad);
        Matrix mS = MatrixScale(sx, sy, 1);
        transformMatrix = MatrixMultiply(MatrixMultiply(mS, mR), mT);
        inverseMatrix = MatrixInvert(transformMatrix);
    }

    // Set with skew support
    void setMatrixFull(float tx, float ty, float rotRad, float sx, float sy, float skewX, float skewY) {
        useMatrix = true;
        // Build custom matrix with skew
        float c = cosf(rotRad), s = sinf(rotRad);

        // Scale + Skew
        Matrix mSS = {
            sx, skewX * sy, 0, 0,
            skewY * sx, sy, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };

        // Rotation
        Matrix mR = MatrixRotateZ(rotRad);

        // Translation
        Matrix mT = MatrixTranslate(tx, ty, 0);

        transformMatrix = MatrixMultiply(MatrixMultiply(mSS, mR), mT);
        inverseMatrix = MatrixInvert(transformMatrix);
    }

    // Convenience: scale uniform
    void setMatrixTRS(float tx, float ty, float rotRad, float scale) {
        setMatrixTRS(tx, ty, rotRad, scale, scale);
    }

    // Transform
    void setTransform(float px, float py, float z) { pan = {px, py}; zoom = z; }
    void setTransform(vec2 p, float z) { pan = p; zoom = z; }
    void resetTransform() { pan = {0, 0}; zoom = 1.0f; }

    // ========== DRAWBOT SYNC ==========
    // mat2d format: [m0, m1, m2, m3, m4, m5] = [a, b, c, d, tx, ty]
    // | a  c  tx |   | m0 m2 m4 |
    // | b  d  ty | = | m1 m3 m5 |
    // | 0  0  1  |   | 0  0  1  |

    void setMatrixFromMat2d(const float* m) {
        // mat2d -> Raylib Matrix (4x4)
        transformMatrix = {
            m[0], m[1], 0, 0,   // column 0
            m[2], m[3], 0, 0,   // column 1
            0,    0,    1, 0,   // column 2
            m[4], m[5], 0, 1    // column 3 (translation)
        };

        // Calculate inverse
        float det = m[0] * m[3] - m[1] * m[2];
        if (fabsf(det) > 0.0001f) {
            float invDet = 1.0f / det;
            inverseMatrix = {
                m[3] * invDet,  -m[1] * invDet, 0, 0,
                -m[2] * invDet, m[0] * invDet,  0, 0,
                0,              0,              1, 0,
                (m[2] * m[5] - m[3] * m[4]) * invDet,
                (m[1] * m[4] - m[0] * m[5]) * invDet, 0, 1
            };
        }
        useMatrix = true;
    }

    void setMatrixFromMat2d(const mat2d& m) {
        setMatrixFromMat2d(m.m);
    }

    // Mode for working in sync with DrawBot
    // IMPORTANT: DrawBot's rlgl transform must be reset
    // before calling this function!
    //
    // Usage:
    //   bot.save();
    //   bot.translate(pan.x, pan.y);
    //   bot.scale(zoom, zoom);
    //   mat2d savedTransform = bot.getTransform();
    //
    //   // temporarily reset DrawBot's transform for SDF drawing
    //   rlLoadIdentity();  // <-- IMPORTANT!
    //   sdf.syncWithDrawBot(savedTransform);
    //
    //   // SDF drawing (in world coordinates)
    //   sdf.drawCircle(100, 100, 50);
    //
    //   sdf.unsyncDrawBot();
    //   bot.applyCurrentTransform();  // restore DrawBot's transform
    //
    //   // DrawBot drawing
    //   bot.circle(...);
    //
    //   bot.restore();
    //
    void syncWithDrawBot(const mat2d& botMatrix) {
        setMatrixFromMat2d(botMatrix);
        pan = {0, 0};
        zoom = 1.0f;
    }

    // Alternative: use only pan/zoom, no matrix
    // Simpler, but no rotation/skew support
    void syncWithDrawBotSimple(float panX, float panY, float zoomLevel) {
        useMatrix = false;
        pan = {panX, panY};
        zoom = zoomLevel;
    }

    void unsyncDrawBot() {
        useMatrix = false;
    }

    // Style setters
    void setFill(cColor c) { fillColor = c; gradientType = 0; }
    void setStroke(cColor c, float w, float soft = 0, int mode = 0) {
        strokeColor = c; strokeWidth = w; strokeSoftness = soft; strokeMode = mode;
    }
    void setStrokeCenter(cColor c, float w, float soft = 0) { setStroke(c, w, soft, 0); }
    void setStrokeOutside(cColor c, float w, float soft = 0) { setStroke(c, w, soft, 1); }
    void setStrokeInside(cColor c, float w, float soft = 0) { setStroke(c, w, soft, 2); }
    // Call every frame for animation
    void update(float dt) { animTime += dt; }

    // ========== CARTOON STROKE PRESETS ==========

    // Normal solid stroke
    void setStrokeSolid(cColor c, float width) {
        strokeColor = c; strokeWidth = width;
        strokeStyle = STROKE_SOLID; strokeMode = 0;
    }

    // Tapered - brush stroke effect
    void setStrokeTapered(cColor c, float width, float taper = 0.6f) {
        strokeColor = c; strokeWidth = width;
        strokeStyle = STROKE_TAPERED; strokeTaper = taper; strokeMode = 0;
    }

    // Sketchy - hand-drawn effect (animated)
    void setStrokeSketchy(cColor c, float width, float wobble = 4.0f, float freq = 0.08f) {
        strokeColor = c; strokeWidth = width;
        strokeStyle = STROKE_SKETCHY;
        strokeWobble = wobble; strokeWobbleFreq = freq; strokeMode = 0;
    }

    // Double outline
    void setStrokeDouble(cColor inner, cColor outer, float width, float spacing = 4.0f) {
        strokeColor = inner; strokeColor2 = outer;
        strokeWidth = width; strokeSpacing = spacing;
        strokeStyle = STROKE_DOUBLE; strokeMode = 0;
    }

    // Triple outline
    void setStrokeTriple(cColor inner, cColor mid, cColor outer, float width, float spacing = 3.0f) {
        strokeColor = inner; strokeColor2 = mid; strokeColor3 = outer;
        strokeWidth = width; strokeSpacing = spacing;
        strokeStyle = STROKE_TRIPLE; strokeMode = 0;
    }

    // Offset shadow
    void setStrokeOffsetShadow(cColor main, cColor shadow, float width, float ox = 3, float oy = 3) {
        strokeColor = main; strokeColor2 = shadow;
        strokeWidth = width; strokeOffset2 = {ox, oy};
        strokeStyle = STROKE_OFFSET_SHADOW; strokeMode = 0;
    }

    // Variable width - pressure simulation
    void setStrokeVariable(cColor c, float width, float pressure = 0.6f) {
        strokeColor = c; strokeWidth = width;
        strokePressure = pressure;
        strokeStyle = STROKE_VARIABLE; strokeMode = 0;
    }

    // Ink spread
    void setStrokeInkSpread(cColor c, float width, float spread = 0.4f) {
        strokeColor = c; strokeWidth = width;
        strokeNoise = spread;
        strokeStyle = STROKE_INK_SPREAD; strokeMode = 0;
    }

    // Brush
    void setStrokeBrush(cColor c, float width, float angle = 0.785f, float ratio = 2.5f) {
        strokeColor = c; strokeWidth = width;
        brushAngle = angle; brushRatio = ratio;
        strokeWobble = 3.0f; strokeWobbleFreq = 0.1f;
        strokeStyle = STROKE_BRUSH; strokeMode = 0;
    }

    // Comic book style - thick outline + highlight
    void setStrokeComic(cColor c, float width) {
        strokeColor = c; strokeWidth = width;
        strokeStyle = STROKE_COMIC; strokeMode = 0;
    }

    // Neon outline - glowing line
    void setStrokeNeon(cColor c, float width) {
        strokeColor = c; strokeWidth = width;
        strokeStyle = STROKE_NEON_OUTLINE; strokeMode = 0;
    }

    // Halftone - dotted comic style (Ben-Day dots)
    void setStrokeHalftone(cColor c, float width) {
        strokeColor = c; strokeWidth = width;
        strokeStyle = STROKE_HALFTONE; strokeMode = 0;
    }

    // ========== DASH STROKE API ==========

    // Simple dashed stroke
    void setDash(float length, float gap, int capStyle = 0) {
        dashEnabled = true;
        dashLength = length;
        dashGap = gap;
        dashCapStyle = capStyle;
        dashCustomEnabled = false;
    }

    // Dashed with animation (flowing)
    void setDashAnimated(float length, float gap, float speed = 50.0f, int capStyle = 1) {
        setDash(length, gap, capStyle);
        dashOffset = animTime * speed;
    }

    // Custom pattern: up to 2 dash-gap pairs
    void setDashCustom(float dash1, float gap1, float dash2 = 0, float gap2 = 0, int capStyle = 0) {
        dashEnabled = true;
        dashCustomEnabled = true;
        dashCapStyle = capStyle;
        dashPatternData[0] = dash1;
        dashPatternData[1] = gap1;
        dashPatternData[2] = dash2;
        dashPatternData[3] = gap2;
    }

    // Dotted line (small dashes, round caps)
    void setDotted(float spacing = 10.0f, float dotSize = 2.0f) {
        setDash(dotSize, spacing, 1);  // round cap
    }

    // Morse code style dash-dot pattern
    void setDashDot(float dashLen = 20.0f, float dotLen = 5.0f, float gap = 8.0f) {
        setDashCustom(dashLen, gap, dotLen, gap, 1);
    }

    // Clear dash (solid stroke)
    void clearDash() {
        dashEnabled = false;
        dashCustomEnabled = false;
    }

    void setGlow(cColor c, float sz, float intensity = 1.0f, GlowMode mode = GLOW_ADDITIVE) {
        glowColor = c; glowSize = sz; glowIntensity = intensity; glowMode = mode;
    }
    void setGlowAdditive(cColor c, float sz, float i = 2.0f) { setGlow(c, sz, i, GLOW_ADDITIVE); }
    void setGlowScreen(cColor c, float sz, float i = 1.5f) { setGlow(c, sz, i, GLOW_SCREEN); }
    void setGlowSoft(cColor c, float sz, float i = 2.0f) { setGlow(c, sz, i, GLOW_SOFT); }
    void setGlowClassic(cColor c, float sz) { setGlow(c, sz, 1.0f, GLOW_CLASSIC); }
    void setShadow(cColor c, float ox, float oy, float blur) { shadowColor = c; shadowOffset = {ox, oy}; shadowBlur = blur; }
    void setGradientLinear(cColor c1, cColor c2, vec2 s, vec2 e) {
        gradientType = 1; gradientColor1 = c1; gradientColor2 = c2; gradientStart = s; gradientEnd = e;
    }
    void setGradientRadial(cColor inner, cColor outer) { gradientType = 2; gradientColor1 = inner; gradientColor2 = outer; }
    void clearGradient() { gradientType = 0; }

    // Deformation setters
    void clearDeform() { deformType = 0; deformParams = {0, 1, 0, 0}; }
    void setDeformBend(float amount) { deformType = 1; deformParams = {amount, 0, 0, 0}; }
    void setDeformTaper(float amount) { deformType = 2; deformParams = {amount, 0, 0, 0}; }
    void setDeformSkew(float amount) { deformType = 3; deformParams = {amount, 0, 0, 0}; }
    void setDeformWave(float amount, float freq = 0.1f, float offset = 0) {
        deformType = 4; deformParams = {amount, freq, offset, 0};
    }
    void setDeformNoise(float amount, float freq = 0.05f, float offset = 0) {
        deformType = 5; deformParams = {amount, freq, offset, 0};
    }
    void setDeformTwist(float amount) { deformType = 6; deformParams = {amount, 0, 0, 0}; }
    void setDeformBulge(float amount) { deformType = 7; deformParams = {amount, 0, 0, 0}; }
    void setDeformPinch(float amount) { deformType = 8; deformParams = {amount, 0, 0, 0}; }

    // ========== 2.5D API ==========

    void set25D(bool enabled, float radius = 1.0f) {
        enable25D = enabled;
        extrudeRadius = radius;
    }
    void enable25DMode(float radius = 1.0f) { enable25D = true; extrudeRadius = radius; }
    void disable25D() { enable25D = false; }

    void setLightDirection(float x, float y, float z) {
        float len = sqrtf(x*x + y*y + z*z);
        if (len > 0.001f) lightDir = {x/len, y/len, z/len};
    }
    void setLightDirection(Vector3 dir) { setLightDirection(dir.x, dir.y, dir.z); }

    void setAmbient(float strength) { ambientStrength = strength; }
    void setDiffuse(float strength) { diffuseStrength = strength; }
    void setSpecular(float strength, float power = 32.0f) {
        specularStrength = strength;
        specularPower = power;
    }
    void setFresnel(float strength, float ior = 1.5f) {
        fresnelStrength = strength;
        fresnelIOR = ior;
    }

    // Preset 2.5D materials
    // void set25DPlastic(float radius=1.0f) {
    //     enable25D = true; extrudeRadius = radius;
    //     lightDir = {-0.5f, -0.5f, 1.0f};
    //     ambientStrength = 0.3f; diffuseStrength = 0.6f;
    //     specularStrength = 0.8f; specularPower = 64.0f;
    //     fresnelStrength = 0.2f; fresnelIOR = 1.5f;
    // }

    // void set25DMatte(float radius=1.0f) {
    //     enable25D = true; extrudeRadius = radius;
    //     lightDir = {-0.5f, -0.5f, 1.0f};
    //     ambientStrength = 0.4f; diffuseStrength = 0.7f;
    //     specularStrength = 0.1f; specularPower = 8.0f;
    //     fresnelStrength = 0.05f; fresnelIOR = 1.3f;
    // }

    // void set25DGlossy(float radius=1.0f) {
    //     enable25D = true; extrudeRadius = radius;
    //     lightDir = {-0.3f, -0.6f, 1.0f};
    //     ambientStrength = 0.2f; diffuseStrength = 0.5f;
    //     specularStrength = 1.0f; specularPower = 128.0f;
    //     fresnelStrength = 0.5f; fresnelIOR = 1.8f;
    // }

    // void set25DMetallic(float radius=1.0f) {
    //     enable25D = true; extrudeRadius = radius;
    //     lightDir = {-0.4f, -0.4f, 1.0f};
    //     ambientStrength = 0.15f; diffuseStrength = 0.4f;
    //     specularStrength = 1.2f; specularPower = 256.0f;
    //     fresnelStrength = 0.8f; fresnelIOR = 2.5f;
    // }

    // void set25DButton(float radius=0.5f) {
    //     enable25D = true; extrudeRadius = radius;
    //     lightDir = {-0.3f, -0.7f, 1.0f};
    //     ambientStrength = 0.35f; diffuseStrength = 0.6f;
    //     specularStrength = 0.6f; specularPower = 48.0f;
    //     fresnelStrength = 0.15f; fresnelIOR = 1.4f;
    // }

    // void set25DGel(float radius=1.2f) {
    //     enable25D = true; extrudeRadius = radius;
    //     lightDir = {-0.4f, -0.5f, 1.0f};
    //     ambientStrength = 0.25f; diffuseStrength = 0.5f;
    //     specularStrength = 0.9f; specularPower = 96.0f;
    //     fresnelStrength = 0.6f; fresnelIOR = 1.3f;
    // }

    void clearEffects() {
        strokeWidth = 0; strokeSoftness = 0; strokeMode = 0;
        glowSize = 0; shadowColor.a = 0; gradientType = 0;
        deformType = 0; deformParams = {0, 1, 0, 0}; enable25D = false;
        cache.strokeWidth = -1; cache.glowSize = -1;
        cache.shadowColor.a = 255; cache.gradientType = -1;
        cache.deformType = -1; cache.enable25D = -1;
        strokeStyle = STROKE_SOLID;
    }
    void setRotation(float r) { rotation = r; }

    // ========== PATH API ==========

    void setPathFillRule(PathFillRule rule) { pathFillRule = rule; }

    void drawPath(const SDFPath& path, float x, float y)
    {
        if (path.getPointCount() == 0) return;
        if (path.getPointCount() > SDF_MAX_PATH_POINTS) {
            TraceLog(LOG_WARNING, "SDF2D: Path too complex (%d points, max %d)", path.getPointCount(), SDF_MAX_PATH_POINTS);
            return;
        }

        rlPushMatrix();
        rlLoadIdentity();
        BeginShaderMode(shader);

        // Set path mode
        int mode = 1;
        SetShaderValue(shader, pathModeLoc, &mode, SHADER_UNIFORM_INT);
        int bmode = 0;
        SetShaderValue(shader, batchModeLoc, &bmode, SHADER_UNIFORM_INT);

        updateCachedTransform();
        updateCachedStyle();

        // Matrix transform
        int um = useMatrix ? 1 : 0;
        SetShaderValue(shader, useMatrixLoc, &um, SHADER_UNIFORM_INT);

        if (useMatrix) {
            // Combine path offset with transform matrix
            Matrix offsetMat = MatrixTranslate(x, y, 0);
            Matrix combined = MatrixMultiply(offsetMat, transformMatrix);
            Matrix combinedInv = MatrixInvert(combined);

            // Send as separate vec3 columns
            Vector3 tCol0 = {combined.m0, combined.m1, 0.0f};
            Vector3 tCol1 = {combined.m4, combined.m5, 0.0f};
            Vector3 tCol2 = {combined.m12, combined.m13, 1.0f};

            Vector3 iCol0 = {combinedInv.m0, combinedInv.m1, 0.0f};
            Vector3 iCol1 = {combinedInv.m4, combinedInv.m5, 0.0f};
            Vector3 iCol2 = {combinedInv.m12, combinedInv.m13, 1.0f};

            SetShaderValue(shader, transformCol0Loc, &tCol0, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, transformCol1Loc, &tCol1, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, transformCol2Loc, &tCol2, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, inverseCol0Loc, &iCol0, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, inverseCol1Loc, &iCol1, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, inverseCol2Loc, &iCol2, SHADER_UNIFORM_VEC3);
        } else {
            position = {x, y};
            SetShaderValue(shader, shapePosLoc, &position, SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, shapeRotationLoc, &rotation, SHADER_UNIFORM_FLOAT);
        }

        int pointCount = path.getPointCount();
        int fillRule = (int)pathFillRule;
        SetShaderValue(shader, pathPointCountLoc, &pointCount, SHADER_UNIFORM_INT);
        SetShaderValue(shader, pathFillRuleLoc, &fillRule, SHADER_UNIFORM_INT);
        SetShaderValueV(shader, pathDataLoc, path.getData(), SHADER_UNIFORM_VEC4, pointCount);

        // Calculate bounds (with matrix transform consideration)
        Rectangle pathBounds = path.getBounds();

        // Send path bounds to shader for early culling
        Vector4 boundsVec = {pathBounds.x, pathBounds.y, pathBounds.width, pathBounds.height};
        SetShaderValue(shader, pathBoundsLoc, &boundsVec, SHADER_UNIFORM_VEC4);

        float shadowPad = (shadowColor.a > 0) ? (fabsf(shadowOffset.x) + fabsf(shadowOffset.y) + shadowBlur) : 0;
        float pad = fmaxf(fmaxf(glowSize, shadowPad), strokeWidth + strokeSoftness) + 20;

        Rectangle screenBounds;
        if (useMatrix) {
            // Combine path offset with transform matrix for bounds calculation
            Matrix offsetMat = MatrixTranslate(x, y, 0);
            Matrix combined = MatrixMultiply(offsetMat, transformMatrix);

            // Local space bounding box corners (path bounds + padding)
            float lx0 = pathBounds.x - pad;
            float ly0 = pathBounds.y - pad;
            float lx1 = pathBounds.x + pathBounds.width + pad;
            float ly1 = pathBounds.y + pathBounds.height + pad;

            // Transform all 4 corners
            float corners[4][2] = {
                {lx0, ly0}, {lx1, ly0},
                {lx0, ly1}, {lx1, ly1}
            };

            float minX = 1e10f, maxX = -1e10f;
            float minY = 1e10f, maxY = -1e10f;

            for (int i = 0; i < 4; i++) {
                float wx = combined.m0 * corners[i][0] + combined.m4 * corners[i][1] + combined.m12;
                float wy = combined.m1 * corners[i][0] + combined.m5 * corners[i][1] + combined.m13;
                minX = fminf(minX, wx);
                maxX = fmaxf(maxX, wx);
                minY = fminf(minY, wy);
                maxY = fmaxf(maxY, wy);
            }

            screenBounds = {
                minX * zoom + pan.x,
                minY * zoom + pan.y,
                (maxX - minX) * zoom,
                (maxY - minY) * zoom
            };
        } else {
            screenBounds = {
                (x + pathBounds.x - pad) * zoom + pan.x,
                (y + pathBounds.y - pad) * zoom + pan.y,
                (pathBounds.width + pad * 2) * zoom,
                (pathBounds.height + pad * 2) * zoom
            };
        }

        rlSetBlendFactors(RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_ONE);
        rlSetBlendMode(BLEND_CUSTOM);
        DrawRectangleRec(screenBounds, WHITE);
        EndShaderMode();
        rlSetBlendMode(BLEND_ALPHA);
        rlPopMatrix();
        mode = 0;
        SetShaderValue(shader, pathModeLoc, &mode, SHADER_UNIFORM_INT);
    }

    // Convenience: draw path at origin
    void drawPath(const SDFPath& path) { drawPath(path, 0, 0); }

    // ========== BATCH API ==========
    // ========== Cache System ==========
private:
    float cachedScreenHeight = 0;
    float targetRenderHeight = 0;
    struct {
        vec2 pan = {-99999, -99999};
        float zoom = -1;
        cColor fillColor = {255,255,255,0};
        cColor strokeColor = {0,0,0,0};
        float strokeWidth = -1;
        float strokeSoftness = -1;
        int strokeMode = -1;
        cColor glowColor = {0,0,0,0};
        float glowSize = -1;
        float glowIntensity = -1;
        int glowMode = -1;
        cColor shadowColor = {0,0,0,0};
        vec2 shadowOffset = {-99999,-99999};
        float shadowBlur = -1;
        int gradientType = -1;
        int deformType = -1;
        Vector4 deformParams = {-99999,-99999,-99999,-99999};
        int enable25D = -1;
        float extrudeRadius = -1;
        Vector3 lightDir = {-99999,-99999,-99999};
        float ambientStrength = -1;
        float diffuseStrength = -1;
        float specularStrength = -1;
        float specularPower = -1;
        float fresnelStrength = -1;
        float fresnelIOR = -1;
    } cache;

    static bool colorEq(cColor a, cColor b) { return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a; }

    void updateCachedTransform() {
        float screenH = (targetRenderHeight > 0) ? targetRenderHeight : (float)GetScreenHeight();
        if (cachedScreenHeight != screenH) {
            SetShaderValue(shader, screenHeightLoc, &screenH, SHADER_UNIFORM_FLOAT);
            cachedScreenHeight = screenH;
        }
        if (cache.pan.x != pan.x || cache.pan.y != pan.y) {
            SetShaderValue(shader, panLoc, &pan, SHADER_UNIFORM_VEC2);
            cache.pan = pan;
        }
        if (cache.zoom != zoom) {
            SetShaderValue(shader, zoomLoc, &zoom, SHADER_UNIFORM_FLOAT);
            cache.zoom = zoom;
        }
    }

    void updateCachedStyle() {
        if (!colorEq(cache.fillColor, fillColor)) {
            vec4 fc = fillColor.toVec4();
            SetShaderValue(shader, fillColorLoc, &fc, SHADER_UNIFORM_VEC4);
            cache.fillColor = fillColor;
        }
        if (!colorEq(cache.strokeColor, strokeColor) || cache.strokeWidth != strokeWidth ||
            cache.strokeSoftness != strokeSoftness || cache.strokeMode != strokeMode) {
            vec4 sc = strokeColor.toVec4();
            SetShaderValue(shader, strokeColorLoc, &sc, SHADER_UNIFORM_VEC4);
            SetShaderValue(shader, strokeWidthLoc, &strokeWidth, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, strokeSoftnessLoc, &strokeSoftness, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, strokeModeLoc, &strokeMode, SHADER_UNIFORM_INT);
            cache.strokeColor = strokeColor;
            cache.strokeWidth = strokeWidth;
            cache.strokeSoftness = strokeSoftness;
            cache.strokeMode = strokeMode;
        }
        if (!colorEq(cache.glowColor, glowColor) || cache.glowSize != glowSize ||
            cache.glowIntensity != glowIntensity || cache.glowMode != (int)glowMode) {
            vec4 gc = glowColor.toVec4();
            SetShaderValue(shader, glowColorLoc, &gc, SHADER_UNIFORM_VEC4);
            SetShaderValue(shader, glowSizeLoc, &glowSize, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, glowIntensityLoc, &glowIntensity, SHADER_UNIFORM_FLOAT);
            int gm = (int)glowMode;
            SetShaderValue(shader, glowModeLoc, &gm, SHADER_UNIFORM_INT);
            cache.glowColor = glowColor;
            cache.glowSize = glowSize;
            cache.glowIntensity = glowIntensity;
            cache.glowMode = gm;
        }
        if (!colorEq(cache.shadowColor, shadowColor) || cache.shadowBlur != shadowBlur ||
            cache.shadowOffset.x != shadowOffset.x || cache.shadowOffset.y != shadowOffset.y) {
            vec4 shc = shadowColor.toVec4();
            SetShaderValue(shader, shadowColorLoc, &shc, SHADER_UNIFORM_VEC4);
            SetShaderValue(shader, shadowOffsetLoc, &shadowOffset, SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, shadowBlurLoc, &shadowBlur, SHADER_UNIFORM_FLOAT);
            cache.shadowColor = shadowColor;
            cache.shadowOffset = shadowOffset;
            cache.shadowBlur = shadowBlur;
        }
        if (cache.gradientType != gradientType) {
            SetShaderValue(shader, gradientTypeLoc, &gradientType, SHADER_UNIFORM_INT);
            cache.gradientType = gradientType;
        }
        if (gradientType > 0) {
            vec4 gc1 = gradientColor1.toVec4();
            vec4 gc2 = gradientColor2.toVec4();
            SetShaderValue(shader, gradientColor1Loc, &gc1, SHADER_UNIFORM_VEC4);
            SetShaderValue(shader, gradientColor2Loc, &gc2, SHADER_UNIFORM_VEC4);
            SetShaderValue(shader, gradientStartLoc, &gradientStart, SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, gradientEndLoc, &gradientEnd, SHADER_UNIFORM_VEC2);
        }
        // Deformation
        if (cache.deformType != deformType ||
            cache.deformParams.x != deformParams.x || cache.deformParams.y != deformParams.y ||
            cache.deformParams.z != deformParams.z || cache.deformParams.w != deformParams.w) {
            SetShaderValue(shader, deformTypeLoc, &deformType, SHADER_UNIFORM_INT);
            SetShaderValue(shader, deformParamsLoc, &deformParams, SHADER_UNIFORM_VEC4);
            cache.deformType = deformType;
            cache.deformParams = deformParams;
        }
        // 2.5D
        int e25d = enable25D ? 1 : 0;
        if (cache.enable25D != e25d) {
            SetShaderValue(shader, enable25DLoc, &e25d, SHADER_UNIFORM_INT);
            cache.enable25D = e25d;
        }
        if (cache.extrudeRadius != extrudeRadius) {
            SetShaderValue(shader, extrudeRadiusLoc, &extrudeRadius, SHADER_UNIFORM_FLOAT);
            cache.extrudeRadius = extrudeRadius;
        }
        if (cache.lightDir.x != lightDir.x || cache.lightDir.y != lightDir.y || cache.lightDir.z != lightDir.z) {
            SetShaderValue(shader, lightDirLoc, &lightDir, SHADER_UNIFORM_VEC3);
            cache.lightDir = lightDir;
        }
        if (cache.ambientStrength != ambientStrength) {
            SetShaderValue(shader, ambientStrengthLoc, &ambientStrength, SHADER_UNIFORM_FLOAT);
            cache.ambientStrength = ambientStrength;
        }
        if (cache.diffuseStrength != diffuseStrength) {
            SetShaderValue(shader, diffuseStrengthLoc, &diffuseStrength, SHADER_UNIFORM_FLOAT);
            cache.diffuseStrength = diffuseStrength;
        }
        if (cache.specularStrength != specularStrength || cache.specularPower != specularPower) {
            SetShaderValue(shader, specularStrengthLoc, &specularStrength, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, specularPowerLoc, &specularPower, SHADER_UNIFORM_FLOAT);
            cache.specularStrength = specularStrength;
            cache.specularPower = specularPower;
        }
        if (cache.fresnelStrength != fresnelStrength || cache.fresnelIOR != fresnelIOR) {
            SetShaderValue(shader, fresnelStrengthLoc, &fresnelStrength, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, fresnelIORLoc, &fresnelIOR, SHADER_UNIFORM_FLOAT);
            cache.fresnelStrength = fresnelStrength;
            cache.fresnelIOR = fresnelIOR;
        }
        // Cartoon stroke uniforms
        int ss = (int)strokeStyle;
        SetShaderValue(shader, strokeStyleLoc, &ss, SHADER_UNIFORM_INT);
        SetShaderValue(shader, strokeTaperLoc, &strokeTaper, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, strokeWobbleLoc, &strokeWobble, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, strokeWobbleFreqLoc, &strokeWobbleFreq, SHADER_UNIFORM_FLOAT);
        vec4 sc2 = strokeColor2.toVec4();
        vec4 sc3 = strokeColor3.toVec4();
        SetShaderValue(shader, strokeColor2Loc, &sc2, SHADER_UNIFORM_VEC4);
        SetShaderValue(shader, strokeColor3Loc, &sc3, SHADER_UNIFORM_VEC4);
        SetShaderValue(shader, strokeOffset2Loc, &strokeOffset2, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, strokeSpacingLoc, &strokeSpacing, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, strokeNoiseLoc, &strokeNoise, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, strokePressureLoc, &strokePressure, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, brushAngleLoc, &brushAngle, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, brushRatioLoc, &brushRatio, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, animTimeLoc, &animTime, SHADER_UNIFORM_FLOAT);

        // Dash uniforms - packed into vec4
        // dashParams: x=length, y=gap, z=offset, w=enabled
        float dashParams[4] = {dashLength, dashGap, dashOffset, dashEnabled ? 1.0f : 0.0f};
        SetShaderValue(shader, dashParamsLoc, dashParams, SHADER_UNIFORM_VEC4);

        // dashParams2: x=capStyle, y=customEnabled, z=reserved, w=reserved
        float dashParams2[4] = {(float)dashCapStyle, dashCustomEnabled ? 1.0f : 0.0f, 0.0f, 0.0f};
        SetShaderValue(shader, dashParams2Loc, dashParams2, SHADER_UNIFORM_VEC4);

        // Custom pattern
        SetShaderValue(shader, dashPatternLoc, dashPatternData, SHADER_UNIFORM_VEC4);
    }

public:
    void beginDraw() { }
    void endDraw() { }

    void updateUniforms(SDFShape type, Vector4 p, Vector4 p2 = {0,0,0,0}) {
        int mode = 0;
        SetShaderValue(shader, batchModeLoc, &mode, SHADER_UNIFORM_INT);
        SetShaderValue(shader, pathModeLoc, &mode, SHADER_UNIFORM_INT);
        updateCachedTransform();
        updateCachedStyle();

        // Matrix transform
        int um = useMatrix ? 1 : 0;
        SetShaderValue(shader, useMatrixLoc, &um, SHADER_UNIFORM_INT);

        if (useMatrix) {
            // Send as separate vec3 columns
            Vector3 tCol0 = {transformMatrix.m0, transformMatrix.m1, 0.0f};
            Vector3 tCol1 = {transformMatrix.m4, transformMatrix.m5, 0.0f};
            Vector3 tCol2 = {transformMatrix.m12, transformMatrix.m13, 1.0f};

            Vector3 iCol0 = {inverseMatrix.m0, inverseMatrix.m1, 0.0f};
            Vector3 iCol1 = {inverseMatrix.m4, inverseMatrix.m5, 0.0f};
            Vector3 iCol2 = {inverseMatrix.m12, inverseMatrix.m13, 1.0f};

            SetShaderValue(shader, transformCol0Loc, &tCol0, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, transformCol1Loc, &tCol1, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, transformCol2Loc, &tCol2, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, inverseCol0Loc, &iCol0, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, inverseCol1Loc, &iCol1, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, inverseCol2Loc, &iCol2, SHADER_UNIFORM_VEC3);
        } else {
            SetShaderValue(shader, shapePosLoc, &position, SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, shapeRotationLoc, &rotation, SHADER_UNIFORM_FLOAT);
        }

        SetShaderValue(shader, shapeSizeLoc, &size, SHADER_UNIFORM_VEC2);
        int t = (int)type;
        SetShaderValue(shader, shapeTypeLoc, &t, SHADER_UNIFORM_INT);
        SetShaderValue(shader, shapeParamsLoc, &p, SHADER_UNIFORM_VEC4);
        SetShaderValue(shader, shapeParams2Loc, &p2, SHADER_UNIFORM_VEC4);
    }

    void drawShape(Rectangle worldBounds)
    {
        float shadowPad = (shadowColor.a > 0) ? (fabsf(shadowOffset.x) + fabsf(shadowOffset.y) + shadowBlur) : 0;
        float pad = fmaxf(fmaxf(glowSize, shadowPad), strokeWidth+strokeSoftness+strokeSpacing*3) + 30;
        Rectangle screenBounds = {
            (worldBounds.x - pad) * zoom + pan.x,
            (worldBounds.y - pad) * zoom + pan.y,
            (worldBounds.width + pad * 2) * zoom,
            (worldBounds.height + pad * 2) * zoom
        };
        rlPushMatrix();
        rlLoadIdentity();
        BeginShaderMode(shader);
        rlSetBlendFactors(RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_ONE);
        rlSetBlendMode(BLEND_CUSTOM);
        DrawRectangleRec(screenBounds, WHITE);
        EndShaderMode();
        rlSetBlendMode(BLEND_ALPHA);
        rlPopMatrix();
    }

    Rectangle worldToScreen(Rectangle r) {
        return { r.x * zoom + pan.x, r.y * zoom + pan.y, r.width * zoom, r.height * zoom };
    }

    // ========== SINGLE DRAW ==========
private:
    // Defined after TileBatchSystem.h is included
    inline void autoBatchAddShapeInternal(SDFShape type, Vector4 p, Vector4 p2);

    void drawSingle(SDFShape type, Vector4 p, Vector4 p2, float bx, float by, float bw, float bh)
    {
        // If batch mode is active, add to batch instead of drawing immediately
        if (batchMode && tileBatch) {
            autoBatchAddShapeInternal(type, p, p2);
            return;
        }

        rlPushMatrix();
        rlLoadIdentity();
        BeginShaderMode(shader);

        int mode = 0;
        SetShaderValue(shader, batchModeLoc, &mode, SHADER_UNIFORM_INT);
        SetShaderValue(shader, pathModeLoc, &mode, SHADER_UNIFORM_INT);

        updateCachedTransform();
        updateCachedStyle();

        // Matrix transform
        int um = useMatrix ? 1 : 0;
        SetShaderValue(shader, useMatrixLoc, &um, SHADER_UNIFORM_INT);

        // Effective matrix (includes position)
        Matrix effectiveMatrix = transformMatrix;
        Matrix effectiveInverse = inverseMatrix;

        if (useMatrix) {
            // Fold position into the matrix
            // drawCircle(100, 100, 50) sets position = {100, 100}
            // Adding this position to the matrix draws the shape in the right place
            Matrix posMatrix = MatrixTranslate(position.x, position.y, 0);
            effectiveMatrix = MatrixMultiply(posMatrix, transformMatrix);
            effectiveInverse = MatrixInvert(effectiveMatrix);

            // Send as separate vec3 columns
            Vector3 tCol0 = {effectiveMatrix.m0, effectiveMatrix.m1, 0.0f};
            Vector3 tCol1 = {effectiveMatrix.m4, effectiveMatrix.m5, 0.0f};
            Vector3 tCol2 = {effectiveMatrix.m12, effectiveMatrix.m13, 1.0f};

            Vector3 iCol0 = {effectiveInverse.m0, effectiveInverse.m1, 0.0f};
            Vector3 iCol1 = {effectiveInverse.m4, effectiveInverse.m5, 0.0f};
            Vector3 iCol2 = {effectiveInverse.m12, effectiveInverse.m13, 1.0f};

            SetShaderValue(shader, transformCol0Loc, &tCol0, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, transformCol1Loc, &tCol1, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, transformCol2Loc, &tCol2, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, inverseCol0Loc, &iCol0, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, inverseCol1Loc, &iCol1, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, inverseCol2Loc, &iCol2, SHADER_UNIFORM_VEC3);
        }
        else {
            SetShaderValue(shader, shapePosLoc, &position, SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, shapeRotationLoc, &rotation, SHADER_UNIFORM_FLOAT);
        }

        SetShaderValue(shader, shapeSizeLoc, &size, SHADER_UNIFORM_VEC2);
        int t = (int)type;
        SetShaderValue(shader, shapeTypeLoc, &t, SHADER_UNIFORM_INT);
        SetShaderValue(shader, shapeParamsLoc, &p, SHADER_UNIFORM_VEC4);
        SetShaderValue(shader, shapeParams2Loc, &p2, SHADER_UNIFORM_VEC4);

        // Bounding box computation
        float shadowPad = (shadowColor.a > 0) ? (fabsf(shadowOffset.x) + fabsf(shadowOffset.y) + shadowBlur) : 0;
        float pad = fmaxf(fmaxf(glowSize, shadowPad), strokeWidth+strokeSoftness+strokeSpacing*3) + 30;
        Rectangle screenBounds;

        if (useMatrix) {
            // With a matrix: transform the local bbox corners
            float halfW = (bw / 2.0f) + pad;
            float halfH = (bh / 2.0f) + pad;

            // Transform the 4 corners (using effectiveMatrix)
            float corners[4][2] = {
                {-halfW, -halfH}, { halfW, -halfH},
                {-halfW,  halfH}, { halfW,  halfH}
            };

            float minX = 1e10f, maxX = -1e10f;
            float minY = 1e10f, maxY = -1e10f;

            for (int i = 0; i < 4; i++) {
                float wx = effectiveMatrix.m0 * corners[i][0] + effectiveMatrix.m4 * corners[i][1] + effectiveMatrix.m12;
                float wy = effectiveMatrix.m1 * corners[i][0] + effectiveMatrix.m5 * corners[i][1] + effectiveMatrix.m13;
                minX = fminf(minX, wx);
                maxX = fmaxf(maxX, wx);
                minY = fminf(minY, wy);
                maxY = fmaxf(maxY, wy);
            }

            screenBounds = {
                minX * zoom + pan.x,
                minY * zoom + pan.y,
                (maxX - minX) * zoom,
                (maxY - minY) * zoom
            };
        } else {
            screenBounds = {
                (bx - pad) * zoom + pan.x,
                (by - pad) * zoom + pan.y,
                (bw + pad * 2) * zoom,
                (bh + pad * 2) * zoom
            };
        }

        rlSetBlendFactors(RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_ONE);
        rlSetBlendMode(BLEND_CUSTOM);
        DrawRectangleRec(screenBounds, WHITE);
        EndShaderMode();
        rlSetBlendMode(BLEND_ALPHA);
        rlPopMatrix();
    }

public:
    // Draw functions
    void drawCircle(float x, float y, float r, cColor c = cBlank) {
        position = {x, y}; size = {r*2, r*2};
        if(c.a != 0) fillColor = c;
        drawSingle(SDF_CIRCLE, {r,0,0,0}, {0,0,0,0}, x-r, y-r, r*2, r*2);
    }
    void drawCircle(vec2 pos, float r, cColor c = cBlank) {
        drawCircle(pos.x, pos.y, r, c);
    }
    void drawBox(float x, float y, float w, float h) {
        position = {x+w/2, y+h/2}; size = {w, h};
        drawSingle(SDF_BOX, {w/2,h/2,0,0}, {0,0,0,0}, x, y, w, h);
    }

    void drawRoundedBox(float x, float y, float w, float h, float r) {
        position = {x+w/2, y+h/2}; size = {w, h};
        drawSingle(SDF_ROUNDED_BOX, {r,0,0,0}, {0,0,0,0}, x, y, w, h);
    }

    void drawRoundedBoxEx(float x, float y, float w, float h, float tl, float tr, float br, float bl) {
        position = {x+w/2, y+h/2}; size = {w, h};
        drawSingle(SDF_ROUNDED_BOX, {0,0,0,0}, {tr, br, tl, bl}, x, y, w, h);
    }

    void drawHexagon(float x, float y, float r) {
        position = {x, y}; size = {r*2, r*2};
        drawSingle(SDF_HEXAGON, {r,0,0,0}, {0,0,0,0}, x-r, y-r, r*2, r*2);
    }

    void drawPentagon(float x, float y, float r) {
        position = {x, y}; size = {r*2, r*2};
        drawSingle(SDF_PENTAGON, {r,0,0,0}, {0,0,0,0}, x-r, y-r, r*2, r*2);
    }

    void drawOctagon(float x, float y, float r) {
        position = {x, y}; size = {r*2, r*2};
        drawSingle(SDF_OCTAGON, {r,0,0,0}, {0,0,0,0}, x-r, y-r, r*2, r*2);
    }

    void drawHexagram(float x, float y, float r) {
        position = {x, y}; size = {r*2, r*2};
        drawSingle(SDF_HEXAGRAM, {r,0,0,0}, {0,0,0,0}, x-r, y-r, r*2, r*2);
    }

    void drawStar5(float x, float y, float r, float rf = 0.5f) {
        position = {x, y}; size = {r*2, r*2};
        drawSingle(SDF_STAR5, {r,rf,0,0}, {0,0,0,0}, x-r, y-r, r*2, r*2);
    }

    void drawStar(float x, float y, float r, int n, float m) {
        position = {x, y}; size = {r*2, r*2};
        drawSingle(SDF_STAR, {r,(float)n,m,0}, {0,0,0,0}, x-r, y-r, r*2, r*2);
    }

    void drawHeart(float x, float y, float s) {
        position = {x, y}; size = {s*2, s*2};
        drawSingle(SDF_HEART, {s,0,0,0}, {0,0,0,0}, x-s, y-s, s*2, s*2);
    }

    void drawTriangle(float x, float y, float s) {
        position = {x, y}; size = {s*2, s*2};
        drawSingle(SDF_EQUILATERAL_TRIANGLE, {s,0,0,0}, {0,0,0,0}, x-s, y-s, s*2, s*2);
    }

    void drawPie(float x, float y, float r, float angle) {
        position = {x, y}; size = {r*2, r*2};
        drawSingle(SDF_PIE, {r,angle,0,0}, {0,0,0,0}, x-r, y-r, r*2, r*2);
    }

    void drawArc(float x, float y, float r, float thickness, float angle) {
        position = {x, y}; size = {r*2, r*2};
        drawSingle(SDF_ARC, {r,thickness,angle,0}, {0,0,0,0}, x-r-thickness, y-r-thickness, r*2+thickness*2, r*2+thickness*2);
    }

    void drawMoon(float x, float y, float d, float ra, float rb) {
        position = {x, y}; size = {ra*2, ra*2};
        drawSingle(SDF_MOON, {d,ra,rb,0}, {0,0,0,0}, x-ra, y-ra, ra*2, ra*2);
    }

    void drawEgg(float x, float y, float ra, float rb) {
        position = {x, y}; float maxR = fmaxf(ra, ra+rb)*2; size = {maxR, maxR};
        drawSingle(SDF_EGG, {ra,rb,0,0}, {0,0,0,0}, x-maxR/2, y-maxR/2, maxR, maxR);
    }

    void drawEllipse(float x, float y, float rx, float ry) {
        position = {x, y}; float maxR = fmaxf(rx, ry); size = {rx*2, ry*2};
        drawSingle(SDF_ELLIPSE, {rx,ry,0,0}, {0,0,0,0}, x-maxR, y-maxR, maxR*2, maxR*2);
    }
    void drawEllipse(vec2 p, vec2 sz) {
        position = {p.x, p.y}; float maxR = fmaxf(sz.x, sz.y); size = {sz.x*2, sz.y*2};
        drawSingle(SDF_ELLIPSE, {sz.x,sz.y,0,0}, {0,0,0,0}, p.x-maxR, p.y-maxR, maxR*2, maxR*2);
    }

    void drawRhombus(float x, float y, float w, float h) {
        position = {x, y}; size = {w, h};
        drawSingle(SDF_RHOMBUS, {w/2,h/2,0,0}, {0,0,0,0}, x-w/2, y-h/2, w, h);
    }

    void drawCross(float x, float y, float w, float h, float r = 0) {
        position = {x, y}; float maxS = fmaxf(w, h); size = {maxS*2, maxS*2};
        drawSingle(SDF_CROSS, {w,h,r,0}, {0,0,0,0}, x-maxS, y-maxS, maxS*2, maxS*2);
    }

    void drawRoundedX(float x, float y, float w, float r) {
        position = {x, y}; size = {w*2, w*2};
        drawSingle(SDF_ROUNDED_X, {w,r,0,0}, {0,0,0,0}, x-w-r, y-w-r, w*2+r*2, w*2+r*2);
    }

    void drawSegment(float x1, float y1, float x2, float y2, float thickness) {
        float cx = (x1+x2)/2, cy = (y1+y2)/2;
        position = {cx, cy};
        float minX = fminf(x1,x2), minY = fminf(y1,y2);
        float maxX = fmaxf(x1,x2), maxY = fmaxf(y1,y2);
        size = {maxX-minX, maxY-minY};
        float oldStroke = strokeWidth; strokeWidth = thickness;
        drawSingle(SDF_SEGMENT, {x1-cx, y1-cy, x2-cx, y2-cy}, {0,0,0,0}, minX, minY, maxX-minX, maxY-minY);
        strokeWidth = oldStroke;
    }

    // Line drawing - uses current stroke settings
    void drawLine(float x1, float y1, float x2, float y2) {
        float cx = (x1+x2)/2, cy = (y1+y2)/2;
        position = {cx, cy};
        float minX = fminf(x1,x2), minY = fminf(y1,y2);
        float maxX = fmaxf(x1,x2), maxY = fmaxf(y1,y2);
        size = {maxX-minX+1, maxY-minY+1};
        drawSingle(SDF_SEGMENT, {x1-cx, y1-cy, x2-cx, y2-cy}, {0,0,0,0}, minX, minY, maxX-minX, maxY-minY);
    }

    void drawLine(vec2 p1, vec2 p2) {
        drawLine(p1.x, p1.y, p2.x, p2.y);
    }

    // Polyline - connect multiple points
    void drawPolyline(const vec2* points, int count) {
        for (int i = 0; i < count - 1; i++) {
            drawLine(points[i], points[i+1]);
        }
    }

    void drawPolyline(const std::vector<vec2>& points) {
        for (size_t i = 0; i < points.size() - 1; i++) {
            drawLine(points[i], points[i+1]);
        }
    }

    // Closed polygon outline
    void drawPolygon(const vec2* points, int count) {
        for (int i = 0; i < count; i++) {
            drawLine(points[i], points[(i+1) % count]);
        }
    }

    void drawPolygon(const std::vector<vec2>& points) {
        for (size_t i = 0; i < points.size(); i++) {
            drawLine(points[i], points[(i+1) % points.size()]);
        }
    }

    // Line with arrow at the end - single unified shape
    void drawLineWithArrow(vec2 from, vec2 to, cColor col, float strk = 1.0f, float arrowSize = 6.0f) {
        // Calculate arrow direction
        vec2 dir = {to.x - from.x, to.y - from.y};
        float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
        if (len < 0.001f) return;

        // Normalize direction
        dir.x /= len;
        dir.y /= len;

        // Perpendicular vector
        vec2 perp = {-dir.y, dir.x};

        // Arrow head points
        float arrowWidth = arrowSize * 0.6f;
        float halfStrk = strk * 0.5f;

        // Line end point (where arrow base starts)
        vec2 arrowBase = {to.x - dir.x * arrowSize, to.y - dir.y * arrowSize};

        // Build unified path: line body + arrow head
        // Start from bottom-left of line, go around clockwise
        SDFPath path;

        // Line left side (from start)
        path.moveTo(from.x + perp.x * halfStrk, from.y + perp.y * halfStrk);

        // Go to arrow base left
        path.lineTo(arrowBase.x + perp.x * halfStrk, arrowBase.y + perp.y * halfStrk);

        // Arrow left wing
        path.lineTo(arrowBase.x + perp.x * arrowWidth, arrowBase.y + perp.y * arrowWidth);

        // Arrow tip
        path.lineTo(to.x, to.y);

        // Arrow right wing
        path.lineTo(arrowBase.x - perp.x * arrowWidth, arrowBase.y - perp.y * arrowWidth);

        // Go to arrow base right
        path.lineTo(arrowBase.x - perp.x * halfStrk, arrowBase.y - perp.y * halfStrk);

        // Line right side (back to start)
        path.lineTo(from.x - perp.x * halfStrk, from.y - perp.y * halfStrk);

        path.close();
        path.end();

        // Draw as filled shape
        setFill(col);
        setStroke(col, 0);
        drawPath(path, 0, 0);
    }

    // ========== SDF-BASED LINE WITH CAPS ==========
    // Uses pure SDF formula, no path subdivision

    // Generic line with configurable end caps
    // capA, capB: CAP_BUTT (0), CAP_ROUND (1), CAP_ARROW (2)
    void drawLineWithCaps(vec2 from, vec2 to, float thickness, LineCap capA, LineCap capB, float arrowSize = 0.0f)
    {
        float cx = (from.x + to.x) * 0.5f;
        float cy = (from.y + to.y) * 0.5f;
        position = {cx, cy};

        float minX = fminf(from.x, to.x);
        float minY = fminf(from.y, to.y);
        float maxX = fmaxf(from.x, to.x);
        float maxY = fmaxf(from.y, to.y);

        // Add padding for arrow heads and thickness
        float pad = fmaxf(thickness, arrowSize) + 5.0f;
        size = {maxX - minX + pad * 2, maxY - minY + pad * 2};

        // Relative coordinates from center
        vec2 relA = {from.x - cx, from.y - cy};
        vec2 relB = {to.x - cx, to.y - cy};

        // If no arrow size specified, use thickness-based default
        float aSize = (arrowSize > 0.0f) ? arrowSize : thickness * 3.0f;

        if(capA==CAP_ROUND) relA.x += 0.5f*thickness;
        if(capB==CAP_ROUND) relB.x -= 0.5f*thickness;

        // ep.xy = point A, ep.zw = point B
        // ep2.x = thickness, ep2.y = capA, ep2.z = capB, ep2.w = arrowSize
        drawSingle(SDF_ARROW,
                   {relA.x, relA.y, relB.x, relB.y},
                   {thickness, (float)capA, (float)capB, aSize},
                   minX - pad, minY - pad, maxX - minX + pad * 2, maxY - minY + pad * 2);
    }

    // Line with arrows at both ends
    void drawLineWithArrows(vec2 from, vec2 to, cColor col, float thickness = 1.0f, float arrowSize = 6.0f) {
        setFill(col);
        setStroke(col, 0);
        drawLineWithCaps(from, to, thickness, CAP_ARROW, CAP_ARROW, arrowSize);
    }

    // Line with round caps
    void drawLineRound(vec2 from, vec2 to, cColor col, float thickness = 1.0f) {
        setFill(col);
        setStroke(col, 0);
        drawLineWithCaps(from, to, thickness, CAP_ROUND, CAP_ROUND, 0);
    }

    // Line with flat/butt caps
    void drawLineFlat(vec2 from, vec2 to, cColor col, float thickness = 1.0f) {
        setFill(col);
        setStroke(col, 0);
        drawLineWithCaps(from, to, thickness, CAP_BUTT, CAP_BUTT, 0);
    }

    // Arrow pointing from -> to (line + arrow at end)
    void drawArrow(vec2 from, vec2 to, cColor col, float thickness = 2.0f, float arrowSize = 8.0f) {
        setFill(col);
        setStroke(col, 0);
        drawLineWithCaps(from, to, thickness, CAP_ROUND, CAP_ARROW, arrowSize);
    }

    // Double-headed arrow
    void drawDoubleArrow(vec2 from, vec2 to, cColor col, float thickness = 2.0f, float arrowSize = 8.0f) {
        setFill(col);
        setStroke(col, 0);
        drawLineWithCaps(from, to, thickness, CAP_ARROW, CAP_ARROW, arrowSize);
    }

    void drawVesica(float x, float y, float r, float d) {
        position = {x, y}; size = {r*2, r*2};
        drawSingle(SDF_VESICA, {r,d,0,0}, {0,0,0,0}, x-r, y-r, r*2, r*2);
    }

    void drawCutDisk(float x, float y, float r, float h) {
        position = {x, y}; size = {r*2, r*2};
        drawSingle(SDF_CUT_DISK, {r,h,0,0}, {0,0,0,0}, x-r, y-r, r*2, r*2);
    }

    void drawTrapezoid(float x, float y, float r1, float r2, float h) {
        position = {x, y}; float maxR = fmaxf(r1, r2); size = {maxR*2, h*2};
        drawSingle(SDF_TRAPEZOID, {r1,r2,h,0}, {0,0,0,0}, x-maxR, y-h, maxR*2, h*2);
    }

    void drawParallelogram(float x, float y, float w, float h, float skew) {
        position = {x, y}; size = {w*2, h*2};
        drawSingle(SDF_PARALLELOGRAM, {skew,0,0,0}, {0,0,0,0}, x-w-fabsf(skew), y-h, w*2+fabsf(skew)*2, h*2);
    }

    void drawTrianglePoints(float x, float y, Vector2 p0, Vector2 p1, Vector2 p2) {
        position = {x, y};
        float minX = fminf(fminf(p0.x,p1.x),p2.x), maxX = fmaxf(fmaxf(p0.x,p1.x),p2.x);
        float minY = fminf(fminf(p0.y,p1.y),p2.y), maxY = fmaxf(fmaxf(p0.y,p1.y),p2.y);
        size = {maxX-minX, maxY-minY};
        drawSingle(SDF_TRIANGLE, {p0.x,p0.y,p1.x,p1.y}, {p2.x,p2.y,0,0}, x+minX, y+minY, maxX-minX, maxY-minY);
    }

    void drawRoundedCross(float x, float y, float scale, float h) {
        position = {x, y}; size = {scale*2, scale*2};
        drawSingle(SDF_ROUNDED_CROSS, {scale,h,0,0}, {0,0,0,0}, x-scale, y-scale, scale*2, scale*2);
    }

    void drawBlobbyCross(float x, float y, float scale, float h) {
        position = {x, y}; size = {scale*2, scale*2};
        drawSingle(SDF_BLOBBY_CROSS, {scale,h,0,0}, {0,0,0,0}, x-scale, y-scale, scale*2, scale*2);
    }

    void drawTunnel(float x, float y, float w, float h) {
        position = {x, y}; float maxS = fmaxf(w, h); size = {maxS*2, maxS*2};
        drawSingle(SDF_TUNNEL, {w,h,0,0}, {0,0,0,0}, x-maxS, y-maxS, maxS*2, maxS*2);
    }

    void drawCoolS(float x, float y, float scale) {
        position = {x, y}; size = {scale*2, scale*2};
        drawSingle(SDF_COOL_S, {scale,0,0,0}, {0,0,0,0}, x-scale, y-scale, scale*2, scale*2);
    }

    void drawBezier(float x, float y, Vector2 p0, Vector2 p1, Vector2 p2, float thickness) {
        position = {x, y};
        float minX = fminf(fminf(p0.x,p1.x),p2.x), maxX = fmaxf(fmaxf(p0.x,p1.x),p2.x);
        float minY = fminf(fminf(p0.y,p1.y),p2.y), maxY = fmaxf(fmaxf(p0.y,p1.y),p2.y);
        size = {maxX-minX, maxY-minY};
        float oldStroke = strokeWidth; strokeWidth = thickness;
        drawSingle(SDF_BEZIER, {p0.x,p0.y,p1.x,p1.y}, {p2.x,p2.y,0,0}, x+minX, y+minY, maxX-minX, maxY-minY);
        strokeWidth = oldStroke;
    }

    // ========== IMAGE ==========

    Vector2 worldToScreen(float x, float y) {
        return { x * zoom + pan.x, y * zoom + pan.y };
    }

    // ========== KAWASE BLUR ==========

    Shader kawaseDownShader = {0};
    Shader kawaseUpShader = {0};
    bool kawaseLoaded = false;
    int kawaseDownOffsetLoc = -1;
    int kawaseUpOffsetLoc = -1;

    RenderTexture2D blurRT[6] = {0};
    int blurRTSizes[6][2] = {{0}};

    void loadKawaseShaders() {
        if (kawaseLoaded) return;

        const char* kawaseVs = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
out vec2 uv;
uniform mat4 mvp;
void main() { uv = vertexTexCoord; gl_Position = mvp * vec4(vertexPosition, 1.0); }
)";
        const char* kawaseDownFS = R"(#version 330
in vec2 uv;
out vec4 fragColor;
uniform sampler2D texture0;
uniform float offset;
void main() {
    vec2 texel = 1.0 / vec2(textureSize(texture0, 0));
    vec2 o = texel * offset;
    vec4 c = texture(texture0, uv) * 4.0;
    c += texture(texture0, uv + vec2(-o.x, -o.y));
    c += texture(texture0, uv + vec2( o.x, -o.y));
    c += texture(texture0, uv + vec2(-o.x,  o.y));
    c += texture(texture0, uv + vec2( o.x,  o.y));
    fragColor = c / 8.0;
}
)";
        const char* kawaseUpFS = R"(#version 330
in vec2 uv;
out vec4 fragColor;
uniform sampler2D texture0;
uniform float offset;
void main() {
    vec2 texel = 1.0 / vec2(textureSize(texture0, 0));
    vec2 o = texel * offset;
    vec4 c = vec4(0.0);
    c += texture(texture0, uv + vec2(-o.x * 2.0, 0.0));
    c += texture(texture0, uv + vec2(-o.x, -o.y)) * 2.0;
    c += texture(texture0, uv + vec2(0.0, -o.y * 2.0));
    c += texture(texture0, uv + vec2( o.x, -o.y)) * 2.0;
    c += texture(texture0, uv + vec2( o.x * 2.0, 0.0));
    c += texture(texture0, uv + vec2( o.x,  o.y)) * 2.0;
    c += texture(texture0, uv + vec2(0.0,  o.y * 2.0));
    c += texture(texture0, uv + vec2(-o.x,  o.y)) * 2.0;
    fragColor = c / 12.0;
}
)";

        kawaseDownShader = LoadShaderFromMemory(kawaseVs, kawaseDownFS);
        kawaseUpShader = LoadShaderFromMemory(kawaseVs, kawaseUpFS);
        kawaseDownOffsetLoc = GetShaderLocation(kawaseDownShader, "offset");
        kawaseUpOffsetLoc = GetShaderLocation(kawaseUpShader, "offset");
        kawaseLoaded = true;
    }

    void ensureBlurRTs(int w, int h, int levels) {
        if (blurRT[0].id != 0 && blurRTSizes[0][0] >= w && blurRTSizes[0][1] >= h) return;

        for (int i = 0; i < 6; i++) {
            if (blurRT[i].id != 0) { UnloadRenderTexture(blurRT[i]); blurRT[i].id = 0; }
        }

        int rtW = w, rtH = h;
        for (int i = 0; i < levels && i < 6; i++) {
            blurRT[i] = LoadRenderTexture(rtW, rtH);
            SetTextureFilter(blurRT[i].texture, TEXTURE_FILTER_BILINEAR);
            blurRTSizes[i][0] = rtW;
            blurRTSizes[i][1] = rtH;
            rtW = (rtW + 1) / 2;
            rtH = (rtH + 1) / 2;
            if (rtW < 8) rtW = 8;
            if (rtH < 8) rtH = 8;
        }
    }

    void unloadBlur() {
        if (kawaseLoaded) {
            UnloadShader(kawaseDownShader);
            UnloadShader(kawaseUpShader);
            kawaseLoaded = false;
        }
        for (int i = 0; i < 6; i++) {
            if (blurRT[i].id != 0) { UnloadRenderTexture(blurRT[i]); blurRT[i].id = 0; }
        }
    }

    void applyKawaseBlur(int iterations) {
        if (iterations < 1) iterations = 1;
        if (iterations > 5) iterations = 5;
        float offset = 1.0f;

        for (int i = 0; i < iterations; i++) {
            BeginTextureMode(blurRT[i + 1]);
            ClearBackground(BLANK);
            BeginShaderMode(kawaseDownShader);
            SetShaderValue(kawaseDownShader, kawaseDownOffsetLoc, &offset, SHADER_UNIFORM_FLOAT);
            DrawTexturePro(blurRT[i].texture,
                           {0, 0, (float)blurRTSizes[i][0], (float)-blurRTSizes[i][1]},
                           {0, 0, (float)blurRTSizes[i+1][0], (float)blurRTSizes[i+1][1]},
                           {0, 0}, 0, WHITE);
            EndShaderMode();
            EndTextureMode();
        }

        for (int i = iterations; i > 0; i--) {
            BeginTextureMode(blurRT[i - 1]);
            ClearBackground(BLANK);
            BeginShaderMode(kawaseUpShader);
            SetShaderValue(kawaseUpShader, kawaseUpOffsetLoc, &offset, SHADER_UNIFORM_FLOAT);
            DrawTexturePro(blurRT[i].texture,
                           {0, 0, (float)blurRTSizes[i][0], (float)-blurRTSizes[i][1]},
                           {0, 0, (float)blurRTSizes[i-1][0], (float)blurRTSizes[i-1][1]},
                           {0, 0}, 0, WHITE);
            EndShaderMode();
            EndTextureMode();
        }
    }

    void drawImageBlurred(Texture2D texture, float x, float y, float w, float h, int blurIterations = 2, Color tint = WHITE) {
        if (blurIterations < 1) { drawImage(texture, x, y, w, h, tint); return; }
        loadKawaseShaders();
        int rtW = (int)(w * zoom), rtH = (int)(h * zoom);
        ensureBlurRTs(rtW, rtH, blurIterations + 1);

        BeginTextureMode(blurRT[0]);
        ClearBackground(BLANK);
        DrawTexturePro(texture, {0, 0, (float)texture.width, (float)texture.height},
                       {0, 0, (float)rtW, (float)rtH}, {0, 0}, 0, WHITE);
        EndTextureMode();

        applyKawaseBlur(blurIterations);

        Vector2 screenPos = worldToScreen(x, y);
        DrawTexturePro(blurRT[0].texture, {0, 0, (float)rtW, (float)-rtH},
                       {screenPos.x, screenPos.y, (float)rtW, (float)rtH}, {0, 0}, 0, tint);
    }

    void beginBlur(int width, int height, int iterations = 2) {
        loadKawaseShaders();
        ensureBlurRTs(width, height, iterations + 1);
        targetRenderHeight = (float)height;
        cachedScreenHeight = 0;
        savedPan = pan; savedZoom = zoom;
        pan = {0, 0}; zoom = 1.0f;
        cache.pan = {-99999, -99999}; cache.zoom = -1;
        BeginTextureMode(blurRT[0]);
        ClearBackground(BLANK);
    }

    void endBlur(float x, float y, int iterations = 2, Color tint = WHITE) {
        EndTextureMode();
        float z = savedZoom; vec2 p = savedPan;
        pan = savedPan; zoom = savedZoom;
        targetRenderHeight = 0; cachedScreenHeight = 0;
        cache.pan = {-99999, -99999}; cache.zoom = -1;

        applyKawaseBlur(iterations);

        Vector2 screenPos = { x * z + p.x, y * z + p.y };
        int rtW = blurRTSizes[0][0], rtH = blurRTSizes[0][1];
        DrawTexturePro(blurRT[0].texture, {0, 0, (float)rtW, (float)-rtH},
                       {screenPos.x, screenPos.y, (float)rtW, (float)rtH}, {0, 0}, 0, tint);
    }

    // ========== MASK SYSTEM ==========

    RenderTexture2D maskRT = {0};
    int maskRTW = 0, maskRTH = 0;
    Shader maskShader = {0};
    bool maskShaderLoaded = false;
    int maskTexLoc = -1, maskInvertLoc = -1, maskSoftLoc = -1;
    int maskUVOffsetLoc = -1, maskUVScaleLoc = -1, maskRTScaleLoc = -1;

    void loadMaskShader() {
        if (maskShaderLoaded) return;
        const char* vs = R"(#version 330
in vec3 vertexPosition; in vec2 vertexTexCoord; in vec4 vertexColor;
out vec2 fragTexCoord; out vec4 fragColor; uniform mat4 mvp;
void main() { fragTexCoord = vertexTexCoord; fragColor = vertexColor; gl_Position = mvp * vec4(vertexPosition, 1.0); })";

        const char* fs = R"(#version 330
in vec2 fragTexCoord; in vec4 fragColor; out vec4 finalColor;
uniform sampler2D texture0; uniform sampler2D texture1;
uniform float invert; uniform float softness;
uniform vec2 maskUVOffset; uniform vec2 maskUVScale;
uniform vec2 maskRTScale;
void main() {
    vec4 imgColor = texture(texture0, fragTexCoord) * fragColor;
    vec2 normUV = (fragTexCoord - maskUVOffset) / maskUVScale;
    vec2 maskUV = vec2(normUV.x * maskRTScale.x, 1.0 - normUV.y * maskRTScale.y);
    vec4 maskColor = texture(texture1, maskUV);
    float lumaVal = maskColor.r;
    float alphaVal = maskColor.a;
    float maskVal = maskColor.r;

    if (softness > 0.0) maskVal = smoothstep(0.5 - softness, 0.5 + softness, maskVal);
    if (invert > 0.5) maskVal = 1.0 - maskVal;
    finalColor = vec4(imgColor.rgb * maskVal, imgColor.a*maskVal);
})";
        maskShader = LoadShaderFromMemory(vs, fs);
        maskTexLoc = GetShaderLocation(maskShader, "texture1");
        maskInvertLoc = GetShaderLocation(maskShader, "invert");
        maskSoftLoc = GetShaderLocation(maskShader, "softness");
        maskUVOffsetLoc = GetShaderLocation(maskShader, "maskUVOffset");
        maskUVScaleLoc = GetShaderLocation(maskShader, "maskUVScale");
        maskRTScaleLoc = GetShaderLocation(maskShader, "maskRTScale");
        maskShaderLoaded = true;
    }

    void ensureMaskRT(int w, int h) {
        if (maskRT.id == 0 || maskRTW < w || maskRTH < h) {
            if (maskRT.id != 0) UnloadRenderTexture(maskRT);
            maskRTW = w; maskRTH = h;
            maskRT = LoadRenderTexture(w, h);
            SetTextureFilter(maskRT.texture, TEXTURE_FILTER_BILINEAR);
            SetTextureWrap(maskRT.texture, TEXTURE_WRAP_MIRROR_CLAMP);  // ← add
            axLog("Created mask RT of size %dx%d", w, h);
        }
    }

    void unloadMask() {
        if (maskShaderLoaded) { UnloadShader(maskShader); maskShaderLoaded = false; }
        if (maskRT.id != 0) { UnloadRenderTexture(maskRT); maskRT.id = 0; }
    }

    int currentMaskHeight = 0;
    int currentMaskWidth = 0;
    vec2 savedPan = {0, 0};
    float savedZoom = 1.0f;

    void beginMask(int width, int height) {
        ensureMaskRT(width, height);
        currentMaskWidth = width;
        currentMaskHeight = height;
        targetRenderHeight = (float)maskRTH;
        cachedScreenHeight = 0;
        savedPan = pan; savedZoom = zoom;
        pan = {0, 0}; zoom = 1.0f;
        cache.pan = {-99999, -99999}; cache.zoom = -1;
        BeginTextureMode(maskRT);
        ClearBackground(BLACK);
        rlSetBlendFactors(1, 0x0303, 0x0304);  // GL_ONE, GL_ONE_MINUS_SRC_ALPHA
        rlSetBlendMode(BLEND_CUSTOM);           // premultiplied alpha blending
    }

    void endMask() {
        EndTextureMode();
        rlSetBlendMode(BLEND_ALPHA_PREMULTIPLY);
        targetRenderHeight = 0; cachedScreenHeight = 0;
        pan = savedPan; zoom = savedZoom;
        cache.pan = {-99999, -99999}; cache.zoom = -1;
        rlDrawRenderBatchActive();
    }

    // Apply Kawase blur to the mask RT for feather/soft edge effect
    // featherAmount: 0-100, maps to blur iterations and offset
    void blurMask(float featherAmount) {
        if (featherAmount < 0.1f || maskRT.id == 0) return;

        loadKawaseShaders();

        // Map feather amount to iterations (1-5) based on strength
        int iterations = (int)(featherAmount / 20.0f) + 1;
        if (iterations > 5) iterations = 5;

        // Use blur RTs for ping-pong, starting from maskRT
        ensureBlurRTs(maskRTW, maskRTH, iterations + 1);

        // Copy maskRT to blurRT[0]
        BeginTextureMode(blurRT[0]);
        ClearBackground(BLANK);
        DrawTexturePro(maskRT.texture,
                       {0, 0, (float)maskRTW, (float)-maskRTH},
                       {0, 0, (float)blurRTSizes[0][0], (float)blurRTSizes[0][1]},
                       {0, 0}, 0, WHITE);
        EndTextureMode();

        // Apply Kawase blur
        applyKawaseBlur(iterations);

        // Copy blurred result back to maskRT
        BeginTextureMode(maskRT);
        ClearBackground(BLANK);
        DrawTexturePro(blurRT[0].texture,
                       {0, 0, (float)blurRTSizes[0][0], (float)-blurRTSizes[0][1]},
                       {0, 0, (float)maskRTW, (float)maskRTH},
                       {0, 0}, 0, WHITE);
        EndTextureMode();
        rlDrawRenderBatchActive();
    }

    Texture2D getMaskTexture() { return maskRT.texture; }

    void drawImageMasked(Texture2D texture, float x, float y, float w, float h,
                         bool invert = false, float softness = 0.0f, Color tint = WHITE) {
        loadMaskShader();

        Rectangle src = { 0, 0, (float)texture.width, (float)texture.height };
        Rectangle dst;
        float drawRot = 0;
        Vector2 origin = {0, 0};

        if (useMatrix) {
            ImageDrawParams p = imageDrawParams(x, y, w, h);
            dst = { p.cx, p.cy, p.w, p.h };
            origin = { p.w * 0.5f, p.h * 0.5f };
            drawRot = p.rot;
        } else {
            Vector2 screenPos = worldToScreen(x, y);
            dst = { screenPos.x, screenPos.y, w * zoom, h * zoom };
        }

        float inv = invert ? 1.0f : 0.0f;
        SetShaderValue(maskShader, maskInvertLoc, &inv, SHADER_UNIFORM_FLOAT);
        SetShaderValue(maskShader, maskSoftLoc, &softness, SHADER_UNIFORM_FLOAT);
        // Default: no UV remapping (fragTexCoord 0→1 maps directly to mask 0→1)
        Vector2 uvOff = {0.0f, 0.0f}; Vector2 uvScl = {1.0f, 1.0f};
        float maskScaleX = (maskRTW > 0) ? ((float)currentMaskWidth / (float)maskRTW) : 1.0f;
        float maskScaleY = (maskRTH > 0) ? ((float)currentMaskHeight / (float)maskRTH) : 1.0f;
        Vector2 maskRTScale = { maskScaleX, maskScaleY };
        SetShaderValue(maskShader, maskUVOffsetLoc, &uvOff, SHADER_UNIFORM_VEC2);
        SetShaderValue(maskShader, maskUVScaleLoc, &uvScl, SHADER_UNIFORM_VEC2);
        SetShaderValue(maskShader, maskRTScaleLoc, &maskRTScale, SHADER_UNIFORM_VEC2);
        rlActiveTextureSlot(1); rlEnableTexture(maskRT.texture.id); rlActiveTextureSlot(0);
        int slot = 1; SetShaderValue(maskShader, maskTexLoc, &slot, SHADER_UNIFORM_INT);
        rlPushMatrix();
        rlLoadIdentity();
        BeginShaderMode(maskShader);
        DrawTexturePro(texture, src, dst, origin, drawRot, tint);
        EndShaderMode();
        rlPopMatrix();
        rlActiveTextureSlot(1); rlDisableTexture(); rlActiveTextureSlot(0);
    }

    void drawImageMasked(Texture2D texture, float x, float y, float w, float h,
                         Texture2D mask, bool invert = false, float softness = 0.0f, Color tint = WHITE) {
        loadMaskShader();

        Rectangle src = { 0, 0, (float)texture.width, (float)texture.height };
        Rectangle dst;
        float drawRot = 0;
        Vector2 origin = {0, 0};

        if (useMatrix) {
            ImageDrawParams p = imageDrawParams(x, y, w, h);
            dst = { p.cx, p.cy, p.w, p.h };
            origin = { p.w * 0.5f, p.h * 0.5f };
            drawRot = p.rot;
        } else {
            Vector2 screenPos = worldToScreen(x, y);
            dst = { screenPos.x, screenPos.y, w * zoom, h * zoom };
        }

        float inv = invert ? 1.0f : 0.0f;
        SetShaderValue(maskShader, maskInvertLoc, &inv, SHADER_UNIFORM_FLOAT);
        SetShaderValue(maskShader, maskSoftLoc, &softness, SHADER_UNIFORM_FLOAT);
        // Default: no UV remapping
        Vector2 uvOff = {0.0f, 0.0f}; Vector2 uvScl = {1.0f, 1.0f};
        Vector2 maskRTScale = {1.0f, 1.0f}; // External mask: full texture used
        SetShaderValue(maskShader, maskUVOffsetLoc, &uvOff, SHADER_UNIFORM_VEC2);
        SetShaderValue(maskShader, maskUVScaleLoc, &uvScl, SHADER_UNIFORM_VEC2);
        SetShaderValue(maskShader, maskRTScaleLoc, &maskRTScale, SHADER_UNIFORM_VEC2);
        rlActiveTextureSlot(1); rlEnableTexture(mask.id); rlActiveTextureSlot(0);
        int slot = 1; SetShaderValue(maskShader, maskTexLoc, &slot, SHADER_UNIFORM_INT);
        rlPushMatrix();
        rlLoadIdentity();
        BeginShaderMode(maskShader);
        DrawTexturePro(texture, src, dst, origin, drawRot, tint);
        EndShaderMode();
        rlPopMatrix();
        rlActiveTextureSlot(1); rlDisableTexture(); rlActiveTextureSlot(0);
    }

    // Masked image draw with custom source rectangle (supports Y-flip for RenderTexture2D)
    void drawImageMaskedRect(Texture2D texture, Rectangle srcRect, float x, float y, float w, float h,
                             bool invert = false, float softness = 0.0f, Color tint = WHITE) {
        loadMaskShader();

        Rectangle dst;
        float drawRot = 0;
        Vector2 origin = {0, 0};

        if (useMatrix) {
            ImageDrawParams p = imageDrawParams(x, y, w, h);
            dst = { p.cx, p.cy, p.w, p.h };
            origin = { p.w * 0.5f, p.h * 0.5f };
            drawRot = p.rot;
        } else {
            Vector2 screenPos = worldToScreen(x, y);
            dst = { screenPos.x, screenPos.y, w * zoom, h * zoom };
        }

        float inv = invert ? 1.0f : 0.0f;
        SetShaderValue(maskShader, maskInvertLoc, &inv, SHADER_UNIFORM_FLOAT);
        SetShaderValue(maskShader, maskSoftLoc, &softness, SHADER_UNIFORM_FLOAT);

        // Compute UV remapping: fragTexCoord is in image texture UV space
        // (srcRect.x/texW → (srcRect.x+srcRect.w)/texW), but mask is in 0→1 space.
        // normUV = (fragTexCoord - uvOffset) / uvScale maps image UV → mask UV
        float texW = (float)texture.width;
        float texH = (float)texture.height;
        Vector2 uvOff = { srcRect.x / texW, srcRect.y / texH };
        Vector2 uvScl = { fabsf(srcRect.width) / texW, fabsf(srcRect.height) / texH };

        // When mask RT is larger than actual mask content (grow-only allocation),
        // scale mask UVs so that the mask content maps to [0, contentSize/rtSize]
        // instead of [0, 1]. The shader's normUV maps to [0,1], so we need to
        // compress it into the used portion of the RT.
        float maskScaleX = (maskRTW > 0) ? ((float)currentMaskWidth / (float)maskRTW) : 1.0f;
        float maskScaleY = (maskRTH > 0) ? ((float)currentMaskHeight / (float)maskRTH) : 1.0f;
        Vector2 maskRTScale = { maskScaleX, maskScaleY };
        SetShaderValue(maskShader, maskUVOffsetLoc, &uvOff, SHADER_UNIFORM_VEC2);
        SetShaderValue(maskShader, maskUVScaleLoc, &uvScl, SHADER_UNIFORM_VEC2);
        SetShaderValue(maskShader, maskRTScaleLoc, &maskRTScale, SHADER_UNIFORM_VEC2);

        rlActiveTextureSlot(1); rlEnableTexture(maskRT.texture.id); rlActiveTextureSlot(0);
        int slot = 1; SetShaderValue(maskShader, maskTexLoc, &slot, SHADER_UNIFORM_INT);
        rlPushMatrix();
        rlLoadIdentity();
        BeginShaderMode(maskShader);
        DrawTexturePro(texture, srcRect, dst, origin, drawRot, tint);
        EndShaderMode();
        rlPopMatrix();
        rlActiveTextureSlot(1); rlDisableTexture(); rlActiveTextureSlot(0);
    }

    void drawImageCircleMask(Texture2D texture, float x, float y, float w, float h,
                             float cx, float cy, float radius, bool invert = false, Color tint = WHITE) {
        if (w < 1 || h < 1) return;

        // Save current matrix state
        bool oldMatrix = useMatrix;
        Matrix oldTransform = transformMatrix;
        Matrix oldInverse = inverseMatrix;

        // Draw mask in local space (no matrix, no pan/zoom)
        resetMatrix();
        beginMask((int)w, (int)h);
        cColor oldFill = fillColor; fillColor = cWhite;
        drawCircle(cx - x, cy - y, radius);
        fillColor = oldFill;
        endMask();

        // Restore matrix state for image drawing
        useMatrix = oldMatrix;
        transformMatrix = oldTransform;
        inverseMatrix = oldInverse;

        drawImageMasked(texture, x, y, w, h, invert, 0.0f, tint);
    }

    void drawImageRoundedMask(Texture2D texture, float x, float y, float w, float h,
                              float radius, bool invert = false, Color tint = WHITE) {
        if (w < 1 || h < 1) return;

        // Save current matrix state
        bool oldMatrix = useMatrix;
        Matrix oldTransform = transformMatrix;
        Matrix oldInverse = inverseMatrix;

        // Draw mask in local space (no matrix, no pan/zoom) — same pattern as ImageNode
        resetMatrix();
        beginMask((int)w, (int)h);
        cColor oldFill = fillColor; fillColor = cWhite;
        drawRoundedBox(0, 0, w, h, radius);
        fillColor = oldFill;
        endMask();

        // Restore matrix state for image drawing
        useMatrix = oldMatrix;
        transformMatrix = oldTransform;
        inverseMatrix = oldInverse;

        drawImageMasked(texture, x, y, w, h, invert, 0.0f, tint);
    }

    // ========== IMAGE FUNCTIONS (Matrix-Aware) ==========

    // Helper: get rotation angle from current matrix (radians)
    float getMatrixRotation() {
        if (!useMatrix) return 0;
        return atan2f(transformMatrix.m1, transformMatrix.m0);
    }

    // Helper: get scale from current matrix
    Vector2 getMatrixScale() {
        if (!useMatrix) return {1, 1};
        float sx = sqrtf(transformMatrix.m0 * transformMatrix.m0 + transformMatrix.m1 * transformMatrix.m1);
        float sy = sqrtf(transformMatrix.m4 * transformMatrix.m4 + transformMatrix.m5 * transformMatrix.m5);
        return {sx, sy};
    }

    // Helper: compute screen center + screen size for a local-space rect,
    // using the exact same effectiveMatrix logic as drawSingle.
    // Returns {screenCenterX, screenCenterY, dstW, dstH}
    Vector4 imageScreenRect(float x, float y, float w, float h) {
        float cx = x + w * 0.5f;
        float cy = y + h * 0.5f;

        if (!useMatrix) {
            return {
                cx * zoom + pan.x,
                cy * zoom + pan.y,
                w * zoom,
                h * zoom
            };
        }

        // Same as drawSingle: effectiveMatrix = Translate(cx,cy) * transformMatrix
        Matrix posMatrix = MatrixTranslate(cx, cy, 0);
        Matrix eff = MatrixMultiply(posMatrix, transformMatrix);

        // Screen center = effective origin
        float scrX = eff.m12 * zoom + pan.x;
        float scrY = eff.m13 * zoom + pan.y;

        // Screen size: transform local corners through effective matrix, take AABB
        float hw = w * 0.5f, hh = h * 0.5f;
        float corners[4][2] = {{-hw,-hh},{hw,-hh},{-hw,hh},{hw,hh}};
        float minX = 1e10f, maxX = -1e10f, minY = 1e10f, maxY = -1e10f;
        for (int i = 0; i < 4; i++) {
            float wx = eff.m0 * corners[i][0] + eff.m4 * corners[i][1] + eff.m12;
            float wy = eff.m1 * corners[i][0] + eff.m5 * corners[i][1] + eff.m13;
            minX = fminf(minX, wx); maxX = fmaxf(maxX, wx);
            minY = fminf(minY, wy); maxY = fmaxf(maxY, wy);
        }
        return { scrX, scrY, (maxX - minX) * zoom, (maxY - minY) * zoom };
    }

    // Helper: compute screen position, unrotated size, and rotation for DrawTexturePro.
    // Unlike imageScreenRect (which returns AABB), this returns the actual drawable params.
    // Returns: screenCenterX, screenCenterY, scaledW, scaledH  (unrotated dimensions)
    // rotation is returned separately
    struct ImageDrawParams {
        float cx, cy;    // screen center
        float w, h;      // screen size (unrotated, scaled)
        float rot;       // rotation in degrees
    };

    ImageDrawParams imageDrawParams(float x, float y, float w, float h) {
        float lcx = x + w * 0.5f;
        float lcy = y + h * 0.5f;

        if (!useMatrix) {
            return {
                lcx * zoom + pan.x,
                lcy * zoom + pan.y,
                w * zoom,
                h * zoom,
                0.0f
            };
        }

        // Effective matrix = Translate(localCenter) * transformMatrix
        Matrix posMatrix = MatrixTranslate(lcx, lcy, 0);
        Matrix eff = MatrixMultiply(posMatrix, transformMatrix);

        // Screen center
        float scrX = eff.m12 * zoom + pan.x;
        float scrY = eff.m13 * zoom + pan.y;

        // Extract scale from matrix (not from effective - effective includes local offset)
        float sx = sqrtf(transformMatrix.m0 * transformMatrix.m0 + transformMatrix.m1 * transformMatrix.m1);
        float sy = sqrtf(transformMatrix.m4 * transformMatrix.m4 + transformMatrix.m5 * transformMatrix.m5);

        // Extract rotation
        float rot = atan2f(transformMatrix.m1, transformMatrix.m0) * RAD2DEG;

        return { scrX, scrY, w * sx * zoom, h * sy * zoom, rot };
    }

    void drawImage(Texture2D texture, float x, float y) {
        drawImage(texture, x, y, (float)texture.width, (float)texture.height, WHITE);
    }

    void drawImage(Texture2D texture, float x, float y, float w, float h, Color tint = WHITE) {
        EndShaderMode();
        rlSetBlendMode(BLEND_ALPHA);
        rlPushMatrix();
        rlLoadIdentity();
        if (useMatrix) {
            ImageDrawParams p = imageDrawParams(x, y, w, h);
            DrawTexturePro(texture,
                           {0, 0, (float)texture.width, (float)texture.height},
                           {p.cx, p.cy, p.w, p.h},
                           {p.w * 0.5f, p.h * 0.5f},
                           p.rot, tint);
        } else {
            Vector2 screenPos = worldToScreen(x, y);
            DrawTexturePro(texture, {0, 0, (float)texture.width, (float)texture.height},
                           {screenPos.x, screenPos.y, w * zoom, h * zoom}, {0,0}, 0, tint);
        }
        rlPopMatrix();
    }

    void drawImageEx(Texture2D texture, float x, float y, float w, float h, float rot, Vector2 origin = {0,0}, Color tint = WHITE) {
        EndShaderMode();
        rlSetBlendMode(BLEND_ALPHA);
        rlPushMatrix();
        rlLoadIdentity();
        if (useMatrix) {
            ImageDrawParams p = imageDrawParams(x, y, w, h);
            DrawTexturePro(texture, {0, 0, (float)texture.width, (float)texture.height},
                           {p.cx, p.cy, p.w, p.h},
                           {origin.x / w * p.w, origin.y / h * p.h},
                           p.rot + rot * RAD2DEG, tint);
        } else {
            Vector2 screenPos = worldToScreen(x, y);
            DrawTexturePro(texture, {0, 0, (float)texture.width, (float)texture.height},
                           {screenPos.x, screenPos.y, w * zoom, h * zoom},
                           {origin.x * zoom, origin.y * zoom}, rot * RAD2DEG, tint);
        }
        rlPopMatrix();
    }

    void drawImageRect(Texture2D texture, Rectangle srcRect, float x, float y, float w, float h, Color tint = WHITE) {
        EndShaderMode();
        rlSetBlendMode(BLEND_ALPHA);
        rlPushMatrix();
        rlLoadIdentity();
        if (useMatrix) {
            ImageDrawParams p = imageDrawParams(x, y, w, h);
            DrawTexturePro(texture, srcRect,
                           {p.cx, p.cy, p.w, p.h},
                           {p.w * 0.5f, p.h * 0.5f},
                           p.rot, tint);
        } else {
            Vector2 screenPos = worldToScreen(x, y);
            DrawTexturePro(texture, srcRect, {screenPos.x, screenPos.y, w * zoom, h * zoom}, {0,0}, 0, tint);
        }
        rlPopMatrix();
    }

    void drawImageFlip(Texture2D texture, float x, float y, float w, float h, bool flipX = false, bool flipY = false, Color tint = WHITE) {
        float srcW = flipX ? -(float)texture.width : (float)texture.width;
        float srcH = flipY ? -(float)texture.height : (float)texture.height;
        drawImageRect(texture, {0, 0, srcW, srcH}, x, y, fabsf(w), fabsf(h), tint);
    }

    // ================================================================
    // BATCH RENDERING SYSTEM
    // ================================================================
private:
    bool batchMode = false;
    TileBatchSystem* tileBatch = nullptr;

public:
    // Batch API
    void beginBatch();
    void endBatch();

    // Check if batch mode is active
    bool isBatchMode() const { return batchMode; }

    // Get batch stats
    int getBatchDrawCalls() const;
    int getBatchShapesRendered() const;
    int getBatchShapesCulled() const;

    // Legacy aliases (deprecated - use beginBatch/endBatch)
    void beginAutoBatch() { beginBatch(); }
    void endAutoBatch() { endBatch(); }
    bool isAutoBatchMode() const { return isBatchMode(); }
    int getAutoBatchDrawCalls() const { return getBatchDrawCalls(); }
    int getAutoBatchShapesRendered() const { return getBatchShapesRendered(); }
    int getAutoBatchShapesCulled() const { return getBatchShapesCulled(); }
};

#include "TileBatchSystem.h"

// ================================================================
// Implementation of TileBatchSystem-dependent functions
// ================================================================

inline void SdfRenderer2D::autoBatchAddShapeInternal(SDFShape type, Vector4 p, Vector4 p2) {
    if (tileBatch) {
        tileBatch->addShape(type, position.x, position.y, size.x, size.y,
                            p.x, p.y, p.z, p.w, p2.x, p2.y, p2.z, p2.w);
    }
}

inline void SdfRenderer2D::beginBatch() {
    if (!tileBatch) {
        tileBatch = new TileBatchSystem();
        tileBatch->init(this);
    }

    tileBatch->begin();
    batchMode = true;
}

inline void SdfRenderer2D::endBatch() {
    if (batchMode && tileBatch) {

        rlPushMatrix();
        rlLoadIdentity();
        tileBatch->end();
        batchMode = false;
        rlPopMatrix();
    }
}

inline int SdfRenderer2D::getBatchDrawCalls() const {
    return tileBatch ? tileBatch->totalDrawCalls : 0;
}

inline int SdfRenderer2D::getBatchShapesRendered() const {
    return tileBatch ? tileBatch->shapesRendered : 0;
}

inline int SdfRenderer2D::getBatchShapesCulled() const {
    return tileBatch ? tileBatch->shapesCulled : 0;
}
