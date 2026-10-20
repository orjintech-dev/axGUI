#ifndef AXGUISYSTEM_H
#define AXGUISYSTEM_H

// #include <GL/glew.h>
#include "raylib.h"
// #include "rlgl.h"
#include <string>
#include <vector>
// #include "nanovg.h"
#include "axDrawBot.h"
#include "axDefs.h"
// #include "axGUITextBox.h"
// #include <iostream>
#include <cstring>
// #include <map>
#include <unordered_map>
#include "parameter.h"
#include "axFont.h"
#include "axGUITable.h"
// #include "axUTF8.h"
// #include "myAnimSystem.h"

// #include "utf8.h"  // from utf8cpp
// "using namespace std;" is not used here: it would leak into every file that includes this header

#define MAX_LINES 128
#define MAX_LINE_LEN 256

// ---- language (library strings: style editor etc.; the application can use it too)
enum axLanguage { AX_LANG_EN = 0, AX_LANG_TR = 1 };
inline int axLang = AX_LANG_EN;
inline const char *axTr(const char *en, const char *tr) { return axLang == AX_LANG_TR ? tr : en; }
// "Label###id" (like ImGui): the id is whatever follows "###" and stays the same when the language / label changes (window position,
// section / tab state are preserved); the visible text is whatever precedes "##". Applies to windows, sections, group boxes, tabs and separators.
inline const char *axIdPart(const char *s) { const char *p = s ? strstr(s, "###") : nullptr; return p ? p + 3 : s; }
inline std::string axLabelPart(const char *s) { if (!s) return std::string(); const char *p = strstr(s, "##"); return p ? std::string(s, p - s) : std::string(s); }

inline float titleBarH=24;   // inline instead of static: a single variable across all translation units (so the style editor can modify it)
inline long frameCnt=0;   // C++17 inline: single copy across all translation units


template<typename T> T Clamp(T value, T min, T max) {
    return (value < min) ? min : (value > max) ? max : value;
}
enum ItemType {
    ITEM_0=0,
    ITEM_LABEL,
    ITEM_BUTTON,
    ITEM_CHECKBOX,
    ITEM_SLIDER,
    ITEM_COLORPICKER,
    ITEM_COMBOBOX,
    ITEM_DRAGFLOAT,
    ITEM_TEXTBOX,
};

// struct GuiItem
// {
//     int id;
//     ItemType type;
//     cString name;
//     rect geometry;
//     bool hovered, focused, clicked;
//     float valuef;
//     int valuei;
//     cColor color;
//     std::vector<cString> items; // for combobox
// };

struct GroupStyle
{
    float bevel;
    bool shadow;
    cColor fClr;
    float stroke=0;
    cColor sClr;
    float padding=8;   // inner padding (configurable from outside)
};

struct ItemState
{
    cString name;
    bool movable = false;
    bool clicked = false;
    bool hovered = false;
    bool focused = false;
    bool dragging = false;
    bool open = true;
    long frame = 0;
    float rad = 0, strk = 0;
    vec2 pos, size;

    void set(const cString &nm, const vec2 &p, const vec2 &s, bool h, bool f, float r)
    {
        name = nm;
        pos = p;
        size = s;
        hovered = h;
        focused = f;
        frame = frameCnt;
        rad = r;
    }
};

struct ColorPickerState
{
    cColor color;
    bool draggingR = false;
    bool draggingG = false;
    bool draggingB = false;
};

struct ComboBoxState : public ItemState
{
    bool open = false;
    int selIx = 0;
    bool lastClick = false;  // move here
};

struct DragFloatState : public ItemState
{
    bool isDragging;
    bool isBtn1Down;
    bool isBtn2Down;
    bool isEditing;        // NEW
    std::string editBuffer; // NEW
    float initialValue;
    vec2 msDown;
    int cursor = 0, selLen = 0;

};


struct ColorSliderState {
    bool isDragging;
};

// struct TextBoxState0 {
//     std::string text;             // UTF-8
//     int cursor = 0;               // byte index
//     int sel_anchor = 0;           // byte index
//     bool focused = false;

//     double blinkStart = 0.0;
//     float  scrollX = 0.0f;
//     const char* placeholder = nullptr;

//     // runtime-only helpers
//     bool dragging = false;        // mouse selection drag capture
//     double lastClickTime = 0.0;   // for double/triple click
//     int    clickCount = 0;
// };
struct TextBoxState;
struct RichTextState;   // rich/axGUIRichText.h

struct GroupBoxState {
    bool isOpen = true, scrolling = false;
    bool usedThisFrame = false;
    bool clicked = false;
    vec2 speed = vec2(0,0);
    vec2 scrollOffset = vec2(0, 0);
    vec2 scrollOffsetStart = vec2(0, 0);
    float contentHeight = 0;
    vec2 contentStartPos;
    vec2 winScroll;
    vec2 curPos, size;
    rect layoutArea;
    float openT = 1;          // open animation 0..1 (collapseDuration)
    bool animInit = false;
    SmoothScroll wheelScroll; // vertical wheel scrolling (gui.wheelSmooth)
};

struct GroupBoxContext {

    vec2 startPos;
    vec2 size;
    rect oldClip;
    unsigned int id;
    bool isOpen, clicked;
    int scrollable;
    bool animating = false, prevUseInput = true;   // no input while opening / closing
    float outerLineH = 0;                           // height of the outer row (zeroed inside, restored afterwards)
    float padding = 8;                              // GroupStyle::padding
};

// Open / close animation of collapsible containers (beginSection, ...)
struct CollapseAnim { float t = 1; bool init = false; float contentH = 0; };
struct CollapseFrame { int id = 0; float startY = 0, shownH = 0; bool animating = false, prevUseInput = true; };


// Tiny “check” animation (0..1)

struct Anim
{
    float t = 0.f, speed=1.0f;
    long frame=0;
    cColor startClr, endClr;
    vec2 startV2, endV2;

    void init(float spd, cColor str, cColor end)
    {
        t=0; speed=spd;
        frame = frameCnt;
        startClr=str;
        endClr=end;
    }
    void init(float spd, vec2 v1, vec2 v2)
    {
        t=0; speed=spd;
        frame = frameCnt;
        startV2=v1;
        endV2=v2;
    }
    cColor color()
    {
        frame = frameCnt;
        t += speed*GetFrameTime();
        float tt = (t<0)?0:(t>1)?1:t;
        // return lerp<cColor>(start, end, t);
        return startClr + (endClr-startClr)*tt;
    }

    vec2 v2()
    {
        frame = frameCnt;
        t += speed*GetFrameTime();
        float tt = (t<0)?0:(t>1)?1:t;
        // return lerp<cColor>(start, end, t);
        return startV2 + (endV2-startV2)*tt;
    }
};


// Key repeat timing
// struct KeyRepeat {
//     int key = -1;
//     double pressTime = 0.0;
//     double lastRepeatTime = 0.0;
//     double initialDelay = 0.4;
//     double repeatInterval = 0.033;
// };


// struct IconDrawOpts {
//     float rotation = 0.0f;     // radians
//     vec2  pivot   = {0.5f,0.5f}; // normalized pivot inside dst rect
//     NVGcolor tint = nvgRGBA(255,255,255,255); // modulate color (acts like multiply)
//     float alpha   = 1.0f;      // additional alpha
//     bool  pixelSnap = true;    // snap dst pos to whole pixels
// };

// ---------- Per-popup state (keyed by items buffer) ----------
struct PopupState {
    bool open;
    vec2 pos, size;
    int hover = 0;   // currently selected/hovered index
    int first = 0;   // first visible index (scroll)
};

enum WinFlags_ {
    WIN_0 = 0,
    WIN_SCROLLBAR     = 1<<0,
    WIN_NO_MOVE       = 1<<1,
    WIN_AUTO_NEWLINE  = 1<<2,
    WIN_NO_SAMELINE   = 1<<3,
    WIN_TITLE_BAR     = 1<<4,
    WIN_POPUP         = 1<<5,
    WIN_CHILD_WINDOW  = 1<<6,
    WIN_RESIZABLE     = 1<<7,
    WIN_TOPMOST       = 1<<8,
};

typedef int WinFlags;

struct LayoutStateGuard
{
    WinFlags &flags;
    vec2  &curPos, &curPos0, &nextPos;
    rect  &layoutArea;
    WinFlags sFlags;
    vec2   sCurPos, sCurPos0, sNextPos;
    rect   sLayoutArea;

    LayoutStateGuard(WinFlags &f, vec2& cp, vec2& cp0, vec2& np, rect& la)
        : flags(f), curPos(cp), curPos0(cp0), nextPos(np), layoutArea(la),
        sFlags(f), sCurPos(cp), sCurPos0(cp0), sNextPos(np), sLayoutArea(la) {}

    ~LayoutStateGuard() noexcept {
        curPos     = sCurPos;
        curPos0    = sCurPos0;
        nextPos    = sNextPos;
        layoutArea = sLayoutArea;
    }
};

struct TabItemInfo {
    std::string label;
    uint32_t    id          = 0;
    float       width       = 0.0f;   // computed tab width
    float       offsetX     = 0.0f;   // x position inside the bar
    bool        visible     = true;
    bool        closable    = false;
};

struct TabBarState {
    uint32_t                  activeTabId    = 0;
    uint32_t                  hoveredTabId   = 0;
    std::vector<TabItemInfo>  tabs;
    float                     scrollOffset   = 0;
    float                     scrollTarget   = 0;   // target scroll position (for momentum scrolling)
    float                     scrollVelocity = 0;    // momentum scroll velocity
    float                     totalTabWidth  = 0;
    float                     visibleWidth   = 0;
    bool                      needsLayout    = true;
    bool                      dragging       = false; // left mouse drag
    float                     dragStartScroll= 0;     // scroll at drag start
    float                     dragStartMs    = 0;     // mouse X at drag start
    float                     dragPrevMs     = 0;     // mouse X in the previous frame

    bool hasOverflow() const { return totalTabWidth > visibleWidth; }
    float maxScroll() const  { return fmax(0.0f, totalTabWidth - visibleWidth); }
};

struct TabBarStyle {
    float   tabHeight         = 24.0f;
    float   tabPaddingH       = 8.0f;   // horizontal padding
    float   tabSpacing        = 2.0f;    // spacing between tabs
    float   tabBorderRadius   = 4.0f;    // top corner rounding
    float   underlineHeight   = 3.0f;    // active tab underline
    float   closeButtonSize   = 14.0f;

    cColor   barBackground     = {235, 235, 235, 255};
    cColor   tabNormal         = {220, 220, 220, 255};
    cColor   tabHovered        = {230, 230, 235, 255};
    cColor   tabActive         = {255, 255, 255, 255};
    cColor   tabTextNormal     = {100, 100, 100, 255};
    cColor   tabTextActive     = {30,  30,  30,  255};
    cColor   underlineColor    = {66, 133, 244, 255};   // blue accent
    cColor   separatorColor    = {200, 200, 200, 255};
    cColor   closeHovered      = {220, 60,  60,  255};
};

static KeyRepeat keyRepeat;

static bool shouldKeyTrigger(int key)
{
    double currentTime = GetTime();

    if (IsKeyPressed(key)) {
        keyRepeat.key = key;
        keyRepeat.pressTime = currentTime;
        keyRepeat.lastRepeatTime = currentTime;
        return true;
    }

    if (IsKeyDown(key) && keyRepeat.key == key) {
        double timeSincePress = currentTime - keyRepeat.pressTime;
        double timeSinceRepeat = currentTime - keyRepeat.lastRepeatTime;

        if (timeSincePress >= keyRepeat.initialDelay &&
            timeSinceRepeat >= keyRepeat.repeatInterval) {
            keyRepeat.lastRepeatTime = currentTime;
            return true;
        }
    }

    if (!IsKeyDown(key) && keyRepeat.key == key) {
        keyRepeat.key = -1;
    }

    return false;
}
struct WindowState
{
    // const char *winID=NULL;
    const char *prWinName=NULL; // parent window name
    const char *name=NULL;
    long ID=0;
    int order=0, zOrder=0;
    bool init=false;
    WinFlags flags;
    bool moving=false;
    bool scrolling=false;
    long frame=0;
    vec2 scroll0;
    vec2 scroll = vec2(0,0);
    SmoothScroll wheelScroll;   // vertical wheel scrolling (gui.wheelSmooth)
    vec2 speed = vec2(0,0);
    vec2 pos, size, relPos;
    vec2 anchor;      // inner window: origin of the position (parent window's cursor + scroll); pos = anchor + relPos
    vec2 minSize = vec2(100,50);
    vec2 curPos, curPos0, nextPos = vec2(0,0);
    rect sLayoutArea;
    void setGeometry(const vec2 &p, const vec2 &s) { pos=p; size=s; sLayoutArea=rect(p,s); }
    void setMargins(float mx, float my) { sLayoutArea = rect(pos.x+mx, pos.y+my, size.x-2*mx, size.y-2*my); }
    rect geometry() { return rect(pos,size); }
    rect resizeArea() { return rect(pos+size-vec2(20,20), vec2(20,20)); }
    rect resizeRightArea() { return rect(pos+vec2(size.x-6,0), vec2(6,size.y)); }
    rect resizeLeftArea() { return rect(pos, vec2(6,size.y)); }
    rect resizeBottomArea() { return rect(pos+vec2(0,size.y-6), vec2(size.x,6)); }
    rect resizeTopArea() { return rect(pos, vec2(size.x,6)); }
    rect titleArea() { return rect(pos,vec2(size.x,titleBarH)); }
    rect layoutArea()
    {
        rect r = sLayoutArea;
        // if(flags&WIN_SCROLLBAR) r.size.x -= 12;
        if(flags&WIN_TITLE_BAR) { r.pos += vec2(0,titleBarH); r.size.y -= titleBarH; }
        return r;
    }
    const char* debugStr()
    {
        return axFormat("%s<%d>:%d,  pos:%.0f,%.0f,  scr:%.0f,%.0f,  mov:%d",name, ID, zOrder, pos.x, pos.y, scroll.x, scroll.y, moving);
    }
};

enum GUICOLORS {
    GUI_IDLE_COLOR=0,
    GUI_HOVER_COLOR,
    GUI_BTN_COLOR,
    GUI_BTN_HOVER_COLOR,
    GUI_BTN_SEL_COLOR,
    GUI_BTN_TOGGLE_COLOR,
    GUI_TEXT_COLOR,
    GUI_TEXT_HL_COLOR,
    GUI_TEXT_SEL_COLOR,
    GUI_WIN_BACKCLR,
    GUI_WIN_BORDERCLR,
    GUI_TOOLTIP_BG,      // tooltip background
    GUI_TOOLTIP_TEXT,    // tooltip text
    GUI_COLOR_COUNT
};

// Pointer type (set by the application before startFrame). On touch, the last position after the finger lifts is not
// considered hover; mouse and a hovering pen (S Pen) do hover.
enum GuiPointer {
    PTR_MOUSE=0,   // mouse: always hover
    PTR_TOUCH,     // finger / pen touching the screen: hover only while pressed (and on the release frame)
    PTR_PEN        // pen hovering without touching the screen: hover
};

static const float TAB_ARROW_WIDTH = 22.0f;

// Check if point is inside rectangle
class axGUI
{
    enum { HNDL_NONE=0, HNDL_TOP=1, HNDL_BOTTOM=2, HNDL_LEFT=3, HNDL_RIGHT=4, HNDL_BOTRIGHT=5, HNDL_TOPLEFT, HNDL_TOPRIGHT, HNDL_BOTLEFT };
    rect winClip;
    vec2 curPos, curPos0, nextPos = vec2(0,0);
    WindowState moveRefWin, hoverWin;
    bool winFocused = false, winHovered = false;
    const char *prWinName=NULL;     //--- current window's parent window name
    const char *hoverWinName=NULL;
    int msCursorType = MOUSE_CURSOR_DEFAULT;
    bool drawComboList=false;
    // if the mouse is over an open dropdown list, widgets underneath do not see the mouse (ms is taken away from the GUI) and the list reads the real position from msCombo; otherwise pressing the list would also focus the widget underneath (e.g. dragInt)
    bool overComboList=false;
    vec2 msCombo = vec2(0, 0);
    ComboBoxState actComboState;
    vec2 actComboPos, actCombosize;
    int actComboID = 0;
    std::vector<IdStrPair> actComboitems;
    int winOrder=1;
    // --- TabBar state ---
    struct ActiveTabBar {
        uint32_t     id          = 0;
        vec2         origin      = {0, 0};   // bar start position
        float        barHeight   = 0.0f;
        float        contentY    = 0.0f;     // Y where the tab content starts
        bool         anyTabOpen  = false;     // at least one tab item is open
        int          currentTabIndex = -1;
        float        contentEndY = 0.0f;   // Y position where the content ends
        vec2         savedCurPos;           // cursor before beginTabBar
        vec2         savedNextPos;
    };
    std::vector<ActiveTabBar> tabBarStack;
public:
    TabBarStyle tabStyle;          // accessible to the style editor / theme

public:
    axFont *curFont=nullptr;
    // cString fontFace="default";
    float scrw, scrh;
    int focusWinID=0;
    const char *focusWinName=NULL;
    const char *scrollWinName=NULL;  //--- scrolling window name
    WinFlags winflag=WIN_AUTO_NEWLINE;
    bool debug=false;
    bool mNewLine=false, mSameLine=false;
    bool overGUI = false, proc=false;
    bool useInput = true;

    int iconsImage=0;
    int ID, cntID;        // current widget ID
    int focusID=0; // focused GUI item
    int hoverID=0; // hovered GUI item
    int editID=0;  // editing GUI item
    int dragID=0;

    int hoverWinID=0;  // hovered window ID
    int hoverWinID0=0; // hovered window ID previous frame

    // --- input ownership (for the application; like ImGui io.WantCaptureMouse / WantCaptureKeyboard) ---
    // While a button is held, ownership is decided where the press started: a drag started in the GUI does not pass to the canvas, a drag
    // started on the canvas does not pass to the GUI when over a panel. With no button held, the GUI owns the mouse if it is over a window.
    int pressWinID=0;                // window where the last press started (0: outside the GUI); until buttons are released
    bool wantCaptureMouse=false;     // computed in startFrame
    bool wantCaptureKeyboard=false;  // a textBox was focused in the previous frame
    bool kbCaptureNext=false;        // a focused textBox was drawn this frame (next frame's wantCaptureKeyboard)
    // --- pointer type ---
    GuiPointer pointerType = PTR_MOUSE;   // set before startFrame
    bool hoverActive = true;              // whether hover is valid this frame (if not, ms is treated as outside the GUI)
    bool canHover() const { return pointerType != PTR_TOUCH; }   // whether it can be hovered without pressing (highlight, tooltip)
    int cursorRequest = -1;               // mouse cursor (MOUSE_CURSOR_*) requested by a widget this frame; endFrame applies it
    int moveWinID=0;  // moving window ID
    int resizeWinID=0;  // resizing window ID
    int scrollGroup = 0;
    int winResizeHndl = 0;
    vec2 winResizeFactor = vec2(1,1);
    vec2 dragStartPos;

    float btnRadius=4, btnStrk=2;
    float checkRadius=4;     // checkbox corner radius (default min(4, box/4))
    float frameRadius=4;     // input fields: dragFloat / dragInt / comboBox (default 2 / 4)
    float grpRadius=8, grpStrk=2;
    float winRadius=8, winStrk=2;

    float fontSize=12, textSize=12;
    float leftMargin=4; // text left space
    float winSpaceX=8, winSpaceY=4;
    float itemSpaceX=4, itemSpaceY=4;
    float lineHMax=0; //--- max item height for the line
    double msTime=0;
    // tooltip: appears hintTime (ms) after the mouse enters a new item; stays while the mouse moves within the same item;
    // hides when the mouse leaves the item or clicks it (after a click it does not reappear until another item is hovered).
    float hintTime=400;
    int hintOwner=0, hintReqID=0;
    bool hintReq=false, hintSuppressed=false;
    double hintStart=0;

    bool msDragging=false;
    bool mouseMoved=false;
    double msDownTime=0;
    vec2 ms, msDown, msPre, msTarget,  msSpeed=vec2(0,0);
    bool alt=false, shift=false, ctrl=false, esc=false, qual=false;
    vec2 winScroll = vec2(0,0);
    bool ignoreScroll = false;
    // char lines[MAX_LINES][MAX_LINE_LEN] = { "" };
    axDrawBot *bot = nullptr; // pointer to the draw;

    int cursorLine = 0, cursorCol = 0;
    rect layoutArea = rect(0,0,0,0);
    // NVGcontext* vg;
    cString hintStr, lastHintStr;
    vec2 hintPos, hintSize = vec2(80,60);;
    bool msLeftAction; // current mouse left state
    bool msrightAction; // current mouse right state
    bool msLeftPressed, msLeftDown, msLeftReleased;
    bool msRightPressed, msRightDown, msRightReleased;
    bool msAnyPressed, msAnyReleased;
    float msWheel=0;                 // this frame's mouse wheel; a widget that consumes the wheel sets it to 0 (so the window does not scroll)
    float wheelStep=48;              // window scrolling: px per wheel notch
    float wheelSmooth=0.08f;         // wheel scrolling eases toward its target with this time constant (s); 0: instant
    // Wheel latch: once a window starts scrolling by wheel, the wheel stays with that window until scrolling ends;
    // widgets underneath (table, text box, dropdown, group box...) do not receive it. Ends when the wheel is idle for wheelLatchDelay
    // seconds or the cursor leaves the window.
    float wheelLatchDelay=0.35f;
    long wheelLatchID=0;             // ID of the latched window (0: none)
    std::string wheelLatchName;      // name of the latched window (windowStates key)
    double wheelLatchLast=0;         // last wheel event (GetTime)
    float wheelLatched=0;
    // Collapsible containers: beginSection / beginGroupBox open and close their content over this duration (s);
    // 0: no animation. Content is fully laid out every frame; only the animated height is visible.
    float collapseDuration=0.18f;
    // text position: false (default) = pixel-snapped, fractional (smooth) in a window while its collapse animation runs;
    // true = always smooth (static text softens slightly at fractional positions).
    bool smoothText=false;
    bool winSnap=true;   // snap to the edges of other windows and the screen when moving / resizing windows
    float hoverDuration=0.25f;   // button / section hover transition (s); 0: instant
    std::unordered_map<int, CollapseAnim> collapseAnims;
    std::vector<CollapseFrame> collapseStack;
    // Generic collapsible container: animates open toward its target. If it returns false the content is not drawn (endCollapse must not be called);
    // if it returns true, endCollapse() must be called after the content.
    bool beginCollapse(int id, bool open);
    void endCollapse();
    float stepOpenAnim(float &t, bool open);   // advances t toward the target, returns the eased value (0..1)            // this frame's wheel saved for the latched window (widgets see msWheel=0)
    KeyRepeat keyRepeat;

    cColor colors[GUI_COLOR_COUNT];
    cColor colorsSaved[GUI_COLOR_COUNT];
    cColor *clrPickerPtr=NULL;
    cColor clrPickerColor;
    vec2 clrPickerPos;
    bool clrPickerUpdate=false;

    bool showClrPicker=false;
    rect clrPickerAnchor = rect(0, 0, 0, 0);   // button that opened the picker (screen); opens above if there is no room below
    // color picker eyedropper
    bool pickerEyedrop=false, pickerEyedropClick=false, pickerSwallow=false;
    vec2 pickerEyedropPos = vec2(0, 0);
    // int lineCount = 1;
    // float lineHeight = 24;
    GroupBoxContext currentGroupBox;

    std::unordered_map<int, Anim> anims;
    std::unordered_map<int, DragFloatState> dragStates;
    std::unordered_map<int, ComboBoxState> comboStates;
    std::unordered_map<unsigned int, ColorSliderState> colorSliderStates; // Add alongside dragStates
    std::unordered_map<int, ItemState> itemStates;
    std::unordered_map<int, ItemState> sectionStates;
    // global/static map holding the state of the tab bars each frame
    std::unordered_map<uint32_t, TabBarState> g_tabBarStates;

    std::unordered_map<std::string, WindowState> windowStates;
    // std::unordered_map<std::string, IconDef> iconMap;
    std::unordered_map<unsigned int, GroupBoxState> groupBoxStates;
    std::vector<GroupBoxContext> groupBoxStack;
    std::stack<rect> layoutStack, clipStack;
    std::stack<vec2> scrollStack;
    std::vector<vec2> snapPts;

    axGUI();

    void genID(const char* label = nullptr);
    int lastID();
    int hashString(const char* str)
    {
        int hash = 5381;
        while(*str) {
            hash = ((hash << 5) + hash) + *str++;
        }
        return hash;
    }

    bool isPointInRec(vec2 pt, vec2 pos, vec2 size);

    void setCursor(vec2 p);
    void setCursorX(float x);

    vec2 cursorPos();
    vec2 lastItemPos();

    void newline(float lineH=0);
    void spacerV(float lineH=0);
    void setLineH(float lineH);
    void spacerH(float spaceX);

    void toBeginOfline();

    ///---------------------------------------------------------------------
    void advanceCursor(vec2 size);

    bool outOfLayoutArea(const vec2 &size);

    ///---------------------------------------------------------------------
    void setfont(const cString &face, float sz);
    void setfont(axFont *f, float sz);

    ///---------------------------------------------------------------------
    void setFontsize(float sz) { fontSize = sz; bot->setfontsize(sz); }

    void saveColors();

    void restoreColors();

    // ---- derive from theme colors (instead of fixed colors)
    static cColor mixColor(const cColor &a, const cColor &b, float t) {
        return cColor((int)roundf(a.r + (b.r - a.r) * t), (int)roundf(a.g + (b.g - a.g) * t),
                      (int)roundf(a.b + (b.b - a.b) * t), (int)roundf(a.a + (b.a - a.a) * t));
    }
    static cColor withAlpha(cColor c, int a) { c.a = a; return c; }
    // hover for visuals: no highlight while a button is held elsewhere (another widget is being dragged)
    bool hoverVisual(bool hovered, bool active) const { return hovered && (active || !msLeftDown || msLeftPressed); }
    // shifts c toward the text color by t: lightens in a dark theme, darkens in a light theme (alpha preserved)
    cColor contrast(const cColor &c, float t) const { cColor r = mixColor(c, colors[GUI_TEXT_COLOR], t); r.a = c.a; return r; }

    void setColor(int which, cColor c)
    {
        if(which>=0 && which<GUI_COLOR_COUNT)  colors[which] = c;
    }

    void resetStates()
    {
        windowStates.clear();
    }

    void resized(vec2 sz, vec2 sz0)
    {
        float scrw = GetScreenWidth();
        float scrh = GetScreenHeight();
        for(auto it=windowStates.begin(); it!=windowStates.end(); ++it)
        {
            auto &win = it->second;
            win.pos.x = (win.pos.x/sz0.x)*sz.x;
            win.pos.y = (win.pos.y/sz0.y)*sz.y;
            /*if(win.pos.y+win.size.y>scrh) */win.size.y = (win.size.y/sz0.y)*sz.y;
            win.size.x = std::fmax(win.minSize.x, (win.size.x/sz0.x)*sz.x);
        }
    }

    ///---------------------------------------------------------------------
    void startFrame(bool retained);

    void normalizeZOrders();

    ///---------------------------------------------------------------------
    void endFrame();

    float getContainerWidth();

    long winID(const char *id);

    void setWinSize(const char *name, vec2 size);

    ///---------------------------------------------------------------------
    WindowState& beginWindow(const char *name, vec2 &pos, vec2 &size, WinFlags flags=WinFlags(0));

    ///---------------------------------------------------------------------
    void endWindow(const char *name);

    vec2 snapToWindows(WindowState &ourWin, vec2 p);

    void resetScroll(const char *id);


    void pushWinStack();

    void popWinStack();

    ///---------------------------------------------------------------------
    bool isInsideLayout();

    void sameline();

    bool beginGroupBox(const char* title, vec2 size, bool collapsible = true,
                       int scrollable = 0, const GroupStyle &st={}, const char* hint = NULL);


    void endGroupBox();

    ///---------------------------------------------------------------------
    void loadImages();
    // Loads the icon atlas and registers the names the library uses
    // (tri1_icon: open section, tri2_icon: closed section, circle_icon). Cell size in px.
    bool loadIconAtlas(const char *path, float cell = 64.0f);

    ///---------------------------------------------------------------------
    // Register a free-rect icon by name
    // IconDef *iconDef(const std::string& name);
    // void addIcon(const std::string& name, int image, rect srcPx, float pad = 0.0f);
    // void drawIcon(const std::string& name, const vec2 &pos, float sz, const cColor &tint=cWhite);

    ///---------------------------------------------------------------------

    void setHint(bool hovered, const char *hint, vec2 pos);

    ///---------------------------------------------------------------------

    void text(const std::string &str, float fsz, cColor col, vec2 off=vec2(0,0), int align=BOT_ALIGN_LEFT|BOT_ALIGN_TOP);

    void label(const char *text, const vec2 &size, int align=BOT_ALIGN_LEFT|BOT_ALIGN_MIDDLE);

    void separator(float gap=4, const char *title=nullptr);

    bool beginSection(const char *title=nullptr);

    void endSection(const char *title=nullptr);

    bool ComboBoxList(ComboBoxState& state, const vec2 &pos, const vec2 &size, const std::vector<IdStrPair> &items, bool endMode);

    bool comboBox(const vec2 &size, const std::vector<IdStrPair> &items, int &value, const char* hint=NULL);

    //---- size.x=0 auto width
    ItemState& dummy(vec2 size=vec2(0,0), const char *hint=NULL);

    bool buttonHovered(const vec2 &pos, const vec2 &size);

    // hover animation: updates bgClr / pos; returns the eased hover (0..1), for the text color
    float buttonAnim(int ID, ItemState &item, bool hovered, cColor &bgClr, vec2 &pos);

    bool buttonBehavior(ItemState &item, bool hovered);

    bool selectable(const char *text, bool selected, bool fitH=true, float textSize=0);

    //---- size.x=0 auto width
    bool button(const char *text, vec2 size=vec2(0,0), float fsz=0, const char *hint=NULL);

    //---- size.x=0 auto width
    bool toggleButton(const char *text, bool* value, vec2 size=vec2(0,0), const char *hint=NULL);

    bool imageButton(const std::string& name, vec2 size, cColor tint=cColor(255), const char *hint=NULL);

    bool imageButton(IconDef &def, vec2 size, cColor tint=cColor(255), const char *hint=NULL);

    bool shapeButton(vec2 size, const std::string &shape, vec2 shapeSize, cColor clr, float strk, cColor sClr, const char *hint=NULL);

    bool colorButton(vec2 size, vec2 shapeSize, cColor *clrPtr, float strk, cColor sClr, const char *hint=NULL);

    bool checkbox(const char* label, bool* checked, const vec2& size, vec2 box=vec2(0,0));


    bool dragVec2(const char *txt, float txSize, int align, vec2 size, int min, int max, vec2 *val1, vec2 def, float speed=0.1f, const char *frmt="%.1f", bool buttons=false, const cString &hint="");   // NULL -> std::string would crash

    bool dragFloat(vec2 size, Param &prm, float speed=0.1f, const char *frmt="%.2f", bool buttons=false, const char *hint=NULL);

    bool dragFloat(vec2 size, float min, float max, float* value, float def=0, float speed=0.1f, const char *frmt="%.1f", bool buttons=false, const char *hint=NULL);

    bool dragFloat3(vec2 size, float min, float max, float v[3], float speed=0.1f, const char *frmt="%.1f", bool buttons=false, const char *hint=NULL);

    bool dragInt(vec2 size, Param &prm, float speed=0.1f, bool buttons=false, const char *hint=NULL);

    bool dragInt(vec2 size, int min, int max, int* value, float speed=1.0f, bool buttons=false, const char *hint=NULL);

    bool dragColor(vec2 size, cColor &clr, float speed=0.1f, const char *frmt="%.1f", bool buttons=false, const char *hint=NULL);

    float slider(vec2 off, vec2 size, float min, float max, float* value);


    bool drawColorSlider(vec2 off, vec2 size, float* value, float vmax, cColor barColor);

    std::vector<cColor> pickerColors={ cBlack, cWhite, {255,0,0},{0,255,0},{0,0,255},{255,255,0},{255,0,255},{0,255,255},{255,128,0},{128,0,255},{128,128,128},{255,192,192},
                                  {192,255,192},{192,192,255},{255,255,192},{255,192,255},{192,255,255},{0,0,0},{-1}};

    ColorPickerState picker;
    bool drawColorPicker(cColor &clr, vec2 pos, vec2 size, bool &open);

    bool colorSelector(const char *text, bool &open, vec2 cpos, vec2 size, float itemH, cColor &clr);

    int popupMenu(const char *text, PopupState &st, vec2 pos, vec2 size, float itemH, const std::vector<cString> &items);


    // Returns true if confirmed: Enter in single-line, Ctrl+Enter in multi-line.
    // Use st.changed for text changes, st.focused for focus.
    bool textBox(TextBoxState& st, vec2 size);

    // Rich text box (rich/axGUIRichText.h): Quill Delta document, formatting, images, links.
    // size.x=0 remaining width, size.y=0 200 px. Returns true if the document changed this frame.
    bool richTextBox(RichTextState &st, vec2 size = vec2(0,0));

    // Table (axGUITable.h): size.x=0 remaining width, size.y=0 all rows. Row indices are data indices.
    // Returns true if an event occurred this frame (selection, click, double click / Enter, right click, sort); details in st.
    bool table(TableState &st, vec2 size, const std::vector<TableColumn> &cols, int rows, const TableCellFn &cell,
               int flags = TBL_DEFAULT, const TableDrawFn &drawCell = nullptr, const TableStyle &style = TableStyle());

    void printGUIStats(int showStates);

    bool beginTabBar(const char* strId);
    bool beginTabItem(const char* label, bool* pOpen);
    void endTabItem();
    void endTabBar();

    // ---- style (axGUIStyle.cpp). Similar to ImGui ShowStyleEditor.
    // styleEditor(): content only (call it in your own window; edits the live style and is affected by it).
    // showStyleWindow(): ready-made window; drawn with its own fixed look, unaffected by edits.
    // The look is taken from the style at first open; resetStyleWindowLook() makes the current style the new look (e.g. after a theme change).
    void styleEditor();
    void showStyleWindow(bool *open = nullptr);
    // initial position / size of the style window (x < 0: top right of the screen). After opening, state lives in windowStates["axgui_style"].
    vec2 styleWinPos = vec2(-1, -1), styleWinSize = vec2(500, 640);
    void resetStyleWindowLook();
    bool saveStyle(const char *path);              // JSON: sizes, colors, text box, tabs
    bool loadStyle(const char *path);
    std::string styleToCpp();                      // C++ lines that set up the same style
    std::string styleFile = "axgui_style.json";
    // void setfont(Font *f, float sz);
    cString scrollInfo();
private:
    void startDrag(int ID, vec2 p);
    void calcWinSnaps();
};


#endif // AXGUISYSTEM_H
