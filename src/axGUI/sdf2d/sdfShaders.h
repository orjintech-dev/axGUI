#ifndef SDFSHADERS_H
#define SDFSHADERS_H
// #include "raylib.h"
#include "rlgl.h"
// #include <cmath>
// #include <cstdio>
#include <cstdlib>
// #include <algorithm>
// #include <vector>
// #include "axDefs.h"

// ===== Shader kodu =====
static const char* sdfShaderFS = R"(
#version 330

#define MAX_BATCH 32
#define MAX_PATH_POINTS 512

// ============================================================================
// OPTIMIZATION CONSTANTS
// ============================================================================
#define EPSILON 0.001
#define INF_DIST 1e10
#define BEZIER_MIN_STEPS 3
#define BEZIER_MAX_STEPS 16
#define BEZIER_FLATNESS 1.0

// Transform
uniform vec2 pan;
uniform float zoom;
uniform float screenHeight;

// Single shape mode uniforms
uniform vec2 shapePos;
uniform vec2 shapeSize;
uniform float shapeRotation;
uniform int shapeType;
uniform vec4 shapeParams;
uniform vec4 shapeParams2;

// Matrix Transform (3x3 packed as 3 vec3)
// Supports: translate, rotate, scale, skew, and any combination
uniform int useMatrix;          // 0 = use pos/rotation, 1 = use matrix
uniform vec3 transformCol0;     // Column 0 of transform matrix
uniform vec3 transformCol1;     // Column 1 of transform matrix
uniform vec3 transformCol2;     // Column 2 of transform matrix (translation)
uniform vec3 inverseCol0;       // Column 0 of inverse matrix
uniform vec3 inverseCol1;       // Column 1 of inverse matrix
uniform vec3 inverseCol2;       // Column 2 of inverse matrix (translation)

uniform vec4 fillColor;
uniform vec4 strokeColor;
uniform float strokeWidth;
uniform float strokeSoftness;
uniform int strokeMode;
uniform int strokeStyle;
uniform float strokeTaper;
uniform float strokeWobble;
uniform float strokeWobbleFreq;
uniform vec4 strokeColor2;
uniform vec4 strokeColor3;
uniform vec2 strokeOffset2;
uniform float strokeSpacing;
uniform float strokeNoise;
uniform float strokePressure;
uniform float brushAngle;
uniform float brushRatio;
uniform float animTime;

uniform vec4 glowColor;
uniform float glowSize;
uniform float glowIntensity;
uniform int glowMode;
uniform vec4 shadowColor;
uniform vec2 shadowOffset;
uniform float shadowBlur;
uniform int gradientType;
uniform vec4 gradientColor1;
uniform vec4 gradientColor2;
uniform vec2 gradientStart;
uniform vec2 gradientEnd;

// Deformation
uniform int deformType;
uniform vec4 deformParams;

// 2.5D Lighting
uniform int enable25D;
uniform float extrudeRadius;
uniform vec3 lightDir;
uniform float ambientStrength;
uniform float diffuseStrength;
uniform float specularStrength;
uniform float specularPower;
uniform float fresnelStrength;
uniform float fresnelIOR;

// Batch mode - EXTENDED
uniform int batchMode;           // 0=single, 1=basic batch, 2=extended batch
uniform int batchCount;
uniform vec4 batchPosSize[MAX_BATCH];
uniform vec4 batchParams[MAX_BATCH];      // params1: p1, p2, p3, p4 (ep.xyzw)
uniform vec4 batchParams2[MAX_BATCH];     // params2: p5, p6, p7, p8 (ep2.xyzw) - NEW!
uniform vec2 batchTypeRot[MAX_BATCH];     // type, rotation per shape - NEW!
uniform vec4 batchFillColor[MAX_BATCH];
uniform vec4 batchStrokeColor[MAX_BATCH];
uniform vec4 batchGlowColor[MAX_BATCH];
uniform vec4 batchEffects[MAX_BATCH];     // strokeWidth, glowSize, glowIntensity, glowMode
uniform vec4 batchEffects2[MAX_BATCH];    // strokeSoftness, strokeMode, reserved, reserved

// SVG Path uniforms
uniform int pathMode;
uniform int pathPointCount;
uniform int pathFillRule;
uniform vec4 pathData[MAX_PATH_POINTS];
uniform vec4 pathBounds;  // x, y, width, height - for early culling

// Dash stroke uniforms - PACKED for register efficiency
// dashParams.x = dashLength, dashParams.y = dashGap, dashParams.z = dashOffset, dashParams.w = enabled (0/1)
// dashParams2.x = capStyle (0=butt,1=round,2=square), dashParams2.y = customEnabled (0/1), dashParams2.zw = reserved
uniform vec4 dashParams;
uniform vec4 dashParams2;
uniform vec4 dashPattern;        // Custom pattern: (dash1, gap1, dash2, gap2) - max 2 pairs

// Dash parameter accessors
#define dashLength dashParams.x
#define dashGap dashParams.y
#define dashOffset dashParams.z
#define dashEnabled int(dashParams.w)
#define dashCapStyle int(dashParams2.x)
#define dashCustomEnabled int(dashParams2.y)

out vec4 finalColor;

#define AA_WIDTH 1.5

// ============================================================================
// Matrix Transform Helpers
// ============================================================================

// Build mat3 from column vectors
mat3 buildTransformMatrix() {
    return mat3(transformCol0, transformCol1, transformCol2);
}

mat3 buildInverseMatrix() {
    return mat3(inverseCol0, inverseCol1, inverseCol2);
}

// Apply inverse transform for SDF evaluation (world -> local)
vec2 applyInverseTransform(vec2 worldP) {
    if (useMatrix == 0) {
        // Legacy: position + rotation
        vec2 p = worldP - shapePos;
        float c = cos(shapeRotation), s = sin(shapeRotation);
        return mat2(c, -s, s, c) * p;
    }
    // Matrix transform: apply inverse
    mat3 invMat = buildInverseMatrix();
    vec3 p3 = invMat * vec3(worldP, 1.0);
    return p3.xy;
}

// Get scale factor from matrix (for stroke width adjustment)
// float getMatrixScale() {
//     if (useMatrix == 0) return 1.0;
//     // Extract scale from matrix (average of x and y scale)
//     float sx = length(vec2(transformMatrix[0][0], transformMatrix[1][0]));
//     float sy = length(vec2(transformMatrix[0][1], transformMatrix[1][1]));
//     return (sx + sy) * 0.5;
// }
float getMatrixScale() {
    if (useMatrix == 0) return 1.0;
    // transformCol0 = first column = X-axis after transform
    // transformCol1 = second column = Y-axis after transform
    float sx = length(transformCol0.xy);  // Length of X-axis
    float sy = length(transformCol1.xy);  // Length of Y-axis
    return (sx + sy) * 0.5;
}

// Transform direction vector (for gradients, etc.)
// vec2 transformDirection(vec2 dir) {
//     if (useMatrix == 0) {
//         float c = cos(shapeRotation), s = sin(shapeRotation);
//         return mat2(c, s, -s, c) * dir;
//     }
//     // Use upper-left 2x2 of matrix (rotation/scale only, no translation)
//     return vec2(
//         transformMatrix[0][0] * dir.x + transformMatrix[0][1] * dir.y,
//         transformMatrix[1][0] * dir.x + transformMatrix[1][1] * dir.y
//     );
// }
vec2 transformDirection(vec2 dir) {
    if (useMatrix == 0) {
        float c = cos(shapeRotation), s = sin(shapeRotation);
        return mat2(c, s, -s, c) * dir;
    }
    // To transform direction (no translation): col0 * dir.x + col1 * dir.y
    return transformCol0.xy * dir.x + transformCol1.xy * dir.y;
}

// ============================================================================
// Path State Variables
// ============================================================================

vec2 _pathStart;
vec2 _pathCurrent;
float _pathDist;
float _pathWinding;
float _pathArcLength;
float _pathClosestArcPos;
float _pathTotalLength;

// ============================================================================
// Core Functions
// ============================================================================

// Flatness test - determines if curve is flat enough to approximate with line
float bezierFlatness(vec2 a, vec2 b, vec2 c, vec2 d) {
    vec2 ad = d - a;
    float len = length(ad);
    if (len < EPSILON) return 0.0;
    vec2 n = vec2(-ad.y, ad.x) / len;
    float d1 = abs(dot(b - a, n));
    float d2 = abs(dot(c - a, n));
    return max(d1, d2);
}

float quadBezierFlatness(vec2 a, vec2 b, vec2 c) {
    vec2 ac = c - a;
    float len = length(ac);
    if (len < EPSILON) return 0.0;
    vec2 n = vec2(-ac.y, ac.x) / len;
    return abs(dot(b - a, n));
}

float sdSegment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

// Segment with arc length tracking - returns (distance, arcPosition)
vec2 sdSegmentArc(vec2 p, vec2 a, vec2 b, float arcStart) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    float dist = length(pa - ba * h);
    float segLen = length(ba);
    float arcPos = arcStart + h * segLen;
    return vec2(dist, arcPos);
}

// Inline bezier point calculations
vec2 cubicBezierPoint(vec2 a, vec2 b, vec2 c, vec2 d, float t) {
    float s = 1.0 - t;
    float s2 = s * s;
    float t2 = t * t;
    return s2 * s * a + 3.0 * s2 * t * b + 3.0 * s * t2 * c + t2 * t * d;
}

vec2 quadBezierPoint(vec2 a, vec2 b, vec2 c, float t) {
    float s = 1.0 - t;
    return s * s * a + 2.0 * s * t * b + t * t * c;
}

// ============================================================================
// ANALYTIC BEZIER SDF - No subdivision needed!
// Based on Inigo Quilez's work: https://iquilezles.org/articles/distfunctions2d/
// ============================================================================

// Analytic SDF for Quadratic Bezier - solves cubic equation
float sdBezierAnalytic(vec2 pos, vec2 A, vec2 B, vec2 C) {
    vec2 a = B - A;
    vec2 b = A - 2.0*B + C;
    vec2 c = a * 2.0;
    vec2 d = A - pos;

    float kk = 1.0/dot(b,b);
    float kx = kk * dot(a,b);
    float ky = kk * (2.0*dot(a,a)+dot(d,b)) / 3.0;
    float kz = kk * dot(d,a);

    float res = 0.0;
    float p = ky - kx*kx;
    float p3 = p*p*p;
    float q = kx*(2.0*kx*kx - 3.0*ky) + kz;
    float h = q*q + 4.0*p3;

    if (h >= 0.0) {
        h = sqrt(h);
        vec2 x = (vec2(h,-h)-q)/2.0;
        vec2 uv = sign(x)*pow(abs(x), vec2(1.0/3.0));
        float t = clamp(uv.x+uv.y-kx, 0.0, 1.0);
        res = dot(d+(c+b*t)*t, d+(c+b*t)*t);
    } else {
        float z = sqrt(-p);
        float v = acos(q/(p*z*2.0)) / 3.0;
        float m = cos(v);
        float n = sin(v)*1.732050808;
        vec3 t = clamp(vec3(m+m,-n-m,n-m)*z-kx, 0.0, 1.0);
        res = min(dot(d+(c+b*t.x)*t.x, d+(c+b*t.x)*t.x),
                  dot(d+(c+b*t.y)*t.y, d+(c+b*t.y)*t.y));
    }
    return sqrt(res);
}

// Analytic SDF for Cubic Bezier - uses iterative refinement but much faster
float sdCubicBezierAnalytic(vec2 pos, vec2 A, vec2 B, vec2 C, vec2 D) {
    const int kNum = 5;  // Only 5 iterations needed!

    vec2 a = -1.0*A + 3.0*B - 3.0*C + 1.0*D;
    vec2 b =  3.0*A - 6.0*B + 3.0*C;
    vec2 cc = -3.0*A + 3.0*B;
    vec2 d =  1.0*A - pos;

    // Find approximate t using Newton iteration
    float t = 0.5;
    for (int i = 0; i < kNum; i++) {
        vec2 p = d + t*(cc + t*(b + t*a));
        vec2 dp = cc + t*(2.0*b + 3.0*a*t);
        t -= dot(p, dp) / (dot(dp, dp) + dot(p, 2.0*b + 6.0*a*t));
        t = clamp(t, 0.0, 1.0);
    }

    // Also check endpoints
    vec2 p = d + t*(cc + t*(b + t*a));
    float dist = dot(p, p);

    // Check t=0
    float d0 = dot(d, d);
    if (d0 < dist) { dist = d0; }

    // Check t=1
    vec2 p1 = A - D - pos + (D - pos);
    p1 = d + cc + b + a;
    float d1 = dot(p1, p1);
    if (d1 < dist) { dist = d1; }

    return sqrt(dist);
}

// Cubic bezier arc length approximation - FAST
float cubicBezierLength(vec2 a, vec2 b, vec2 c, vec2 d, int steps) {
    float len = 0.0;
    vec2 prev = a;
    float invSteps = 1.0 / float(steps);
    for (int i = 1; i <= steps; i++) {
        vec2 curr = cubicBezierPoint(a, b, c, d, float(i) * invSteps);
        len += length(curr - prev);
        prev = curr;
    }
    return len;
}

// Quadratic bezier arc length approximation - FAST
float quadBezierLength(vec2 a, vec2 b, vec2 c, int steps) {
    float len = 0.0;
    vec2 prev = a;
    float invSteps = 1.0 / float(steps);
    for (int i = 1; i <= steps; i++) {
        vec2 curr = quadBezierPoint(a, b, c, float(i) * invSteps);
        len += length(curr - prev);
        prev = curr;
    }
    return len;
}

// Cubic bezier SDF with arc length tracking - OPTIMIZED
vec2 sdCubicBezierArc(vec2 p, vec2 a, vec2 b, vec2 c, vec2 d, float arcStart, float tolerance) {
    // Early bbox culling for bezier
    vec2 bmin = min(min(a, b), min(c, d));
    vec2 bmax = max(max(a, b), max(c, d));
    float cullDist = max(max(bmin.x - p.x, p.x - bmax.x), max(bmin.y - p.y, p.y - bmax.y));
    if (cullDist > tolerance * 10.0) {
        // Too far, approximate with line
        return sdSegmentArc(p, a, d, arcStart);
    }

    float flatness = bezierFlatness(a, b, c, d);
    if (flatness < tolerance) return sdSegmentArc(p, a, d, arcStart);

    int steps = int(clamp(flatness / tolerance * 2.0, float(BEZIER_MIN_STEPS), float(BEZIER_MAX_STEPS)));

    float dist = INF_DIST;
    float closestArc = arcStart;
    vec2 prev = a;
    float arcLen = arcStart;
    float invSteps = 1.0 / float(steps);

    for (int i = 1; i <= steps; i++) {
        vec2 curr = cubicBezierPoint(a, b, c, d, float(i) * invSteps);
        vec2 result = sdSegmentArc(p, prev, curr, arcLen);
        if (result.x < dist) { dist = result.x; closestArc = result.y; }
        arcLen += length(curr - prev);
        prev = curr;
    }
    return vec2(dist, closestArc);
}

// Quadratic bezier SDF with arc length tracking - OPTIMIZED
vec2 sdQuadBezierArc(vec2 p, vec2 a, vec2 b, vec2 c, float arcStart, float tolerance) {
    // Early bbox culling
    vec2 bmin = min(min(a, b), c);
    vec2 bmax = max(max(a, b), c);
    float cullDist = max(max(bmin.x - p.x, p.x - bmax.x), max(bmin.y - p.y, p.y - bmax.y));
    if (cullDist > tolerance * 10.0) {
        return sdSegmentArc(p, a, c, arcStart);
    }

    float flatness = quadBezierFlatness(a, b, c);
    if (flatness < tolerance) return sdSegmentArc(p, a, c, arcStart);

    int steps = int(clamp(flatness / tolerance * 2.0, float(BEZIER_MIN_STEPS), float(BEZIER_MAX_STEPS)));

    float dist = INF_DIST;
    float closestArc = arcStart;
    vec2 prev = a;
    float arcLen = arcStart;
    float invSteps = 1.0 / float(steps);

    for (int i = 1; i <= steps; i++) {
        vec2 curr = quadBezierPoint(a, b, c, float(i) * invSteps);
        vec2 result = sdSegmentArc(p, prev, curr, arcLen);
        if (result.x < dist) { dist = result.x; closestArc = result.y; }
        arcLen += length(curr - prev);
        prev = curr;
    }
    return vec2(dist, closestArc);
}

// ============================================================================
// OPTIMIZATION 1: Adaptive Bezier Subdivision
// ============================================================================

float sdCubicBezierAdaptive(vec2 p, vec2 a, vec2 b, vec2 c, vec2 d, float tolerance) {
    float flatness = bezierFlatness(a, b, c, d);
    if (flatness < tolerance) return sdSegment(p, a, d);

    int steps = int(clamp(flatness / tolerance * 4.0, float(BEZIER_MIN_STEPS), float(BEZIER_MAX_STEPS)));
    float dist = INF_DIST;
    vec2 prev = a;
    float invSteps = 1.0 / float(steps);

    for (int i = 1; i <= steps; i++) {
        vec2 curr = cubicBezierPoint(a, b, c, d, float(i) * invSteps);
        dist = min(dist, sdSegment(p, prev, curr));
        prev = curr;
    }
    return dist;
}

float sdQuadBezierAdaptive(vec2 p, vec2 a, vec2 b, vec2 c, float tolerance) {
    float flatness = quadBezierFlatness(a, b, c);
    if (flatness < tolerance) return sdSegment(p, a, c);

    int steps = int(clamp(flatness / tolerance * 4.0, float(BEZIER_MIN_STEPS), float(BEZIER_MAX_STEPS)));
    float dist = INF_DIST;
    vec2 prev = a;
    float invSteps = 1.0 / float(steps);

    for (int i = 1; i <= steps; i++) {
        vec2 curr = quadBezierPoint(a, b, c, float(i) * invSteps);
        dist = min(dist, sdSegment(p, prev, curr));
        prev = curr;
    }
    return dist;
}

// ============================================================================
// OPTIMIZATION 2: Adaptive Winding Calculation
// ============================================================================

float windLine(vec2 p, vec2 a, vec2 b) {
    if (a.y <= p.y) {
        if (b.y > p.y && (b.x - a.x) * (p.y - a.y) > (p.x - a.x) * (b.y - a.y))
            return 1.0;
    } else {
        if (b.y <= p.y && (b.x - a.x) * (p.y - a.y) < (p.x - a.x) * (b.y - a.y))
            return -1.0;
    }
    return 0.0;
}

float windCubicBezierAdaptive(vec2 p, vec2 a, vec2 b, vec2 c, vec2 d, float tolerance) {
    // Early bbox culling - winding only matters if we're near the curve
    vec2 bmin = min(min(a, b), min(c, d));
    vec2 bmax = max(max(a, b), max(c, d));
    // For winding, we need to check if ray from p to +infinity crosses
    if (p.y < bmin.y || p.y > bmax.y) return 0.0;
    if (p.x > bmax.x) return 0.0;

    float flatness = bezierFlatness(a, b, c, d);
    if (flatness < tolerance) return windLine(p, a, d);

    int steps = int(clamp(flatness / tolerance * 2.0, float(BEZIER_MIN_STEPS), float(BEZIER_MAX_STEPS)));
    float w = 0.0;
    vec2 prev = a;
    float invSteps = 1.0 / float(steps);

    for (int i = 1; i <= steps; i++) {
        vec2 curr = cubicBezierPoint(a, b, c, d, float(i) * invSteps);
        w += windLine(p, prev, curr);
        prev = curr;
    }
    return w;
}

float windQuadBezierAdaptive(vec2 p, vec2 a, vec2 b, vec2 c, float tolerance) {
    // Early bbox culling
    vec2 bmin = min(min(a, b), c);
    vec2 bmax = max(max(a, b), c);
    if (p.y < bmin.y || p.y > bmax.y) return 0.0;
    if (p.x > bmax.x) return 0.0;

    float flatness = quadBezierFlatness(a, b, c);
    if (flatness < tolerance) return windLine(p, a, c);

    int steps = int(clamp(flatness / tolerance * 2.0, float(BEZIER_MIN_STEPS), float(BEZIER_MAX_STEPS)));
    float w = 0.0;
    vec2 prev = a;
    float invSteps = 1.0 / float(steps);

    for (int i = 1; i <= steps; i++) {
        vec2 curr = quadBezierPoint(a, b, c, float(i) * invSteps);
        w += windLine(p, prev, curr);
        prev = curr;
    }
    return w;
}

// ============================================================================
// SVG Path Evaluation - HEAVILY OPTIMIZED with ANALYTIC BEZIER
// ============================================================================

// Set to 1 for analytic bezier (faster, no subdivision)
// Set to 0 for iterative bezier (supports arc length for dash)
#define USE_ANALYTIC_BEZIER 1

void evalPath(vec2 uv) {
    _pathStart = vec2(0.0);
    _pathCurrent = vec2(0.0);
    _pathDist = INF_DIST;
    _pathWinding = 0.0;
    _pathArcLength = 0.0;
    _pathClosestArcPos = 0.0;
    _pathTotalLength = 0.0;

    // Tolerance: shrinks as zoom grows, but don't let it get too small
    float tolerance = max(BEZIER_FLATNESS / zoom, 0.1);
    float currentArc = 0.0;

    // Arc length is computed only when there is a dash stroke
    bool needArcLength = (dashParams.w > 0.5);

    int i = 0;
    while (i < pathPointCount) {
        int cmd = int(pathData[i].x);

        if (cmd == 77) { // 'M'
            vec2 p = pathData[i].zw;
            _pathStart = p;
            _pathCurrent = p;
            i += 1;
        }
        else if (cmd == 76) { // 'L'
            vec2 p = pathData[i].zw;

            // Segment bbox culling
            vec2 segMin = min(_pathCurrent, p);
            vec2 segMax = max(_pathCurrent, p);
            float cullDist = max(max(segMin.x - uv.x, uv.x - segMax.x),
                                 max(segMin.y - uv.y, uv.y - segMax.y));

            if (cullDist < _pathDist + tolerance) {
                float dist = sdSegment(uv, _pathCurrent, p);
                if (dist < _pathDist) {
                    _pathDist = dist;
                    if (needArcLength) {
                        // Approximate arc position
                        vec2 pa = uv - _pathCurrent;
                        vec2 ba = p - _pathCurrent;
                        float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
                        _pathClosestArcPos = currentArc + h * length(ba);
                    }
                }
            }

            _pathWinding += windLine(uv, _pathCurrent, p);
            if (needArcLength) currentArc += length(p - _pathCurrent);
            _pathCurrent = p;
            i += 1;
        }
        else if (cmd == 67) { // 'C' - Cubic Bezier
            vec2 c1 = pathData[i].zw;
            vec2 c2 = pathData[i+1].xy;
            vec2 p = pathData[i+1].zw;

#if USE_ANALYTIC_BEZIER
            // Analytic cubic bezier - O(1) no subdivision!
            float dist = sdCubicBezierAnalytic(uv, _pathCurrent, c1, c2, p);
            if (dist < _pathDist) {
                _pathDist = dist;
            }
#else
            // Iterative with arc length tracking
            vec2 result = sdCubicBezierArc(uv, _pathCurrent, c1, c2, p, currentArc, tolerance);
            if (result.x < _pathDist) {
                _pathDist = result.x;
                _pathClosestArcPos = result.y;
            }
#endif

            _pathWinding += windCubicBezierAdaptive(uv, _pathCurrent, c1, c2, p, tolerance);
            if (needArcLength) currentArc += cubicBezierLength(_pathCurrent, c1, c2, p, 8);
            _pathCurrent = p;
            i += 2;
        }
        else if (cmd == 81) { // 'Q' - Quadratic Bezier
            vec2 c = pathData[i].zw;
            vec2 p = pathData[i+1].xy;

#if USE_ANALYTIC_BEZIER
            // Analytic quadratic bezier - O(1) exact solution!
            float dist = sdBezierAnalytic(uv, _pathCurrent, c, p);
            if (dist < _pathDist) {
                _pathDist = dist;
            }
#else
            // Iterative with arc length tracking
            vec2 result = sdQuadBezierArc(uv, _pathCurrent, c, p, currentArc, tolerance);
            if (result.x < _pathDist) {
                _pathDist = result.x;
                _pathClosestArcPos = result.y;
            }
#endif

            _pathWinding += windQuadBezierAdaptive(uv, _pathCurrent, c, p, tolerance);
            if (needArcLength) currentArc += quadBezierLength(_pathCurrent, c, p, 8);
            _pathCurrent = p;
            i += 2;
        }
        else if (cmd == 90) { // 'Z'
            if (distance(_pathCurrent, _pathStart) > EPSILON) {
                // Segment bbox culling
                vec2 segMin = min(_pathCurrent, _pathStart);
                vec2 segMax = max(_pathCurrent, _pathStart);
                float cullDist = max(max(segMin.x - uv.x, uv.x - segMax.x),
                                     max(segMin.y - uv.y, uv.y - segMax.y));

                if (cullDist < _pathDist + tolerance) {
                    float dist = sdSegment(uv, _pathCurrent, _pathStart);
                    if (dist < _pathDist) {
                        _pathDist = dist;
                    }
                }

                _pathWinding += windLine(uv, _pathCurrent, _pathStart);
                if (needArcLength) currentArc += length(_pathStart - _pathCurrent);
            }
            _pathCurrent = _pathStart;
            i += 1;
        }
        else if (cmd == 255) { // END
            break;
        }
        else {
            i += 1;
        }
    }

    _pathTotalLength = currentArc;
}

// ============================================================================
// Noise Functions
// ============================================================================

float hash21(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

float noise2D(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// ============================================================================
// Blending
// ============================================================================

vec3 blendScreen(vec3 base, vec3 blend) {
    return 1.0 - (1.0 - base) * (1.0 - blend);
}

vec4 blendOver(vec4 back, vec4 front) {
    float a = front.a + back.a * (1.0 - front.a);
    if (a < EPSILON) return vec4(0.0);
    vec3 c = (front.rgb * front.a + back.rgb * back.a * (1.0 - front.a)) / a;
    return vec4(c, a);
}

// ============================================================================
// Deformation Functions
// ============================================================================

vec2 applyDeform(vec2 p, vec2 size, int type, vec4 params) {
    if (type == 0) return p;

    float amount = params.x;
    float freq = params.y;
    float offset = params.z;

    vec2 n = p / max(size * 0.5, vec2(1.0));

    if (type == 1) { // bend
        float bend = n.y * amount;
        float c = cos(bend);
        float s = sin(bend);
        p.x = p.x * c - (p.y - size.y * 0.5 * sign(amount)) * s;
    }
    else if (type == 2) { // taper
        float t = (n.y + 1.0) * 0.5;
        float scale = mix(1.0, 1.0 - amount, t);
        p.x /= max(scale, 0.01);
    }
    else if (type == 3) { // skew
        p.x -= p.y * amount;
    }
    else if (type == 4) { // wave
        p.x += sin(p.y * freq + offset) * amount;
        p.y += sin(p.x * freq + offset) * amount * 0.5;
    }
    else if (type == 5) { // noise
        float n1 = noise2D(p * freq + offset) * 2.0 - 1.0;
        float n2 = noise2D(p * freq + offset + 100.0) * 2.0 - 1.0;
        p += vec2(n1, n2) * amount;
    }
    else if (type == 6) { // twist
        float angle = length(n) * amount;
        float c = cos(angle);
        float s = sin(angle);
        p = vec2(p.x * c - p.y * s, p.x * s + p.y * c);
    }
    else if (type == 7) { // bulge
        float dist = length(n);
        float factor = 1.0 + amount * (1.0 - dist * dist);
        p *= factor;
    }
    else if (type == 8) { // pinch
        float dist = length(n);
        float factor = 1.0 - amount * exp(-dist * dist * 2.0);
        p /= max(factor, 0.1);
    }

    return p;
}

// ============================================================================
// SDF Functions
// ============================================================================

float sdCircle(vec2 p, float r) { return length(p) - r; }

float sdBox(vec2 p, vec2 b) {
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 d = abs(p) - b + r;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;
}

float sdRoundedBoxEx(vec2 p, vec2 b, vec4 r) {
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x = (p.y > 0.0) ? r.x : r.y;
    vec2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

float sdHexagon(vec2 p, float r) {
    const vec3 k = vec3(-0.866025404, 0.5, 0.577350269);
    p = abs(p);
    p -= 2.0 * min(dot(k.xy, p), 0.0) * k.xy;
    p -= vec2(clamp(p.x, -k.z * r, k.z * r), r);
    return length(p) * sign(p.y);
}

float sdStar5(vec2 p, float r, float rf) {
    const vec2 k1 = vec2(0.809016994375, -0.587785252292);
    const vec2 k2 = vec2(-k1.x, k1.y);
    p.x = abs(p.x);
    p -= 2.0 * max(dot(k1, p), 0.0) * k1;
    p -= 2.0 * max(dot(k2, p), 0.0) * k2;
    p.x = abs(p.x);
    p.y -= r;
    vec2 ba = rf * vec2(-k1.y, k1.x) - vec2(0, 1);
    float h = clamp(dot(p, ba) / dot(ba, ba), 0.0, r);
    return length(p - ba * h) * sign(p.y * ba.x - p.x * ba.y);
}

float sdPentagon(vec2 p, float r) {
    const vec3 k = vec3(0.809016994, 0.587785252, 0.726542528);
    p.x = abs(p.x);
    p -= 2.0 * min(dot(vec2(-k.x, k.y), p), 0.0) * vec2(-k.x, k.y);
    p -= 2.0 * min(dot(vec2(k.x, k.y), p), 0.0) * vec2(k.x, k.y);
    p -= vec2(clamp(p.x, -r * k.z, r * k.z), r);
    return length(p) * sign(p.y);
}

float sdOctagon(vec2 p, float r) {
    const vec3 k = vec3(-0.9238795325, 0.3826834323, 0.4142135623);
    p = abs(p);
    p -= 2.0 * min(dot(vec2(k.x, k.y), p), 0.0) * vec2(k.x, k.y);
    p -= 2.0 * min(dot(vec2(-k.x, k.y), p), 0.0) * vec2(-k.x, k.y);
    p -= vec2(clamp(p.x, -k.z * r, k.z * r), r);
    return length(p) * sign(p.y);
}

float sdHeart(vec2 p, float s) {
    p /= s;
    p.x = abs(p.x);
    if (p.y + p.x > 1.0)
        return (sqrt(dot(p - vec2(0.25, 0.75), p - vec2(0.25, 0.75))) - sqrt(2.0) / 4.0) * s;
    return (sqrt(min(dot(p - vec2(0.0, 1.0), p - vec2(0.0, 1.0)),
                    dot(p - 0.5 * max(p.x + p.y, 0.0), p - 0.5 * max(p.x + p.y, 0.0)))) * sign(p.x - p.y)) * s;
}

float sdEquilateralTriangle(vec2 p, float r) {
    const float k = sqrt(3.0);
    p /= r;
    p.x = abs(p.x) - 1.0;
    p.y = p.y + 1.0 / k;
    if (p.x + k * p.y > 0.0) p = vec2(p.x - k * p.y, -k * p.x - p.y) / 2.0;
    p.x -= clamp(p.x, -2.0, 0.0);
    return -length(p) * sign(p.y) * r;
}

float sdStar(vec2 p, float r, int n, float m) {
    float an = 3.141593 / float(n);
    float en = 3.141593 / m;
    vec2 acs = vec2(cos(an), sin(an));
    vec2 ecs = vec2(cos(en), sin(en));
    float bn = mod(atan(p.x, p.y), 2.0 * an) - an;
    p = length(p) * vec2(cos(bn), abs(sin(bn)));
    p -= r * acs;
    p += ecs * clamp(-dot(p, ecs), 0.0, r * acs.y / ecs.y);
    return length(p) * sign(p.x);
}

float sdHexagram(vec2 p, float r) {
    const vec4 k = vec4(-0.5, 0.8660254038, 0.5773502692, 1.7320508076);
    p = abs(p);
    p -= 2.0 * min(dot(k.xy, p), 0.0) * k.xy;
    p -= 2.0 * min(dot(k.yx, p), 0.0) * k.yx;
    p -= vec2(clamp(p.x, r * k.z, r * k.w), r);
    return length(p) * sign(p.y);
}

float sdRhombus(vec2 p, vec2 b) {
    p = abs(p);
    float h = clamp((-2.0 * dot(p, b) + dot(b, b)) / dot(b, b), -1.0, 1.0);
    float d = length(p - 0.5 * b * vec2(1.0 - h, 1.0 + h));
    return d * sign(p.x * b.y + p.y * b.x - b.x * b.y);
}

float sdEllipse2(vec2 p, vec2 ab) {
    // Circle fallback: avoid division by zero
    if (abs(ab.x - ab.y) < 0.001) {
        return length(p) - ab.x;
    }

    p = abs(p);
    if (p.x > p.y) { p = p.yx; ab = ab.yx; }
    float l = ab.y * ab.y - ab.x * ab.x;
    float m = ab.x * p.x / l;
    float n = ab.y * p.y / l;
    float m2 = m * m, n2 = n * n;
    float c = (m2 + n2 - 1.0) / 3.0;
    float c3 = c * c * c;
    float q = c3 + m2 * n2 * 2.0;
    float d = c3 + m2 * n2;
    float g = m + m * n2;
    float co;
    if (d < 0.0) {
        float h = acos(q / c3) / 3.0;
        float s = cos(h), t = sin(h) * sqrt(3.0);
        float rx = sqrt(-c * (s + t + 2.0) + m2);
        float ry = sqrt(-c * (s - t + 2.0) + m2);
        co = (ry + sign(l) * rx + abs(g) / (rx * ry) - m) / 2.0;
    } else {
        float h = 2.0 * m * n * sqrt(d);
        float s = sign(q + h) * pow(abs(q + h), 1.0 / 3.0);
        float u = sign(q - h) * pow(abs(q - h), 1.0 / 3.0);
        float rx = -s - u - c * 4.0 + 2.0 * m2;
        float ry = (s - u) * sqrt(3.0);
        float rm = sqrt(rx * rx + ry * ry);
        co = (ry / sqrt(rm - rx) + 2.0 * g / rm - m) / 2.0;
    }
    vec2 r = ab * vec2(co, sqrt(1.0 - co * co));
    return length(r - p) * sign(p.y - r.y);
}
float sdEllipse(vec2 p, vec2 ab) {
    float k = length(p / ab);
    if (k < 0.001) return -min(ab.x, ab.y);
    float k2 = length(p / (ab * ab));
    return k * (k - 1.0) / k2;
}
float sdSegment0(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

// ============================================================================
// Arrow/Line SDF with configurable end caps - UNIFIED POLYGON
// Cap types: 0 = butt (flat), 1 = round, 2 = arrow
// Single continuous shape, no seams
// ============================================================================

// Unified arrow-line as single polygon SDF
float sdLineWithCaps(vec2 p, vec2 a, vec2 b, float thickness, int capA, int capB, float arrowSize) {
    vec2 d = b - a;
    float len = length(d);
    if (len < 0.001) return length(p - a) - thickness * 0.5;

    d /= len;  // normalize
    vec2 n = vec2(-d.y, d.x);  // perpendicular

    float ht = thickness * 0.5;  // half thickness
    float aw = arrowSize * 0.7;  // arrow half-width

    // Build polygon vertices clockwise starting from top-left
    vec2 v[12];
    int count = 0;

    // === TOP EDGE (left to right) ===

    // Start cap top
    if (capA == 2) {
        // Arrow start: tip, then wing, then body
        v[count++] = a;                          // arrow tip
        v[count++] = a + d * arrowSize + n * aw; // top wing
        v[count++] = a + d * arrowSize + n * ht; // join to body
    } else {
        v[count++] = a + n * ht;                 // top-left corner
    }

    // End cap top
    if (capB == 2) {
        v[count++] = b - d * arrowSize + n * ht; // body to arrow
        v[count++] = b - d * arrowSize + n * aw; // top wing
        v[count++] = b;                          // arrow tip
    } else {
        v[count++] = b + n * ht;                 // top-right corner
    }

    // === BOTTOM EDGE (right to left) ===

    // End cap bottom
    if (capB == 2) {
        v[count++] = b - d * arrowSize - n * aw; // bottom wing
        v[count++] = b - d * arrowSize - n * ht; // join to body
    } else {
        v[count++] = b - n * ht;                 // bottom-right corner
    }

    // Start cap bottom
    if (capA == 2) {
        v[count++] = a + d * arrowSize - n * ht; // body to arrow
        v[count++] = a + d * arrowSize - n * aw; // bottom wing
        // Closes back to v[0] which is arrow tip
    } else {
        v[count++] = a - n * ht;                 // bottom-left corner
    }

    // === COMPUTE POLYGON SDF ===
    float dist = 1e10;
    float wind = 0.0;

    for (int i = 0; i < count; i++) {
        vec2 v0 = v[i];
        vec2 v1 = v[(i + 1) % count];

        // Distance to edge
        vec2 e = v1 - v0;
        vec2 w = p - v0;
        float t = clamp(dot(w, e) / dot(e, e), 0.0, 1.0);
        dist = min(dist, length(w - e * t));

        // Winding number (crossing test)
        if (v0.y <= p.y) {
            if (v1.y > p.y && (e.x * w.y - e.y * w.x) > 0.0)
                wind += 1.0;
        } else {
            if (v1.y <= p.y && (e.x * w.y - e.y * w.x) < 0.0)
                wind -= 1.0;
        }
    }

    float polyDist = (wind != 0.0) ? -dist : dist;

    // Handle round caps (union with circles)
    if (capA == 1) {
        float circDist = length(p - a) - ht;
        polyDist = min(polyDist, circDist);
    }
    if (capB == 1) {
        float circDist = length(p - b) - ht;
        polyDist = min(polyDist, circDist);
    }

    return polyDist;
}

float sdPie(vec2 p, vec2 c, float r) {
    p.x = abs(p.x);
    float l = length(p) - r;
    float m = length(p - c * clamp(dot(p, c), 0.0, r));
    return max(l, m * sign(c.y * p.x - c.x * p.y));
}

float sdArc(vec2 p, vec2 sc, float ra, float rb) {
    p.x = abs(p.x);
    return ((sc.y * p.x > sc.x * p.y) ? length(p - sc * ra) : abs(length(p) - ra)) - rb;
}

float sdMoon(vec2 p, float d, float ra, float rb) {
    p.y = abs(p.y);
    float a = (ra * ra - rb * rb + d * d) / (2.0 * d);
    float b = sqrt(max(ra * ra - a * a, 0.0));
    if (d * (p.x * b - p.y * a) > d * d * max(b - p.y, 0.0))
        return length(p - vec2(a, b));
    return max((length(p) - ra), -(length(p - vec2(d, 0)) - rb));
}

float sdEgg(vec2 p, float ra, float rb) {
    const float k = sqrt(3.0);
    p.x = abs(p.x);
    float r = ra - rb;
    return ((p.y < 0.0) ? length(p) - r :
            (k * (p.x + r) < p.y) ? length(vec2(p.x, p.y - k * r)) :
            length(vec2(p.x + r, p.y)) - 2.0 * r) - rb;
}

float sdVesica(vec2 p, float r, float d) {
    p = abs(p);
    float b = sqrt(r * r - d * d);
    return ((p.y - b) * d > p.x * b) ? length(p - vec2(0.0, b)) : length(p - vec2(-d, 0.0)) - r;
}

float sdCutDisk(vec2 p, float r, float h) {
    float w = sqrt(r * r - h * h);
    p.x = abs(p.x);
    float s = max((h - r) * p.x * p.x + w * w * (h + r - 2.0 * p.y), h * p.x - w * p.y);
    return (s < 0.0) ? length(p) - r : (p.x < w) ? h - p.y : length(p - vec2(w, h));
}

float sdTrapezoid(vec2 p, float r1, float r2, float he) {
    vec2 k1 = vec2(r2, he);
    vec2 k2 = vec2(r2 - r1, 2.0 * he);
    p.x = abs(p.x);
    vec2 ca = vec2(p.x - min(p.x, (p.y < 0.0) ? r1 : r2), abs(p.y) - he);
    vec2 cb = p - k1 + k2 * clamp(dot(k1 - p, k2) / dot(k2, k2), 0.0, 1.0);
    float s = (cb.x < 0.0 && ca.y < 0.0) ? -1.0 : 1.0;
    return s * sqrt(min(dot(ca, ca), dot(cb, cb)));
}

float sdParallelogram(vec2 p, float wi, float he, float sk) {
    vec2 e = vec2(sk, he);
    p = (p.y < 0.0) ? -p : p;
    vec2 w = p - e; w.x -= clamp(w.x, -wi, wi);
    vec2 d = vec2(dot(w, w), -w.y);
    float s = p.x * e.y - p.y * e.x;
    p = (s < 0.0) ? -p : p;
    vec2 v = p - vec2(wi, 0); v -= e * clamp(dot(v, e) / dot(e, e), -1.0, 1.0);
    d = min(d, vec2(dot(v, v), wi * he - abs(s)));
    return sqrt(d.x) * sign(-d.y);
}

float sdCross(vec2 p, vec2 b, float r) {
    p = abs(p); p = (p.y > p.x) ? p.yx : p.xy;
    vec2 q = p - b;
    float k = max(q.y, q.x);
    vec2 w = (k > 0.0) ? q : vec2(b.y - p.x, -k);
    return sign(k) * length(max(w, 0.0)) + r;
}

float sdRoundedX(vec2 p, float w, float r) {
    p = abs(p);
    return length(p - min(p.x + p.y, w) * 0.5) - r;
}

float sdTriangle(vec2 p, vec2 p0, vec2 p1, vec2 p2) {
    vec2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
    vec2 v0 = p - p0, v1 = p - p1, v2 = p - p2;
    vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
    vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
    vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
    float s = sign(e0.x * e2.y - e0.y * e2.x);
    vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
                     vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
                     vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
    return -sqrt(d.x) * sign(d.y);
}

float sdRoundedCross(vec2 p, float s, float h) {
    float k = 0.5 * (h + 1.0 / h);
    p = abs(p / s);
    float res = (p.x < 1.0 && p.y < p.x * (k - h) + h) ?
           k - sqrt(dot(p - vec2(1, k), p - vec2(1, k))) :
           sqrt(min(dot(p - vec2(0, h), p - vec2(0, h)),
                    dot(p - vec2(1, 0), p - vec2(1, 0))));
    return res * s;
}

float sdBlobbyCross(vec2 pos, float s, float he) {
    pos = abs(pos / s);
    pos = vec2(abs(pos.x - pos.y), 1.0 - pos.x - pos.y) / sqrt(2.0);
    float p = (he - pos.y - 0.25 / he) / (6.0 * he);
    float q = pos.x / (he * he * 16.0);
    float h = q * q - p * p * p;
    float x;
    if (h > 0.0) { float r = sqrt(h); x = pow(q + r, 1.0 / 3.0) - pow(abs(q - r), 1.0 / 3.0) * sign(r - q); }
    else { float r = sqrt(p); x = 2.0 * r * cos(acos(q / (p * r)) / 3.0); }
    x = min(x, sqrt(2.0) / 2.0);
    vec2 z = vec2(x, he * (1.0 - 2.0 * x * x)) - pos;
    return length(z) * sign(z.y) * s;
}

float sdTunnel(vec2 p, vec2 wh) {
    p.x = abs(p.x); p.y = -p.y;
    vec2 q = p - wh;
    float d1 = dot(vec2(max(q.x, 0.0), q.y), vec2(max(q.x, 0.0), q.y));
    q.x = (p.y > 0.0) ? q.x : length(p) - wh.x;
    float d2 = dot(vec2(q.x, max(q.y, 0.0)), vec2(q.x, max(q.y, 0.0)));
    float dd = sqrt(min(d1, d2));
    return (max(q.x, q.y) < 0.0) ? -dd : dd;
}

float sdCoolS(vec2 p, float s) {
    p /= s;
    float six = (p.y < 0.0) ? -p.x : p.x;
    p.x = abs(p.x);
    p.y = abs(p.y) - 0.2;
    float rex = p.x - min(round(p.x / 0.4), 0.4);
    float aby = abs(p.y - 0.2) - 0.6;
    float d = sqrt(dot(vec2(max(rex, 0.0), max(aby, 0.0)), vec2(max(rex, 0.0), max(aby, 0.0))));
    d = (max(rex, aby) < 0.0) ? -d : d;
    if (d > 0.0) d = sqrt(d);
    d = abs(d) - 0.1;
    if (six > 0.0) d = max(d, abs(p.x - 0.2) - 0.2);
    return d * s;
}

float sdBezier0(vec2 pos, vec2 A, vec2 B, vec2 C) {
    vec2 a = B - A, b = A - 2.0 * B + C, c = a * 2.0, d = A - pos;
    float kk = 1.0 / dot(b, b);
    float kx = kk * dot(a, b);
    float ky = kk * (2.0 * dot(a, a) + dot(d, b)) / 3.0;
    float kz = kk * dot(d, a);
    float pp = ky - kx * kx;
    float p3 = pp * pp * pp;
    float q = kx * (2.0 * kx * kx - 3.0 * ky) + kz;
    float h = q * q + 4.0 * p3;
    float res;
    if (h >= 0.0) {
        h = sqrt(h);
        vec2 x = (vec2(h, -h) - q) / 2.0;
        vec2 uv = sign(x) * pow(abs(x), vec2(1.0 / 3.0));
        float t = clamp(uv.x + uv.y - kx, 0.0, 1.0);
        res = dot(d + (c + b * t) * t, d + (c + b * t) * t);
    } else {
        float z = sqrt(-pp);
        float v = acos(q / (pp * z * 2.0)) / 3.0;
        float m = cos(v), n = sin(v) * 1.732050808;
        vec3 t = clamp(vec3(m + m, -n - m, n - m) * z - kx, 0.0, 1.0);
        res = min(dot(d + (c + b * t.x) * t.x, d + (c + b * t.x) * t.x),
                  dot(d + (c + b * t.y) * t.y, d + (c + b * t.y) * t.y));
    }
    return sqrt(res);
}

// ============================================================================
// SDF Selection
// ============================================================================

float getShapeSDF(vec2 p, int type, vec2 size, float p1, float p2, vec4 ep, vec4 ep2) {
    if (type == 0) return sdCircle(p, p1);
    if (type == 1) return sdBox(p, size * 0.5);
    if (type == 2) {
        if (ep2.x > 0.0 || ep2.y > 0.0 || ep2.z > 0.0 || ep2.w > 0.0) {
            return sdRoundedBoxEx(p, size * 0.5, ep2);
        }
        return sdRoundedBox(p, size * 0.5, p1);
    }
    if (type == 3) return sdSegment0(p, ep.xy, ep.zw);
    if (type == 4) return sdRhombus(p, size * 0.5);
    if (type == 5) return sdTrapezoid(p, p1, p2, size.y * 0.5);
    if (type == 6) return sdParallelogram(p, size.x * 0.5, size.y * 0.5, p1);
    if (type == 7) return sdEquilateralTriangle(p, p1);
    if (type == 8) return sdTriangle(p, ep.xy, ep.zw, ep2.xy);
    if (type == 9) return sdPentagon(p, p1);
    if (type == 10) return sdHexagon(p, p1);
    if (type == 11) return sdOctagon(p, p1);
    if (type == 12) return sdHexagram(p, p1);
    if (type == 13) return sdStar5(p, p1, p2);
    if (type == 14) return sdStar(p, p1, int(p2), ep.z);
    if (type == 15) return sdPie(p, vec2(sin(p2), cos(p2)), p1);
    if (type == 16) return sdCutDisk(p, p1, p2);
    if (type == 17) return sdArc(p, vec2(sin(ep.z), cos(ep.z)), p1, p2);
    if (type == 18) return sdVesica(p, p1, p2);
    if (type == 19) return sdMoon(p, p1, p2, ep.z);
    if (type == 20) return sdEgg(p, p1, p2);
    if (type == 21) return sdHeart(p, p1);
    if (type == 22) return sdCross(p, size * 0.5, p1);
    if (type == 23) return sdRoundedX(p, p1, p2);
    if (type == 24) return sdEllipse(p, size * 0.5);
    if (type == 25) return sdRoundedCross(p, p1, p2);
    if (type == 26) return sdCoolS(p, p1);
    if (type == 27) return sdBlobbyCross(p, p1, p2);
    if (type == 28) return sdTunnel(p, size * 0.5);
    if (type == 29) return sdBezier0(p, ep.xy, ep.zw, ep2.xy);
    // type 30 = SDF_ARROW: ep.xy = point A, ep.zw = point B
    // ep2.x = thickness, ep2.y = capA type, ep2.z = capB type, ep2.w = arrow size
    if (type == 30) return sdLineWithCaps(p, ep.xy, ep.zw, ep2.x, int(ep2.y), int(ep2.z), ep2.w);
    return sdCircle(p, 50.0);
}

// ============================================================================
// Stroke Helper Functions
// ============================================================================

float getStrokeAngle(vec2 p) {
    return atan(p.y, p.x);
}

float getTaperedWidth(float baseWidth, float angle, float taper) {
    float t = (angle + 3.14159) / (2.0 * 3.14159);
    float taperFactor = mix(1.0, 1.0 - taper, smoothstep(0.0, 0.3, t) * (1.0 - smoothstep(0.7, 1.0, t)));
    return baseWidth * max(taperFactor, 0.1);
}

float getSketchyOffset(vec2 p, float wobble, float freq, float time) {
    float n1 = noise2D(p * freq + time * 0.5) * 2.0 - 1.0;
    float n2 = noise2D(p * freq * 1.5 + vec2(100.0) + time * 0.3) * 2.0 - 1.0;
    return (n1 + n2 * 0.5) * wobble;
}

float getVariableWidth(float baseWidth, vec2 p, float pressure, float angle) {
    float pressureVar = sin(angle * 3.0) * 0.3 + 0.7;
    float noiseVar = noise2D(p * 0.1) * 0.4 + 0.8;
    return baseWidth * mix(1.0, pressureVar * noiseVar, pressure);
}

float getBrushWidth(float baseWidth, float angle, float brushAng, float ratio) {
    float relAngle = angle - brushAng;
    float ellipse = sqrt(cos(relAngle) * cos(relAngle) +
                        (sin(relAngle) * sin(relAngle)) / (ratio * ratio));
    return baseWidth * ellipse;
}

float getInkSpreadWidth(vec2 p, float baseWidth, float spread) {
    float n = noise2D(p * 0.05);
    for (int i = 1; i < 4; i++) {
        n += noise2D(p * 0.05 * float(i * 2)) * (0.5 / float(i));
    }
    return baseWidth * (1.0 + (n * 2.0 - 1.0) * spread);
}

// ============================================================================
// 2.5D Lighting Functions
// ============================================================================

float sphereHeight(float sdf, float radius) {
    float d = clamp(-sdf / radius, 0.0, 1.0);
    return sqrt(1.0 - (1.0 - d) * (1.0 - d));
}

vec3 calcNormal25D(vec2 p, float sdf, float height, int shapeType, vec2 shapeSize,
                   float p1, float p2, vec4 ep, vec4 ep2) {
    // Larger eps smooths out harsh creases at concave junctions
    float eps = max(1.5, 0.5 + 3.0 * (1.0 - height));
    float dx = getShapeSDF(p + vec2(eps, 0.0), shapeType, shapeSize, p1, p2, ep, ep2) -
               getShapeSDF(p - vec2(eps, 0.0), shapeType, shapeSize, p1, p2, ep, ep2);
    float dy = getShapeSDF(p + vec2(0.0, eps), shapeType, shapeSize, p1, p2, ep, ep2) -
               getShapeSDF(p - vec2(0.0, eps), shapeType, shapeSize, p1, p2, ep, ep2);

    vec2 grad2d = vec2(dx, dy);
    float gradLen = length(grad2d);
    if (gradLen > EPSILON) grad2d /= gradLen;

    float z = height;
    vec2 xy = grad2d * (1.0 - z * z);

    return normalize(vec3(xy, z + EPSILON));
}

float fresnelSchlick(vec3 viewDir, vec3 normal, float ior) {
    float f0 = (ior - 1.0) / (ior + 1.0);
    f0 *= f0;
    float cosTheta = max(dot(-viewDir, normal), 0.0);
    return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

vec3 compute25DLighting(vec2 p, float sdf, vec3 baseColor, float radius,
                        int shapeType, vec2 shapeSize, float p1, float p2, vec4 ep, vec4 ep2) {
    float height = sphereHeight(sdf, radius);
    vec3 normal = calcNormal25D(p, sdf, height, shapeType, shapeSize, p1, p2, ep, ep2);
    vec3 viewDir = vec3(0.0, 0.0, -1.0);
    vec3 L = normalize(lightDir);

    float NdotL = max(dot(normal, L), 0.0);
    float halfLambert = NdotL * 0.5 + 0.5;

    vec3 H = normalize(L + (-viewDir));
    float NdotH = max(dot(normal, H), 0.0);
    float specular = pow(NdotH, specularPower);

    float fresnel = fresnelSchlick(viewDir, normal, fresnelIOR);
    float ao = normal.z * 0.5 + 0.5;

    vec3 ambient = baseColor * ambientStrength * ao;
    vec3 diffuse = baseColor * NdotL * diffuseStrength;
    vec3 halfLambertColor = baseColor * halfLambert * 0.15 * diffuseStrength;
    vec3 spec = vec3(1.0) * specular * specularStrength;
    vec3 rim = vec3(0.8, 0.9, 1.0) * fresnel * fresnelStrength;

    return ambient + diffuse + halfLambertColor + spec + rim;
}

// ============================================================================
// Common Effect Functions
// ============================================================================

// Correct "source over" alpha compositing (replaces mix(dst, src, src.a))
vec4 alphaOver(vec4 dst, vec4 src) {
    float srcA = src.a;
    float outA = srcA + dst.a * (1.0 - srcA);
    if (outA < 0.001) return vec4(0.0);
    vec3 outRGB = (src.rgb * srcA + dst.rgb * dst.a * (1.0 - srcA)) / outA;
    return vec4(outRGB, outA);
}

vec4 applyShadow(float sdf, vec4 shadow, float blur) {
    if (shadow.a < 0.01) return vec4(0.0);
    float sa = 1.0 - smoothstep(0.0, blur, sdf);
    if (sa < 0.005) return vec4(0.0);
    return vec4(shadow.rgb, shadow.a * sa);
}

vec4 applyGlow(vec4 result, float sdf, vec4 glow, float glowSz, float glowInt, int glowMd) {
    if (glow.a < 0.01 || glowSz < 0.1) return result;

    float glowD = max(sdf, 0.0);
    float glowAlpha = 1.0 - smoothstep(0.0, glowSz, glowD);
    glowAlpha = pow(glowAlpha, 2.0);
    vec3 gc = glow.rgb * glowAlpha * glow.a * glowInt;

    if (glowMd == 0) {
        result.rgb += gc;
        result.a = max(result.a, glowAlpha * glow.a);
    }
    else if (glowMd == 1) {
        result.rgb = blendScreen(result.rgb, gc);
        result.a = max(result.a, glowAlpha * glow.a);
    }
    else if (glowMd == 2) {
        result.rgb += gc;
        result.rgb = result.rgb / (1.0 + result.rgb);
        result.a = max(result.a, glowAlpha * glow.a);
    }
    else {
        result = alphaOver(result, vec4(glow.rgb, glow.a * glowAlpha));
    }

    return result;
}

vec4 applyFill(vec4 result, float sdf, vec4 fill, float aa) {
    if (fill.a < 0.01) return result;
    float fillAlpha = 1.0 - smoothstep(-aa, aa, sdf);
    return alphaOver(result, vec4(fill.rgb, fill.a * fillAlpha));
}

vec4 applyStroke(vec4 result, float sdf, vec4 stroke, float width, float soft, int mode, float aa) {
    if (width < 0.1 || stroke.a < 0.01) return result;

    float edge = aa + soft;
    float strokeAlpha = 0.0;

    if (mode == 0) {
        strokeAlpha = 1.0 - smoothstep(width * 0.5 - edge, width * 0.5 + edge, abs(sdf));
    }
    else if (mode == 1) {
        float strokeD = sdf - width * 0.5;
        strokeAlpha = 1.0 - smoothstep(-edge, edge, strokeD);
    }
    else {
        float strokeD = -sdf - width;
        strokeAlpha = 1.0 - smoothstep(-edge, edge, strokeD);
        strokeAlpha *= 1.0 - smoothstep(-aa, aa, sdf);
    }

    return alphaOver(result, vec4(stroke.rgb, stroke.a * strokeAlpha));
}

float getPathFillAlpha(float aa) {
    bool inside = false;
    if (pathFillRule == 0) {
        inside = abs(_pathWinding) > 0.5;
    } else {
        inside = mod(abs(_pathWinding), 2.0) > 0.5;
    }
    float sdf = inside ? -_pathDist : _pathDist;
    return 1.0 - smoothstep(-aa, aa, sdf);
}

float getPathStrokeAlpha(float width, float aa) {
    return 1.0 - smoothstep(width * 0.5 - aa, width * 0.5 + aa, _pathDist);
}

float getPathSDF() {
    bool inside = (pathFillRule == 0) ? (abs(_pathWinding) > 0.5) : (mod(abs(_pathWinding), 2.0) > 0.5);
    return inside ? -_pathDist : _pathDist;
}

// ============================================================================
// DASH STROKE FUNCTIONS
// ============================================================================

// Simple dash pattern - returns 1.0 if in dash, 0.0 if in gap
float getSimpleDashMask(float arcPos, float dashLen, float gapLen, float offset) {
    float pattern = dashLen + gapLen;
    float pos = mod(arcPos + offset, pattern);
    return step(pos, dashLen);
}

// Custom dash pattern - supports up to 4 values (2 dash-gap pairs)
float getCustomDashMask(float arcPos, float offset) {
    // Pattern: dashPattern = (dash1, gap1, dash2, gap2)
    float totalLen = dashPattern.x + dashPattern.y + dashPattern.z + dashPattern.w;

    if (totalLen < EPSILON) return 1.0;

    float pos = mod(arcPos + offset, totalLen);

    // dash1
    if (pos < dashPattern.x) return 1.0;
    pos -= dashPattern.x;

    // gap1
    if (pos < dashPattern.y) return 0.0;
    pos -= dashPattern.y;

    // dash2 (if exists)
    if (dashPattern.z > 0.0) {
        if (pos < dashPattern.z) return 1.0;
        pos -= dashPattern.z;
    }

    // gap2 (if exists)
    if (dashPattern.w > 0.0) {
        if (pos < dashPattern.w) return 0.0;
    }

    return 1.0;
}

// Shape perimeter estimation for dash on shapes
float estimateShapePerimeter(int shapeType, vec2 size, float p1, float p2) {
    float w = size.x, h = size.y;
    if (shapeType == 0) return 6.28318 * p1;  // Circle
    if (shapeType == 24) {  // Ellipse - Ramanujan
        float a = w * 0.5, b = h * 0.5;
        return 3.14159 * (3.0 * (a + b) - sqrt((3.0 * a + b) * (a + 3.0 * b)));
    }
    if (shapeType == 7) return 3.0 * p1 * 1.732;  // Triangle
    if (shapeType == 10) return 6.0 * p1;  // Hexagon
    if (shapeType == 11) return 6.12 * p1;  // Octagon
    return 2.0 * (w + h);  // Default: rectangle
}

// Get arc position for shape based on angle
float getShapeArcPosition(vec2 p, float sdf, int shapeType, vec2 size, float perimeter) {
    float angle = atan(p.y, p.x);
    return (angle + 3.14159) / 6.28318 * perimeter;
}

// Main dash mask function
float getDashMask(float arcPos, float strokeWidth, int capStyle) {
    if (dashEnabled == 0) return 1.0;
    return (dashCustomEnabled == 1) ?
        getCustomDashMask(arcPos, dashOffset) :
        getSimpleDashMask(arcPos, dashLength, dashGap, dashOffset);
}


// ============================================================================
// UNIFIED STROKE STYLE RENDERER
// ============================================================================
// dist: unsigned distance to stroke center (always positive)
// signedDist: signed distance (negative inside, positive outside) - for inside stroke mode
// isPath: true for path mode (affects offset shadow calculation)

vec4 renderStrokeStyle(vec2 p, float dist, float signedDist, float arcPos, float aa,
    vec4 stroke1, vec4 stroke2, vec4 stroke3,
    float baseWidth, float softness, int mode, int style,
    float taper, float wobble, float wobbleFreq,
    vec2 offset2, float spacing, float noiseAmt,
    float pressure, float brushAng, float brushRat, float time,
    float dashMask, bool isPath,
    int shapeT, vec2 shapeS, float sp1, float sp2, vec4 sp, vec4 sp2_) {

    vec4 result = vec4(0.0);
    if (baseWidth < 0.1 || stroke1.a < 0.01) return result;

    float angle = getStrokeAngle(p);
    float edge = aa + softness;

    // STROKE_SOLID (0)
    if (style == 0) {
        float strokeAlpha = 0.0;
        if (mode == 0) { // center
            strokeAlpha = 1.0 - smoothstep(baseWidth * 0.5 - edge, baseWidth * 0.5 + edge, dist);
        } else if (mode == 1) { // outside
            float strokeD = dist - baseWidth * 0.5;
            strokeAlpha = 1.0 - smoothstep(-edge, edge, strokeD);
        } else { // inside
            float strokeD = -signedDist - baseWidth;
            strokeAlpha = 1.0 - smoothstep(-edge, edge, strokeD);
            strokeAlpha *= 1.0 - smoothstep(-aa, aa, signedDist);
        }
        strokeAlpha *= dashMask;
        result = vec4(stroke1.rgb, stroke1.a * strokeAlpha);
    }
    // STROKE_TAPERED (1)
    else if (style == 1) {
        float taperedWidth = getTaperedWidth(baseWidth, angle, taper);
        float strokeAlpha = 1.0 - smoothstep(taperedWidth * 0.5 - edge, taperedWidth * 0.5 + edge, dist);
        strokeAlpha *= dashMask;
        result = vec4(stroke1.rgb, stroke1.a * strokeAlpha);
    }
    // STROKE_SKETCHY (2)
    else if (style == 2) {
        float wobbleOffset = getSketchyOffset(p, wobble, wobbleFreq, time);
        float sketchyDist = dist + wobbleOffset;
        float strokeAlpha = 1.0 - smoothstep(baseWidth * 0.5 - edge, baseWidth * 0.5 + edge, abs(sketchyDist));

        float wobbleOffset2 = getSketchyOffset(p + vec2(10.0), wobble * 0.7, wobbleFreq * 1.2, time);
        float sketchyDist2 = dist + wobbleOffset2;
        float strokeAlpha2 = 1.0 - smoothstep(baseWidth * 0.3 - edge, baseWidth * 0.3 + edge, abs(sketchyDist2));
        strokeAlpha = max(strokeAlpha, strokeAlpha2 * 0.5);
        strokeAlpha *= dashMask;
        result = vec4(stroke1.rgb, stroke1.a * strokeAlpha);
    }
    // STROKE_DOUBLE (3)
    else if (style == 3) {
        float outerWidth = baseWidth + spacing;
        float outerAlpha = 1.0 - smoothstep(outerWidth * 0.5 - edge, outerWidth * 0.5 + edge, dist);
        float innerAlpha = 1.0 - smoothstep(baseWidth * 0.5 - edge, baseWidth * 0.5 + edge, dist);
        outerAlpha *= dashMask;
        innerAlpha *= dashMask;
        result = alphaOver(vec4(0.0), vec4(stroke2.rgb, stroke2.a * outerAlpha));
        result = alphaOver(result, vec4(stroke1.rgb, stroke1.a * innerAlpha));
    }
    // STROKE_TRIPLE (4)
    else if (style == 4) {
        float outer2Width = baseWidth + spacing * 2.0;
        float outer2Alpha = 1.0 - smoothstep(outer2Width * 0.5 - edge, outer2Width * 0.5 + edge, dist);
        float outerWidth = baseWidth + spacing;
        float outerAlpha = 1.0 - smoothstep(outerWidth * 0.5 - edge, outerWidth * 0.5 + edge, dist);
        float innerAlpha = 1.0 - smoothstep(baseWidth * 0.5 - edge, baseWidth * 0.5 + edge, dist);
        outer2Alpha *= dashMask;
        outerAlpha *= dashMask;
        innerAlpha *= dashMask;
        result = vec4(stroke3.rgb, stroke3.a * outer2Alpha);
        result = alphaOver(result, vec4(stroke2.rgb, stroke2.a * outerAlpha));
        result = alphaOver(result, vec4(stroke1.rgb, stroke1.a * innerAlpha));
    }
    // STROKE_OFFSET_SHADOW (5)
    else if (style == 5) {
        float shadowDist;
        if (isPath) {
            float mainDist = _pathDist;
            float mainWind = _pathWinding;
            float mainArcPos = _pathClosestArcPos;
            evalPath(p - offset2);
            shadowDist = _pathDist;
            _pathDist = mainDist;
            _pathWinding = mainWind;
            _pathClosestArcPos = mainArcPos;
        } else {
            vec2 shadowP = p - offset2;
            shadowDist = abs(getShapeSDF(shadowP, shapeT, shapeS, sp1, sp2, sp, sp2_));
        }

        float shadowAlpha = 1.0 - smoothstep(baseWidth * 0.5 - edge, baseWidth * 0.5 + edge, shadowDist);
        float mainAlpha = 1.0 - smoothstep(baseWidth * 0.5 - edge, baseWidth * 0.5 + edge, dist);
        shadowAlpha *= dashMask;
        mainAlpha *= dashMask;

        result = vec4(stroke2.rgb, stroke2.a * shadowAlpha);
        result = alphaOver(result, vec4(stroke1.rgb, stroke1.a * mainAlpha));
    }
    // STROKE_VARIABLE (6)
    else if (style == 6) {
        float varWidth = getVariableWidth(baseWidth, p, pressure, angle);
        float strokeAlpha = 1.0 - smoothstep(varWidth * 0.5 - edge, varWidth * 0.5 + edge, dist);
        strokeAlpha *= dashMask;
        result = vec4(stroke1.rgb, stroke1.a * strokeAlpha);
    }
    // STROKE_INK_SPREAD (7)
    else if (style == 7) {
        float inkWidth = getInkSpreadWidth(p, baseWidth, noiseAmt);
        float strokeAlpha = 1.0 - smoothstep(inkWidth * 0.5 - edge * 2.0, inkWidth * 0.5 + edge, dist);
        float edgeNoise = noise2D(p * 0.1 + time * 0.1) * noiseAmt * 0.3;
        strokeAlpha *= 1.0 - edgeNoise;
        strokeAlpha *= dashMask;
        result = vec4(stroke1.rgb, stroke1.a * strokeAlpha);
    }
    // STROKE_BRUSH (8)
    else if (style == 8) {
        float brushWidth = getBrushWidth(baseWidth, angle, brushAng, brushRat);
        float wobbleOffset = getSketchyOffset(p, wobble * 0.3, wobbleFreq, time);
        float brushDist = dist + wobbleOffset;
        float strokeAlpha = 1.0 - smoothstep(brushWidth * 0.5 - edge, brushWidth * 0.5 + edge, abs(brushDist));
        strokeAlpha *= dashMask;
        result = vec4(stroke1.rgb, stroke1.a * strokeAlpha);
    }
    // STROKE_COMIC (9)
    else if (style == 9) {
        float outerWidth = baseWidth * 1.5;
        float outerAlpha = 1.0 - smoothstep(outerWidth * 0.5 - aa, outerWidth * 0.5, dist);

        float innerDist = dist + baseWidth * 0.3;
        float innerAlpha = 1.0 - smoothstep(baseWidth * 0.15 - aa, baseWidth * 0.15 + aa, innerDist);
        innerAlpha *= step(signedDist, 0.0);
        outerAlpha *= dashMask;
        innerAlpha *= dashMask;

        result = vec4(stroke1.rgb, stroke1.a * outerAlpha);
        vec3 highlight = mix(stroke1.rgb, vec3(1.0), 0.5);
        result = alphaOver(result, vec4(highlight, innerAlpha * 0.3));
    }
    // STROKE_NEON_OUTLINE (10)
    else if (style == 10) {
        float glowWidth = baseWidth * 3.0;
        float glowDist = max(dist - baseWidth * 0.5, 0.0);
        float glowAlpha = exp(-glowDist * glowDist / (glowWidth * glowWidth) * 4.0);
        float coreAlpha = 1.0 - smoothstep(baseWidth * 0.3 - aa, baseWidth * 0.3 + aa, dist);
        glowAlpha *= dashMask;
        coreAlpha *= dashMask;
        vec3 coreCol = mix(stroke1.rgb, vec3(1.0), 0.8);
        result = vec4(stroke1.rgb, glowAlpha * stroke1.a * 0.6);
        result = alphaOver(result, vec4(coreCol, coreAlpha * stroke1.a));
    }
    // STROKE_HALFTONE (11)
    else if (style == 11) {
        float inStroke = 1.0 - smoothstep(baseWidth * 0.5 - aa, baseWidth * 0.5 + aa, dist);
        float dotSize = baseWidth * 0.15;
        float dotSpacing = dotSize * 2.5;
        vec2 dotUV = mod(p, dotSpacing) - dotSpacing * 0.5;
        float dotDist = length(dotUV);
        float edgeFactor = smoothstep(0.0, baseWidth * 0.5, dist);
        float adjustedDotSize = dotSize * (1.0 - edgeFactor * 0.5);
        float dotAlpha = 1.0 - smoothstep(adjustedDotSize - aa, adjustedDotSize + aa, dotDist);
        dotAlpha *= inStroke;
        float solidAlpha = 1.0 - smoothstep(baseWidth * 0.2 - aa, baseWidth * 0.2 + aa, dist);
        float finalAlpha = max(solidAlpha, dotAlpha);
        finalAlpha *= dashMask;
        result = vec4(stroke1.rgb, stroke1.a * finalAlpha);
    }

    return result;
}

// ============================================================================
// DASH MASK CALCULATOR
// ============================================================================

float calculateDashMask(float arcPos, float perpDist, float baseWidth, float aa) {
    if (dashEnabled == 0) return 1.0;

    float dashMask = 1.0;

    if (dashCustomEnabled == 1) {
        dashMask = getCustomDashMask(arcPos, dashOffset);
    } else {
        float pattern = dashLength + dashGap;
        float pos = mod(arcPos + dashOffset, pattern);

        if (dashCapStyle == 0) { // Butt cap
            dashMask = step(pos, dashLength);
        }
        else if (dashCapStyle == 1) { // Round cap
            float halfWidth = baseWidth * 0.5;

            if (pos <= dashLength) {
                dashMask = 1.0;
                if (pos < halfWidth) {
                    float capDist = halfWidth - pos;
                    float totalDist = sqrt(capDist * capDist + perpDist * perpDist);
                    dashMask = 1.0 - smoothstep(halfWidth - aa, halfWidth + aa, totalDist);
                }
                else if (pos > dashLength - halfWidth) {
                    float capDist = pos - (dashLength - halfWidth);
                    float totalDist = sqrt(capDist * capDist + perpDist * perpDist);
                    dashMask = 1.0 - smoothstep(halfWidth - aa, halfWidth + aa, totalDist);
                }
            }
            else {
                float distToEnd = pos - dashLength;
                float distToStart = pattern - pos;

                if (distToEnd < halfWidth) {
                    float totalDist = sqrt(distToEnd * distToEnd + perpDist * perpDist);
                    dashMask = 1.0 - smoothstep(halfWidth - aa, halfWidth + aa, totalDist);
                }
                else if (distToStart < halfWidth) {
                    float totalDist = sqrt(distToStart * distToStart + perpDist * perpDist);
                    dashMask = 1.0 - smoothstep(halfWidth - aa, halfWidth + aa, totalDist);
                }
                else {
                    dashMask = 0.0;
                }
            }
        }
        else if (dashCapStyle == 2) { // Square cap
            float ext = baseWidth * 0.5;
            dashMask = step(pos, dashLength + ext * 2.0);
        }
    }

    return dashMask;
}

// ============================================================================
// PATH STROKE RENDERER (wrapper)
// ============================================================================

vec4 renderPathStroke(vec2 p, float aa,
    vec4 stroke1, vec4 stroke2, vec4 stroke3,
    float baseWidth, float softness, int mode, int style,
    float taper, float wobble, float wobbleFreq,
    vec2 offset2, float spacing, float noiseAmt,
    float pressure, float brushAng, float brushRat, float time) {

    if (baseWidth < 0.1 || stroke1.a < 0.01) return vec4(0.0);

    float dist = _pathDist;
    float signedDist = getPathSDF();
    float arcPos = _pathClosestArcPos;

    float dashMask = calculateDashMask(arcPos, dist, baseWidth, aa);
    if (dashMask < 0.01) return vec4(0.0);

    return renderStrokeStyle(p, dist, signedDist, arcPos, aa,
        stroke1, stroke2, stroke3,
        baseWidth, softness, mode, style,
        taper, wobble, wobbleFreq,
        offset2, spacing, noiseAmt,
        pressure, brushAng, brushRat, time,
        dashMask, true,
        0, vec2(0.0), 0.0, 0.0, vec4(0.0), vec4(0.0));
}

// ============================================================================
// SHAPE STROKE RENDERER (wrapper)
// ============================================================================

vec4 renderCartoonStroke(float sdf, vec2 p, float aa,
    vec4 stroke1, vec4 stroke2, vec4 stroke3,
    float baseWidth, float softness, int mode, int style,
    float taper, float wobble, float wobbleFreq,
    vec2 offset2, float spacing, float noiseAmt,
    float pressure, float brushAng, float brushRat, float time,
    int shapeT, vec2 shapeS, float sp1, float sp2, vec4 sp, vec4 sp2_) {

    if (baseWidth < 0.1 || stroke1.a < 0.01) return vec4(0.0);

    float dist = abs(sdf);
    float signedDist = sdf;

    // Compute shape perimeter and arc position
    float perimeter = estimateShapePerimeter(shapeT, shapeS, sp1, sp2);
    float arcPos = getShapeArcPosition(p, sdf, shapeT, shapeS, perimeter);

    float dashMask = calculateDashMask(arcPos, dist, baseWidth, aa);
    if (dashMask < 0.01) return vec4(0.0);

    return renderStrokeStyle(p, dist, signedDist, arcPos, aa,
        stroke1, stroke2, stroke3,
        baseWidth, softness, mode, style,
        taper, wobble, wobbleFreq,
        offset2, spacing, noiseAmt,
        pressure, brushAng, brushRat, time,
        dashMask, false,
        shapeT, shapeS, sp1, sp2, sp, sp2_);
}

// ============================================================================
// Batch Mode Renderer
// ============================================================================

vec4 renderShape(float sdf, float aa, vec4 fill, vec4 stroke, float strokeW, float strokeSoft, int strokeMd,
                 vec4 glow, float glowSz, float glowInt, int glowMd) {
    vec4 result = vec4(0.0);

    result = applyGlow(result, sdf, glow, glowSz, glowInt, glowMd);

    if (strokeMd == 1) {
        result = applyStroke(result, sdf, stroke, strokeW, strokeSoft, strokeMd, aa);
    }

    result = applyFill(result, sdf, fill, aa);

    if (strokeMd != 1) {
        result = applyStroke(result, sdf, stroke, strokeW, strokeSoft, strokeMd, aa);
    }

    return result;
}

// ============================================================================
// Main
// ============================================================================

void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    float rayY = screenHeight - fragCoord.y;
    vec2 worldPos = vec2((fragCoord.x - pan.x) / zoom, (rayY - pan.y) / zoom);

    float aa = 1.5 / zoom;
    vec4 result = vec4(0.0);

    // ========== PATH MODE ==========
    if (pathMode == 1) {
        vec2 p = applyInverseTransform(worldPos);
        float matScale = getMatrixScale();
        float adjustedAA = aa / matScale;

        // Early culling: exit fast when outside the path bounds
        float shadowCullPad = (shadowColor.a > 0.0) ? (abs(shadowOffset.x) + abs(shadowOffset.y) + shadowBlur) : 0.0;
        float cullPad = max(max(strokeWidth, glowSize), shadowCullPad) / matScale + 10.0;
        if (p.x < pathBounds.x - cullPad || p.x > pathBounds.x + pathBounds.z + cullPad ||
            p.y < pathBounds.y - cullPad || p.y > pathBounds.y + pathBounds.w + cullPad) {
            finalColor = vec4(0.0);
            return;
        }

        evalPath(p);

        // Shadow - OPTIMIZED
        if (shadowColor.a > 0.0) {
            float mainDist = _pathDist;
            float mainWind = _pathWinding;

            vec2 sp = p - shadowOffset;
            evalPath(sp);
            float shadowSdf = getPathSDF();
            result = applyShadow(shadowSdf, shadowColor, shadowBlur);

            _pathDist = mainDist;
            _pathWinding = mainWind;
        }

        // Glow
        result = applyGlow(result, _pathDist, glowColor, glowSize, glowIntensity, glowMode);

        // Outside stroke
        if (strokeWidth > 0.0 && strokeColor.a > 0.0 && strokeMode == 1) {
            vec4 strokeResult = renderPathStroke(p, adjustedAA,
                strokeColor, strokeColor2, strokeColor3,
                strokeWidth / matScale, strokeSoftness, strokeMode, strokeStyle,
                strokeTaper, strokeWobble, strokeWobbleFreq,
                strokeOffset2, strokeSpacing, strokeNoise,
                strokePressure, brushAngle, brushRatio, animTime);
            result = alphaOver(result, strokeResult);
        }

        // Fill
        float fillAlpha = getPathFillAlpha(adjustedAA);
        if (fillAlpha > 0.01) {
            result = alphaOver(result, vec4(fillColor.rgb, fillColor.a * fillAlpha));
        }

        // Center/Inside stroke
        if (strokeWidth > 0.0 && strokeColor.a > 0.0 && strokeMode != 1) {
            vec4 strokeResult = renderPathStroke(p, adjustedAA,
                strokeColor, strokeColor2, strokeColor3,
                strokeWidth / matScale, strokeSoftness, strokeMode, strokeStyle,
                strokeTaper, strokeWobble, strokeWobbleFreq,
                strokeOffset2, strokeSpacing, strokeNoise,
                strokePressure, brushAngle, brushRatio, animTime);

            if (strokeMode == 2) {
                strokeResult.a *= fillAlpha;
            }
            result = alphaOver(result, strokeResult);
        }
    }
    // ========== BATCH MODE (Tile-Based) ==========
    else if (batchMode == 2) {
        for (int i = 0; i < MAX_BATCH; i++) {
            if (i >= batchCount) break;

            vec2 pos = batchPosSize[i].xy;
            vec2 sz = batchPosSize[i].zw;

            vec4 ep = batchParams[i];
            vec4 ep2 = batchParams2[i];
            float rot = batchTypeRot[i].y;
            int type = int(batchTypeRot[i].x);

            // Transform
            vec2 diff = worldPos - pos;
            float c = cos(rot), s = sin(rot);
            vec2 p = mat2(c, -s, s, c) * diff;
            p = applyDeform(p, sz, deformType, deformParams);

            // SDF
            float sdf = getShapeSDF(p, type, sz, ep.x, ep.y, ep, ep2);

            // Skip if too far (account for shadow offset + blur)
            float maxEffect = max(batchEffects[i].x + batchEffects2[i].x, max(batchEffects[i].y,
                                  length(shadowOffset) + shadowBlur));
            if (sdf > maxEffect + 50.0) continue;

            // Get shape colors and effects
            vec4 shapeFillColor = batchFillColor[i];
            vec4 shapeStrokeColor = batchStrokeColor[i];
            vec4 shapeGlowColor = batchGlowColor[i];
            float shapeStrokeWidth = batchEffects[i].x;
            float shapeGlowSize = batchEffects[i].y;
            float shapeGlowIntensity = batchEffects[i].z;
            int shapeGlowMode = int(batchEffects[i].w);
            float shapeStrokeSoftness = batchEffects2[i].x;
            int shapeStrokeMode = int(batchEffects2[i].y);

            // Apply shadow first (behind everything)
            if (shadowColor.a > 0.0) {
                vec2 sp = p - shadowOffset;
                float sd = getShapeSDF(sp, type, sz, ep.x, ep.y, ep, ep2);
                vec4 shadowResult = applyShadow(sd, shadowColor, shadowBlur);
                // Correct alpha compositing - shadow goes behind existing content
                result.rgb = result.rgb + shadowResult.rgb * shadowResult.a * (1.0 - result.a);
                result.a = result.a + shadowResult.a * (1.0 - result.a);
            }

            // Apply glow (behind fill)
            if (shapeGlowColor.a > 0.01 && shapeGlowSize > 0.1) {
                float glowD = max(sdf, 0.0);
                float gAlpha = 1.0 - smoothstep(0.0, shapeGlowSize, glowD);
                gAlpha = pow(gAlpha, 2.0);
                vec3 glow = shapeGlowColor.rgb * gAlpha * shapeGlowColor.a * shapeGlowIntensity;
                result.rgb += glow;
                result.a = max(result.a, gAlpha * shapeGlowColor.a);
            }

            // Outside stroke (shapeStrokeMode == 1, before fill)
            if (shapeStrokeWidth > 0.0 && shapeStrokeColor.a > 0.0 && shapeStrokeMode == 1) {
                vec4 strokeResult = renderCartoonStroke(sdf, p, aa,
                    shapeStrokeColor, strokeColor2, strokeColor3,
                    shapeStrokeWidth, shapeStrokeSoftness, shapeStrokeMode, strokeStyle,
                    strokeTaper, strokeWobble, strokeWobbleFreq,
                    strokeOffset2, strokeSpacing, strokeNoise,
                    strokePressure, brushAngle, brushRatio, animTime,
                    type, sz, ep.x, ep.y, ep, ep2);
                result = alphaOver(result, strokeResult);
            }

            // Fill with gradient and 2.5D lighting support
            float fillAlpha = 1.0 - smoothstep(-aa, aa, sdf);
            if (fillAlpha > 0.01) {
                vec4 baseColor = shapeFillColor;

                // Apply gradient (uses global gradient settings)
                if (gradientType == 1) {
                    vec2 gd = normalize(gradientEnd - gradientStart);
                    float gl = length(gradientEnd - gradientStart);
                    float t = clamp(dot(p - gradientStart, gd) / gl, 0.0, 1.0);
                    baseColor.rgb = mix(mix(baseColor.rgb, gradientColor1.rgb, gradientColor1.a),
                                       mix(baseColor.rgb, gradientColor2.rgb, gradientColor2.a), t);
                } else if (gradientType == 2) {
                    float t = clamp(length(p) / length(sz * 0.5), 0.0, 1.0);
                    baseColor.rgb = mix(mix(baseColor.rgb, gradientColor1.rgb, gradientColor1.a),
                                       mix(baseColor.rgb, gradientColor2.rgb, gradientColor2.a), t);
                }

                // Apply 2.5D lighting (uses global lighting settings)
                if (enable25D == 1) {
                    vec3 litColor = compute25DLighting(p, sdf, baseColor.rgb, extrudeRadius,
                                                       type, sz, ep.x, ep.y, ep, ep2);
                    result = alphaOver(result, vec4(litColor, baseColor.a * fillAlpha));
                } else {
                    result = alphaOver(result, vec4(baseColor.rgb, baseColor.a * fillAlpha));
                }
            }

            // Center/Inside stroke (shapeStrokeMode != 1, after fill)
            if (shapeStrokeWidth > 0.0 && shapeStrokeColor.a > 0.0 && shapeStrokeMode != 1) {
                vec4 strokeResult = renderCartoonStroke(sdf, p, aa,
                    shapeStrokeColor, strokeColor2, strokeColor3,
                    shapeStrokeWidth, shapeStrokeSoftness, shapeStrokeMode, strokeStyle,
                    strokeTaper, strokeWobble, strokeWobbleFreq,
                    strokeOffset2, strokeSpacing, strokeNoise,
                    strokePressure, brushAngle, brushRatio, animTime,
                    type, sz, ep.x, ep.y, ep, ep2);
                result = alphaOver(result, strokeResult);
            }
        }
    }
    // ========== SINGLE MODE ==========
    else {
        vec2 p = applyInverseTransform(worldPos);
        float matScale = getMatrixScale();
        float adjustedAA = aa / matScale;
        p = applyDeform(p, shapeSize, deformType, deformParams);

        float sdf = getShapeSDF(p, shapeType, shapeSize, shapeParams.x, shapeParams.y, shapeParams, shapeParams2);

        // Shadow
        if (shadowColor.a > 0.0) {
            vec2 sp = p - shadowOffset;
            float sd = getShapeSDF(sp, shapeType, shapeSize, shapeParams.x, shapeParams.y, shapeParams, shapeParams2);
            result = applyShadow(sd, shadowColor, shadowBlur);
        }

        // Glow
        result = applyGlow(result, sdf, glowColor, glowSize, glowIntensity, glowMode);

        // Adjusted stroke width for matrix scale
        float adjustedStrokeW = strokeWidth / matScale;

        // Outside stroke (before fill)
        if (strokeWidth > 0.0 && strokeColor.a > 0.0 && strokeMode == 1) {
            vec4 strokeResult = renderCartoonStroke(sdf, p, adjustedAA,
                strokeColor, strokeColor2, strokeColor3,
                adjustedStrokeW, strokeSoftness, strokeMode, strokeStyle,
                strokeTaper, strokeWobble, strokeWobbleFreq,
                strokeOffset2, strokeSpacing, strokeNoise,
                strokePressure, brushAngle, brushRatio, animTime,
                shapeType, shapeSize, shapeParams.x, shapeParams.y, shapeParams, shapeParams2);
            result = alphaOver(result, strokeResult);
        }

        // Fill with optional 2.5D and gradient
        // Extend fill slightly into stroke area to prevent AA gap (white line)
        float fillAlpha = 1.0 - smoothstep(-adjustedAA, adjustedAA, sdf);
        vec4 baseColor = fillColor;

        // Gradient
        if (gradientType == 1) {
            vec2 gd = normalize(gradientEnd - gradientStart);
            float gl = length(gradientEnd - gradientStart);
            float t = clamp(dot(p - gradientStart, gd) / gl, 0.0, 1.0);
            baseColor.rgb = mix(mix(baseColor.rgb, gradientColor1.rgb, gradientColor1.a),
                               mix(baseColor.rgb, gradientColor2.rgb, gradientColor2.a), t);
        } else if (gradientType == 2) {
            float t = clamp(length(p) / length(shapeSize * 0.5), 0.0, 1.0);
            baseColor.rgb = mix(mix(baseColor.rgb, gradientColor1.rgb, gradientColor1.a),
                               mix(baseColor.rgb, gradientColor2.rgb, gradientColor2.a), t);
        }

        // Apply fill with optional 2.5D lighting
        if (enable25D == 1 && fillAlpha > 0.01) {
            vec3 litColor = compute25DLighting(p, sdf, baseColor.rgb, extrudeRadius,
                                               shapeType, shapeSize, shapeParams.x, shapeParams.y,
                                               shapeParams, shapeParams2);
            result = alphaOver(result, vec4(litColor, baseColor.a * fillAlpha));
        } else {
            result = alphaOver(result, vec4(baseColor.rgb, baseColor.a * fillAlpha));
        }

        // Center/Inside stroke (after fill)
        if (strokeWidth > 0.0 && strokeColor.a > 0.0 && strokeMode != 1) {
            vec4 strokeResult = renderCartoonStroke(sdf, p, adjustedAA,
                strokeColor, strokeColor2, strokeColor3,
                adjustedStrokeW, strokeSoftness, strokeMode, strokeStyle,
                strokeTaper, strokeWobble, strokeWobbleFreq,
                strokeOffset2, strokeSpacing, strokeNoise,
                strokePressure, brushAngle, brushRatio, animTime,
                shapeType, shapeSize, shapeParams.x, shapeParams.y, shapeParams, shapeParams2);
            result = alphaOver(result, strokeResult);
        }
    }

    // Premultiplied alpha output for correct RenderTexture compositing
    if (result.a < 0.005) result = vec4(0.0);
    else result.rgb *= result.a;
    finalColor = result;
}
)";

#endif // SDFSHADERS_H
