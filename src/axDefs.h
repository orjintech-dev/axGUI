#ifndef AXDEFS_H
#define AXDEFS_H
#pragma once

#ifdef __EMSCRIPTEN__
#include <iostream>
#endif
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"

#include "math.h"
#include "stdarg.h"
#include <string>
#include <stack>

// Smooth wheel scrolling: the wheel moves a target and the scroll value eases toward it.
// Per frame: on wheel input `ss.add(value, deltaPx)`, then always `ss.step(value, lo, hi, tau, dt)`.
// tau is the time constant in seconds (0: jump immediately). If something else moves the value
// (scrollbar drag, keyboard, scroll-into-view) the animation stops and that change wins.
struct SmoothScroll {
    float target = 0, last = 0;
    bool active = false;
    void add(float cur, float delta) {
        if (!active || fabsf(cur - last) > 0.01f) target = cur;
        target += delta;
        active = true;
        last = cur;
    }
    bool step(float &cur, float lo, float hi, float tau, float dt) {
        if (!active) return false;
        if (fabsf(cur - last) > 0.01f) { active = false; return false; }
        if (hi < lo) hi = lo;
        target = target < lo ? lo : (target > hi ? hi : target);
        if (tau <= 0 || dt <= 0) cur = target;
        else cur += (target - cur) * (1.0f - expf(-dt / tau));
        if (fabsf(target - cur) < 0.25f) { cur = target; active = false; }
        last = cur;
        return true;
    }
    void stop() { active = false; }
};
#include <json.hpp>
#include <sstream>
#include <regex>
#include <variant>
#include <cassert>
#include <mutex>

// #include "Vector3.hpp"
// #include "Matrix.hpp"

using json = nlohmann::ordered_json;

#define USE_ANGELSCRIPT

#ifdef PLATFORM_DESKTOP
// #define USE_PYTHON
#endif

// #define cString std::string
typedef std::string cString;

#define ZOOM_MIN 0.05f
#define ZOOM_MAX 45.0f
#define leftPressed IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
#define midPressed IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)
#define rightPressed IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)
#define leftDown IsMouseButtonDown(MOUSE_LEFT_BUTTON)
#define midDown IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)
#define rightDown IsMouseButtonDown(MOUSE_RIGHT_BUTTON)
#define leftReleased IsMouseButtonReleased(MOUSE_LEFT_BUTTON)
#define midReleased IsMouseButtonReleased(MOUSE_MIDDLE_BUTTON)
#define rightReleased IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)

#define Int long long
#define S(x) std::string(x)
#define LS(x) std::to_string((long)x)
#define RS(x) std::to_string((float)x)
#define PB(n) (n?n->name.c_str():"NULL")
#define VS(v) RS(v.x)+","+RS(v.y)+","+RS(v.z)
#define QS(v) RS(v.x)+","+RS(v.y)+","+RS(v.z)+","+RS(v.w)
#define CS(c) LS(c.r)+","+LS(c.g)+","+LS(c.b)+","+LS(c.a)
#define V2S(v) (RS(v.x)+","+RS(v.y))
#define V2LS(v) LS((v).x)+","+LS((v).y)
#define IMV_SF(v) sf::Vector2f(v.x,v.y)
#define toCOLOR4(c) Color{(unsigned char)c.r,(unsigned char)c.g,(unsigned char)c.b,(unsigned char)c.a}
// #define TOCOLOR(c) Color{c.r,c.g,c.b,c.a}
// #define toVec2(v) vec2(v.x, v.y)

#define V2JSON(v) json{{"x", v.x}, {"y", v.y}};
#define V3JSON(v) json{{"x", v.x}, {"y", v.y}, {"z", v.z}};
#define C3JSON(v) json{{"r", v.r}, {"g", v.g}, {"b", v.b}};
#define C4JSON(v) json{{"r", v.r}, {"g", v.g}, {"b", v.b}, {"a", v.a}};
#define V4JSON(v) json{{"x", v.x}, {"y", v.y}, {"z", v.z}, {"w", v.w}};
#define MyArray std::vector


enum DirtyFlags { DIRTY_NONE = 1<<0, DIRTY_TRANSFORM = 1<<1, DIRTY_BOUNDS = 1<<2, DIRTY_MESH = 1<<3, DIRTY_ANIM = 1<<4, DIRTY_ALL = 0xff };

struct IdStrPair
{
    int id;
    std::string title, desc;
    std::string icon;   // optional icon name (axDrawBot::drawIcon); the comboBox shows it to the left of the item
    IdStrPair(int v, const std::string& s) : id(v), title(s), desc("") {}
    IdStrPair(int v, const std::string& s, const std::string& d) : id(v), title(s), desc(d) {}
    IdStrPair(int v, const std::string& s, const std::string& d, const std::string& ic) : id(v), title(s), desc(d), icon(ic) {}
    IdStrPair() : id(0), title(""), desc("") {}
};

//----------------------------------------------------------
// FontFamily struct (at the top of myTextEdit.h or in a separate header)
//----------------------------------------------------------
// struct FontFamily
// {
//     std::string name;
//     std::string path;
//     std::string regular;
//     std::string bold;
//     std::string italic;
//     std::string boldItalic;
//     std::string source;
//     bool selected = false;

//     int regularID = -1;
//     int boldID = -1;
//     int italicID = -1;
//     int boldItalicID = -1;

//     // Setters
//     void set(const std::string& _name,
//              const std::string& _path,
//              const std::string& _regular,
//              const std::string& _bold = "",
//              const std::string& _italic = "",
//              const std::string& _boldItalic = "",
//              const std::string& _source = "local")
//     {
//         name = _name;
//         path = _path;
//         regular = _regular;
//         bold = _bold;
//         italic = _italic;
//         boldItalic = _boldItalic;
//         source = _source;
//         resetIDs();
//     }

//     void setVariants(const std::string& _regular,
//                      const std::string& _bold = "",
//                      const std::string& _italic = "",
//                      const std::string& _boldItalic = "")
//     {
//         regular = _regular;
//         bold = _bold;
//         italic = _italic;
//         boldItalic = _boldItalic;
//         resetIDs();
//     }

//     void setVariant(bool b, bool i, const std::string& filename)
//     {
//         if(b && i) boldItalic = filename;
//         else if(b) bold = filename;
//         else if(i) italic = filename;
//         else regular = filename;
//     }

//     void resetIDs()
//     {
//         regularID = boldID = italicID = boldItalicID = -1;
//     }

//     void clear()
//     {
//         name.clear(); path.clear(); regular.clear();
//         bold.clear(); italic.clear(); boldItalic.clear();
//         source = "local";
//         selected = false;
//         resetIDs();
//     }

//     std::string getFullPath(bool b, bool i) const
//     {
//         const std::string& filename = getFilename(b, i);
//         if(filename.empty()) return "";
//         if(path.empty()) return filename;

//         char lastChar = path.back();
//         if(lastChar == '/' || lastChar == '\\') return path + filename;
//         return path + "/" + filename;
//     }

//     const std::string& getFilename(bool b, bool i) const
//     {
//         if(b && i) return boldItalic.empty() ? (bold.empty() ? regular : bold) : boldItalic;
//         if(b) return bold.empty() ? regular : bold;
//         if(i) return italic.empty() ? regular : italic;
//         return regular;
//     }

//     bool hasVariant(bool b, bool i) const
//     {
//         if(b && i) return !boldItalic.empty();
//         if(b) return !bold.empty();
//         if(i) return !italic.empty();
//         return !regular.empty();
//     }

//     int getID(bool b, bool i) const
//     {
//         if(b && i && boldItalicID != -1) return boldItalicID;
//         if(b && boldID != -1) return boldID;
//         if(i && italicID != -1) return italicID;
//         if(regularID != -1) return regularID;
//         if(boldID != -1) return boldID;
//         if(italicID != -1) return italicID;
//         if(boldItalicID != -1) return boldItalicID;
//         return -1;
//     }

//     void setID(bool b, bool i, int id)
//     {
//         if(b && i) boldItalicID = id;
//         else if(b) boldID = id;
//         else if(i) italicID = id;
//         else regularID = id;
//     }

//     bool hasAnyFont() const { return !regular.empty(); }
//     bool isLoaded() const { return regularID != -1 || boldID != -1 || italicID != -1 || boldItalicID != -1; }

//     int variantCount() const
//     {
//         int c = 0;
//         if(!regular.empty()) c++;
//         if(!bold.empty()) c++;
//         if(!italic.empty()) c++;
//         if(!boldItalic.empty()) c++;
//         return c;
//     }
// };

#define LOG_MAX_LINES 200
static std::vector<cString> logArray;
static int logPos = 0;

class Logger
{
public:
    static const int MAX_LOG_LINES = 200;
    static const int MAX_LINE_LENGTH = 500;

private:
    int cnt=0;
    std::vector<std::string> logArray;
    int displayStartPos;
    std::mutex logMutex;  // Thread safety

    // Private constructor for singleton
    Logger() : displayStartPos(0) {
        logArray.reserve(MAX_LOG_LINES + 10); // Pre-allocate
    }

public:
    // Singleton instance
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    // Delete copy constructor and assignment
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Log a string
    void log(const std::string& message)
    {
        std::lock_guard<std::mutex> lock(logMutex);

        logArray.push_back(message);
        cnt++;
        printf("%d. %s\n", (int)logArray.size(), message.c_str());
        fflush(stdout);   // make output appear immediately (stdout is buffered when piped or redirected to a file)

        // Trim if exceeds max
        trimLogs();
    }

    // Log with printf-style formatting (SAFE version)
    void logv(const char* fmt, va_list args)
    {
        char buffer[MAX_LINE_LENGTH];
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        log(std::string(buffer));
    }


    // Render logs to screen
    void render(float scrh, float startX = 220, float startY = 100,
                void (*printFunc)(float, float, const char*, ...) = nullptr)
    {
        std::lock_guard<std::mutex> lock(logMutex);

        if (!printFunc) return;

        float lineHeight = 14;
        float availableHeight = 400;
        int maxVisibleLines = static_cast<int>(availableHeight / lineHeight);

        int totalLogs = static_cast<int>(logArray.size());

        // Calculate scroll position (show most recent logs)
        displayStartPos = totalLogs - maxVisibleLines;
        if (displayStartPos < 0) displayStartPos = 0;

        // Render visible logs
        float currentY = startY;
        for (int i = displayStartPos; i < totalLogs; i++) {
            if (i >= displayStartPos + maxVisibleLines) break;

            printFunc(startX, currentY, "%d. %s", cnt-totalLogs+i, logArray[i].c_str());
            currentY += lineHeight;
        }
    }

    // Get log info
    int getLogCount() const {
        // std::lock_guard<std::mutex> lock(logMutex);
        return static_cast<int>(logArray.size());
    }

    int getDisplayStartPos() const {
        return displayStartPos;
    }

    // Manual scroll control
    void scrollUp(int lines = 1) {
        std::lock_guard<std::mutex> lock(logMutex);
        displayStartPos = std::max(0, displayStartPos - lines);
    }

    void scrollDown(int lines = 1) {
        std::lock_guard<std::mutex> lock(logMutex);
        int maxScroll = static_cast<int>(logArray.size()) - 1;
        displayStartPos = std::min(maxScroll, displayStartPos + lines);
    }

    // Clear all logs
    void clear() {
        std::lock_guard<std::mutex> lock(logMutex);
        logArray.clear();
        displayStartPos = 0;
    }

    // Get log entry
    std::string getLog(int index) const {
        // std::lock_guard<std::mutex> lock(logMutex);
        if (index >= 0 && index < (int)logArray.size()) {
            return logArray[index];
        }
        return "";
    }

private:
    void trimLogs() {
        // Already locked by caller
        if (logArray.size() > MAX_LOG_LINES) {
            int excess = logArray.size() - MAX_LOG_LINES;
            logArray.erase(logArray.begin(), logArray.begin() + excess);
        }
    }
};

// Log line; a function rather than a macro, since a macro named axLog would break any identifier of the same name
inline void axLog(const std::string& message) { Logger::getInstance().log(message); }
inline void axLog(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Logger::getInstance().logv(fmt, args);
    va_end(args);
}


// printf-style temporary text (one 200-byte buffer per translation unit; overwritten on the next call)
static char* axFormat(const char *fmt, ...)
{
    va_list arg;
    static char buf[200];

    va_start(arg, fmt);
    vsnprintf(buf, sizeof(buf), fmt, arg); // TODO: txt might overflow
    va_end(arg);
    return buf;
}

// static void axLog(const std::string &s)
// {
//     std::mutex logMutex;  // Thread safety
//     logArray.push_back(s);
//     printf("%d. %s\n", (int)logArray.size(), s.c_str());
// }

// static void axLog(const char *format, ...)
// {
//     std::mutex logMutex;  // Thread safety
//     va_list arg;
//     static char buf[200];

//     // Get string
//     va_start(arg, format);
//     vsnprintf(buf, sizeof(buf), format, arg); // TODO: txt might overflow
//     va_end(arg);
//     logArray.push_back(buf);
//     printf("%d. %s\n", (int)logArray.size(), buf);
// }

static long nodeID=10000;
static long getUniqueID() { nodeID++; return nodeID; }

// Key repeat timing
struct KeyRepeat {
    int key = -1;
    double pressTime = 0.0;
    double lastRepeatTime = 0.0;
    double initialDelay = 0.4;
    double repeatInterval = 0.033;
};

///-----------------------------------------------------------------------------------
static std::string CompactArrays(const std::string& input)
{
    std::regex arrayPattern(R"(\[\s*(-?\d+(?:\.\d+)?),\s*\n\s*(-?\d+(?:\.\d+)?)\s*\])");
    return std::regex_replace(input, arrayPattern, "[$1,$2]");
}

static void writeJsonWithOneLineParams(const json& j,
                                       std::ostream& out,
                                       int indent = 0,
                                       int step = 2,
                                       const char* parentKey = nullptr)
{
    auto indentSpaces = [&](int n){ for (int i=0; i<n; i++) out.put(' '); };

    switch (j.type())
    {
    case json::value_t::object: {
        out << "{\n";
        bool first = true;
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (!first) out << ",\n";
            first = false;
            indentSpaces(indent + step);
            out << "\"" << it.key() << "\": ";
            writeJsonWithOneLineParams(it.value(), out, indent + step, step, it.key().c_str());
        }
        out << '\n';
        indentSpaces(indent);
        out << '}';
        break;
    }

    case json::value_t::array: {
        bool oneLineParams = (parentKey && std::strcmp(parentKey, "parameters") == 0);

        auto isSmallPrimitiveArray = [&](const json& a) {
            if (a.size() == 0 || a.size() > 4) return false;
            for (const auto& v : a) {
                if (!(v.is_number() || v.is_string() || v.is_boolean() || v.is_null()))
                    return false;
            }
            return true;
        };

        // Compact rule: parameters -> one object per line,
        // small primitive arrays -> single line
        if (!oneLineParams && isSmallPrimitiveArray(j)) {
            out << j.dump();  // e.g. [296.478,492.054]
            break;
        }

        if (!oneLineParams) {
            out << "[\n";
            for (size_t i = 0; i < j.size(); ++i) {
                indentSpaces(indent + step);
                writeJsonWithOneLineParams(j[i], out, indent + step, step, nullptr);
                if (i + 1 < j.size()) out << ',';
                out << '\n';
            }
            indentSpaces(indent);
            out << ']';
        } else {
            out << "[\n";
            for (size_t i = 0; i < j.size(); ++i) {
                indentSpaces(indent + step);
                out << j[i].dump();   // single-line per parameter
                if (i + 1 < j.size()) out << ',';
                out << '\n';
            }
            indentSpaces(indent);
            out << ']';
        }
        break;
    }

    default:
        out << j.dump(); // primitive
        break;
    }
}

// static float Clamp(float v, float lo, float hi){ return v<lo?lo:(v>hi?hi:v); }

// typedef raylib::Vector3 vec3;
// typedef raylib::Matrix  matrix;

constexpr float axEpsilon = 0.00001f;   // not a "#define epsilon": it would clobber parameter names in headers such as SDL's
struct vec2
{
    float x,y;
    vec2() { x=y=0; }
    vec2(float x, float y) : x(x), y(y) {}
    vec2 operator*(const vec2& v) const { return vec2(x*v.x, y*v.y); }
    vec2 operator+(const vec2& v) const { return vec2(x+v.x, y+v.y); }
    vec2 operator+=( const vec2& v) { x+=v.x; y+=v.y; return *this; }
    vec2 operator-() { return vec2(-x, -y); }

    vec2 operator-(const vec2& v) const { return vec2(x-v.x, y-v.y); }
    vec2 operator/(const vec2& v) const { return vec2(x/v.x, y/v.y); }
    vec2 operator*(float s) const { return vec2(x*s, y*s);  }

    // Optional: for float * vec2
    friend vec2 operator*(float s, const vec2& v) {
        return vec2(v.x * s, v.y * s);
    }
    vec2 operator/(float s) const {
        return vec2(x/s, y/s);
    }
    bool operator==(const vec2& v) const {
        return (fabs(x-v.x)<axEpsilon && fabs(y-v.y)<axEpsilon);
    }
    bool operator!=(const vec2& v) const {
        return (fabs(x-v.x)>=axEpsilon || fabs(y-v.y)>=axEpsilon);
    }
    // bool operator!=(const vec2& v) const {
    //     return (x != v.x || y != v.y);
    // }
    float length() const
    {
        return sqrt(x*x + y*y);
    }
    // Calculate two vectors dot product
    float dot(const vec2 &v2) const
    {
        return (x*v2.x + y*v2.y);
    }
    float cross(const vec2 &v2) const
    {
        return (x*v2.y - y*v2.x);
    }
    float distSqr(const vec2 &v2) const
    {
        return ((x - v2.x)*(x - v2.x) + (y - v2.y)*(y - v2.y));
    }
    float angle(const vec2 &v2) const
    {
        float result = 0.0f;
        float dot = x*v2.x + y*v2.y;
        float det = x*v2.y - y*v2.x;
        result = atan2f(det, dot);
        return result;
    }
    vec2 rotated(float angleRad) const
    {
        float s = sinf(angleRad);
        float c = cosf(angleRad);
        return vec2(x*c - y*s, x*s + y*c);
    }

    vec2 normalized() const
    {
        vec2 result = { 0,0 };
        float length = sqrtf((x*x) + (y*y));

        if(length > 0)
        {
            float ilength = 1.0f/length;
            result.x = x*ilength;
            result.y = y*ilength;
        }
        return result;
    }
    float dist(const vec2 &v2) const
    {
        float result = sqrtf((x - v2.x)*(x - v2.x) + (y - v2.y)*(y - v2.y));
        return result;
    }

    bool inEllipse(vec2 center, float rx, float ry, float angleRad) const
    {
        if (rx <= 0.0f || ry <= 0.0f) return false; // degenerate
        const double c = std::cos(angleRad), s = std::sin(angleRad);
        const double ux = x - center.x,    uy = y - center.y;
        // rotate point into ellipse-local axes
        const double xp =  ux*c + uy*s;
        const double yp = -ux*s + uy*c;
        const double dx = xp / rx;
        const double dy = yp / ry;
        return dx*dx + dy*dy <= 1.0;
    }
};

// ---------- local shape tests (centered at 0,0; size = full extents) ----------
static inline bool hitRectLocal(vec2 q, vec2 half){ // axis-aligned in local
    return (q.x >= -half.x && q.x <= half.x && q.y >= -half.y && q.y <= half.y);
}

static inline bool hitEllipseLocal(vec2 q, float rx, float ry){
    if (rx <= 1e-8f || ry <= 1e-8f) return false;
    float nx = q.x / rx, ny = q.y / ry;
    return (nx*nx + ny*ny) <= 1.0f;
}

struct vec3
{
    float x,y,z;
    vec3() { x=y=z=0; }
    vec3(float x, float y, float z) : x(x), y(y), z(z) {}
};

struct vec4
{
    float x,y,z,w;
    vec4() { x=y=z=w=0; }
    vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
};

struct mat2d
{
    // m = [ a, b, c, d, tx, ty ]
    float m[6];

    constexpr mat2d(float a=1, float b=0, float c=0, float d=1, float tx=0, float ty=0)
        : m{a,b,c,d,tx,ty} {}

    static mat2d Identity()                  { return mat2d(); }
    static mat2d Translation(vec2 t){ return mat2d(1,0,0,1,t.x,t.y); }
    static mat2d Translation(float x, float y){ return mat2d(1,0,0,1,x,y); }
    static mat2d Scaling(float sx, float sy)  { return mat2d(sx,0,0,sy,0,0); }
    static mat2d Rotation(float r) {
        float s = std::sin(r), c = std::cos(r);
        return mat2d(c, s, -s, c, 0, 0);
    }
    // x' = x + shx*y,  y' = shy*x + y
    static mat2d Shear(float shx, float shy)  { return mat2d(1,shy,shx,1,0,0); }

    vec2 transformPoint(const vec2& p)  const
    {
        return { m[0]*p.x + m[2]*p.y + m[4], m[1]*p.x + m[3]*p.y + m[5] };
    }

    vec2 transformVector(const vec2& v) const
    {
        return { m[0]*v.x + m[2]*v.y, m[1]*v.x + m[3]*v.y };
    }

    // Member multiply: apply B first, then *this
    inline constexpr mat2d operator*(const mat2d& B) const noexcept {
        const mat2d& A = *this;
        mat2d R;
        R.m[0] = A.m[0]*B.m[0] + A.m[2]*B.m[1];          // a'
        R.m[1] = A.m[1]*B.m[0] + A.m[3]*B.m[1];          // b'
        R.m[2] = A.m[0]*B.m[2] + A.m[2]*B.m[3];          // c'
        R.m[3] = A.m[1]*B.m[2] + A.m[3]*B.m[3];          // d'
        R.m[4] = A.m[0]*B.m[4] + A.m[2]*B.m[5] + A.m[4]; // tx'
        R.m[5] = A.m[1]*B.m[4] + A.m[3]*B.m[5] + A.m[5]; // ty'
        return R;
    }

    inline mat2d& operator*=(const mat2d& B) noexcept {
        float a  = m[0]*B.m[0] + m[2]*B.m[1];
        float b  = m[1]*B.m[0] + m[3]*B.m[1];
        float c  = m[0]*B.m[2] + m[2]*B.m[3];
        float d  = m[1]*B.m[2] + m[3]*B.m[3];
        float tx = m[0]*B.m[4] + m[2]*B.m[5] + m[4];
        float ty = m[1]*B.m[4] + m[3]*B.m[5] + m[5];
        m[0]=a; m[1]=b; m[2]=c; m[3]=d; m[4]=tx; m[5]=ty;
        return *this;
    }

    // multiply point by mat2d
    inline vec2 operator*(const vec2 &p)
    {
        return { m[0]*p.x + m[2]*p.y + m[4],
                m[1]*p.x + m[3]*p.y + m[5] };
    }
    // mat2d& operator*=(const mat2d& o) { *this = *this * o; return *this; }

    float det() const { return m[0]*m[3] - m[1]*m[2]; }

    // Inverse; returns false if non-invertible
    [[nodiscard]] mat2d inverted() const
    {
        float determinant = det();
        if (std::fabs(determinant) < 1e-8f) return mat2d();
        float invDet = 1.0f / determinant;
        float ia =  m[3] * invDet;
        float ib = -m[1] * invDet;
        float ic = -m[2] * invDet;
        float id =  m[0] * invDet;
        float itx = -(ia*m[4] + ic*m[5]);
        float ity = -(ib*m[4] + id*m[5]);
        return mat2d(ia, ib, ic, id, itx, ity);
    }

    inline vec2 pos() const   { return vec2(m[4], m[5]); }

    inline void setPos(const vec2& p)   { m[4] = p.x;  m[5] = p.y; }

    inline void setRot(float r)  //setRotationKeepScale
    {
        float sx = std::hypot(m[0], m[1]);   // |first column|
        float sy = std::hypot(m[2], m[3]);   // |second column|

        // preserve handedness (reflection) if det < 0 by flipping sy
        float det = m[0]*m[3] - m[1]*m[2];
        if (det < 0) sy = -sy;

        float s = std::sin(r), c = std::cos(r);
        m[0] =  c * sx;  m[1] =  s * sx;
        m[2] = -s * sy;  m[3] =  c * sy;
        // m[4], m[5] unchanged
    }

    inline void setScale(const vec2& scl)
    {
        float r = rot();
        float s = std::sin(r), c = std::cos(r);
        m[0] =  c * scl.x;  m[1] =  s * scl.x;
        m[2] = -s * scl.y;  m[3] =  c * scl.y;
    }

    //---- rotation from direction vector
    inline void setRot(vec2 dir)  //setRotationKeepScale
    {
        float r = std::atan2(dir.y, dir.x);
        setRot(r);
    }

    inline float rot() const  //Safe wrapper with fallback
    {
        float a=m[0], b=m[1], c=m[2], d=m[3];
        float num = b - c, den = a + d;
        if (std::fabs(num) + std::fabs(den) > 1e-12f) return std::atan2(num, den);
        return std::atan2(b, a); // degenerate fallback
    }

    inline vec2 scale() const
    {
        float sx = std::hypot(m[0], m[1]); // |[a b]^T|
        float sy = std::hypot(m[2], m[3]); // |[c d]^T|
        return vec2(sx,sy);
    }

    // combined transform: translate, rotate, scale
    static mat2d TRS(const vec2& t, float r, const vec2& s){
        float sn=std::sin(r), cs=std::cos(r);
        return mat2d(cs*s.x, sn*s.x, -sn*s.y, cs*s.y, t.x, t.y);
    }

    // handy accessors
    const float* data() const { return m; }
    float* data() { return m; }
};

// convenience: transform point by matrix
inline vec2 operator*(const mat2d& M, const vec2& p) { return M.transformPoint(p); }

// constexpr float deg2rad(float deg) { return deg * 0.01745329251994329577f; }
static inline void expandAABB(vec2 p, vec2& minv, vec2& maxv)
{
    minv.x = std::min(minv.x, p.x); minv.y = std::min(minv.y, p.y);
    maxv.x = std::max(maxv.x, p.x); maxv.y = std::max(maxv.y, p.y);
}

inline void mat2dToNVG(const mat2d& M, float xf[6])
{
    xf[0]=M.m[0]; xf[1]=M.m[1]; xf[2]=M.m[2];
    xf[3]=M.m[3]; xf[4]=M.m[4]; xf[5]=M.m[5];
}

inline mat2d nvgToMat2d(const float xf[6])
{
    return mat2d(xf[0], xf[1], xf[2], xf[3], xf[4], xf[5]);
}

// Rotate by r (radians) around world-space pivot (cx, cy)
static mat2d RotationAround(float r, float cx, float cy)
{
    float s = std::sin(r), c = std::cos(r);
    float tx = cx - c*cx + s*cy;
    float ty = cy - s*cx - c*cy;
    return mat2d(c, s, -s, c, tx, ty);
}

inline mat2d rotateAroundLocal(const mat2d& M, float r, float px, float py)
{
    return M * (mat2d::Translation(px,py) * mat2d::Rotation(r) * mat2d::Translation(-px,-py));
}

static vec2 toVec2(const std::string &val)
{
    vec2 v;
    if(val.find("vec2(")==0 && val.find(")")!=std::string::npos)  sscanf(val.c_str(),"vec2(%f,%f)",&v.x,&v.y);
    else sscanf(val.c_str(),"%f,%f",&v.x,&v.y);
    return v;
}

static inline vec2 mul2x2(const mat2d& M, const vec2& v)
{
    return { M.m[0]*v.x + M.m[2]*v.y, M.m[1]*v.x + M.m[3]*v.y };
}

static inline vec2 XformPoint(const mat2d& M, const vec2& p)
{
    return { M.m[0]*p.x + M.m[2]*p.y + M.m[4],  M.m[1]*p.x + M.m[3]*p.y + M.m[5] };
}

// Invert affine matrix (no projective component)
static inline mat2d invertAffine(const mat2d& M)
{
    float a=M.m[0], b=M.m[1], c=M.m[2], d=M.m[3], tx=M.m[4], ty=M.m[5];
    float det = a*d - b*c;
    if (fabsf(det) < 1e-12f) return mat2d::Identity();
    float inv = 1.0f/det;
    mat2d R;
    R.m[0]= d*inv; R.m[1]=-b*inv; R.m[2]=-c*inv; R.m[3]= a*inv;
    R.m[4]=-(R.m[0]*tx + R.m[2]*ty);
    R.m[5]=-(R.m[1]*tx + R.m[3]*ty);
    return R;
}


//--- top-left aligned rectangle
struct rect
{
    vec2 pos;
    vec2 size;
    rect() {}
    rect(vec2 p, vec2 s) : pos(p), size(s)
    {
        fixDims();
    }
    rect(float x, float y, float w, float h) : pos(x,y), size(w,h)
    {
        fixDims();
    }
    rect(std::vector<vec2> &pts)
    {
        rect bb = getBounds(pts);
        pos = bb.pos;
        size = bb.size;
        fixDims();
    }
    void fixDims()
    {
        if(size.x<0) { pos.x += size.x; size.x = -size.x; }
        if(size.y<0) { pos.y += size.y; size.y = -size.y; }
    }
    float left() { return pos.x; }
    float right() { return pos.x + size.x; }
    float top() { return pos.y; }
    float bottom() { return pos.y + size.y; }
    vec2 minPt() { return pos; }
    vec2 maxPt() { return vec2(pos.x+size.x, pos.y+size.y); }
    vec2 topright() { return vec2(pos.x+size.x, pos.y); }
    vec2 bottomleft() { return vec2(pos.x, pos.y+size.y); }
    vec2 bottomright() { return vec2(pos.x+size.x, pos.y+size.y); }

    vec2 center()
    {
        return vec2(pos.x+size.x*0.5f, pos.y+size.y*0.5f);
    }
    rect operator+(const vec2& v) const {
        return rect(pos + v, size);
    }
    rect operator-(const vec2& v) const {
        return rect(pos - v, size);
    }
    rect operator*(const vec2& v) const {
        return rect(pos * v, size * v);
    }
    rect operator*=(const vec2& v) {
        pos = pos * v;
        size = size * v;
        return *this;
    }
    bool operator!=(const rect& r) const {
        return pos != r.pos || size != r.size;
    }
    bool isPointInRec(vec2 pt)
    {
        if(pt.x>=pos.x && pt.x<pos.x+size.x && pt.y>=pos.y && pt.y<pos.y+size.y) return true;
        return false;
    }
    bool collideWithCircle(vec2 circlePos, float radius)
    {
        // Find the point of the rectangle closest to the circle center
        float closestX = fmaxf(pos.x, fminf(circlePos.x, pos.x + size.x));
        float closestY = fmaxf(pos.y, fminf(circlePos.y, pos.y + size.y));

        // Squared distance between that point and the circle center
        float dx = circlePos.x - closestX;
        float dy = circlePos.y - closestY;

        return (dx * dx + dy * dy) <= (radius * radius);
    }
    bool collideWithEllipse(vec2 ePos, vec2 radius)
    {
        // Transform the rectangle and ellipse space so that ellipse becomes a unit circle
        // Scale the rectangle and point into "circle space"
        vec2 halfSize = size * 0.5f;
        vec2 rectCenter = pos + halfSize;

        vec2 scale = vec2(1.0f / radius.x, 1.0f / radius.y);

        // Transform to unit circle space
        vec2 rectCenterNorm = (rectCenter - ePos) * scale;
        vec2 halfSizeNorm   = halfSize * scale;

        // Clamp closest point in normalized rectangle
        vec2 closest = rectCenterNorm;
        closest.x = std::max(rectCenterNorm.x - halfSizeNorm.x, std::min(0.0f, rectCenterNorm.x + halfSizeNorm.x));
        closest.y = std::max(rectCenterNorm.y - halfSizeNorm.y, std::min(0.0f, rectCenterNorm.y + halfSizeNorm.y));

        // Check distance to origin (0,0), which is ellipse center in normalized space
        float distSq = closest.x * closest.x + closest.y * closest.y;
        return distSq <= 1.0f;  // If within unit circle, there is a collision
    }

    bool collideWithRect(rect r, bool inside=false)
    {
        if(inside)
        {
            if(pos.x >= r.pos.x && pos.x + size.x <= r.pos.x + r.size.x &&
                pos.y >= r.pos.y && pos.y + size.y <= r.pos.y + r.size.y) return true;
        }
        else
        {
            if(pos.x < r.pos.x + r.size.x && pos.x + size.x > r.pos.x &&
                pos.y < r.pos.y + r.size.y && pos.y + size.y > r.pos.y) return true;
        }
        return false;
    }
    cString toStr()
    {
        return V2LS(pos)+", "+V2LS(size);
    }

    rect getBounds(std::vector<vec2> &pts)
    {
        if(pts.empty()) return rect(0, 0, 0, 0);
        float minX = pts[0].x, maxX = pts[0].x;
        float minY = pts[0].y, maxY = pts[0].y;
        for(const auto& p : pts) {
            if (p.x < minX) minX = p.x;
            if (p.x > maxX) maxX = p.x;
            if (p.y < minY) minY = p.y;
            if (p.y > maxY) maxY = p.y;
        }
        return rect(minX, minY, maxX - minX, maxY - minY);
    }
    rect getIntersection(const rect& other) const
    {
        // Compute the intersection of the two rectangles
        float x1 = std::max(pos.x, other.pos.x);
        float y1 = std::max(pos.y, other.pos.y);
        float x2 = std::min(pos.x + size.x, other.pos.x + other.size.x);
        float y2 = std::min(pos.y + size.y, other.pos.y + other.size.y);

        // Return an empty rect if they do not intersect
        if(x2 < x1 || y2 < y1)
        {
            return rect(0, 0, 0, 0); // veya rect(x1, y1, 0, 0)
        }

        return rect(x1, y1, x2 - x1, y2 - y1);
    }
};


class cColor
{
public:
    int r, g, b, a;
    cColor() {}
    cColor(int v) : r(v), g(v), b(v), a(255) {}
    cColor(int r, int g, int b) : r(r), g(g), b(b), a(255) {}
    cColor(int r, int g, int b, int a) : r(r), g(g), b(b), a(a) {}
    cColor(const std::string& hex)
    {
        a = 255; // Default alpha
        fromStr(hex);
    }
    float* tofloat3() const { static float f[3]; f[0]=r; f[1]=g; f[2]=b; return f; }

    cColor operator+(const cColor& v) const {
        return cColor(r + v.r, g + v.g, b + v.b, a + v.a);
    }
    cColor operator-(const cColor& v) const {
        return cColor(r - v.r, g - v.g, b - v.b, a - v.a);
    }
    cColor operator*(float s) const {
        return cColor(int(r*s), int(g*s), int(b*s), int(a*s));
    }
    bool operator!=(const cColor& v) const {
        return r != v.r || g != v.g || b != v.b || a != v.a;
    }
    bool operator==(const cColor& v) const {
        return r == v.r && g == v.g && b == v.b && a == v.a;
    }

    std::string toHex() {
        char buf[20];
        sprintf(buf, "#%02X%02X%02X%02X", r, g, b, a);
        return buf;
    }
    std::string toStr() {
        char buf[30];
        sprintf(buf, "(%d,%d,%d,%d)", r, g, b, a);
        return buf;
    }
    cColor normalized() const {
        return cColor(int(r/255.0f*255), int(g/255.0f*255), int(b/255.0f*255), int(a/255.0f*255));
    }
    vec4 toVec4() const {
        return vec4(r/255.0f, g/255.0f, b/255.0f, a/255.0f);
    }
    // Converts "#RRGGBB" s string to normalized RGB (0.0 - 1.0)
    void fromStr(const std::string& s)
    {
        // Named / special cases
        if (s.empty())          { r=0;   g=0;   b=0;   a=255; return; }
        if (s=="none")          { r=0;   g=0;   b=0;   a=0;   return; }
        if (s=="red")           { r=255; g=0;   b=0;   a=255; return; }
        if (s=="green")         { r=0;   g=255; b=0;   a=255; return; }
        if (s=="blue")          { r=0;   g=0;   b=255; a=255; return; }
        if (s=="black")         { r=0;   g=0;   b=0;   a=255; return; }
        if (s=="white")         { r=255; g=255; b=255; a=255; return; }
        if (s=="yellow")        { r=255; g=255; b=0;   a=255; return; }
        if (s=="cyan")          { r=0;   g=255; b=255; a=255; return; }
        if (s=="magenta")       { r=255; g=0;   b=255; a=255; return; }

        // Hex: #RRGGBB or #RRGGBBAA
        if ( (s.size() == 7 || s.size() == 9) && s[0] == '#') {   // <-- AND, not OR
            r = std::stoi(s.substr(1, 2), nullptr, 16);
            g = std::stoi(s.substr(3, 2), nullptr, 16);
            b = std::stoi(s.substr(5, 2), nullptr, 16);
            a = (s.size() == 9) ? std::stoi(s.substr(7, 2), nullptr, 16) : 255;
            return;
        }
        const char *str = s.c_str();
        // rgb(r, g, b) or rgba(r, g, b, a)
        if (strncmp(str, "rgb(", 4) == 0 || strncmp(str, "rgba(", 5) == 0) {
            const char *start = strchr(str, '(') + 1;
            const char *end = strchr(str, ')');

            if (start && end) {
                char values[128];
                strncpy(values, start, end - start);
                values[end - start] = '\0';

                // Parse comma-separated values
                int count = sscanf(values, "%d,%d,%d,%d", &r, &g, &b, &a);
                if (count < 4) a = 255; // No alpha provided

                // Clamp values
                r = (r < 0) ? 0 : (r > 255) ? 255 : r;
                g = (g < 0) ? 0 : (g > 255) ? 255 : g;
                b = (b < 0) ? 0 : (b > 255) ? 255 : b;
                a = (a < 0) ? 0 : (a > 255) ? 255 : a;
                return;
            }
        }

        axLog("Color must be named or hex #RRGGBB[#AA]");
        axLog(s);
        // throw std::invalid_argument("Color must be named or hex #RRGGBB[#AA]");
    }
};

#define cRed cColor(255,0,0)
#define cGreen cColor(0,255,0)
#define cBlue cColor(0,0,255)
#define cBlack cColor(0,0,0)
#define cWhite cColor(255,255,255)
#define cYellow cColor(255,255,0)
#define cCyan cColor(0,255,255)
#define cMagenta cColor(255,0,255)
#define cGray cColor(128,128,128)
#define cLightGray cColor(200,200,200)
#define cDarkGray cColor(64,64,64)
#define cTransparent cColor(0,0,0,0)
#define cBlank cColor(0,0,0,0)
#define cOrange cColor(255,165,0)
#define cPurple cColor(128,0,128)
#define cBrown cColor(165,42,42)
#define cPink cColor(255,192,203)
#define cGold cColor(255,215,0)


enum class DataType { BOOL, INT, ENUM, FLOAT, STRING, VEC2, COLOR };

// Type-safe Data2D using std::variant
class Data2D {
private:
    std::variant<bool, int, float, vec2, cColor> m_data;

public:
    Data2D() : m_data(0.0f) {}
    Data2D(bool v) : m_data(v) {}
    Data2D(int v) : m_data(v) {}
    Data2D(float v) : m_data(v) {}
    Data2D(const vec2 &v) : m_data(v) {}
    Data2D(const cColor &v) : m_data(v) {}

    void set(bool v) { m_data = v; }
    void set(int v) { m_data = v; }
    void set(float v) { m_data = v; }
    void set(const vec2 &v) { m_data = v; }
    void set(const cColor &v) { m_data = v; }

    template<typename T>
    bool holds() const { return std::holds_alternative<T>(m_data); }

    bool getBool() const { return std::get<bool>(m_data); }
    int getInt() const { return std::get<int>(m_data); }
    float getFloat() const { return std::get<float>(m_data); }
    const vec2& getVec2() const { return std::get<vec2>(m_data); }
    // vec2& getVec2Ref() { return std::get<vec2>(m_data); }
    // int& getIntRef() { return std::get<int>(m_data); }
    const cColor& getColor() const { return std::get<cColor>(m_data); }

    // Get pointer to float (for external APIs like ImGui)
    float* getFloatPtr() {
        if (std::holds_alternative<float>(m_data)) {
            return &std::get<float>(m_data);
        }
        return nullptr;
    }
    bool* getBoolPtr() {
        if (std::holds_alternative<bool>(m_data)) {
            return &std::get<bool>(m_data);
        }
        return nullptr;
    }
    int* getIntPtr() {
        if (std::holds_alternative<int>(m_data)) {
            return &std::get<int>(m_data);
        }
        return nullptr;
    }
    vec2* getVec2Ptr() {
        if (std::holds_alternative<vec2>(m_data)) {
            return &std::get<vec2>(m_data);
        }
        return nullptr;
    }
    cColor* getColorPtr() {
        if (std::holds_alternative<cColor>(m_data)) {
            return &std::get<cColor>(m_data);
        }
        return nullptr;
    }

    // const float* getFloatPtr() const {
    //     if (std::holds_alternative<float>(m_data)) {
    //         return &std::get<float>(m_data);
    //     }
    //     return nullptr;
    // }

    bool operator==(const Data2D& other) const { return m_data == other.m_data; }
    bool operator!=(const Data2D& other) const { return m_data != other.m_data; }

    Data2D operator*(float s) const {
        if (holds<float>()) return Data2D(getFloat() * s);
        return *this;
    }

    Data2D operator+(const Data2D& v) const {
        if (holds<float>() && v.holds<float>())
            return Data2D(getFloat() + v.getFloat());
        return *this;
    }

    Data2D operator-(const Data2D& v) const {
        if (holds<float>() && v.holds<float>())
            return Data2D(getFloat() - v.getFloat());
        return *this;
    }
};


//----------------------------------------------------------
struct cStyle
{
    float fontSize = -1;
    cColor color = cTransparent;
    cColor bgColor = cTransparent;
    int familyIndex = -1;
    uint8_t flags = 0;

    static const uint8_t FLAG_IGNORE = 1;
    static const uint8_t FLAG_BOLD = 2;
    static const uint8_t FLAG_ITALIC = 4;

    cStyle() { flags = FLAG_IGNORE; }

    cStyle(float sz, cColor c, bool b = false, bool i = false, int fi = 0)
        : fontSize(sz), color(c), familyIndex(fi), flags(0)
    {
        if(b) flags |= FLAG_BOLD;
        if(i) flags |= FLAG_ITALIC;
    }

    bool ignore() const { return flags & FLAG_IGNORE; }
    void setIgnore(bool v) { if(v) flags |= FLAG_IGNORE; else flags &= ~FLAG_IGNORE; }
    bool bold() const { return flags & FLAG_BOLD; }
    void setBold(bool v) { if(v) flags |= FLAG_BOLD; else flags &= ~FLAG_BOLD; }
    bool italic() const { return flags & FLAG_ITALIC; }
    void setItalic(bool v) { if(v) flags |= FLAG_ITALIC; else flags &= ~FLAG_ITALIC; }

    bool operator==(const cStyle &other) const
    {
        return fontSize == other.fontSize && color == other.color &&
               familyIndex == other.familyIndex && flags == other.flags && bgColor == other.bgColor;
    }

};

struct ParsedChar
{
    cString ch;
    cStyle style;
};


//-------------------------------------------
template <typename T> class TreeIterator
{
public:
    using Iterator = TreeIterator<T>;

private:
    std::stack<T> nodeStack;

public:
    explicit TreeIterator(T root) {
        if (root) {
            nodeStack.push(root);
        }
    }

    bool hasNext() const {
        return !nodeStack.empty();
    }

    T next()
    {
        if (!hasNext())  throw std::out_of_range("No more elements");
        auto curNode = nodeStack.top();
        nodeStack.pop();
        //if(curNode->type!=SHP_GROUP)
        {
            // Push child nodes onto the stack in reverse order to maintain preorder traversal
            for (auto it = curNode->childs.rbegin(); it != curNode->childs.rend(); ++it)
            {
                nodeStack.push(*it);
            }
        }
        return curNode;
    }
};


inline cColor HexToColor(const std::string& hex)
{
    cColor c{0, 0, 0, 255};
    if (hex.size() == 9 && hex[0] == '#') {
        sscanf(hex.c_str() + 1, "%02hhX%02hhX%02hhX%02hhX", &c.r, &c.g, &c.b, &c.a);
    }
    return c;
}

inline std::string compactVec2(const vec2& v)
{
    std::ostringstream oss;
    oss << "[" << v.x << "," << v.y << "]";
    return oss.str();
}

inline nlohmann::json CompactVec3Array(float x, float y, float z) {
    std::ostringstream oss;
    oss << "[" << x << "," << y << "," << z << "]";
    return nlohmann::json::parse(oss.str());  // force single-line array
}

inline nlohmann::json CompactVec2Array(const vec2& v) {
    std::ostringstream oss;
    oss << "[" << v.x << "," << v.y << "]";
    return nlohmann::json::parse(oss.str());  // force single-line array
}

inline nlohmann::json CompactRectArray(const rect& r) {
    std::ostringstream oss;
    oss << "[" << r.pos.x << "," << r.pos.y << "," << r.size.x << "," << r.size.y << "]";
    return nlohmann::json::parse(oss.str());  // force single-line array
}

inline bool jsonToBool(const nlohmann::json& j, const cString &key, bool defVal=false)
{
    if(j.contains(key))    { return j[key]; }
    return defVal;
}

inline int jsonToInt(const nlohmann::json& j, const cString &key, int defVal=false)
{
    if(j.contains(key))    { return j[key]; }
    return defVal;
}

inline float jsonToFloat(const nlohmann::json& j, const cString &key, float defVal=0.0f)
{
    if(j.contains(key))    { return j[key]; }
    return defVal;
}
inline cString jsonToString(const nlohmann::json& j, const cString &key, const cString &defVal="")
{
    if(j.contains(key))    { return j[key]; }
    return defVal;
}

inline vec2 jsonToVec2(const nlohmann::json& j, const cString &key, vec2 defVal=vec2(0,0))
{
    vec2 v=defVal;
    if(j.contains(key))
    {
        json jv = j[key];
        if(jv.is_array() && jv.size()==2) {
            v.x = jv[0];
            v.y = jv[1];
        }
    }
    return v;
}

inline cColor jsonToColor(const nlohmann::json& j, const cString &key, const cColor &defVal=cBlack)
{
    cColor c = defVal;
    if(j.contains(key))
    {
        c = HexToColor(j[key]);
    }
    return c;
}

inline rect jsonToRect(const nlohmann::json& j, const cString &key)
{
    rect r;
    if(j.contains(key))
    {
        json jr = j[key];
        if(jr.is_array() && jr.size()==4) {
            r.pos.x = jr[0];
            r.pos.y = jr[1];
            r.size.x = jr[2];
            r.size.y = jr[3];
        }
    }
    return r;
}

// static std::string removeCarriageReturn(const std::string& str)
// {
//     std::string result;
//     result.reserve(str.size());
//     for (char c : str) {
//         if (c != '\r') result += c;
//     }
//     return result;
// }

// inline std::string ColorToHex(const cColor& c) {
//     char buf[10];
//     sprintf(buf, "#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
//     return buf;
// }

enum { ASSET_WAITFOR_DOWNLOAD=0, ASSET_IN_CACHE, ASSET_DOWNLOADING, ASSET_DOWNLOAD_OK, ASSET_DOWNLOAD_FAILED };

struct cAsset
{
    int img=0;
    int w,h;
    int prevW,prevH;
    int stat;
    cString fn, prevFn;
    cString title, creator, license, url, landing;
    cString prevUrl;
    cString description;
};

#define MAX_CLIPBOARD_SIZE 10000

// Content type enumeration
typedef enum {
    CLIPBOARD_EMPTY,
    CLIPBOARD_TEXT,
    CLIPBOARD_NUMBER,
    CLIPBOARD_URL,
    CLIPBOARD_EMAIL,
    CLIPBOARD_FILE_PATH,
    CLIPBOARD_COLOR_HEX,
    CLIPBOARD_JSON,
    CLIPBOARD_XML,
    CLIPBOARD_CODE,
    CLIPBOARD_UNKNOWN
} ClipboardContentType;

// Structure to hold clipboard analysis
typedef struct {
    ClipboardContentType type;
    char content[MAX_CLIPBOARD_SIZE];
    int length;
    bool hasNewlines;
    int lineCount;
    bool hasNumbers;
    bool hasSpecialChars;
} ClipboardData;

// Function to detect if string is XML
static bool IsXML(const char *text)
{
    if (text == NULL || strlen(text) == 0) return false;
    return (strncmp(text, "<?xml", 5) == 0 || text[0] == '<');
}

// Function to detect if string is JSON
static bool IsJSON(const char *text)
{
    if (text == NULL || strlen(text) == 0) return false;
    // Simple check: starts with { or [ and contains common JSON patterns
    if ((text[0] == '{' || text[0] == '[') &&
        (strstr(text, "\":") != NULL || strstr(text, "\": ") != NULL)) {
        return true;
    }
    return false;
}

// Analyze clipboard content
static ClipboardData AnalyzeClipboard(const char *text)
{
    ClipboardData data = {CLIPBOARD_EMPTY};

    if (text == NULL || strlen(text) == 0) {
        data.type = CLIPBOARD_EMPTY;
        return data;
    }

    strncpy(data.content, text, MAX_CLIPBOARD_SIZE - 1);
    data.length = strlen(text);

    // Analyze content characteristics
    data.lineCount = 1;
    for (int i = 0; i < data.length; i++) {
        if (text[i] == '\n') {
            data.hasNewlines = true;
            data.lineCount++;
        }
        if (text[i] >= '0' && text[i] <= '9') {
            data.hasNumbers = true;
        }
        if (strchr("!@#$%^&*(){}[]<>:;'\",./\\|", text[i]) != NULL) {
            data.hasSpecialChars = true;
        }
    }
    // Detect content type (order matters!)
    if (IsJSON(text)) {
        data.type = CLIPBOARD_JSON;
    } else if (IsXML(text)) {
        data.type = CLIPBOARD_XML;
    }
    // else if (IsHexColor(text)) {
    //     data.type = CLIPBOARD_COLOR_HEX;
    // }
    // else if (IsURL(text)) {
    //     data.type = CLIPBOARD_URL;
    // }
    // else if (IsEmail(text)) {
    //     data.type = CLIPBOARD_EMAIL;
    // }
    // else if (IsFilePath(text)) {
    //     data.type = CLIPBOARD_FILE_PATH;
    // }
    // else if (IsNumber(text)) {
    //     data.type = CLIPBOARD_NUMBER;
    // }
    // else if (IsCode(text)) {
    //     data.type = CLIPBOARD_CODE;
    // }
    else {
        data.type = CLIPBOARD_TEXT;
    }

    return data;
}

#endif // AXDEFS_H
