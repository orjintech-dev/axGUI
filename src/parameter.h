#ifndef PARAMETER_H
#define PARAMETER_H
#pragma once

// #include "rlgl.h"
#include "axDefs.h"
#include <vector>
#include <string>
#include <map>
#include <json.hpp>


// Enum to manage which type is currently in use
// enum NODE_TYPE { TP_NONE=0, TP_BASE, TP_HANDLE, TP_NODE3D, TP_SPLINE, TP_CAMERA, TP_LIGHT, TP_PRIMITIVE, TP_CLONER, TP_SHAPE, TP_EXTRUDE, TP_PARAM, TP_PARAM_CONTAINER };
enum CLASS_TYPE { TP_NONE=0, TP_BASE, TP_HANDLE, TP_IMAGE, TP_TEXT, TP_PATH, TP_LINE, TP_SPLINE, TP_TABLE, TP_CAMERA, TP_LIGHT, TP_PRIMITIVE, TP_CLONER,
                  TP_SHAPE, TP_EXTRUDE, TP_PARAM, TP_PARAM_CONTAINER, TP_DST};

enum NODE2D_TYPE { SHP_ROOT=0, SHP_HANDLE, SHP_GROUP, SHP_LINE, SHP_SDF, SHP_CIRCLE, SHP_RECT, SHP_PATH, SHP_POLYGON, SHP_STAR, SHP_TEXT, SHP_IMAGE, SHP_ARC, SHP_CLONER, SHP_TABLE, SHP_PIE, SHP_GRAPH, SHP_GRID, SHP_DST, SHP_NONE=-1 };
static cString shapeName[] = { "Root", "Handle", "Group", "Line", "SDF", "Circle", "Rectangle", "Path", "Polygon", "Star", "Text", "Image", "Arc", "Cloner", "Table", "Pie", "Graph", "Grid", "Dst", "None" };

static NODE2D_TYPE getShapeType(const cString &name)
{
    for(int i=0; i<sizeof(shapeName)/sizeof(cString); i++)
    {
        if(shapeName[i]==name) return (NODE2D_TYPE)i;
    }
    return SHP_NONE;
}

enum { NONE=0, AXISX=1<<0, AXISY=1<<1, FREE=1<<2, RADIUS=1<<3, ANGLE=1<<4, DOUBLE=1<<5};
#define PARAM_ID(_id, _enum, _name) _id = _enum,

    enum class ParamId : uint16_t {
    // Core transform
    PARAM_ID(P_UNKNOWN,             0, "unknown")
    PARAM_ID(P_TYPE,                1, "type")
    PARAM_ID(P_POS,                 2, "pos")
    PARAM_ID(P_ROT,                 3, "rot")
    PARAM_ID(P_SCALE,               4, "scale")
    PARAM_ID(P_OPACITY,             5, "opacity")
    PARAM_ID(P_COLOR,               6, "color")
    PARAM_ID(P_USE_TEXTURE,         7, "useTexture")
    PARAM_ID(P_FONTSIZE,            8, "fontsize")
    PARAM_ID(P_TEXT,                9, "text")
    PARAM_ID(P_SDF_SHAPE,           10, "sdfshape")

    // Dimensions
    PARAM_ID(P_WIDTH,               12, "width")
    PARAM_ID(P_HEIGHT,              13, "height")
    PARAM_ID(P_BEVEL,               14, "bevel")
    PARAM_ID(P_RADIUS,              15, "radius")
    PARAM_ID(P_RADIUS2,             16, "radius2")
    PARAM_ID(P_ANGLE,               17, "angle")
    PARAM_ID(P_ANGLE2,              18, "angle2")
    PARAM_ID(P_THICKNESS,           19, "thickness")


    // Count/Array
    PARAM_ID(P_COUNT,               30, "count")
    PARAM_ID(P_COUNT_X,             31, "count.x")
    PARAM_ID(P_COUNT_Y,             32, "count.y")
    PARAM_ID(P_COUNT_Z,             33, "count.z")

    // General
    PARAM_ID(P_OFFSET,              40, "offset")
    PARAM_ID(P_ROUND,               41, "round")
    PARAM_ID(P_ROUND2,              42, "round2")
    PARAM_ID(P_FEATHER,             43, "feather")
    PARAM_ID(P_TEETH_CNT,           44, "teethCount")

    PARAM_ID(P_FILL_MODE,           80, "fillMode")
    PARAM_ID(P_FILL_COLOR,          81, "fillColor")
    PARAM_ID(P_FILL_GLSL,           82, "fillGlsl")

    PARAM_ID(P_BRIGHTNESS,          83, "brightness")
    PARAM_ID(P_CONTRAST,            84, "contrast")
    PARAM_ID(P_SATURATION,          85, "saturation")
    PARAM_ID(P_HUE,                 86, "hue")
    PARAM_ID(P_BLUR,                87, "blur")




    // Stroke 100-149
    PARAM_ID(P_STROKE_MODE,         100, "strokeMode")
    PARAM_ID(P_STROKE_STYLE,        101, "strokeStyle")
    PARAM_ID(P_STROKE_WIDTH,        102, "strokeWidth")
    PARAM_ID(P_STROKE_SMOOTH,       103, "strokeSmoothness")
    PARAM_ID(P_STROKE_TAPER,        104, "strokeTaper")
    PARAM_ID(P_STROKE_WOOBLE,       105, "strokeWobble")
    PARAM_ID(P_STROKE_WOOBLEFREQ,   106, "strokeWobbleFreq")
    PARAM_ID(P_STROKE_COLOR,        107, "strokeColor")
    PARAM_ID(P_STROKE_COLOR2,       108, "strokeColor2")
    PARAM_ID(P_STROKE_COLOR3,       109, "strokeColor3")
    PARAM_ID(P_STROKE_OFFSET,       110, "strokeOffset2")
    PARAM_ID(P_STROKE_SPACING,      111, "strokeSpacing")
    PARAM_ID(P_STROKE_NOISE,        112, "strokeNoise")
    PARAM_ID(P_STROKE_PRESSURE,     113, "strokePressure")

    PARAM_ID(P_STROKE_DASHED,       114, "dashEnabled")
    PARAM_ID(P_STROKE_DASH_LEN,     115, "dashLenght")
    PARAM_ID(P_STROKE_DASH_GAP,     116, "dashGap")
    PARAM_ID(P_STROKE_CAPSTYLE,     117, "strokeCapStyle")
    PARAM_ID(P_STROKE_DASH_OFF,     118, "dashOffset")
    PARAM_ID(P_ENABLE_CUSTOMDASH,   119, "customDashEnabled")
    PARAM_ID(P_DASH_PATTERN,        120, "dashPattern")


    // Brush 150-199
    PARAM_ID(P_BRUSH_ANGLE,         150, "brushAngle")
    PARAM_ID(P_BRUSH_RATIO,         151, "brushRatio")

    // Effects - Glow 200-229
    PARAM_ID(P_GLOW_COLOR,          200, "glowColor")
    PARAM_ID(P_GLOW_SIZE,           201, "glowSize")
    PARAM_ID(P_GLOW_INT,            202, "glowIntensity")
    PARAM_ID(P_GLOW_MODE,           203, "glowMode")

    // Effects - Shadow 230-259
    PARAM_ID(P_SHADOW_COLOR,        210, "shadowColor")
    PARAM_ID(P_SHADOW_OFFSET,       211, "shadowOffset")
    PARAM_ID(P_SHADOW_BLUR,         212, "shadowBlur")

    // Gradient 220-229
    PARAM_ID(P_GRADIENT_TYPE,       220, "gradientType")
    PARAM_ID(P_GRADIENT_COLOR1,     221, "gradientColor1")
    PARAM_ID(P_GRADIENT_COLOR2,     222, "gradientColor2")
    PARAM_ID(P_GRADIENT_START,      223, "gradientStart")
    PARAM_ID(P_GRADIENT_END,        224, "gradientEnd")

    // Deform 230-249
    PARAM_ID(P_DEFORM_TYPE,         230, "deformType")
    PARAM_ID(P_DEFORM_PARAM0,       231, "deformParam0")
    PARAM_ID(P_DEFORM_PARAM1,       232, "deformParam1")
    PARAM_ID(P_DEFORM_PARAM2,       233, "deformParam2")
    PARAM_ID(P_DEFORM_PARAM3,       234, "deformParam3")
    PARAM_ID(P_DEFORM_ANIMATE,      235, "animateDeform")

    // 2.5D Lighting 250-269
    PARAM_ID(P_ENABLE_25D,          250, "enable25D")
    PARAM_ID(P_EXTRUDE_RADIUS,      251, "extrudeRadius")
    PARAM_ID(P_LIGHT_DIR_X,         252, "lightDirX")
    PARAM_ID(P_LIGHT_DIR_Y,         253, "lightDirY")
    PARAM_ID(P_LIGHT_DIR_Z,         254, "lightDirZ")
    PARAM_ID(P_AMBIENT,             255, "ambientStrength")
    PARAM_ID(P_DIFFUSE,             256, "diffuseStrength")
    PARAM_ID(P_SPECULAR,            257, "specularStrength")
    PARAM_ID(P_SPECULAR_POWER,      258, "specularPower")
    PARAM_ID(P_FRESNEL,             259, "fresnelStrength")
    PARAM_ID(P_FRESNEL_IOR,         260, "fresnelIOR")

    PARAM_ID(P_DIMENSION,       280, "dimension")
    PARAM_ID(P_BEGIN,           281, "begin")
    PARAM_ID(P_END,             282, "end")
    PARAM_ID(P_ARROW,           283, "arrow")
    PARAM_ID(P_END1,            284, "end1")
    PARAM_ID(P_END2,            285, "end2")



    // Position components 300-319
    PARAM_ID(P_POSITION_X,          300, "positionX")
    PARAM_ID(P_POSITION_Y,          301, "positionY")

    PARAM_ID(P_PROGRESS,        310, "progress")
    PARAM_ID(P_SHOW_JUMPS,      311, "jumps")
    PARAM_ID(P_SHOW_POINTS,     312, "points")
    PARAM_ID(P_STITCH_WIDTH,    313, "stitch width")
    PARAM_ID(P_THREAD_SHADE,    314, "thread shade")

    // ... leave room for other categories

    PARAM_ID(P_LAST_ELEMENT,        999, "_lastElement")
};

#undef PARAM_ID

// For name lookup (only needed in debug/serialization)
    inline const char* paramIdToName(ParamId id) {
        switch(id) {
#define PARAM_ID(_id, _enum, _name) case ParamId::_id: return _name;
            // ... include all definitions
#undef PARAM_ID
        default: return "unknown";
        }
    }

struct VisDependency {
    ParamId controllerId;

    int visibleWhen; // its value
    VisDependency() : controllerId(ParamId::P_UNKNOWN), visibleWhen(1) {}
};

class Param {
public:
    bool dirty = false, marked=false;
    int ix = -1; // index in ParamContainer
    int grpId = -1; // group id
    ParamId id = ParamId::P_UNKNOWN;
    cString name;
    Data2D value, defVal, minVal, maxVal;
    Int handle = NONE;
    DataType _type = DataType::INT;
    float speed = 1.0f;
    std::vector<IdStrPair> enumVal;
    cString strValue;
    VisDependency visDep;

    // Default constructor
    Param() : value(0) {}

    // Type and name constructor
    Param(const cString &nm, DataType t) : name(nm) {
        switch(t) {
        case DataType::BOOL: value = false; break;
        case DataType::INT: value = 0; break;
        case DataType::FLOAT: value = 0.0f; break;
        case DataType::VEC2: value = vec2(0, 0); break;
        case DataType::STRING: break; // strValue handles this
        case DataType::ENUM: value = 0; break;
        case DataType::COLOR: value = cWhite;
            break;
        }
    }

    // Bool constructors
    Param(bool val) : value(val), handle(NONE), _type(DataType::BOOL) {}
    Param(const cString &nm, bool val) : name(nm), value(val), handle(NONE), _type(DataType::BOOL) {}

    // Int constructors
    Param(const cString &nm, int val, int mn = 0, int mx = 100) : name(nm), value(val), defVal(val), minVal(mn), maxVal(mx), handle(NONE), _type(DataType::INT) {}

    // Enum constructor
    Param(const cString &nm, int val, const std::vector<IdStrPair> &vals) : name(nm), value(val), defVal(val), handle(NONE), _type(DataType::ENUM), enumVal(vals) {}

    // Float constructors
    Param(float val, float mn, float mx) : value(val), defVal(val), minVal(mn), maxVal(mx), handle(NONE), _type(DataType::FLOAT) {}
    Param(const cString &nm, float val, float mn, float mx, float spd = 1.0f) : name(nm), value(val), defVal(val), minVal(mn), maxVal(mx), handle(NONE), _type(DataType::FLOAT), speed(spd) {}
    Param(const cString &nm, float val, float mn, float mx, int handle) : name(nm), value(val), defVal(val), minVal(mn), maxVal(mx), handle(handle), _type(DataType::FLOAT) {}

    // Vec2 constructors
    Param(const cString &nm, const vec2 &val, int handle)
        : name(nm), value(val), defVal(val),
        minVal(vec2(-10e23, -10e23)), maxVal(vec2(10e23, 10e23)), handle(handle), _type(DataType::VEC2) {}

    Param(const cString &nm, const vec2 &val, int handle, float spd)
        : name(nm), value(val), defVal(val),
        minVal(vec2(-10e23, -10e23)), maxVal(vec2(10e23, 10e23)), handle(handle), _type(DataType::VEC2), speed(spd) {}


    Param(const vec2 &val) : value(val), _type(DataType::VEC2) {}

    // Color constructor
    Param(const cString &nm, const cColor &val) : name(nm), value(val), defVal(val), _type(DataType::COLOR) {}

    // String constructor
    Param(const cString &nm, const cString &val) : name(nm), _type(DataType::STRING), strValue(val) {}

    void setVisDep(ParamId controller, int visibleWhen) {
        visDep.controllerId = controller;
        visDep.visibleWhen = visibleWhen;
    }

    // Helper to get current type
    DataType type() const
    {
        return _type;

        // if (value.holds<bool>()) return DataType::BOOL;
        // if (value.holds<int>()) {
        //     return enumVal.empty() ? DataType::INT : DataType::ENUM;
        // }
        // if (value.holds<float>()) return DataType::FLOAT;
        // if (value.holds<vec2>()) return DataType::VEC2;
        // if (!strValue.empty()) return DataType::STRING;
        // if (value.holds<cColor>()) return DataType::COLOR;
        // return DataType::INT;
    }

    // Serialization
    cString serialize()
    {
        cString s;
        DataType t = type();
        s += "{type:" + LS(t) + ", name:" + name + ", handle:" + LS(handle);

        if(t==DataType::INT) {
            s += ", val:" + LS(value.getInt()) +
                 ", min:" + LS(minVal.getInt()) +
                 ", max:" + LS(maxVal.getInt());
        }
        if(t==DataType::BOOL) {
            s += ", val:" + LS(value.getBool()) + ", xx:12";
        }
        if(t==DataType::FLOAT) {
            s += ", val:" + RS(value.getFloat()) +
                 ", min:" + RS(minVal.getFloat()) +
                 ", max:" + RS(maxVal.getFloat());
        }
        if(t==DataType::STRING) {
            s += ", val:" + strValue;
        }
        if(t==DataType::ENUM) {
            s += ", val:" + LS(value.getInt()) + ", enums:(";
            int i = 0, cnt = (int)enumVal.size();
            for (auto &ev : enumVal) {
                s += LS(ev.id) + "," + ev.title;
                if (i < cnt - 1) s += ",";
                i++;
            }
            s += ")";
        }
        if(t==DataType::COLOR) {
            cColor col = value.getColor();
            s += ", val:(" + LS(col.r) + "," + LS(col.g) + "," + LS(col.b) + "," + LS(col.a) + ")";
        }
        s += "}";
        return s;
    }

    inline ParamId nameToParamId(const cString& name) {
        static std::map<cString, ParamId> nameMap = {
            {"pos", ParamId::P_POS},
            {"rot", ParamId::P_ROT},
            {"scale", ParamId::P_SCALE},
            // ... others
        };
        auto it = nameMap.find(name);
        return (it != nameMap.end()) ? it->second : ParamId::P_UNKNOWN;
    }

    void toJson(json &j)
    {
        DataType t = type();
        // j = json{{"type", t}, {"name", name}, {"handle", handle}};
        j = json{{"type", t}, {"name", name}, {"handle", handle}, {"id", (int)id}};

        if(t==DataType::INT) {
            j["val"] = value.getInt();
            j["min"] = minVal.getInt();
            j["max"] = maxVal.getInt();
        }
        if(t==DataType::BOOL) {
            j["val"] = value.getBool();
        }
        if(t==DataType::FLOAT) {
            j["val"] = value.getFloat();
            j["min"] = minVal.getFloat();
            j["max"] = maxVal.getFloat();
        }
        if(t==DataType::STRING) {
            j["val"] = strValue;
        }
        if(t==DataType::VEC2) {
            j["val"] = "vec2(" + V2S(value.getVec2()) + ")";
        }
        if(t==DataType::ENUM) {
            j["val"] = value.getInt();
        }
        if(t==DataType::COLOR) {
            j["val"] = ColorToHex(value.getColor());
        }
    }

    void fromJSON(json &j)
    {
        name = j["name"];
        handle = j["handle"];
        if (j.contains("id")) id = (ParamId)(int)j["id"];
        else id = nameToParamId(name);
        DataType type = j["type"];

        if (type == DataType::FLOAT) {
            value.set((float)j["val"]);
            if (j.contains("min")) minVal.set((float)j["min"]);
            if (j.contains("max")) maxVal.set((float)j["max"]);
        }
        if (type == DataType::INT) {
            value.set((int)j["val"]);
            if (j.contains("min")) minVal.set((int)j["min"]);
            if (j.contains("max")) maxVal.set((int)j["max"]);
        }
        if (type == DataType::BOOL) {
            value.set((bool)j["val"]);
        }
        if (type == DataType::STRING) {
            strValue = j["val"];
        }
        if (type == DataType::VEC2) {
            value.set(toVec2(j["val"]));
        }
        if (type == DataType::ENUM) {
            value.set((int)j["val"]);
        }
        if (type == DataType::COLOR) {
            value.set(HexToColor(j["val"]));
        }
    }

    inline cColor HexToColor(const std::string& hex) {
        cColor c{0, 0, 0, 255};
        if (hex.size() >= 7 && hex[0] == '#') {
            sscanf(hex.c_str() + 1, "%02hhX%02hhX%02hhX", &c.r, &c.g, &c.b);
            if (hex.size() >= 9) {
                sscanf(hex.c_str() + 7, "%02hhX", &c.a);
            }
        }
        return c;
    }

    inline cString ColorToHex(const cColor &c) {
        char buf[10];
        snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
        return cString(buf);
    }

    void setName(const cString &nm) { name = nm; }
    void setCombo(const char *items) { strValue = cString(items); }

    // Getters
    float getFloat()    { return value.getFloat(); }
    bool getBool()      { return value.getBool(); }
    int getInt()        { return value.getInt(); }
    vec2 getVec2()      { return value.getVec2(); }
    cColor getColor()   { return value.getColor(); }
    cString getString() { return strValue; }
    float getMin()      { return minVal.getFloat(); }
    float getMax()      { return maxVal.getFloat(); }
    float* floatPtr()   { return value.getFloatPtr(); } // Careful with this!
    int* intPtr()       { return value.getIntPtr(); }   // Careful with this!
    bool* boolPtr()     { return value.getBoolPtr(); }  // Careful with this!
    cColor* colorPtr()  { return value.getColorPtr(); } // Careful with this!

    void readValue(Data2D &d, const cString &val) {
        DataType t = type();
        if(t == DataType::FLOAT)  d.set((float)atof(val.c_str()));
        if(t==DataType::INT)    d.set(atoi(val.c_str()));
        if(t==DataType::ENUM)   d.set(atoi(val.c_str()));
        if(t==DataType::BOOL)   d.set((bool)atoi(val.c_str()));
        if(t==DataType::VEC2)   d.set(toVec2(val));
        if(t==DataType::COLOR)  { cColor c; c.fromStr(val); d.set(c); }
        if(t==DataType::STRING) strValue = val;
    }

    // Setters
    void setVal(float v)  { dirty = (value.getFloat()!=v); value.set(v); }
    void setVal(int v)    { dirty = (value.getInt()!=v); value.set(v); axLog("SET INT"); }
    void setVal(bool v)   { dirty = (value.getBool()!=v); value.set(v);  }
    void setVal(const vec2 &v)    { dirty = (value.getVec2()!=v);  value.set(v);  }
    void setVal(const cString &v) { dirty = (strValue!=v);  strValue = v;  }
    void setVal(const cColor &v) { dirty = (value.getColor()!=v); value.set(v);  }

    void setMin(float v) { minVal.set(v); }
    void setMin(int v)   { minVal.set(v); }
    void setMin(bool v)  { minVal.set(v); }
    void setMin(const vec2 &v) { minVal.set(v); }

    void setMax(float v) { maxVal.set(v); }
    void setMax(int v)   { maxVal.set(v); }
    void setMax(bool v)  { maxVal.set(v); }
    void setMax(const vec2 &v) { maxVal.set(v); }

    // Comparison operators
    bool operator==(const Param &other) const {
        DataType t = type();
        DataType otherType = other.type();

        if (t != otherType) return false;

        if(t==DataType::FLOAT) return value.getFloat() == other.value.getFloat();
        if(t==DataType::INT) return value.getInt() == other.value.getInt();
        if(t==DataType::ENUM) return value.getInt() == other.value.getInt();
        if(t==DataType::BOOL) return value.getBool() == other.value.getBool();
        if(t==DataType::VEC2) return value.getVec2() == other.value.getVec2();
        if(t==DataType::STRING) return strValue == other.strValue;
        if(t==DataType::COLOR) return value.getColor() == other.value.getColor();

        return false;
    }

    bool operator!=(const Param &other) const {
        return !(*this == other);
    }

    void copyTo(Param *other)
    {
        DataType t = type();
        DataType otherType = other->type();

        if (t != otherType) return;
        *other = *this; // Copy common fields first

        if(t==DataType::FLOAT) other->value.set(value.getFloat());
        else if(t==DataType::INT) { other->value.set(value.getInt()); axLog("* "+name+", val:"+LS(value.getInt())); }
        else if(t==DataType::ENUM) { other->value.set(value.getInt()); axLog("* "+name+", val:"+LS(value.getInt())); }
        else if(t==DataType::BOOL) other->value.set(value.getBool());
        else if(t==DataType::VEC2) other->value.set(value.getVec2());
        else if(t==DataType::STRING) other->strValue = strValue;
        else if(t==DataType::COLOR) other->value.set(value.getColor());
    }

    cString infoStr() {
        cString s = name + "=";
        DataType t = type();

        if(t==DataType::FLOAT) s += axFormat("%.2f", value.getFloat());
        if(t==DataType::INT) s += axFormat("%d", value.getInt());
        if(t==DataType::BOOL) s += axFormat("%s", value.getBool() ? "true" : "false");
        if(t==DataType::VEC2) {
            vec2 v = value.getVec2();
            s += axFormat("%x(%.2f, %.2f)", this, v.x, v.y);
        }
        if(t==DataType::STRING) s += axFormat("%s", strValue.c_str());
        if(t==DataType::ENUM) s += axFormat("%d", value.getInt());
        if(t==DataType::COLOR) {
            cColor col = value.getColor();
            s += axFormat("(%d, %d, %d, %d)", col.r, col.g, col.b, col.a);
        }

        return s;
    }
};


//-----------------------------------------------------------------------------------
struct ParamContainer
{
    int actGroup=-1;
    std::map<int,cString> grpList;
    std::vector<Param> prmList;
    std::array<int16_t, 1000> idToIndex;  // ParamId -> index, -1 = absent

    ParamContainer() {
        idToIndex.fill(-1);
    }

    void beginGroup(int gid, const cString &name)
    {
        actGroup = gid;
        grpList[gid] = name;
    }
    void endGroup()
    {
        actGroup = -1;
    }
    cString groupName(int gid) const
    {
        auto it = grpList.find(gid);
        return (it != grpList.end()) ? it->second : "";
    }
    //--- ID-based add (recommended) ---
    int _lastInsertIx = -1;

    void addParam(ParamId id, const Param &p) {
        int16_t existing = idToIndex[(int)id];
        if (existing >= 0) {
            Param &old = prmList[existing];
            old.name = p.name;
            old.defVal = p.defVal;
            old.minVal = p.minVal;
            old.maxVal = p.maxVal;
            old.handle = p.handle;
            old.speed = p.speed;
            old.enumVal = p.enumVal;
            old.visDep = p.visDep;
            old.grpId = actGroup;
            old.marked = false;
            _lastInsertIx = -1;
            return;
        }

        Param newParam = p;
        newParam.id = id;
        newParam.grpId = actGroup;
        newParam.marked = false;
        newParam.ix = (int)prmList.size();
        idToIndex[(int)id] = prmList.size();
        prmList.push_back(newParam);
        _lastInsertIx = -1;
    }

    void insertParam(ParamId id, const Param &p) {
        int16_t existing = idToIndex[(int)id];
        if (existing >= 0) {
            Param &old = prmList[existing];
            old.name = p.name;
            old.defVal = p.defVal;
            old.minVal = p.minVal;
            old.maxVal = p.maxVal;
            old.handle = p.handle;
            old.speed = p.speed;
            old.enumVal = p.enumVal;
            old.visDep = p.visDep;
            old.grpId = actGroup;
            old.marked = false;
            _lastInsertIx = existing;
            return;
        }

        Param newParam = p;
        newParam.id = id;
        newParam.grpId = actGroup;
        newParam.marked = false;

        int insertAt = (_lastInsertIx >= 0) ? _lastInsertIx + 1 : (int)prmList.size();

        prmList.insert(prmList.begin() + insertAt, newParam);
        _lastInsertIx = insertAt;
        rebuildIndex();
    }

    void markAll()
    {
        for (auto &p : prmList) {
            p.marked = true;
        }
    }

    void deleteMarked()
    {
        std::vector<Param> newList;
        idToIndex.fill(-1);
        for (size_t i = 0; i < prmList.size(); i++) {
            if (!prmList[i].marked) {
                prmList[i].ix = (int)newList.size();
                if (prmList[i].id != ParamId::P_UNKNOWN) {
                    idToIndex[(int)prmList[i].id] = newList.size();
                }
                newList.push_back(prmList[i]);
            }
        }
        prmList = std::move(newList);
    }
    void copyFrom(ParamContainer &src)
    {
        for(auto &p : prmList)
        {
            Param *srcPrm = src.getParam(p.id);
            if(srcPrm) p.copyTo(srcPrm);
        }
    }

    //--- Legacy: string-based add ---
    void addParam(const Param &p) {
        Param newParam = p;
        newParam.ix = (int)prmList.size();
        newParam.grpId = actGroup;
        prmList.push_back(newParam);
    }

    void setParams(const std::vector<Param> &list) {
        prmList = list;
        rebuildIndex();
    }

    void rebuildIndex() {
        idToIndex.fill(-1);
        for (size_t i = 0; i < prmList.size(); i++) {
            if (prmList[i].id != ParamId::P_UNKNOWN) {
                idToIndex[(int)prmList[i].id] = i;
            }
            prmList[i].ix = i;
        }
    }

    void clear()
    {
        actGroup = -1;
        prmList.clear();
        idToIndex.fill(-1);
    }
    //=======================================================================
    // FAST ACCESS - for the draw loop (O(1))
    //=======================================================================

    bool isVisible(Param &prm)
    {
        if(prm.visDep.controllerId==ParamId::P_UNKNOWN) return true;
        int16_t ix = idToIndex[(int)prm.visDep.controllerId];
        if(ix<0) return true; // treated as visible if the controller param is missing
        Param &ctrlPrm = prmList[ix];
        int ctrlVal = ctrlPrm.getInt();
        return (ctrlVal == prm.visDep.visibleWhen);
    }

    // Direct reference - fastest, use when the param is known to exist
    Param& get(ParamId id) {
        return prmList[idToIndex[(int)id]];
    }

    const Param& get(ParamId id) const {
        return prmList[idToIndex[(int)id]];
    }

    // Safe version
    Param* getParam(ParamId id) {
        int16_t ix = idToIndex[(int)id];
        return (ix >= 0) ? &prmList[ix] : nullptr;
    }

    bool has(ParamId id) const {
        return idToIndex[(int)id] >= 0;
    }

    // Typed fast accessors
    float getFloat(ParamId id, float def = 0.0f)
    {
        int16_t ix = idToIndex[(int)id];
        if(ix>=0)
        {
            if (prmList[ix].type() == DataType::FLOAT) {
                return prmList[ix].getFloat();
            }
            else axLog("ParamId %d is not float!", (int)id);
        }
        else axLog("ParamId %d not found!", (int)id);
        return def;
    }

    int getInt(ParamId id, int def = 0)
    {
        int16_t ix = idToIndex[(int)id];
        if(ix>=0)
        {
            DataType type = prmList[ix].type();
            if(type==DataType::INT || type==DataType::ENUM) {
                return prmList[ix].getInt();
            }
            else axLog("ParamId %d not int/enum!", (int)id);
        }
        else axLog("ParamId %d not found!", (int)id);
        return def;
    }

    bool getBool(ParamId id, bool def = false)
    {
        int16_t ix = idToIndex[(int)id];
        if (ix >= 0 && prmList[ix].type() == DataType::BOOL) {
            return prmList[ix].getBool();
        }
        else axLog("ParamId %d not found or not bool!", (int)id);
        return def;
    }

    vec2 getVec2(ParamId id, const vec2 &def = vec2(0, 0)) {
        int16_t ix = idToIndex[(int)id];
        if (ix >= 0 && prmList[ix].type() == DataType::VEC2) {
            return prmList[ix].value.getVec2();
        }
        else axLog("ParamId %d not found or not vec2!", (int)id);
        return def;
    }

    cColor getColor(ParamId id, const cColor &def = cWhite) {
        int16_t ix = idToIndex[(int)id];
        if (ix >= 0 && prmList[ix].type() == DataType::COLOR) {
            return prmList[ix].value.getColor();
        }
        else axLog("ParamId %d not found or not color!", (int)id);
        return def;
    }

    cString getString(ParamId id, const cString &def = "") {
        int16_t ix = idToIndex[(int)id];
        if (ix >= 0 && prmList[ix].type() == DataType::STRING) {
            return prmList[ix].getString();
        }
        else axLog("ParamId %d not found or not string!", (int)id);
        return def;
    }

    void setInt(ParamId id, int val) {
        int16_t ix = idToIndex[(int)id];
        if (ix >= 0) prmList[ix].setVal(val);
    }

    void setFloat(ParamId id, float val) {
        int16_t ix = idToIndex[(int)id];
        if (ix >= 0) prmList[ix].setVal(val);
    }

    void setVec2(ParamId id, const vec2 &v) {
        int16_t ix = idToIndex[(int)id];
        if (ix >= 0) prmList[ix].setVal(v);
    }

    void setColor(ParamId id, const cColor &c) {
        int16_t ix = idToIndex[(int)id];
        if (ix >= 0) prmList[ix].setVal(c);
    }

    //=======================================================================
    // LEGACY STRING ACCESS - for serialization, UI, debug
    //=======================================================================

    Param* getParam(const cString &name) {
        for (size_t i = 0; i < prmList.size(); i++) {
            if (prmList[i].name == name) {
                return &prmList[i];
            }
        }
        return nullptr;
    }

    float getFloat(const cString &name, float v = 0.5f) {
        Param *prm = getParam(name);
        return prm ? prm->getFloat() : v;
    }

    int getInt(const cString &name, int v = 1) {
        Param *prm = getParam(name);
        return prm ? prm->getInt() : v;
    }

    bool getBool(const cString &name, bool v = true) {
        Param *prm = getParam(name);
        return prm ? prm->getBool() : v;
    }

    vec2 getVec2(const cString &name, const vec2 &v = vec2(0, 0)) {
        Param *prm = getParam(name);
        return (prm && prm->type() == DataType::VEC2) ? prm->value.getVec2() : v;
    }

    cColor getColor(const cString &name, const cColor &v = cWhite) {
        Param *prm = getParam(name);
        return (prm && prm->type() == DataType::COLOR) ? prm->value.getColor() : v;
    }

    cString getString(const cString &name, const cString &v = "") {
        Param *prm = getParam(name);
        return prm ? prm->getString() : v;
    }

    void set(const cString &nm, float val, float mn, float mx) {
        Param *prm = getParam(nm);
        if (!prm) {
            axLog("Param '%s' is unknown...", nm.c_str());
            return;
        }
        *prm = Param(nm, val, mn, mx);
    }

    void setFloat(const cString &nm, float val) {
        Param *prm = getParam(nm);
        if (prm && prm->type() == DataType::FLOAT) {
            prm->setVal(val);
        }
    }

    void setVec2(const cString &name, const vec2 &v) {
        Param *prm = getParam(name);
        if (prm && prm->type() == DataType::VEC2) {
            prm->setVal(v);
        }
    }

    //=======================================================================
    // UTILITY
    //=======================================================================

    bool operator!=(const ParamContainer &other) const {
        if (prmList.size() != other.prmList.size()) return true;
        for (size_t i = 0; i < prmList.size(); i++) {
            if (prmList[i] != other.prmList[i]) return true;
        }
        return false;
    }

    bool operator==(const ParamContainer &other) const {
        return !(*this != other);
    }

    void copyValues(ParamContainer &other) {
        size_t cnt = prmList.size();
        if (cnt != other.prmList.size()) return;

        for (size_t i = 0; i < cnt; i++) {
            Param &p = prmList[i];
            Param &p2 = other.prmList[i];
            if (p.type() == p2.type() && p.name == p2.name) {
                p.value = p2.value;
            }
        }
    }

    bool isDirty() const {
        for (const auto &p : prmList) {
            if (p.dirty) return true;
        }
        return false;
    }

    void clearDirty() {
        for (auto &p : prmList) {
            p.dirty = false;
        }
    }

    cString infoStr() {
        cString s;
        for (auto &p : prmList) {
            s += p.infoStr() + ", ";
        }
        return s;
    }

    cString serialize() {
        cString s;
        for (auto &p : prmList) {
            s += p.serialize();
        }
        return s;
    }

    void toJSON(json &j) {
        for (auto &p : prmList) {
            json pj;
            p.toJson(pj);
            j.push_back(pj);
        }
    }

    void fromJSON(json &js) {
        for (auto &j : js) {
            Param p;
            p.fromJSON(j);
            Param *prm = getParam(p.name);
            if(prm) {
                // p.copyTo(prm);
                prm->value = p.value;
                prm->strValue = p.strValue;
            }
            else axLog("Not found: "+p.name);
        }
        rebuildIndex();  // refresh the index after loading from JSON
    }
};

//-----------------------------------------------------------------------------------
// Type definition for a map storing strings and union parameters
using ParamMap = std::map<cString, Param>;
using ParamVector = std::vector<Param>;

#endif // PARAMETER_H
