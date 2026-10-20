#include "SdfRenderer2D.h"
#include "parameter.h"

inline vec2 ToVec2(Vector2 v)    { return vec2(v.x, v.y); }
inline Vector2 ToVector2(vec2 v) { return {v.x, v.y};  }

SDF2Dnode::SDF2Dnode(SDFShape t, vec2 _pos, vec2 sz, float p1, float p2, float p3, float p4, float rot, cColor fill, cColor stroke, int strokeM, float strokeW, float strokeS)
    : type(t), pos(_pos), size(sz), param1(p1), param2(p2), param3(p3), param4(p4),
    rotation(rot) //, fillColor(fill), strokeColor(stroke),
    // strokeMode(strokeM), strokeWidth(strokeW), strokeSmoothness(strokeS)
{
    init();
}

void SDF2Dnode::init()
{
    bc.clear();
    // Stroke

    bc.addParam(ParamId::P_FILL_COLOR,       Param("Fill color", cColor(200, 100, 100, 255)));

    bc.addParam(ParamId::P_STROKE_COLOR,     Param("Stroke color", cColor(100, 100, 100, 255)));
    bc.addParam(ParamId::P_STROKE_WIDTH,     Param("Stroke width", 0.0f, 0.0f, 100.0f, 0.1f));
    bc.addParam(ParamId::P_STROKE_SMOOTH,    Param("Stroke smoothness", 0.0f, 0.0f, 100.0f, 0.1f));
    bc.addParam(ParamId::P_STROKE_MODE,      Param("Stroke mode", 0, {{0,"Center"},{1,"Outside"},{2,"Inside"}}));

    // bc.beginGroup(1, "Stroke");
    bc.addParam(ParamId::P_STROKE_DASHED,    Param("Enable dashed", false));
    bc.addParam(ParamId::P_STROKE_DASH_LEN,  Param("Dash lenght", 5.0f, 0.0f, 100.0f, 0.1f));
    bc.addParam(ParamId::P_STROKE_DASH_GAP,  Param("Gap lenght", 2.0f, 0.0f, 100.0f, 0.1f));
    bc.addParam(ParamId::P_STROKE_CAPSTYLE,  Param("Cap style", 0, {{0,"Butt"},{1,"Round"},{2,"Square"}}));

    bc.addParam(ParamId::P_STROKE_STYLE,     Param("Stroke style", 0, {{0,"Solid"},{1,"Tapered"},{2,"Sketchy"},{3,"Double"},{4,"Triple"},{5,"Offset Shadow"},{6,"Variable"},{7,"Ink Spread"},{8,"Brush"},{9,"Comic"},{10,"Neon"},{11,"Halftone"}}));
    bc.addParam(ParamId::P_STROKE_TAPER,     Param("Stroke taper", 0.5f, 0.0f, 1.0f, 0.01f));
    bc.addParam(ParamId::P_STROKE_WOOBLE,    Param("Stroke wobble", 3.0f, 0.0f, 100.0f, 0.1f));
    bc.addParam(ParamId::P_STROKE_WOOBLEFREQ,Param("Stroke wobble freq", 0.1f, 0.0f, 10.0f, 0.01f));
    bc.addParam(ParamId::P_STROKE_COLOR2,    Param("Stroke color 2", cColor(50, 50, 50, 255)));
    bc.addParam(ParamId::P_STROKE_COLOR3,    Param("Stroke color 3", cColor(50, 50, 50, 255)));
    bc.addParam(ParamId::P_STROKE_OFFSET,    Param("Stroke offset", vec2(3, 3), NONE));
    bc.addParam(ParamId::P_STROKE_SPACING,   Param("Stroke spacing", 4.0f, 0.0f, 100.0f, 0.1f));
    bc.addParam(ParamId::P_STROKE_NOISE,     Param("Stroke noise", 0.3f, 0.0f, 1.0f, 0.01f));
    bc.addParam(ParamId::P_STROKE_PRESSURE,  Param("Stroke pressure", 0.5f, 0.0f, 1.0f, 0.01f));
    bc.getParam(ParamId::P_STROKE_TAPER)->setVisDep(ParamId::P_STROKE_STYLE,1);
    bc.getParam(ParamId::P_STROKE_WOOBLE)->setVisDep(ParamId::P_STROKE_STYLE,1);
    bc.getParam(ParamId::P_STROKE_WOOBLEFREQ)->setVisDep(ParamId::P_STROKE_STYLE,1);
    bc.getParam(ParamId::P_STROKE_COLOR2)->setVisDep(ParamId::P_STROKE_STYLE,1);


    // Brush
    // bc.beginGroup(2, "Brush");
    bc.addParam(ParamId::P_BRUSH_ANGLE,      Param("Brush angle", 0.0f, -180.0f, 180.0f, NONE));
    bc.addParam(ParamId::P_BRUSH_RATIO,      Param("Brush ratio", 2.0f, 0.1f, 10.0f, 0.01f));

    // Glow
    bc.beginGroup(3,"Glow");
    bc.addParam(ParamId::P_GLOW_COLOR,       Param("Glow color", cColor(255, 255, 0, 0)));
    bc.addParam(ParamId::P_GLOW_SIZE,        Param("Glow size", 0.0f, 0.0f, 100.0f, 0.1f));
    bc.addParam(ParamId::P_GLOW_INT,         Param("Glow intensity", 1.0f, 0.0f, 10.0f, 0.01f));
    bc.addParam(ParamId::P_GLOW_MODE,        Param("Glow mode", 0, {{0,"additive"},{1,"soft"},{2,"replace"}}));

    // Shadow
    bc.beginGroup(4,"Shadow");
    bc.addParam(ParamId::P_SHADOW_COLOR,     Param("Shadow color", cTransparent));
    bc.addParam(ParamId::P_SHADOW_OFFSET,    Param("Shadow offset", vec2(5, 5), NONE, 0.1f));
    bc.addParam(ParamId::P_SHADOW_BLUR,      Param("Shadow blur", 5.0f, 0.0f, 100.0f, NONE));

    // Gradient
    bc.beginGroup(5,"Gradient");
    bc.addParam(ParamId::P_GRADIENT_TYPE,    Param("Gradient type", 0, {{0,"none"},{1,"linear"},{2,"radial"}}));
    bc.addParam(ParamId::P_GRADIENT_COLOR1,  Param("Gradient color 1", cWhite));
    bc.addParam(ParamId::P_GRADIENT_COLOR2,  Param("Gradient color 2", cBlack));
    bc.addParam(ParamId::P_GRADIENT_START,   Param("Gradient start", vec2(0, 0), NONE));
    bc.addParam(ParamId::P_GRADIENT_END,     Param("Gradient end", vec2(100, 0), NONE));
    bc.getParam(ParamId::P_GRADIENT_COLOR1)->setVisDep(ParamId::P_GRADIENT_TYPE,1);
    bc.getParam(ParamId::P_GRADIENT_COLOR2)->setVisDep(ParamId::P_GRADIENT_TYPE,1);
    bc.getParam(ParamId::P_GRADIENT_START)->setVisDep(ParamId::P_GRADIENT_TYPE,1);
    bc.getParam(ParamId::P_GRADIENT_END)->setVisDep(ParamId::P_GRADIENT_TYPE,1);


    // Deform
    bc.beginGroup(6,"Deform");
    bc.addParam(ParamId::P_DEFORM_TYPE,      Param("Deform type", 0, {{0,"none"},{1,"bend"},{2,"taper"},{3,"skew"},{4,"wave"},{5,"noise"}}));
    bc.addParam(ParamId::P_DEFORM_PARAM0,    Param("Deform param 1", 0.0f, -10.0f, 10.0f, 0.01f));
    bc.addParam(ParamId::P_DEFORM_PARAM1,    Param("Deform param 2", 1.0f, -10.0f, 10.0f, 0.01f));
    bc.addParam(ParamId::P_DEFORM_PARAM2,    Param("Deform param 3", 0.0f, -10.0f, 10.0f, 0.01f));
    bc.addParam(ParamId::P_DEFORM_PARAM3,    Param("Deform param 4", 0.0f, -10.0f, 10.0f, 0.01f));
    bc.addParam(ParamId::P_DEFORM_ANIMATE,   Param("Animate deform", false));

    // 2.5D Lighting
    bc.beginGroup(7,"2.5D Lighting");
    bc.addParam(ParamId::P_ENABLE_25D,       Param("Enable 2.5D", false));
    bc.addParam(ParamId::P_EXTRUDE_RADIUS,   Param("Extrude radius", 20.0f, 0.0f, 200.0f, NONE));
    bc.addParam(ParamId::P_LIGHT_DIR_X,      Param("Light dir X", -0.5f, -1.0f, 1.0f, 0.01f));
    bc.addParam(ParamId::P_LIGHT_DIR_Y,      Param("Light dir Y", -0.5f, -1.0f, 1.0f, 0.01f));
    bc.addParam(ParamId::P_LIGHT_DIR_Z,      Param("Light dir Z", 1.0f, 0.0f, 2.0f, 0.01f));
    bc.addParam(ParamId::P_AMBIENT,          Param("Ambient", 0.3f, 0.0f, 1.0f, 0.01f));
    bc.addParam(ParamId::P_DIFFUSE,          Param("Diffuse", 0.7f, 0.0f, 1.0f, 0.01f));
    bc.addParam(ParamId::P_SPECULAR,         Param("Specular", 0.5f, 0.0f, 1.0f, 0.01f));
    bc.addParam(ParamId::P_SPECULAR_POWER,   Param("Specular power", 32.0f, 1.0f, 128.0f, 0.1f));
    bc.addParam(ParamId::P_FRESNEL,          Param("Fresnel", 0.3f, 0.0f, 1.0f, 0.01f));
    bc.addParam(ParamId::P_FRESNEL_IOR,      Param("Fresnel IOR", 1.5f, 1.0f, 3.0f, 0.01f));

}

void SDF2Dnode::fromJson(json &j)
{
    type = j["type"];
    pos = vec2(j["pos"][0], j["pos"][1]);
    size = vec2(j["size"][0], j["size"][1]);

    init();
    param1 = j["param1"];
    param2 = j["param2"];
    param3 = j["param3"];
    param4 = j["param4"];
    rotation = j["rotation"];

    // fillColor = HexToColor(j.value("fillColor", "#FFFFFFFF"));
    // strokeColor = HexToColor(j.value("strokeColor", "#FFFFFFFF"));

    // strokeMode = j["strokeMode"];
    // strokeWidth = j["strokeWidth"];
    // strokeSmoothness = j["strokeSmoothness"];

    // glowColor = HexToColor(j.value("glowColor", "#FFFFFFFF"));
    // glowSize = j["glowSize"];
    // glowIntensity = j["glowIntensity"];
    // glowMode = (GlowMode)j["glowMode"];
    // shadowColor = HexToColor(j.value("shadowColor", "#00000000"));
    // shadowOffset = vec2(j["shadowOffset"][0], j["shadowOffset"][1]);

    // shadowBlur = j["shadowBlur"];
    // gradientType = j["gradientType"];
    // gradientColor1 = HexToColor(j.value("gradientColor1", "#FFFFFFFF"));
    // gradientColor2 = HexToColor(j.value("gradientColor2", "#FFFFFFFF"));
    // gradientStart = vec2(j["gradientStart"][0], j["gradientStart"][1]);
    // gradientEnd = vec2(j["gradientEnd"][0], j["gradientEnd"][1]);
    deformType = j["deformType"];
    deformParams[0] = j["deformParams"][0];
    deformParams[1] = j["deformParams"][1];
    deformParams[2] = j["deformParams"][2];
    deformParams[3] = j["deformParams"][3];
    animateDeform = j["animateDeform"];

    enable25D = j["enable25D"];
    extrudeRadius = j["extrudeRadius"];
    lightDir = vec3{j["lightDir"][0], j["lightDir"][1], j["lightDir"][2]};
    ambientStrength = j["ambientStrength"];
    diffuseStrength = j["diffuseStrength"];
    specularStrength = j["specularStrength"];
    specularPower = j["specularPower"];
    fresnelStrength = j["fresnelStrength"];
    fresnelIOR = j["fresnelIOR"];

    if(j.contains("parameters"))
    {
        bc.fromJSON(j["parameters"]);
    }

}

void SDF2Dnode::toJson(json &j)
{
    j["type"] = type;
    j["pos"] = CompactVec2Array(pos);
    j["size"] = CompactVec2Array(size);
    j["param1"] = param1;
    j["param2"] = param2;
    j["param3"] = param3;
    j["param4"] = param4;
    j["rotation"] = rotation;
    // j["fillColor"] = fillColor.toHex();
    // j["strokeColor"] = strokeColor.toHex();
    // j["strokeMode"] = strokeMode;
    // j["strokeWidth"] = strokeWidth;
    // j["strokeSmoothness"] = strokeSmoothness;
    // j["glowColor"] = glowColor.toHex();
    // j["glowSize"] = glowSize;
    // j["glowIntensity"] = glowIntensity;
    // j["glowMode"] = glowMode;
    // j["shadowColor"] = shadowColor.toHex();
    // j["shadowOffset"] = CompactVec2Array(shadowOffset);
    // j["shadowBlur"] = shadowBlur;
    // j["gradientType"] = gradientType;
    // j["gradientColor1"] = gradientColor1.toHex();
    // j["gradientColor2"] = gradientColor2.toHex();
    // j["gradientStart"] = CompactVec2Array(gradientStart);
    // j["gradientEnd"] = CompactVec2Array(gradientEnd);
    j["deformType"] = deformType;
    j["deformParams"] = {deformParams[0], deformParams[1], deformParams[2], deformParams[3]};
    j["animateDeform"] = animateDeform;
    j["enable25D"] = enable25D;
    j["extrudeRadius"] = extrudeRadius;
    j["lightDir"] = CompactVec3Array(lightDir.x, lightDir.y, lightDir.z);
    j["ambientStrength"] = ambientStrength;
    j["diffuseStrength"] = diffuseStrength;
    j["specularStrength"] = specularStrength;
    j["specularPower"] = specularPower;
    j["fresnelStrength"] = fresnelStrength;
    j["fresnelIOR"] = fresnelIOR;
    json arr = json::array();
    for (auto &p : bc.prmList)
    {
        json j;
        p.toJson(j);
        arr.push_back(j);
    }
    j["parameters"] = arr;

}

void SDF2Dnode::copyStyleFrom(SDF2Dnode &other)
{
    bc = other.bc;
    // fillColor = other.fillColor;
    // strokeColor = other.strokeColor;
    // strokeMode = other.strokeMode;
    // strokeWidth = other.strokeWidth;
    // strokeSmoothness = other.strokeSmoothness;
    // glowColor = other.glowColor;
    // glowSize = other.glowSize;
    // glowIntensity = other.glowIntensity;
    // glowMode = other.glowMode;
    // shadowColor = other.shadowColor;
    // shadowOffset = other.shadowOffset;
    // shadowBlur = other.shadowBlur;
    // gradientType = other.gradientType;
    // gradientColor1 = other.gradientColor1;
    // gradientColor2 = other.gradientColor2;
    // gradientStart = other.gradientStart;
    // gradientEnd = other.gradientEnd;
    deformType = other.deformType;
    for(int i=0;i<4;i++) deformParams[i] = other.deformParams[i];
    animateDeform = other.animateDeform;
    enable25D = other.enable25D;
    extrudeRadius = other.extrudeRadius;
    lightDir = other.lightDir;
    ambientStrength = other.ambientStrength;
    diffuseStrength = other.diffuseStrength;
    specularStrength = other.specularStrength;
    specularPower = other.specularPower;
    fresnelStrength = other.fresnelStrength;
    fresnelIOR = other.fresnelIOR;
}

rect SDF2Dnode::bbox()
{
    vec2 sz = size;
    vec2 pos0 = pos;
    pos = vec2(0,0);
    rect bb = rect(pos-sz/2, sz);
    switch(type)
    {
    case SDF_CIRCLE:       bb = rect(pos.x-param1,pos.y-param1,2*param1,2*param1); break;
    case SDF_TRAPEZOID:    sz = vec2(2.0f*fmax(size.x,size.y), 2*param1); bb = rect(pos-0.5*sz,sz); break;
    case SDF_PARALLELOGRAM:    sz = 2.0f*(size+vec2(fabs(param1),0)); bb = rect(pos-0.5*sz,sz); break;
    case SDF_TRIANGLE:
    case SDF_EQUILATERAL_TRIANGLE:
        sz = vec2(size.x, size.x * sqrt(3) / 2);
        bb = rect(pos - 0.5f * sz, sz);
        break;
    case SDF_PENTAGON:
    case SDF_HEXAGON:
    case SDF_OCTAGON:
    case SDF_HEXAGRAM:
        bb = rect(pos.x-param1,pos.y-param1,2*param1,2*param1);
        break;
    case SDF_STAR:
    case SDF_STAR5:
        bb = rect(pos.x-param1,pos.y-param1,2*param1,2*param1);
        break;
    case SDF_CUT_DISK:
        bb = rect(pos.x-param1,pos.y-param1,2*param1,2*param1);
        break;
    default:
        break;
    }
    pos = pos0;
    return bb;
}

bool SDF2Dnode::checkHit(vec2 &pt)
{
    float dist = (pt - pos).length();
    rect bb = bbox()+pos;
    switch(type)
    {
    case SDF_CIRCLE:
        return dist <= param1;
        break;
    default:
        return bb.isPointInRec(pt);
        break;
    }
}

void transferSDFparams(ParamContainer &bc, SdfRenderer2D &sdf)
{
    sdf.setFill(bc.getColor(ParamId::P_FILL_COLOR));
    // Stroke
    int strokeMode = bc.getInt(ParamId::P_STROKE_MODE);
    float strokeWidth = bc.getFloat(ParamId::P_STROKE_WIDTH);
    float strokeSmooth = bc.getFloat(ParamId::P_STROKE_SMOOTH);
    cColor strokeColor = bc.getColor(ParamId::P_STROKE_COLOR);

    if(strokeColor.a > 0) { sdf.setStroke(strokeColor, strokeWidth, strokeSmooth, strokeMode); }
    else { sdf.setStroke(cBlack, 0); }

    sdf.strokeStyle     = (StrokeStyle)bc.getInt(ParamId::P_STROKE_STYLE);
    sdf.strokeTaper     = bc.getFloat(ParamId::P_STROKE_TAPER);
    sdf.strokeWobble    = bc.getFloat(ParamId::P_STROKE_WOOBLE);
    sdf.strokeWobbleFreq= bc.getFloat(ParamId::P_STROKE_WOOBLEFREQ);
    sdf.strokeColor2    = bc.getColor(ParamId::P_STROKE_COLOR2);
    sdf.strokeColor3    = bc.getColor(ParamId::P_STROKE_COLOR3);
    vec2 offset         = bc.getVec2(ParamId::P_STROKE_OFFSET);
    sdf.strokeOffset2   = {offset.x, offset.y};
    sdf.strokeSpacing   = bc.getFloat(ParamId::P_STROKE_SPACING);
    sdf.strokeNoise     = bc.getFloat(ParamId::P_STROKE_NOISE);
    sdf.strokePressure  = bc.getFloat(ParamId::P_STROKE_PRESSURE);
    sdf.brushAngle      = bc.getFloat(ParamId::P_BRUSH_ANGLE);
    sdf.brushRatio      = bc.getFloat(ParamId::P_BRUSH_RATIO);

    sdf.dashEnabled     = bc.getBool(ParamId::P_STROKE_DASHED);
    sdf.dashLength      = bc.getFloat(ParamId::P_STROKE_DASH_LEN);
    sdf.dashGap         = bc.getFloat(ParamId::P_STROKE_DASH_GAP);
    sdf.dashCapStyle    = bc.getInt(ParamId::P_STROKE_CAPSTYLE);

    // Glow
    cColor glowColor    = bc.getColor(ParamId::P_GLOW_COLOR);
    float glowSize      = bc.getFloat(ParamId::P_GLOW_SIZE);
    float glowIntensity = bc.getFloat(ParamId::P_GLOW_INT);
    int glowMode        = bc.getInt(ParamId::P_GLOW_MODE);

    if (glowColor.a > 0 && glowIntensity > 0) {
        sdf.setGlow(glowColor, glowSize, glowIntensity, (GlowMode)glowMode);
    } else {
        sdf.setGlow({0,0,0,0}, 0);
    }

    // Shadow
    cColor shadowColor  = bc.getColor(ParamId::P_SHADOW_COLOR);
    vec2 shadowOffset   = bc.getVec2(ParamId::P_SHADOW_OFFSET);
    float shadowBlur    = bc.getFloat(ParamId::P_SHADOW_BLUR);

    if (shadowColor.a > 0) {
        sdf.setShadow(shadowColor, shadowOffset.x, shadowOffset.y, shadowBlur);
    } else {
        sdf.setShadow({0,0,0,0}, 0, 0, 0);
    }

    // Gradient
    int gradType        = bc.getInt(ParamId::P_GRADIENT_TYPE);
    cColor gradColor1   = bc.getColor(ParamId::P_GRADIENT_COLOR1);
    cColor gradColor2   = bc.getColor(ParamId::P_GRADIENT_COLOR2);
    vec2 gradStart      = bc.getVec2(ParamId::P_GRADIENT_START);
    vec2 gradEnd        = bc.getVec2(ParamId::P_GRADIENT_END);

    if (gradType == 0) sdf.clearGradient();
    else if (gradType == 1) sdf.setGradientLinear(gradColor1, gradColor2, gradStart, gradEnd);
    else if (gradType == 2) sdf.setGradientRadial(gradColor1, gradColor2);

    // Deform
    int deformType      = bc.getInt(ParamId::P_DEFORM_TYPE);
    bool animateDeform  = bc.getBool(ParamId::P_DEFORM_ANIMATE);
    float dp0           = bc.getFloat(ParamId::P_DEFORM_PARAM0);
    float dp1           = bc.getFloat(ParamId::P_DEFORM_PARAM1);
    float dp2           = bc.getFloat(ParamId::P_DEFORM_PARAM2);

    if (deformType > 0) {
        float tm = animateDeform ? GetTime() : 0;
        switch (deformType) {
        case 1: sdf.setDeformBend(dp1); break;
        case 2: sdf.setDeformTaper(dp0); break;
        case 3: sdf.setDeformSkew(dp0); break;
        case 4: sdf.setDeformWave(dp0, dp1, dp2 + tm); break;
        case 5: sdf.setDeformNoise(dp0, dp1, dp2 + tm); break;
        }
    } else {
        sdf.clearDeform();
    }

    // 2.5D
    bool enable25D = bc.getBool(ParamId::P_ENABLE_25D);

    if (enable25D) {
        sdf.enable25DMode(bc.getFloat(ParamId::P_EXTRUDE_RADIUS));
        sdf.setLightDirection(
            bc.getFloat(ParamId::P_LIGHT_DIR_X),
            bc.getFloat(ParamId::P_LIGHT_DIR_Y),
            bc.getFloat(ParamId::P_LIGHT_DIR_Z)
            );
        sdf.setAmbient(bc.getFloat(ParamId::P_AMBIENT));
        sdf.setDiffuse(bc.getFloat(ParamId::P_DIFFUSE));
        sdf.setSpecular(bc.getFloat(ParamId::P_SPECULAR), bc.getFloat(ParamId::P_SPECULAR_POWER));
        sdf.setFresnel(bc.getFloat(ParamId::P_FRESNEL), bc.getFloat(ParamId::P_FRESNEL_IOR));
    } else {
        sdf.disable25D();
    }
}

void SDF2Dnode::draw(SdfRenderer2D &sdf,bool selected)
{

    transferSDFparams(bc, sdf);

    // Selection override
    if (selected) {
        sdf.setFill(cBlank);
        sdf.setStroke(cYellow, 2);
    }

    sdf.setRotation(rotation*DEG2RAD);

    switch(type)
    {
    case SDF_CIRCLE:        sdf.drawCircle(pos.x, pos.y, param1);   break;
    case SDF_BOX:           sdf.drawBox(pos.x, pos.y, size.x, size.y);            break;
    case SDF_ROUNDED_BOX:   sdf.drawRoundedBoxEx(pos.x-size.x/2, pos.y-size.y/2, size.x, size.y, param1, param2, param3, param4); break;
    case SDF_SEGMENT:       sdf.drawSegment(pos.x, pos.y, pos.x+size.x, pos.y+size.y, param1); break;
    case SDF_RHOMBUS:       sdf.drawRhombus(pos.x, pos.y, size.x, size.y); break;
    case SDF_TRAPEZOID:     sdf.drawTrapezoid(pos.x, pos.y, size.x, size.y, param1); break;
    case SDF_PARALLELOGRAM: sdf.drawParallelogram(pos.x, pos.y, size.x, size.y, param1); break;
    case SDF_TRIANGLE:
    case SDF_EQUILATERAL_TRIANGLE: sdf.drawTriangle(pos.x, pos.y, param1); break;
    case SDF_PENTAGON:  sdf.drawPentagon(pos.x, pos.y, param1);            break;
    case SDF_HEXAGON:   sdf.drawHexagon(pos.x, pos.y, param1); break;
    case SDF_OCTAGON:   sdf.drawOctagon(pos.x, pos.y, param1);            break;
    case SDF_HEXAGRAM:  sdf.drawHexagram(pos.x, pos.y, param1); break;
    case SDF_STAR5:     sdf.drawStar5(pos.x, pos.y, param1, (int)param2);  break;
    case SDF_STAR:      sdf.drawStar(pos.x, pos.y, param1, (int)param2, param3);  break;
    case SDF_PIE:       sdf.drawPie(pos.x, pos.y, param1, param2);            break;
    case SDF_CUT_DISK:  sdf.drawCutDisk(pos.x, pos.y, param1, param2); break;
    case SDF_ARC:       sdf.drawArc(pos.x, pos.y, param1, param2, param3);            break;
    case SDF_VESICA:    sdf.drawVesica(pos.x, pos.y, param1, param2); break;
    case SDF_MOON:      sdf.drawMoon(pos.x, pos.y, param1, param2, param3);            break;
    case SDF_EGG:       sdf.drawEgg(pos.x, pos.y, param1, param2);            break;
    case SDF_HEART:     sdf.drawHeart(pos.x, pos.y, param1); break;
    case SDF_CROSS:     sdf.drawCross(pos.x, pos.y, param1, param2); break;
    case SDF_ROUNDED_X:     sdf.drawRoundedX(pos.x, pos.y, param1, param2); break;
    case SDF_ELLIPSE:       sdf.drawEllipse(pos.x, pos.y, size.x/2, size.y/2); break;
    case SDF_ROUNDED_CROSS: sdf.drawRoundedCross(pos.x, pos.y, param1, param2); break;
    case SDF_COOL_S:        sdf.drawCoolS(pos.x, pos.y, param1); break;
    case SDF_BLOBBY_CROSS:  sdf.drawBlobbyCross(pos.x, pos.y, param1, param2); break;
    case SDF_TUNNEL:        sdf.drawTunnel(pos.x, pos.y, param1, param2); break;
    case SDF_BEZIER:        sdf.drawBezier(pos.x, pos.y, ToVector2(vec2(param1, param2)), ToVector2(vec2(param3, param4)), ToVector2(pos+size), 3); break;

    // Add cases for other shape types
    default:
        break;
    }
}
