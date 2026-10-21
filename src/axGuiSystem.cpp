#include "axGuiSystem.h"
#include "axGUITextBox.h"
#include <stdio.h>

static rect comboListRect(const vec2 &boxPos, const vec2 &boxSize, int n);   // drop-down list geometry (below)

inline uint32_t hashTabId(const char* str) {
    // FNV-1a
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= static_cast<uint32_t>(*str++);
        hash *= 16777619u;
    }
    return hash;
}

inline uint32_t combineHash(uint32_t a, uint32_t b) {
    a ^= b + 0x9e3779b9 + (a << 6) + (a >> 2);
    return a;
}

axGUI::axGUI()
{
    curPos = vec2(0,0);
    // default: dark theme
    colors[GUI_IDLE_COLOR] = cColor(50, 52, 58);
    colors[GUI_HOVER_COLOR] = cColor(40, 70, 110);
    colors[GUI_BTN_COLOR] = cColor(48, 50, 56);
    colors[GUI_BTN_HOVER_COLOR] = cColor(66, 69, 77);
    colors[GUI_BTN_SEL_COLOR] = cColor(40, 70, 110);
    colors[GUI_BTN_TOGGLE_COLOR] = cColor(66, 140, 230);
    colors[GUI_TEXT_COLOR] = cColor(228, 230, 234);
    colors[GUI_TEXT_HL_COLOR] = cColor(255, 255, 255);
    colors[GUI_TEXT_SEL_COLOR] = cColor(90, 170, 255);
    colors[GUI_WIN_BACKCLR] = cColor(34, 35, 39, 245);
    colors[GUI_WIN_BORDERCLR] = cColor(64, 66, 72);
    colors[GUI_TOOLTIP_BG] = cColor(20, 21, 24, 240);
    colors[GUI_TOOLTIP_TEXT] = cColor(235, 236, 240);
}

void axGUI::genID(const char *label)
{
    cntID++;
    if(label) {
        ID = hashString(axIdPart(label));   // "Label###id"
    } else {
        cntID++;  // fallback
        ID = cntID;
    }
    // if(label)
    // {
    //     std::string lblStr = std::string(label);
    //     std::hash<std::string> hasher;
    //     size_t hash = hasher(lblStr);
    //     ID += static_cast<int>(hash & 0xFFFFFFFF);
    // }
}

int axGUI::lastID()  { return ID; }

bool axGUI::isPointInRec(vec2 pt, vec2 pos, vec2 size)
{
    if(pt.x>=pos.x && pt.x<pos.x+size.x && pt.y>=pos.y && pt.y<pos.y+size.y) return true;
    return false;
}

void axGUI::setCursor(vec2 p)    {  curPos=p; }

void axGUI::setCursorX(float x)  {  curPos.x=x; }

vec2 axGUI::cursorPos()    {  return curPos;    }

vec2 axGUI::lastItemPos()  {  return curPos0;    }

vec2 axGUI::lastItemSize() {  return curSize0;   }

//----------------------------------------------------
void axGUI::newline(float lineH)
{
    curPos.x = layoutArea.left();
    curPos.y += lineHMax+itemSpaceY;
    if(lineH) lineHMax = lineH;
    else lineHMax = 0;
}

//----------------------------------------------------
void axGUI::spacerV(float lineH)
{
    curPos.x = layoutArea.left();
    curPos.y += lineH;
    newline();
}

//----------------------------------------------------
void axGUI::setLineH(float lineH)
{
    lineHMax = lineH;
}

//----------------------------------------------------
void axGUI::spacerH(float spaceX)
{
    curPos.x += spaceX;
}

//----------------------------------------------------
void axGUI::toBeginOfline() { curPos.x = layoutArea.left(); }

//----------------------------------------------------
vec2 axGUI::advanceCursor(const vec2 &size) { vec2 s = size; return advanceCursor(s); }

vec2 axGUI::advanceCursor(vec2 &size, bool fillW)
{
    // grid: the item goes into the next cell; the row ends after the last column and is as tall as its tallest item
    bool inGrid = false;
    if(!layouts.empty())
    {
        Layout &ly = layouts.top();
        if(ly.type==AX_LAYOUT_GRID && ly.win==prWinName && ly.grpDepth==groupBoxStack.size())
        {
            inGrid = true;
            if(ly.col >= ly.cols)                       // previous row is full: start the next one
            {
                ly.rowH.push_back(ly.lineH);
                ly.col = 0;
                ly.row++;
                ly.rowY += ly.lineH + itemSpaceY;
                ly.lineH = 0;
            }
            const float w = (ly.area.size.x - (ly.cols - 1) * itemSpaceX) / ly.cols;   // column width (itemSpaceX between columns)
            if((ly.flag & AX_LAYOUT_FILL_W) && (fillW || size.x <= 0)) size.x = w;
            float dy = 0;                               // vertical alignment with the previous frame's row height
            if(ly.valign & (BOT_ALIGN_MIDDLE|BOT_ALIGN_BOTTOM))
            {
                auto it = gridMemory.find(ly.id);
                if(it != gridMemory.end() && ly.row < (int)it->second.rowH.size())
                {
                    float free = std::max(0.0f, it->second.rowH[ly.row] - size.y);
                    dy = (ly.valign & BOT_ALIGN_MIDDLE) ? floorf(free * 0.5f) : free;
                }
            }
            curPos = vec2(ly.area.left() + ly.col * (w + itemSpaceX), ly.rowY + dy);
            // cell area: cell columns, outer area's top / bottom (wrapping off, visibility tests still work)
            layoutArea = rect(curPos.x + winScroll.x, ly.outerArea.top(), w, ly.outerArea.size.y);
            ly.lineH = std::max(ly.lineH, size.y);
            ly.col++;
            if(debugLayout) bot->roundedRect(vec2(curPos.x, ly.rowY), vec2(w, size.y + dy), 0, 0.5f, cColor(255,160,0,200));
        }
    }
    // wrap before the item: decided with this item's own size (never with a guess about the next one)
    if(!inGrid && layoutArea.size!=vec2(0,0) && curPos.x > layoutArea.left()+0.5f)
    {
        bool wrap = false;
        if(winflag&WIN_AUTO_NEWLINE)      wrap = curPos.x + size.x > layoutArea.right()+0.5f;
        else if(winflag&WIN_NO_SAMELINE)  wrap = !mSameLine;
        if(wrap) newline();
    }
    mSameLine=false;
    mNewLine=false;

    const vec2 pos = curPos;
    curPos0 = pos;
    curSize0 = size;
    if(debugLayout)
    {
        bot->roundedFRect(pos, size, 0, cColor(255,160,0,28));
        bot->roundedRect(pos, size, 0, 1, cColor(255,160,0,200));
        bot->roundedFRect(pos+vec2(size.x,0), vec2(itemSpaceX,size.y), 0, cColor(0,200,255,60));   // horizontal item spacing
    }
    lineHMax = std::max(lineHMax, size.y);
    curPos.x = pos.x + size.x + itemSpaceX;
    if(prWinName)
    {
        WindowState &win = windowStates[prWinName];
        win.curPos = curPos;
    }
    return pos;
}

//-------------------------------------------------------------------------
bool axGUI::outOfLayoutArea(const vec2 &size) { return outOfLayoutArea(curPos, size); }

bool axGUI::outOfLayoutArea(const vec2 &pos, const vec2 &size)
{
    if(layoutArea.size!=vec2(0,0))
    {
        if(pos.y+winScroll.y+size.y<layoutArea.top()) return true;
        if(pos.y+winScroll.y>layoutArea.bottom()) return true;
    }
    return false;
}

//----------------------------------------------------
void axGUI::setfont(const cString &face, float sz=0)
{
    // fontFace = face;
    bot->setfont(face,sz);
    fontSize = sz;
}
//----------------------------------------------------
void axGUI::setfont(axFont *f, float sz=0)
{
    curFont = f;
    bot->setfont(f,sz);
    fontSize = sz;
}
//----------------------------------------------------
// void axGUI::setfont(Font *f, float sz=0)
// {
//     fontFace = "";
//     font = f;
//     fontSize = sz;
// }

//----------------------------------------------------
void axGUI::saveColors()
{
    for(int i=0; i<GUI_COLOR_COUNT; i++)  colorsSaved[i] = colors[i];
}

//----------------------------------------------------
void axGUI::restoreColors()
{
    for(int i=0; i<GUI_COLOR_COUNT; i++)  colors[i] = colorsSaved[i];
}

//----------------------------------------------------
void axGUI::startFrame(bool retained)
{
    cntID = 100;
    prWinName = NULL;
    winflag = WIN_0;
    mNewLine=false;
    frameCnt++;
    winFocused = winHovered = true;
    winOrder=0;
    curPos = vec2(0,0);
    msPre = ms;
    ms = vec2(GetMousePosition().x, GetMousePosition().y);
    msLeftPressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    msLeftDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    msMidDown = IsMouseButtonDown(MOUSE_MIDDLE_BUTTON);
    msLeftReleased = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
    msRightPressed = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
    msRightDown = IsMouseButtonDown(MOUSE_RIGHT_BUTTON);
    msRightReleased = IsMouseButtonReleased(MOUSE_RIGHT_BUTTON);
    msWheel = GetMouseWheelMove();

    // color picker eyedropper: the click is captured to pick a color from the screen and is not passed to widgets
    if(pickerEyedrop && showClrPicker)
    {
        if(msLeftPressed) { pickerEyedropClick = true; pickerEyedropPos = ms; pickerSwallow = true; }
        if(pickerSwallow) { if(msLeftReleased) pickerSwallow = false; msLeftPressed = msLeftDown = msLeftReleased = false; }
    }
    else if(pickerSwallow) { if(msLeftReleased) pickerSwallow = false; msLeftPressed = msLeftDown = msLeftReleased = false; }

    msLeftAction = msLeftDown || msLeftPressed || msLeftReleased;

    // an open drop-down list swallows the mouse
    overComboList = false;
    if(drawComboList && !actComboitems.empty())
    {
        rect lr = comboListRect(actComboPos, actCombosize, (int)actComboitems.size());
        if(lr.isPointInRec(ms)) { overComboList = true; msCombo = ms; ms = vec2(-100000, -100000); }
    }
    msrightAction = msRightDown || msRightPressed || msRightReleased;

    // on touch, the last position must not count as hover once the finger lifts: put the cursor outside the GUI (no highlight, tooltip or hoverWinID)
    hoverActive = pointerType != PTR_TOUCH || msLeftAction || msrightAction;
    if(!hoverActive) ms = vec2(-100000, -100000);
    proc = overGUI = false;

    vec2 msDif = ms - msPre;
    mouseMoved = (ms!=msPre);

    msAnyReleased = msLeftReleased || msRightReleased;
    msAnyPressed = msLeftPressed || msRightPressed;
    msAnyDown = msLeftDown || msMidDown || msRightDown;

    int focusWinOrder = 0;
    if(focusWinName) focusWinOrder = windowStates[focusWinName].zOrder;

    if(msAnyPressed)
    {
        focusID=0;
        msDown=ms;
        msTarget=ms;
        msDragging=false;
        msDownTime=GetTime();
    }
    if(msLeftAction)  {  if((ms - msDown).length()>10) { msDragging=true; }  }
    if(msAnyReleased)  { msDragging=false; }
    if(msDragging)  { msSpeed = 0.1f*(ms-msTarget); msTarget += msSpeed; }

    msSpeed = msSpeed*0.98f;

    if(mouseMoved || msLeftAction || msrightAction || msWheel) { msTime=GetTime(); }
    hintReq = false;   // this frame's tooltip request comes from widgets (setHint)

    //--- wheel lock: while the lock is held, this frame's wheel is hidden from widgets and only the locked window uses it in endWindow
    wheelLatched = 0;
    if(wheelLatchID)
    {
        auto it = windowStates.find(wheelLatchName);
        bool inside = it != windowStates.end() && it->second.geometry().isPointInRec(ms);
        bool expired = GetTime() - wheelLatchLast > wheelLatchDelay;
        // ends when the cursor leaves the window, pauses (the next wheel event decides again) or on click
        if(!inside || expired || msAnyPressed) { wheelLatchID = 0; wheelLatchName.clear(); }
        else if(msWheel != 0) { wheelLatched = msWheel; msWheel = 0; }
    }

    winClip = rect(0,0,GetScreenWidth(),GetScreenHeight());
    layoutArea = rect(0,0,0,0);

    //--- find hovered and focused window ----
    hoverID=0;
    hoverWinID=0;
    hoverWinName = NULL;
    // if(windowStates.size()>0)   { hoverWin = windowStates.end()->second; }

    int zOrder=0, maxZOrder = 0;
    for(auto& [name, win] : windowStates) {
        maxZOrder = std::max(maxZOrder, win.zOrder);
    }

    for(auto it=windowStates.begin(); it!=windowStates.end(); ++it)
    {
        auto &win = it->second;

        // if(win.flags&WIN_CHILD_WINDOW && win.prWinName)
        // {
        //     WindowState &pWin = windowStates[win.prWinName];
        //     win.zOrder = pWin.zOrder;
        //     if(pWin.ID==hoverWinID) hoverWinID=0;
        // }
        if(win.geometry().isPointInRec(ms))
        {
            if(!hoverWinID || win.zOrder>zOrder ) { hoverWinID=win.ID; hoverWinName=win.name; hoverWin=win; zOrder=win.zOrder;  }
            proc=true;
        }
    }

    //--- input ownership
    bool anyHeld = msLeftAction || msrightAction;   // pressed or released this frame
    if(msAnyPressed) pressWinID = hoverWinID;
    else if(!anyHeld) pressWinID = 0;
    wantCaptureMouse = anyHeld ? pressWinID!=0 : hoverWinID!=0;
    if(overComboList) wantCaptureMouse = true;   // if the list is over the canvas, the click must not reach the canvas
    wantCaptureKeyboard = kbCaptureNext;
    kbCaptureNext = false;

    if(msAnyPressed)
    {
        if(hoverWinID && msLeftPressed)
        {
            WindowState &hoverWin = windowStates[hoverWinName];
            focusWinName = hoverWin.name;
            focusWinID = hoverWin.ID;
            if(hoverWin.zOrder<maxZOrder)  { hoverWin.zOrder = maxZOrder+1; }

            focusWinOrder = hoverWin.zOrder;
            if(hoverWin.prWinName)
            {
                WindowState &parentWin = windowStates[hoverWin.prWinName];
                parentWin.zOrder = hoverWin.zOrder;
                // if(hWin.zOrder<maxZOrder)  pWin.zOrder = maxZOrder+1;
            }
            normalizeZOrders();
        }
        if(hoverWinID==0 && !overComboList) { focusWinName=NULL; focusWinID=0; }   // keep focus while a list is open over it
    }

    //--- hover test, we're on desktop not a window, for widgets on desktop  ----
    winHovered = (0==hoverWinID);

    for(auto it=itemStates.begin(); it!=itemStates.end(); ++it)
    {
        // if(msLeftPressed) { itemStates[ID].clicked=false; }
    }
    // window move / resize (with snapping): runs before drawing, after the mouse state is read.
    // Running it in endFrame makes the window follow the cursor one frame late.

    bot->textSnap = !smoothText;   // text outside windows
    if(retained) bot->beginRetainedMode();
    bot->setfont(curFont, fontSize);
}

//----------------------------------------------------
void axGUI::normalizeZOrders()
{
    // Collect only top-level windows (non-child)
    std::vector<std::pair<std::string, int>> sorted;
    for(auto& [name, win] : windowStates) {
        // Add if it is not a child window
        if(!win.prWinName || !(win.flags & WIN_CHILD_WINDOW)) {
            sorted.push_back({name, win.zOrder});
        }
    }

    // Sort by zOrder
    std::sort(sorted.begin(), sorted.end(),
              [](auto& a, auto& b) { return a.second < b.second; });

    // Assign consecutive numbers to top-level windows
    int newZ = 1;
    for(auto& [name, oldZ] : sorted) {
        windowStates[name].zOrder = newZ++;
        // axLog("win:%s, z:%d",name.c_str(), windowStates[name].zOrder);
    }
    for(auto it=windowStates.begin(); it!=windowStates.end(); ++it)
    {
        auto &win = it->second;
        if(win.flags&WIN_CHILD_WINDOW && win.prWinName)
        {
            WindowState &pWin = windowStates[win.prWinName];
            win.zOrder = pWin.zOrder;
        }
        if(win.flags&WIN_TOPMOST)
        {
            win.zOrder = newZ++; // bring to front
        }
    }
}
cString axGUI::scrollInfo()
{
    if(scrollWinName)
    {
        WindowState &winScr = windowStates[scrollWinName];
        return S("scrWin: <")+scrollWinName+">  scrolling: "+RS(winScr.scrolling);
    }
    else return "scrWin: NULL";
}
//----------------------------------------------------
void axGUI::endFrame()
{
    calcWinSnaps();

    bot->endRetainedMode();
    bot->executeAllLayers();

    // release the lock if the locked window was not drawn this frame (closed / hidden)
    if(wheelLatched != 0) { wheelLatched = 0; wheelLatchID = 0; wheelLatchName.clear(); }

    // if(msLeftReleased) axLog("actComboID:%d, hoverID:%d",actComboID,hoverID);
    if(drawComboList) {
        bool res = ComboBoxList(actComboState, actComboPos, actCombosize, actComboitems, true);   // aligned with the box
        proc=true;
        if(msLeftReleased && actComboID!=hoverID) drawComboList=false;
    }
    if(dragID)
    {
        vec2 p = ms - msDown + dragStartPos;
        ItemState &it = itemStates[dragID];
        if(hoverWinName)
        {
            WindowState &hWin = windowStates[hoverWinName];
            bot->drawLine(vec2(hWin.pos.x,p.y),vec2(hWin.pos.x+hWin.size.x,p.y),colors[GUI_BTN_TOGGLE_COLOR]);
        }
        bot->roundedRect(p,it.size,0,0.5,colors[GUI_TEXT_COLOR]);
        bot->label(itemStates[dragID].name.c_str(), p, fontSize, colors[GUI_TEXT_COLOR], -1);
        if(msLeftReleased)
        {
            itemStates[dragID].dragging = false;
            dragID=0;
        }
    }
    // if(drawComboList) axLog("cmb "+V2S(actComboPos)+" sz:"+V2S(actCombosize)+" items:"+LS(actComboitems.size()));
    if(!hintReq) { hintOwner = 0; hintSuppressed = false; }
    else
    {
        if(hintReqID != hintOwner) { hintOwner = hintReqID; hintStart = GetTime(); hintSuppressed = false; }   // new item
        if(msAnyPressed) hintSuppressed = true;                                                             // clicked
    }
    if(hintReq && !hintSuppressed && !hintStr.empty() && GetTime() >= hintStart + 0.001 * hintTime)
    {
        // theme colors; size derived from the text (so a long tooltip fits)
        const float hs = fmaxf(12.0f, roundf(fontSize * 0.87f));
        vec2 tb = bot->textBounds(hintStr.c_str(), hs);
        hintSize = vec2(tb.x + 16, tb.y + 16 + 10);
        const float sw = (float)GetScreenWidth();
        hintPos.x = fminf(fmaxf(hintPos.x, hintSize.x / 2 + 4), sw - hintSize.x / 2 - 4);   // do not overflow the screen
        bot->speechBubble(hintStr.c_str(),hintPos,hintSize,5,vec2(10,10),hs, colors[GUI_TOOLTIP_BG], colors[GUI_TOOLTIP_TEXT]);
    }

    int msCursorType0 = msCursorType;
    if(hoverID!=hoverWinID) msCursorType = MOUSE_CURSOR_DEFAULT;
    else if(hoverID==hoverWinID) msCursorType = MOUSE_CURSOR_POINTING_HAND;
    bool forced = cursorRequest>=0;
    if(forced) msCursorType = cursorRequest;   // widget request (e.g. table column edge)
    cursorRequest = -1;

    if(msCursorType!=msCursorType0 && (!msLeftAction || forced)) SetMouseCursor(msCursorType);

    //--- SCROLL LAYOUT AREA BY LEFT MOUSE OR WHEEL ----
    if(msLeftDown && !focusWinName)  scrollWinName = NULL;
    if(scrollWinName)
    {
        WindowState &winScr = windowStates[scrollWinName];
        if(msDragging)
        {
            proc=true;
            float ty = 1.0f*(ms.y - msDown.y) + winScr.scroll0.y;
            winScr.speed.y = (ty-winScr.scroll.y)/12.0f;
        }
        if(msLeftPressed && winScr.scrolling) { winScr.scrolling = false; proc=true; axLog("disable scroll"); }
        if(msLeftReleased && winScr.scrolling) { winScr.speed.y=msSpeed.y; proc=true; }
        if(msWheel!=0)
        {
            if(!hoverWinID) scrollWinName=NULL;
            if(msSpeed.y<0 && msWheel>0) msSpeed.y=0;
            if(msSpeed.y>0 && msWheel<0) msSpeed.y=0;
            msSpeed.y += msWheel;
            proc=true;
            overGUI = true;
            scrollWinName = NULL;
            axLog("1-scrollWinName = NULL");
        }

        if(winScr.scrolling)
        {
            winScr.scroll.y += msSpeed.y;
            proc=true;
        }

        if(winScr.scroll.y>0) { winScr.scroll.y=0; msSpeed=vec2(0,0); }
        if(!msLeftAction && winScr.scrolling)
        {
            if(msSpeed.y && fabs(msSpeed.y)<0.1f) {
                msSpeed.y=0;
                winScr.scrolling = false;
                scrollWinName = NULL;
                axLog("scrollWinName = NULL");
            }
        }
        if(msLeftReleased && !winScr.scrolling) scrollWinName = NULL;
    }

    if(showClrPicker)
    {
        vec2 cwinSize=vec2(286, 372);   // new picker
        // open above the button if there is no room below (fitting on screen is done in beginWindow)
        if(clrPickerAnchor.size.y > 0 && clrPickerPos.y + cwinSize.y > GetScreenHeight()
           && clrPickerAnchor.pos.y - cwinSize.y - 4 >= 0)
        {
            clrPickerPos.y = clrPickerAnchor.pos.y - cwinSize.y - 4;
            auto pit = windowStates.find("ColorPicker");
            if(pit != windowStates.end() && pit->second.pos.y > clrPickerPos.y) pit->second.pos.y = clrPickerPos.y;
        }
        bool res = drawColorPicker(clrPickerColor, clrPickerPos, cwinSize, showClrPicker);
        clrPickerUpdate = res;
    }

    //--- Remove unused states
    float y=GetScreenHeight()-14;
    int i=0;
    for(auto it=itemStates.begin(); it!=itemStates.end(); )
    {
        ItemState &s = it->second;
        // if(debug) bot->roundedRect(s.pos, s.size, 0, 1, cRed);
        // if(debug) bot->text(axFormat("%d: %s  %.0f,%.f",i++, s.name.c_str(), s.pos.x,s.pos.y), vec2(0,y), 12, cGreen);
        y-=14;
        if(it->second.frame!=frameCnt)  it = itemStates.erase(it);
        else   ++it;
    }

    //--- Remove unused window states
    for(auto it=windowStates.begin(); it!=windowStates.end(); )
    {
        if(it->second.frame!=frameCnt)  it = windowStates.erase(it);
        else   ++it;
    }
    //--- Remove unused section states
    for(auto it=sectionStates.begin(); it!=sectionStates.end(); )
    {
        if(it->second.frame!=frameCnt)  it = sectionStates.erase(it);
        else   ++it;
    }

    //--- remove unused anim states
    for(auto it=anims.begin(); it!=anims.end(); )
    {
        if(it->second.frame!=frameCnt)  it = anims.erase(it);
        else   ++it;
    }

    if(focusID) {
        auto &s = itemStates[focusID];
        bot->roundedRect(s.pos, s.size, s.rad, 2, cColor(200));
    }

    if(debug)
    {
        for (auto& [id, s] : itemStates) {
            bool valid = (s.frame==frameCnt);
            cColor c(120,200,120,64);
            if(s.hovered) c = cColor(200,200,120);
            if(s.focused) c = cColor(100,220,120);
            bot->roundedRect(s.pos, s.size, 0, 1, c);
            // bot->roundedRect(s.pos-vec2(2,2), s.size+vec2(4,4), 0, 2, cColor(255,0,0,100));
            // text(axFormat("ID:%d, name:%s, hover:%d, focus:%d",id, s.name.c_str(), s.hovered, s.focused), p.x, p.y+=12, 12, c);
        }
        if(hoverID) {
            auto &s = itemStates[hoverID];
            bot->roundedRect(s.pos-vec2(2,2), s.size+vec2(4,4), 0, 1, cRed);
        }
        if(hoverWinName)
        {
            auto &win = windowStates[hoverWinName];
            bot->roundedRect(win.pos, win.size, 8, 2, cColor(200,200,100));
            bot->text(S(win.name)+":"+LS(win.zOrder),win.pos-vec2(0,15),12,cGreen);
        }
    }
}

float axGUI::getContainerWidth()
{
    return layoutArea.size.x;
}

//----------------------------------------------------
long axGUI::winID(const char *id)
{
    WindowState &win = windowStates[axIdPart(id)];
    return win.ID;
}

//----------------------------------------------------
void axGUI::setWinSize(const char *name, vec2 size)
{
    WindowState &win = windowStates[axIdPart(name)];
    win.size = size;
}

void axGUI::calcWinSnaps()
{
    if(!mouseMoved || !msLeftDown || (!moveWinID && !resizeWinID)) return;
    for(auto &[key, win] : windowStates)
    {
        if(win.flags & WIN_CHILD_WINDOW) continue;
        const vec2 size = win.size;
        const int winId = win.ID;

        if(moveWinID==winId)
        {
            vec2 p = ms - msDown + moveRefWin.pos;
            vec2 snap1 = snapToWindows(win, p);
            vec2 snap2 = snapToWindows(win, p + win.size);
            vec2 snap(0, 0);

            // Pick closer snap per axis
            if(snap1.x != 0 && (snap2.x == 0 || fabs(snap1.x) < fabs(snap2.x)))   snap.x = snap1.x;
            else  snap.x = snap2.x;

            if(snap1.y != 0 && (snap2.y == 0 || fabs(snap1.y) < fabs(snap2.y)))   snap.y = snap1.y;
            else   snap.y = snap2.y;

            p += snap;

            // child window: position is set from relPos in beginWindow (the global prWinName is always NULL outside a frame)
            if(win.prWinName) win.relPos = p - win.anchor;
            win.setGeometry(p,size);
            proc=true;
        }

        if(resizeWinID==winId)
        {
            int hndl = winResizeHndl;
            vec2 p = moveRefWin.pos;
            vec2 s = moveRefWin.size;
            vec2 dif = (ms - msDown) * winResizeFactor;

            // Left edge
            if(hndl == HNDL_LEFT || hndl == HNDL_TOPLEFT || hndl == HNDL_BOTLEFT)
            {
                p.x += dif.x;
                s.x -= dif.x;
                vec2 snap = snapToWindows(win, p);
                p.x += snap.x;
                s.x -= snap.x;
            }
            // Right edge
            if(hndl == HNDL_RIGHT || hndl == HNDL_TOPRIGHT || hndl == HNDL_BOTRIGHT)
            {
                s.x += dif.x;
                vec2 snap = snapToWindows(win, p + s);
                s.x += snap.x;
            }
            // Top edge
            if(hndl == HNDL_TOP || hndl == HNDL_TOPLEFT || hndl == HNDL_TOPRIGHT)
            {
                p.y += dif.y;
                s.y -= dif.y;
                vec2 snap = snapToWindows(win, p);
                p.y += snap.y;
                s.y -= snap.y;
            }
            // Bottom edge
            if(hndl == HNDL_BOTTOM || hndl == HNDL_BOTLEFT || hndl == HNDL_BOTRIGHT)
            {
                s.y += dif.y;
                vec2 snap = snapToWindows(win, p + s);
                s.y += snap.y;
            }

            s.x = std::max(s.x, 100.0f);
            s.y = std::max(s.y, 50.0f);
            if(win.prWinName) win.relPos = p - win.anchor;   // also changes the left / top edge position
            win.setGeometry(p, s);
            proc = true;
        }
    }
}

//----------------------------------------------------
WindowState &axGUI::beginWindow(const char *name, vec2 &pos, vec2 &size, WinFlags flags)
{
    const char *title = name;          // "Title###id": state is keyed by id, the label is shown in the title
    bot->textSnap = !smoothText;       // text rounding is reset per window (only that window if animation turns it off)
    name = axIdPart(name);
    genID();
    rect parentClip = winClip;      // child window stays within the parent window's clip area
    const bool isChild = (prWinName != NULL);
    WindowState &win = windowStates[name];
    if(win.wheelScroll.active) bot->textSnap = false;   // content is sliding: keep text moving with it
    win.order = ++winOrder;
    if(!win.zOrder) win.zOrder = win.order;
    win.frame = frameCnt;
    win.ID = ID;
    win.name = name;
    float curWinBevel=winRadius;
    float curWinSpcX=winSpaceX, curWinSpcY=winSpaceY;
    bot->setLayer(win.zOrder);

    if(prWinName)  //--- child of other window
    {
        if(win.init) { pos = win.relPos + curPos; size = win.size; }   // size also comes from the state (move / resize is handled in calcWinSnaps)
        else win.relPos = pos;
        if(flags&WIN_CHILD_WINDOW) {
            WindowState &prnt = windowStates[prWinName];
            curWinSpcX=curWinSpcY=0; curWinBevel=0;
            size.x=prnt.layoutArea().size.x;
            size.y=prnt.layoutArea().bottom()-curPos.y;
            if(debug) bot->roundedRect(prnt.relPos, prnt.size, 0, 2, cYellow);
            // bot->roundedRect(pos, size, 0, 2, cRed);
        }
        win.anchor = curPos+winScroll;                        // calcWinSnaps computes relPos from this
        win.setGeometry(win.anchor+win.relPos,size);          // parent window's scroll
    }
    else  //---- top level window ----
    {
        if(win.init/* && !(flags&WIN_POPUP)*/) { pos = win.pos; size = win.size; }
        if(flags&WIN_POPUP)   // popup must not overflow the screen (pushed up / inward if it does not fit)
        {
            const float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
            pos.x = fmaxf(0.0f, fminf(pos.x, sw - size.x));
            pos.y = fmaxf(0.0f, fminf(pos.y, sh - size.y));
        }
        win.setGeometry(pos,size);
    }

    winFocused = (win.name == focusWinName);
    winHovered = (win.ID==hoverWinID);

    if(win.geometry().isPointInRec(ms))
    {
        proc=true;
        if(hoverWinID==ID)
        {
            if(msLeftPressed && flags&WIN_TITLE_BAR && !(flags&WIN_NO_MOVE))
            {
                if(win.titleArea().isPointInRec(ms))   { moveRefWin=win; moveWinID=ID; scrollWinName=NULL; }
            }
            if(msLeftPressed && flags&WIN_RESIZABLE)
            {
                if(win.resizeArea().isPointInRec(ms))  { moveRefWin=win; resizeWinID=ID; scrollWinName=NULL; winResizeFactor = vec2(1,1); winResizeHndl = HNDL_BOTRIGHT; }
                else if(win.resizeRightArea().isPointInRec(ms))  { moveRefWin=win; resizeWinID=ID; scrollWinName=NULL; winResizeFactor = vec2(1,0); winResizeHndl = HNDL_RIGHT; }
                else if(win.resizeLeftArea().isPointInRec(ms))  { moveRefWin=win; resizeWinID=ID; scrollWinName=NULL; winResizeFactor = vec2(1,0); winResizeHndl = HNDL_LEFT; }
                else if(win.resizeBottomArea().isPointInRec(ms))  { moveRefWin=win; resizeWinID=ID; scrollWinName=NULL; winResizeFactor = vec2(0,1); winResizeHndl = HNDL_BOTTOM;}
                else if(win.resizeTopArea().isPointInRec(ms))  { moveRefWin=win; resizeWinID=ID; scrollWinName=NULL; winResizeFactor = vec2(0,1); winResizeHndl = HNDL_TOP; moveWinID=0; }
            }
            if(msLeftPressed && flags&WIN_SCROLLBAR && !moveWinID && !resizeWinID)
            {
                scrollWinName=win.name;
                axLog("scrollWinName:%s",win.name);
            }
            if(msWheel!=0 && flags&WIN_SCROLLBAR && hoverWinID==ID)
            {
                scrollWinName=win.name;
            }
        }
        // hoverID=ID;
    }

    if(msLeftReleased) { moveWinID=resizeWinID=0; }

    win.setMargins(8,4);
    win.prWinName = prWinName;
    win.flags = flags;
    win.init=true;

    // win.winID = id;
    prWinName = name;
    layoutArea = win.layoutArea();
    curPos = curPos0 = layoutArea.pos;
    winScroll = win.scroll;
    winflag = flags;
    ignoreScroll = moveWinID|resizeWinID;
    lineHMax = 0;
    //--- clipping area
    bot->save();
    bot->resetTransform();
    bot->resetScissor();

    //---- draw window background ----
    bot->roundedFRect(win.geometry(), curWinBevel, colors[GUI_WIN_BACKCLR], name==focusWinName?3:1, colors[GUI_WIN_BORDERCLR]);

    //---- draw window title bar ----
    if(flags&WIN_TITLE_BAR) {
        // cString fs = bot->fontManager.currentFontName;
        bot->roundedFRect(win.titleArea(), curWinBevel-1, colors[GUI_IDLE_COLOR], 1, colors[GUI_WIN_BORDERCLR]);
        if(!debug) bot->text(axLabelPart(title), win.pos+vec2(curWinSpcX,(titleBarH-fontSize)/2), fontSize, colors[GUI_TEXT_COLOR]);   // name only, theme color (ID / z: debug)
        else bot->text(win.debugStr(), win.pos+vec2(curWinSpcX,(titleBarH-fontSize)/2), fontSize, colors[GUI_TEXT_COLOR]);
    }
    else if(debug && win.prWinName) bot->text(axFormat("%s:%d",win.prWinName,win.zOrder), win.pos+vec2(curWinSpcX+140,(titleBarH-fontSize)/2), fontSize, colors[GUI_TEXT_COLOR]);

    //---- draw resize handle ----
    if(flags&WIN_RESIZABLE && winHovered)
    {
        if(win.resizeArea().isPointInRec(ms))
        {
            bot->drawLine(win.resizeArea().topright(), win.resizeArea().bottomright(), colors[GUI_TEXT_SEL_COLOR], 2);
            bot->drawLine(win.resizeArea().bottomleft(), win.resizeArea().bottomright(), colors[GUI_TEXT_SEL_COLOR], 2);
        }
        else if(win.resizeRightArea().isPointInRec(ms))
        {
            bot->drawLine(win.pos+vec2(win.size.x,winRadius), win.pos+win.size-vec2(0,2*winRadius), colors[GUI_TEXT_SEL_COLOR], 2);
        }
        else if(win.resizeLeftArea().isPointInRec(ms))
        {
            bot->drawLine(win.pos+vec2(0,winRadius), win.pos+vec2(0,win.size.y-2*winRadius), colors[GUI_TEXT_SEL_COLOR], 2);
        }
        else if(win.resizeBottomArea().isPointInRec(ms))
        {
            bot->drawLine(win.pos+vec2(winRadius,win.size.y), win.pos+win.size-vec2(2*winRadius,0), colors[GUI_TEXT_SEL_COLOR], 2);
        }
        else if(win.resizeTopArea().isPointInRec(ms))
        {
            bot->drawLine(win.pos+vec2(winRadius,0), win.pos+vec2(win.size.x-2*winRadius,0), colors[GUI_TEXT_SEL_COLOR], 2);
        }
    }

    if(debug) bot->roundedRect(layoutArea, 0, 1.25, cBlue);

    // winClip = rect(win.pos.x+curWinSpcX, win.pos.y+curWinSpcY, size.x-2*curWinSpcX, size.y-2*curWinSpcY);
    winClip = layoutArea;
    if(isChild)   // child window: intersect with the parent window's visible area
    {
        float x0 = fmax(winClip.left(), parentClip.left()), y0 = fmax(winClip.top(), parentClip.top());
        float x1 = fmin(winClip.right(), parentClip.right()), y1 = fmin(winClip.bottom(), parentClip.bottom());
        winClip = rect(x0, y0, fmax(0.0f, x1-x0), fmax(0.0f, y1-y0));
    }
    // bot->roundedRect(winClip, 0, 1, cYellow);
    bot->scissor(winClip.pos, winClip.size);
    bot->translate(win.scroll.x, win.scroll.y);
    return win;
}

void axGUI::endWindow(const char *name)
{
    name = axIdPart(name);
    bot->textSnap = !smoothText;
    WindowState &win = windowStates[name];

    // Mouse wheel scrolling: scrolls when the cursor is over the window's content area (or its child window without a scrollbar)
    // and no widget consumed the wheel (msWheel != 0); scrolling is clamped to the content height.
    if(win.flags & WIN_SCROLLBAR)
    {
        rect la = win.layoutArea();
        float contentBottom = curPos.y + lineHMax;     // unscrolled layout coordinate
        float contentH = contentBottom - la.top() + winSpaceY;
        float maxScroll = fmax(0.0f, contentH - la.size.y);
        bool overChild = false;
        if(hoverWinName && hoverWinID != win.ID)
        {
            auto it = windowStates.find(hoverWinName);
            overChild = it != windowStates.end() && it->second.prWinName && strcmp(it->second.prWinName, name) == 0;
        }
        bool over = la.isPointInRec(ms) && (hoverWinID == win.ID || overChild);
        float wheel = 0;
        if(wheelLatchID == win.ID) { wheel = wheelLatched; wheelLatched = 0; }   // lock is on this window
        else if(over && msWheel != 0 && !ignoreScroll && maxScroll > 0)
        {
            wheel = msWheel;
            msWheel = 0;
            wheelLatchID = win.ID;      // scrolling started: this window owns the wheel until it ends
            wheelLatchName = name;
        }
        if(wheel != 0)
        {
            win.wheelScroll.add(win.scroll.y, wheel * wheelStep);
            wheelLatchLast = GetTime();
            proc = true;
        }
        if(win.scrolling) win.wheelScroll.stop();   // drag scrolling takes over
        win.wheelScroll.step(win.scroll.y, -maxScroll, 0.0f, wheelSmooth, GetFrameTime());
        win.scroll.y = Clamp(win.scroll.y, -maxScroll, 0.0f);
    }

    rect winRect = win.geometry();
    bool hovered = winRect.isPointInRec(ms);
    if((msLeftAction || msrightAction) && hovered)
    {
        proc=true;
    }
    curPos = vec2(winRect.pos.x, winRect.pos.y+winRect.size.y);
    win.curPos = curPos;

    //--- end clipping area
    bot->restore();
    // bot->circle(curPos,3,cRed);

    prWinName = win.prWinName;
    // retained mode: subsequent drawing goes to the parent window's layer (0 for top-level: outside windows)
    bot->setLayer(prWinName ? windowStates[prWinName].zOrder : 0);

    //--- start operation of scroll by mouse
    if(ignoreScroll==false && layoutArea.isPointInRec(ms) && win.flags&WIN_SCROLLBAR)
    {
        if(!win.scrolling  && (scrollWinName/* || hoverWinID*/) && focusID==0)
        {
            if(msLeftPressed && hoverWinID)  { proc=true; }
            if(msDragging && hoverWinID)   // wheel is handled separately above
            {
                win.scrolling = true;
                win.scroll0 = win.scroll;
                proc=true;
            }
        }
    }

    //--- get parent layout
    if(win.prWinName)
    {
        WindowState &winParent = windowStates[win.prWinName];
        layoutArea = winParent.layoutArea();
        winScroll = winParent.scroll;
        winflag = winParent.flags;
        // restore the cursor to after the child window (otherwise following items overlap it), restore the clip area, and restore the parent window's hover / focus state
        winClip = layoutArea;
        winHovered = (winParent.ID == hoverWinID);
        winFocused = (winParent.name == focusWinName);
        curPos = vec2(layoutArea.left(), win.pos.y - winScroll.y + win.size.y + itemSpaceY);
        lineHMax = 0;
        winParent.curPos = curPos;
    }
    else layoutArea = rect(0,0,0,0);

    for(auto &pt : snapPts)
    {
        // bot->circle(pt, 2, cRed);
    }
}


bool axGUI::beginGrid(const char *title, int cols, int rows, int valign)
{
    if(title) genID(axFormat("##grid:%s", axIdPart(title))); else genID();
    const int gridID = ID;
    if(cols < 1) cols = 1;
    // the grid takes the full width on its own row (in a parent grid: its cell); its height is measured in endGrid,
    // the previous frame's height is used to place it (so a parent row can align it)
    if(layoutArea.size.x > 0 && curPos.x > layoutArea.left()+0.5f) newline();
    const float outerLineH = lineHMax;
    auto mem = gridMemory.find(gridID);
    vec2 gsz(layoutArea.size.x > 0 ? layoutArea.size.x : 100.0f, mem != gridMemory.end() ? mem->second.h : 0);
    vec2 pos = advanceCursor(gsz, true);

    Layout ly;
    ly.type = AX_LAYOUT_GRID;
    ly.flag = AX_LAYOUT_FILL_W;
    ly.area = rect(pos, vec2(gsz.x, 0));
    ly.cols = cols;
    ly.rows = rows;
    ly.rowY = pos.y;
    ly.outerCursor = curPos;
    ly.outerLineH = outerLineH;
    ly.id = gridID;
    ly.valign = valign;
    ly.outerArea = layoutArea;
    ly.win = prWinName;
    ly.grpDepth = groupBoxStack.size();

    pushLayout();                     // layoutArea / winScroll / winClip; the cells change layoutArea
    layouts.push(ly);
    lineHMax = 0;
    return true;
}

void axGUI::endGrid()
{
    if(layouts.empty()) return;
    Layout ly = layouts.top();
    layouts.pop();
    popLayout();

    // measured height: up to the bottom of the last row
    const float h = (ly.col > 0 || ly.row > 0) ? ly.rowY + ly.lineH - ly.area.top() : 0;
    if(ly.col > 0) ly.rowH.push_back(ly.lineH);   // last row
    GridMemory &mem = gridMemory[ly.id];
    mem.rowH = ly.rowH;
    mem.h = h;
    if(debugLayout) bot->roundedRect(ly.area.pos, vec2(ly.area.size.x, h), 0, 1, cColor(255,0,0,160));

    // back on the outer row, after the grid; the outer row is at least as tall as the grid
    curPos = ly.outerCursor;
    lineHMax = std::max(ly.outerLineH, h);
    curPos0 = ly.area.pos;
    curSize0 = vec2(ly.area.size.x, h);
    if(!layouts.empty())              // nested grid: the parent row grows with it
    {
        Layout &pr = layouts.top();
        if(pr.type == AX_LAYOUT_GRID) pr.lineH = std::max(pr.lineH, h);
    }
    if(prWinName) { WindowState &win = windowStates[prWinName]; win.curPos = curPos; }
}

bool axGUI::beginSection(const char *title)
{
    genID(title);
    ItemState &item = sectionStates[ID];
    float gap=1.5f*fontSize;
    float iconsz = 1.0f*fontSize;
    float w = layoutArea.size.x;
    if(w<=0) w=100.0f;
    vec2 size(w,gap);
    vec2 pos = advanceCursor(size, true);

    cColor bgClr = colors[GUI_BTN_COLOR];
    bool hovered = buttonHovered(pos, size);
    float he = buttonAnim(ID, item, hovered, bgClr, pos);
    bot->roundedFRect(pos, size, btnRadius, bgClr);

    // The icon is tinted with the theme text color (white would be invisible on a light theme); falls back to a vector triangle without an atlas
    {
        const char *ic = item.open ? "tri1_icon" : "tri2_icon";
        vec2 ip = pos + vec2(itemSpaceX, 0.5f*(size.y-iconsz));
        cColor tc = mixColor(colors[GUI_TEXT_COLOR], colors[GUI_TEXT_HL_COLOR], he);
        if(bot->hasIcon(ic)) bot->drawIcon(ic, ip, iconsz, tc);
        else {
            vec2 c = ip + vec2(iconsz, iconsz) * 0.5f;
            float r = iconsz * 0.3f;
            if(item.open) bot->fillTriangle(c + vec2(-r, -r*0.5f), c + vec2(r, -r*0.5f), c + vec2(0, r*0.6f), tc);
            else          bot->fillTriangle(c + vec2(-r*0.5f, -r), c + vec2(-r*0.5f, r), c + vec2(r*0.6f, 0), tc);
        }
    }
    if(title) {
        bot->text(axLabelPart(title), pos+vec2(iconsz+2.0f*itemSpaceX,0.5f*(size.y-fontSize)), 0, mixColor(colors[GUI_TEXT_COLOR], colors[GUI_TEXT_HL_COLOR], he), BOT_ALIGN_LEFT|BOT_ALIGN_TOP);
        // bot->drawLine(vec2(x,y+gap/2),vec2(x+w,y+gap/2),cColor(44),1);
    }
    // else bot->drawLine(vec2(x,y+gap/2),vec2(x+w,y+gap/2),cColor(44),1);
    // bot->roundedRect(pos,size,0,1,cRed);
    item.set(title, pos+winScroll, size, hovered, ID==focusID, 4);
    bool res = buttonBehavior(item, hovered);
    newline(0);
    return beginCollapse(ID, item.open);   // open / close animation
}

void axGUI::endSection(const char *title)
{
    newline(0);
    endCollapse();
    separator(4);
}

bool axGUI::beginGroupBox(const char *title, vec2 size, bool collapsible, int scrollable, const GroupStyle &st, const char *hint)
{
    // if titled, the id comes from the title (like beginSection): a counter-based id shifts when earlier widgets change
    // (e.g. another section closes), resetting the group's open / closed and scroll state
    if(title) genID(axFormat("##groupbox:%s", axIdPart(title))); else genID();
    GroupBoxState& grp = groupBoxStates[ID];
    grp.usedThisFrame = true;

    float headerHeight = title ? 26 : 0;
    float padding = st.padding;   // GroupStyle::padding
    float w = size.x, h = size.y;
    vec2 scrolldir = vec2(scrollable==1, scrollable==2);

    // open / close animation: the frame is between header-only and full height, the body is clipped to that height
    if(!title) grp.isOpen = true;
    if(!grp.animInit) { grp.animInit = true; grp.openT = grp.isOpen ? 1.0f : 0.0f; }
    float openE = stepOpenAnim(grp.openT, grp.isOpen);
    bool drawBody = grp.openT > 0;
    bool animating = grp.openT > 0 && grp.openT < 1;
    float shownH = headerHeight + openE * (h - headerHeight);   // animated height

    // the box takes its place in the outer row; the cursor after it is restored in endGroupBox
    vec2 bsz(w, shownH);
    vec2 pos = advanceCursor(bsz, w <= 0);
    w = bsz.x;
    float x = pos.x, y = pos.y;
    grp.contentStartPos = pos;
    grp.curPos = curPos;
    grp.size = vec2(w, shownH);

    // Save context
    GroupBoxContext context;
    context.startPos = pos;
    context.size = size;
    context.id = ID;
    context.oldClip = winClip;
    context.isOpen = drawBody;
    context.scrollable = scrollable;
    context.animating = animating;
    context.prevUseInput = useInput;
    context.outerLineH = lineHMax;
    context.padding = padding;
    groupBoxStack.push_back(context);

    // cColor bClr = colors[GUI_IDLE_COLOR] - cColor(15);
    if(!title)
    {
        headerHeight = 0;
        grp.isOpen = true;
        // bClr = cColor(222, 222, 244);
    }
    groupBoxStack.back().headerH = headerHeight;

    // Draw outer border
    cColor shdClr = cColor(220,220,240,0);
    if(st.shadow)
        bot->fillBoxGrad(rect(x-4, y-4, w+8, shownH+8), st.bevel+2, 120, colors[GUI_WIN_BACKCLR]*0.6f, shdClr );
    bot->roundedFRect(x, y, w, shownH, st.bevel, st.fClr, st.stroke, st.sClr);

    if(title)
    {
        // Header area
        rect headerRect(pos+winScroll, vec2(w, headerHeight));
        bool headerHovered = winHovered && headerRect.isPointInRec(ms) && winClip.isPointInRec(ms);   // not if another window is on top

        if(headerHovered) hoverID=ID;
        if(headerHovered && hint) setHint(headerHovered, hint, pos + vec2(size.x/2, 0));

        // Toggle on click
        if(collapsible && msLeftPressed && headerHovered)
        {
            grp.isOpen = !grp.isOpen;
            proc = true;
        }

        // Draw header background
        cColor headerColor = contrast(colors[GUI_IDLE_COLOR], headerHovered ? 0.12f : 0.04f);   // correct direction in both themes
        bot->roundedFRect(x + 1, y + 1, w - 2, headerHeight - 1, st.bevel, headerColor);
        // grp.nextPos = curPos+vec2(w,headerHeight);

        // Draw collapse indicator
        if(collapsible)
        {
            float sz = 10;
            float iconX = x + 10;
            float iconY = y + headerHeight/2;

            if(grp.isOpen)  {
                bot->fillTriangle(vec2(iconX-sz/2, iconY-sz/3), vec2(iconX + sz/2, iconY-sz/3),  vec2(iconX, iconY + sz/3), colors[GUI_TEXT_COLOR]);
            }
            else  {
                bot->fillTriangle( vec2(iconX - sz/3, iconY - sz/2),    vec2(iconX - sz/3, iconY + sz/2),  vec2(iconX + sz/3, iconY), colors[GUI_TEXT_COLOR] );
            }
        }

        // Draw title text
        float titleX = collapsible ? x + 28 : x + 12;
        bot->text(axLabelPart(title), titleX, y + headerHeight/2, fontSize, colors[GUI_TEXT_COLOR], BOT_ALIGN_LEFT | BOT_ALIGN_MIDDLE);
        curPos = vec2(x, y) + vec2(0, headerHeight);
    }

    // Draw body if open (or while opening / closing)
    if(drawBody)
    {
        pushLayout();
        bot->save();
        if(animating) useInput = false;   // content takes no input during animation

        float bodyY = y + headerHeight;
        float bodyH = h - headerHeight;

        // Set up clip region for contents (visible height during animation)
        float visBodyH = shownH - headerHeight;
        rect contentClip(x + padding, bodyY + padding, w - padding * 2, fmax(0.0f, fmin(bodyH, visBodyH) - padding * 2));

        bot->resetTransform();
        bot->resetScissor();

        // Handle scrolling if enabled. Vertical scrolling is limited to the content: 0 (top) .. -(contentHeight - visible height).
        const float viewH = fmax(0.0f, bodyH - padding * 2);
        const float minScrollY = std::min(0.0f, viewH - grp.contentHeight);
        if(scrollable)
        {
            rect bodyRect(vec2(x, bodyY)+winScroll, vec2(w, bodyH));
            // winHovered: when another window (e.g. color picker) is on top, drag / wheel must not reach the group underneath
            bool bodyHovered = winHovered && bodyRect.isPointInRec(ms) && !moveWinID && !resizeWinID;

            if(bodyHovered) hoverID=ID;

            if(bodyHovered && msLeftPressed)
            {
                grp.clicked = true;
                scrollWinName = NULL;
                grp.scrollOffsetStart = grp.scrollOffset;
                proc = true;
            }
            if(grp.clicked && msDragging)
            {
                focusID = ID;
                grp.scrolling = true;
                proc = true;
            }
            if(grp.scrolling)
            {
                if(msLeftDown && focusID == ID)
                {
                    grp.speed = msSpeed * scrolldir;
                    grp.scrollOffset += grp.speed*scrolldir;
                    proc = true;
                }
                if(msLeftReleased) { grp.speed=ms-msPre; grp.clicked=false; }
                if(!msLeftDown && focusID == ID)
                {
                    grp.speed = 0.95f*grp.speed;
                    grp.scrollOffset += grp.speed*scrolldir;
                    if(fabs(grp.speed.length())<0.01f)
                    {
                        grp.scrolling=false;
                    }
                    proc=true;
                }
            }

            if(bodyHovered && msWheel!=0 && minScrollY < 0)   // content fits: leave the wheel to the window
            {
                grp.wheelScroll.add(grp.scrollOffset.y, msWheel * 40.0f);
                msWheel = 0;   // do not scroll the window
                proc = true;
                scrollWinName = NULL;
            }
            if(grp.scrolling) grp.wheelScroll.stop();   // drag scrolling takes over
            if(grp.wheelScroll.active)
            {
                grp.wheelScroll.step(grp.scrollOffset.y, minScrollY, 0.0f, wheelSmooth, GetFrameTime());
                bot->textSnap = false;   // content is sliding: keep text moving with it
            }
            if(debug) bot->text(axFormat("%d, %d, %d",bodyHovered,grp.clicked,grp.scrolling),vec2(x, y+bodyY-20)+winScroll,10,cRed);   // debug only

            // if(grp.scrolling && msLeftReleased) { grp.clicked=grp.scrolling=false; }
            curPos = vec2(x + padding, bodyY + padding);
        }

        // content always starts at the padding (the clip area 8 px inside would otherwise cut the top / left of the first row),
        // and does not carry the outer row's height (otherwise lineHMax of a large neighboring item pushes the first inner newline down).
        curPos = vec2(x + padding, bodyY + padding);
        lineHMax = 0;
        grp.contentTop = curPos.y;
        // keep the scroll inside the content (drag, inertia, or content that got shorter)
        if(grp.scrollOffset.y > 0 || grp.scrollOffset.y < minScrollY)
        {
            grp.scrollOffset.y = Clamp(grp.scrollOffset.y, minScrollY, 0.0f);
            grp.speed.y = 0;
        }
        contentClip = contentClip+winScroll;
        layoutArea = contentClip;

        winScroll = grp.scrollOffset+winScroll;
        // bot->roundedRect(winClip, 0, 2, cBlue);
        winClip = (winClip).getIntersection(contentClip);
        bot->scissor(winClip);
        bot->translate(winScroll);
    }
    return drawBody;   // content is drawn while open or opening / closing
}

void axGUI::endGroupBox()
{
    if(groupBoxStack.empty()) return;

    GroupBoxContext& context = groupBoxStack.back();
    GroupBoxState& grp = groupBoxStates[context.id];

    if(context.isOpen)
    {
        bot->restore();
        popLayout();
        useInput = context.prevUseInput;
        // Content height: from the first row to the bottom of the last one (layout coordinates, not scrolled)
        grp.contentHeight = fmax(0.0f, curPos.y + lineHMax - grp.contentTop);

        // Draw scrollbar (optional; not drawn while opening / closing)
        if(context.scrollable && !context.animating)
        {
            float headerHeight = context.headerH, padding = context.padding;
            float bodyH = context.size.y - headerHeight;

            if(grp.contentHeight > bodyH - padding * 2)
            {
                float scrollbarX = context.startPos.x + context.size.x - 10;
                float scrollbarY = context.startPos.y + headerHeight + 2;
                float scrollbarH = bodyH - 4;

                // Track
                bot->roundedFRect(scrollbarX, scrollbarY, 6, scrollbarH, 3, withAlpha(colors[GUI_TEXT_COLOR], 18));   // same as the table

                // Thumb
                float visibleRatio = (bodyH - padding*2) / grp.contentHeight;
                float thumbHeight = std::max(20.0f, scrollbarH * visibleRatio);
                float scrollRange = grp.contentHeight - (bodyH - padding*2);
                float scrollRatio = scrollRange > 0 ? (-grp.scrollOffset.y / scrollRange) : 0;
                float thumbY = scrollbarY + scrollRatio * (scrollbarH - thumbHeight);

                bot->roundedFRect(scrollbarX, thumbY, 6, thumbHeight, 3, withAlpha(colors[GUI_TEXT_COLOR], 90));
            }
        }

        // Restore clip region
        // winClip = context.oldClip;

        // Calculate final cursor position
        float finalY = context.startPos.y + context.size.y;
        // curPos = vec2(context.startPos.x, finalY);
    }
    // bot->circle(curPos,3,cBlue);
    // bot->circle(grp.nextPos,3,cRed);
    setCursor(grp.curPos);                        // after the box, on the outer row
    lineHMax = groupBoxStack.back().outerLineH;   // restore outer row height (includes the box)
    if(prWinName) { WindowState &win = windowStates[prWinName]; win.curPos = curPos; }
    groupBoxStack.pop_back();
}


vec2 axGUI::snapToWindows(WindowState &ourWin, vec2 p)
{
    if(!winSnap) return vec2(0, 0);   // snapping off
    snapPts.clear();
    snapPts.push_back(vec2(0,0));
    snapPts.push_back(vec2(GetScreenWidth(), GetScreenHeight()));

    // the moving window's child windows (including nested) are not snap targets: they move with it,
    // so the window would snap to its own child's edge and jitter back and forth every frame
    auto isDescendant = [&](const WindowState &w) {
        const char *pr = w.prWinName;
        for(int depth = 0; pr && depth < 16; depth++) {
            auto it = windowStates.find(pr);
            if(it == windowStates.end()) return false;
            if(it->second.ID == ourWin.ID) return true;
            pr = it->second.prWinName;
        }
        return false;
    };

    for(auto &[id, win] : windowStates)
    {
        if(win.ID == ourWin.ID) continue;
        if(win.flags & WIN_CHILD_WINDOW) continue;
        if(isDescendant(win)) continue;
        snapPts.push_back(win.pos);
        snapPts.push_back(win.pos + win.size);
    }

    const float threshold = 20.0f;
    float snapDistX = threshold;
    float snapDistY = threshold;
    vec2 dif = vec2(0,0);
    // vec2 p2 = p + size;

    for(auto &pt : snapPts)
    {
        // Snap left edge
        if(fabs(p.x - pt.x) < snapDistX) {
            dif.x = pt.x - p.x;
            snapDistX = fabs(p.x - pt.x);
        }
        // Snap right edge
        // if(fabs(p2.x - pt.x) < snapDistX) {
        //     dif.x = pt.x - p2.x;
        //     snapDistX = fabs(p2.x - pt.x);
        // }
        // Snap top edge
        if(fabs(p.y - pt.y) < snapDistY) {
            dif.y = pt.y - p.y;
            snapDistY = fabs(p.y - pt.y);
        }
        // Snap bottom edge
        // if(fabs(p2.y - pt.y) < snapDistY) {
        //     dif.y = pt.y - p2.y;
        //     snapDistY = fabs(p2.y - pt.y);
        // }
    }
    return dif;
}

void axGUI::resetScroll(const char *id)
{
    WindowState &win = windowStates[axIdPart(id)];
    win.scroll = vec2(0,0);
    win.scroll0 = vec2(0,0);
}

void axGUI::pushLayout()
{
    scrollStack.push(winScroll);
    layoutStack.push(layoutArea);
    clipStack.push(winClip);
}

void axGUI::popLayout()
{
    if (!scrollStack.empty()) {
        winScroll = scrollStack.top();  // Copy
        scrollStack.pop();                         // Subtract
    }
    if (!layoutStack.empty())   { layoutArea = layoutStack.top();  layoutStack.pop(); }
    if (!clipStack.empty())   { winClip = clipStack.top();  clipStack.pop();  }
}

bool axGUI::isInsideLayout()
{
    return layoutArea.isPointInRec(ms);
}

void axGUI::sameline()
{
    mSameLine=true;   // WIN_NO_SAMELINE: the next item stays on this row
}

void axGUI::loadImages()
{
    // set both gui.iconsImage and bot->iconsImage (drawIcon reads the latter), and register the name
    if(bot->iconsImage > 0) { iconsImage = bot->iconsImage; return; }
    if(!loadIconAtlas("resources/icons.png")) loadIconAtlas("resources/icons2.png");
}

bool axGUI::loadIconAtlas(const char *path, float cell)
{
    if(!bot || !bot->loadIconAtlas(path, cell)) return false;
    iconsImage = bot->iconsImage;
    // icons.png layout (64 px grid): row 11 = arrowheads, row 9 column 4 = white circle
    bot->addIconCell("tri1_icon", 1, 11);     // ⌄  open section
    bot->addIconCell("tri2_icon", 3, 11);     // ›  closed section
    bot->addIconCell("circle_icon", 4, 9);    // ●
    bot->addIconFromDrawing("star_icon", 64, [](axDrawBot &b, float s) {
        b.circle(vec2(s*0.5f, s*0.5f), s*0.4f, cColor(255));   // in 0..s coordinates, draw in white
    });
    // imageButton("yildiz", vec2(24, 24), colors[GUI_BTN_TOGGLE_COLOR]);   // tinted with tint
    return true;
}

void axGUI::setHint(bool hovered, const char *hint, vec2 pos)
{
    if(hovered && hint)
    {
        if(!hint[0]) return;
        // Hint tracking is keyed by the owning widget (ID): mouse movement does not reset the hint timer, and the same hint can be shown again.
        hintPos = winScroll+vec2(pos.x,pos.y+24);
        hintStr = hint;
        hintReqID = ID;
        hintReq = true;
    }
}

// IconDef *axGUI::iconDef(const string &name)
// {
//     auto it = iconMap.find(name);
//     if(it == iconMap.end()) return NULL;
//     return &it->second;
// }

// void axGUI::addIcon(const string &name, int image, rect srcPx, float pad)
// {
//     bot->addIcon(name, image, srcPx, pad);
// }

// void axGUI::drawIcon(const string &name, const vec2 &pos, float sz, const cColor &tint)
// {
//     bot->drawIcon(name, pos, sz, tint);
// }

void axGUI::text(const std::string &str, float fsz, cColor col, vec2 off, int align)
{
    // if(outOfLayoutArea(vec2(0,fsz))) return;
    vec2 tb = bot->textBounds(str, fsz);
    vec2 nsz = vec2(tb.x+1.3*fsz, 1.3*fsz);
    vec2 pos = advanceCursor(nsz)+off;

    // bot->drawLine(pos,pos+vec2(50,0), cRed, 1.0);
    if(align&BOT_ALIGN_RIGHT) pos.x += nsz.x;
    // if(align&BOT_ALIGN_TOP)   pos.y += 0.35*nsz.y;

    bot->text(str, pos, fsz, col, align);
    if(debug) bot->roundedRect(pos,nsz,0,0.5,cColor(0));
}

void axGUI::label(const char *text, const vec2 &size, int align)
{
    genID();
    std::string lbl_ = axLabelPart(text); if(text) text = lbl_.c_str();   // "Label##id": only the label is visible
    vec2 tb, nsz = size;
    const bool autoW = nsz.x==0;
    if(nsz.x==0) {
        tb = bot->textBounds(text, fontSize);
        nsz = vec2(tb.x, fmax(nsz.y,2*fontSize));
    }
    vec2 pos = advanceCursor(nsz, autoW);
    if(outOfLayoutArea(pos, nsz)) return;

    bool hovered = winHovered && rect(pos+winScroll, nsz).isPointInRec(ms) && winClip.isPointInRec(ms);
    if(hovered && !msLeftAction && !msrightAction) hoverID=ID;

    if(align&BOT_ALIGN_LEFT)  bot->text(text, pos+0.5*vec2(0,nsz.y), fontSize, colors[GUI_TEXT_COLOR], align);
    if(align&BOT_ALIGN_RIGHT) bot->text(text, pos+vec2(nsz.x,nsz.y/2), fontSize, colors[GUI_TEXT_COLOR], align);

    // bot->drawText(text, pos+0.5*vec2(0,size.y), textSize, textColor, align);
    if(hovered && msLeftReleased)
    {
        // focusID=ID;
        proc=true;
    }
    if(debug)
    {
        bot->roundedRect(pos,tb,0,0.75,cRed);
        bot->roundedRect(pos,nsz,0,0.75,cYellow);
    }
}

void axGUI::separator(float gap, const char *title)
{
    // genID();
    float w = layoutArea.size.x;
    if(w<=0) w=100;
    if(title) gap = fmax(gap, fontSize*1.4f);
    vec2 ssz(w,gap);
    vec2 pos = advanceCursor(ssz, true);
    w = ssz.x;
    if(outOfLayoutArea(pos, ssz)) { newline(0); return; }
    float x=pos.x, y=pos.y;
    // Titled separator: title on the left (theme text color), the line starts after the title
    float lineY = y+gap/2;
    float lx = x;
    if(title) {
        std::string tl = axLabelPart(title);
        bot->text(tl, vec2(x, lineY), fontSize, colors[GUI_TEXT_COLOR], BOT_ALIGN_LEFT|BOT_ALIGN_MIDDLE);
        lx = x + bot->textWidth(tl, fontSize) + 8;
    }
    cColor lc = colors[GUI_WIN_BORDERCLR];
    bot->drawLine(vec2(lx,lineY),vec2(x+w,lineY),lc,1);
    newline(0);
}


//----------------------------------------------------------------------------- collapsible container animation
float axGUI::stepOpenAnim(float &t, bool open)
{
    float target = open ? 1.0f : 0.0f;
    if(t != target) bot->textSnap = false;   // animation in progress: let later text in this window slide smoothly
    if(collapseDuration <= 0) t = target;
    else {
        float d = GetFrameTime() / collapseDuration;
        t = (t < target) ? fmin(target, t + d) : fmax(target, t - d);
    }
    return t * t * (3.0f - 2.0f * t);   // smoothstep
}

bool axGUI::beginCollapse(int id, bool open)
{
    CollapseAnim &a = collapseAnims[id];
    if(!a.init) { a.init = true; a.t = open ? 1.0f : 0.0f; }
    float e = stepOpenAnim(a.t, open);
    if(a.t <= 0) return false;

    CollapseFrame f;
    f.id = id;
    f.startY = curPos.y;
    f.animating = a.t < 1.0f;
    f.prevUseInput = useInput;
    if(f.animating)
    {
        // content is fully laid out, only the animated height is visible; it takes no input meanwhile
        f.shownH = e * a.contentH;
        float lx = layoutArea.size.x > 0 ? layoutArea.left() : 0.0f;
        float lw = layoutArea.size.x > 0 ? layoutArea.size.x : (float)GetScreenWidth();
        bot->save();
        bot->intersectScissorWorld(vec2(lx - 4, f.startY), vec2(lw + 8, f.shownH));
        useInput = false;
    }
    collapseStack.push_back(f);
    return true;
}

void axGUI::endCollapse()
{
    if(collapseStack.empty()) return;
    CollapseFrame f = collapseStack.back();
    collapseStack.pop_back();
    CollapseAnim &a = collapseAnims[f.id];
    float bottom = curPos.y + lineHMax;
    a.contentH = fmax(0.0f, bottom - f.startY);   // measured every frame (stays correct when the content changes)
    if(f.animating)
    {
        bot->restore();
        useInput = f.prevUseInput;
        float lx = layoutArea.size.x > 0 ? layoutArea.left() : curPos.x;
        curPos = vec2(lx, f.startY + f.shownH);
        lineHMax = 0;
        if(prWinName) { WindowState &w = windowStates[prWinName]; w.curPos = curPos; }
    }
}

// Dropdown list geometry: directly below the box (comboListGap), same x and width as the box; rows use comboListPad inner padding.
// Drawing (endMode, in endFrame) and selection (endMode=false, inside comboBox) share the same computation so the click area matches the drawn list.
static const float comboListGap = 3.0f, comboListPad = 4.0f;
static rect comboListRect(const vec2 &boxPos, const vec2 &boxSize, int n)
{
    return rect(boxPos + vec2(0, boxSize.y + comboListGap), vec2(boxSize.x, n * boxSize.y + 2 * comboListPad));
}

bool axGUI::ComboBoxList(ComboBoxState &state, const vec2 &pos, const vec2 &size, const std::vector<IdStrPair> &items, bool endMode)
{
    bool sel=false;
    const int n = (int)items.size();
    const rect lr = comboListRect(pos, size, n);
    const float h = size.y;
    const float rx = lr.pos.x + comboListPad, rw = lr.size.x - 2 * comboListPad;
    const vec2 lms = overComboList ? msCombo : ms;          // while the list is under the mouse, ms is taken away from the GUI
    const vec2 hms = endMode ? lms : lms - winScroll;        // endMode: screen coordinates; otherwise layout coordinates

    if(endMode)
    {
        // shadow (soft, toward the bottom) + panel + border
        const float r = fmaxf(frameRadius, 4.0f);
        for(int k = 3; k >= 1; k--)
        {
            float e = 2.0f * k;
            bot->roundedFRect(lr.pos + vec2(-e * 0.5f, e * 0.6f), lr.size + vec2(e, e * 0.4f), r + e * 0.5f, cColor(0, 0, 0, 14 + 6 * (3 - k)));
        }
        cColor panel = colors[GUI_WIN_BACKCLR]; panel.a = 255;
        bot->roundedFRect(lr.pos, lr.size, r, panel, 1.0f, colors[GUI_WIN_BORDERCLR]);
    }

    for (int i = 0; i < n; ++i)
    {
        const float itemY = lr.pos.y + comboListPad + i * h;
        const bool itemHovered = rect(vec2(rx, itemY), vec2(rw, h)).isPointInRec(hms);

        if(endMode)
        {
            const bool selRow = (i==state.selIx);
            // row highlight: light highlight for the selected row, hover color for the hovered button; rounded corners
            if(selRow || itemHovered)
            {
                cColor c = itemHovered ? colors[GUI_BTN_HOVER_COLOR] : colors[GUI_BTN_SEL_COLOR];
                if(itemHovered && selRow) c = contrast(colors[GUI_BTN_SEL_COLOR], 0.12f);
                c.a = 255;
                bot->roundedFRect(vec2(rx, itemY), vec2(rw, h), fmaxf(0.0f, frameRadius - 1.0f), c);
            }
            // icon: the item's icon, otherwise a highlight dot on the selected row
            const std::string &ic = items[i].icon;
            const float isz = fminf(h * 0.6f, 18.0f), ix = rx + 6, iy = itemY + (h - isz) * 0.5f;
            const cColor icClr = selRow ? colors[GUI_BTN_TOGGLE_COLOR] : withAlpha(colors[GUI_TEXT_COLOR], itemHovered ? 230 : 160);
            float tx = rx + 8;
            if(!ic.empty() && bot->hasIcon(ic)) { bot->drawIcon(ic, vec2(ix, iy), isz, icClr); tx = ix + isz + 8; }
            else
            {
                if(selRow) bot->circle(vec2(ix + isz * 0.5f, itemY + h * 0.5f), 3.0f, colors[GUI_BTN_TOGGLE_COLOR]);
                tx = ix + isz + 4;
            }
            cColor tc = itemHovered ? colors[GUI_TEXT_HL_COLOR] : colors[GUI_TEXT_COLOR];
            bot->text(items[i].title, vec2(tx, itemY + h / 2), fontSize, tc, BOT_ALIGN_LEFT|BOT_ALIGN_MIDDLE);
        }

        if(msLeftReleased && !endMode)
        {
            if(itemHovered) { state.selIx = i; sel=true; }
            proc=true;
        }
    }
    return sel;
}

bool axGUI::comboBox(const vec2 &size_, const std::vector<IdStrPair> &items, int &value, const char *hint)
{
    vec2 size = size_;   // a grid cell may widen it
    genID();
    ComboBoxState& state = comboStates[ID];
    for(auto it=items.begin(); it!=items.end(); it++) {
        if(it->id==value) {
            state.selIx = it - items.begin();
            break;
        }
    }

    bool sel=false;
    vec2 pos = advanceCursor(size);
    float w = size.x, h = size.y;
    float x=pos.x, y=pos.y;
    bool hovered = rect(pos+winScroll, size).isPointInRec(ms)/* && winClip.isPointInRec(ms)*/;
    bool clicked = msLeftPressed & hovered;
    if(drawComboList && actComboID!=ID) { hovered=false; clicked=false; }

    if(hovered) hoverID=ID;

    if(hovered && winHovered)
    {
        if(!msLeftAction && !msrightAction) hoverID=ID;
        if(hint) setHint(hovered, hint, pos+vec2(size.x/2,0));
        if((msLeftDown||msLeftPressed) ) { focusID=ID; proc=true; }
        if(!state.open && msWheel)
        {
            focusID=ID;
            state.selIx += (msWheel>0)?-1:1;
            msWheel = 0;   // keep the window from scrolling
            state.selIx = Clamp(state.selIx, 0, (int)items.size()-1);
            value = items[state.selIx].id;
            proc=sel=true;
        }
    }

    // Background: lightens slightly on hover / while open, highlight border while open; arrow on the right
    cColor boxClr = colors[GUI_IDLE_COLOR];
    if(state.open || (hovered && winHovered)) boxClr = contrast(boxClr, 0.06f);
    bot->roundedFRect(x, y, w, h, frameRadius, boxClr, (state.open || focusID==ID) ? 1.5f : 0, colors[GUI_BTN_TOGGLE_COLOR]);
    {
        const float ax = x + w - 14, ay = y + h * 0.5f, as = fminf(4.0f, h * 0.16f);
        const cColor ac = withAlpha(colors[GUI_TEXT_COLOR], (hovered || state.open) ? 230 : 150);
        if(state.open) { bot->drawLine(vec2(ax - as, ay + as * 0.5f), vec2(ax, ay - as * 0.5f), ac, 1.5f); bot->drawLine(vec2(ax, ay - as * 0.5f), vec2(ax + as, ay + as * 0.5f), ac, 1.5f); }
        else           { bot->drawLine(vec2(ax - as, ay - as * 0.5f), vec2(ax, ay + as * 0.5f), ac, 1.5f); bot->drawLine(vec2(ax, ay + as * 0.5f), vec2(ax + as, ay - as * 0.5f), ac, 1.5f); }
    }

    // Selected item
    if(state.selIx<(int)items.size())
    {
        // also show the selected item's icon in the box
        const std::string &ic = items[state.selIx].icon;
        float tx = x + leftMargin;
        if(!ic.empty() && bot->hasIcon(ic)) { const float isz = fminf(h * 0.6f, 18.0f); bot->drawIcon(ic, vec2(x + 6, y + (h - isz) * 0.5f), isz, colors[GUI_TEXT_COLOR]); tx = x + 6 + isz + 8; }
        else tx = x + 8;
        bot->text(items[state.selIx].title, tx, y + h / 2, fontSize, colors[GUI_TEXT_COLOR], BOT_ALIGN_LEFT|BOT_ALIGN_MIDDLE);
    }

    // Dropdown list drawing
    // bot->text(axFormat("%d.%d.%d",state.open,hovered,winHovered), x + w - 20, y + h / 2, 10, colors[GUI_TEXT_COLOR], BOT_ALIGN_RIGHT|BOT_ALIGN_MIDDLE);
    if(state.open)
    {
        bot->save();
        bot->resetScissor();
        bool res = ComboBoxList(state, pos, actCombosize, items, false);
        if(res) {
            value = items[state.selIx].id;
            proc=sel=true;
            state.open = false;
            drawComboList = false;
        }
        bot->restore();

        if(msLeftReleased && !hovered)
        {
            if(!sel) state.open = false;
            proc=true;
            focusID=ID;
            drawComboList = false;
        }
        if(clicked) { state.open = false; proc=true;  drawComboList = false; }
    }
    else if(clicked)
    {
        if(hovered && winHovered) {
            focusID=ID;
            state.open = !state.open;
            if(state.open) {
                drawComboList=true;
                actComboID=ID;
                actComboState = state;
                actComboitems = items;
                actComboPos = pos+winScroll;
                actCombosize = size;
            }
            else drawComboList=false;
        }
        else { state.open = false; }
        proc=true;
    }

    return sel;
}

ItemState &axGUI::empty(vec2 size)
{
    genID();
    ItemState &item = itemStates[ID];
    vec2 pos = advanceCursor(size);

    bool hovered = winHovered && rect(pos+winScroll, size).isPointInRec(ms)/* && winClip.isPointInRec(ms)*/;

    cColor clr = colors[GUI_IDLE_COLOR];
    // bot->roundedRect(pos, size, 4, 1, colors[GUI_IDLE_COLOR]);
    // bot->roundedFRect(pos, size, 4, colors[GUI_IDLE_COLOR]);

    return item;
}

ItemState &axGUI::dummy(vec2 size, const char *hint)
{
    genID();
    ItemState &item = itemStates[ID];
    vec2 pos = advanceCursor(size);

    bool hovered = winHovered && rect(pos+winScroll, size).isPointInRec(ms)/* && winClip.isPointInRec(ms)*/;

    cColor clr = colors[GUI_IDLE_COLOR];
    bot->roundedRect(pos, size, 4, 1, colors[GUI_IDLE_COLOR]);
    // bot->roundedFRect(pos, size, 4, colors[GUI_IDLE_COLOR]);

    if(hovered)
    {
        overGUI = true;
        if(!msLeftAction && !msrightAction) {
            hoverID=ID;
            bot->roundedFRect(pos, size, 4, clr);
        }
        if(msLeftPressed) {
            clr = colors[GUI_BTN_SEL_COLOR];
            focusID = ID;
        }
        else clr = colors[GUI_HOVER_COLOR];
    }

    // bot->roundedFRect(pos, size, 4,clr);
    bot->roundedRect(pos, size, 4, 1, clr);
    item.set("dummy", pos+winScroll, size, hovered, ID==focusID, 4);
    if(hovered)
    {
        if(msLeftAction)  { proc=true; }
        if(msLeftPressed) { focusID=ID; return item; }
    }
    if(hovered && hint) setHint(hovered, hint, pos+vec2(size.x/2,0));   // centered (not left-aligned)
    return item;
}

bool axGUI::buttonHovered(const vec2 &pos, const vec2 &size)
{
    // if(drawComboList) return false;
    bool res = useInput && winHovered && rect(pos+winScroll, size).isPointInRec(ms) && winClip.isPointInRec(ms);
    if(res) hoverID=ID;
    return res;
}

// Two-way hover transition of hoverDuration with smoothstep. Target: GUI_BTN_HOVER_COLOR if the base color is the button color, otherwise toward the text color (e.g. an active toggle). Slightly stronger while pressed.
float axGUI::buttonAnim(int ID, ItemState &item, bool hovered, cColor &bgClr, vec2 &pos)
{
    auto& anim = anims[ID];
    anim.frame = frameCnt;
    const float target = hovered ? 1.0f : 0.0f;
    if(hoverDuration <= 0) anim.t = target;
    else {
        float d = GetFrameTime() / hoverDuration;
        anim.t = (anim.t < target) ? fminf(target, anim.t + d) : fmaxf(target, anim.t - d);
    }
    float t = fminf(1.0f, fmaxf(0.0f, anim.t));
    float e = t * t * (3.0f - 2.0f * t);

    const cColor &bc = colors[GUI_BTN_COLOR];
    bool isBtn = bgClr.r==bc.r && bgClr.g==bc.g && bgClr.b==bc.b && bgClr.a==bc.a;
    cColor hoverClr = isBtn ? colors[GUI_BTN_HOVER_COLOR] : contrast(bgClr, 0.18f);
    cColor c = mixColor(bgClr, hoverClr, e);
    if(hovered && msLeftDown && useInput) c = contrast(c, 0.10f);   // pressed
    bgClr = c;
    pos = pos - vec2(0, e);   // slight lift (smooth)
    return e;
}

bool axGUI::buttonBehavior(ItemState &item, bool hovered)
{
    if(!useInput) return false;
    bool res=false;
    if(hovered)
    {
        overGUI = true;
        if(!msLeftAction && !msrightAction) hoverID=ID;
        if(msLeftPressed)
        {
            item.clicked = true;
            proc=true;
            // focusID=ID;
        }
    }
    if(item.clicked)
    {
        if(hovered && msLeftReleased && (ms-msDown).length()<10.0f)
        {
            focusID=ID;
            proc=true;
            res=true;
            item.open = !item.open;
            axLog("open:%d, id:%d",item.open,ID);
        }
        if(msLeftReleased) { item.clicked=false; }
    }
    return res;
}

void axGUI::startDrag(int ID, vec2 p)
{
    dragID = ID;
    dragStartPos = p;
}

bool axGUI::selectable(const char *text, bool selected, bool fitH, float textSize)
{
    genID();
    std::string lbl_ = axLabelPart(text); if(text) text = lbl_.c_str();   // "Label##id": only the label is visible
    float fsz = textSize?textSize:fontSize;
    vec2 size = bot->textBounds(text,fsz);
    if(fitH && layoutArea.size.x>0)  size.x = layoutArea.size.x - 2*itemSpaceX;
    vec2 pos = advanceCursor(size, fitH);
    if(outOfLayoutArea(pos, size)) return false;
    bool sel = selected;
    bool hovered = winHovered && rect(pos+winScroll, size).isPointInRec(ms) && winClip.isPointInRec(ms);
    // bot->roundedFRect(pos.x, pos.y, size.x, size.y, 4, sel?colors[GUI_HOVER_COLOR]:colors[GUI_IDLE_COLOR]);
    int align=BOT_ALIGN_LEFT|BOT_ALIGN_MIDDLE;
    bot->text(axFormat("%s",text), pos+0.5*vec2(0,size.y), fsz, sel?colors[GUI_TEXT_SEL_COLOR]:(hovered?colors[GUI_TEXT_HL_COLOR]:colors[GUI_TEXT_COLOR]), align);
    // bot->roundedRect(pos,size,0,0.5,cColor(0));

    ItemState &item = itemStates[ID];
    item.set(text, pos+winScroll, size, hovered, ID==focusID, 0);
    // bool res = buttonBehavior(item, hovered);
    bool res=false;
    if(hovered)
    {
        overGUI = true;
        if(!msLeftAction && !msrightAction) hoverID=ID;
        if(msLeftPressed)  { item.clicked = true; proc=true; }
    }
    if(item.clicked)
    {
        if(hovered && msLeftPressed)
        {
            focusID=ID;
            res=proc=true;
            item.open = !item.open;
        }
        if(!item.dragging && hovered && msLeftDown && GetTime() - msDownTime > 0.25f)
        {
            item.dragging = true;
            startDrag(ID, winScroll+pos);
            dragID=ID;
            proc=true;
            res=false;
        }
        if(msLeftReleased) { item.clicked=false; }
    }
    // if(hovered && msLeftPressed) focusID=ID;
    return res;
}

bool axGUI::button(const char *text, vec2 size, float fsz, const char *hint)
{
    genID();
    std::string lbl_ = axLabelPart(text);
    if(text) text = lbl_.c_str();   // "Label##id": only the label is visible
    float fsize = fsz?fsz:fontSize;
    vec2 tb = bot->textBounds(text, fsize);
    const bool autoW = size.x==0;
    if(size.x==0) size.x = tb.x+2*fsize;
    if(size.y==0) size.y = 2*fsize;
    vec2 pos = advanceCursor(size, autoW);
    if(outOfLayoutArea(pos, size)) return false;
    ItemState &item = itemStates[ID];

    cColor bgClr = colors[GUI_BTN_COLOR];
    bool hovered = buttonHovered(pos, size);
    float he = buttonAnim(ID, item, hovered, bgClr, pos);

    bot->roundedFRect(pos, size, btnRadius, bgClr);

    if(text)   bot->text(text, pos+(size-tb)/2, fsize, mixColor(colors[GUI_TEXT_COLOR], colors[GUI_TEXT_HL_COLOR], he));

    item.set(text, pos+winScroll, size, hovered, ID==focusID, 4);
    bool res = buttonBehavior(item, hovered);

    if(hovered && hint) setHint(hovered, hint, pos+vec2(size.x/2,0));   // centered (not left-aligned)
    return res;
}

bool axGUI::toggleButton(const char *text, bool *value, vec2 size, const char *hint)
{
    genID();
    std::string lbl_ = axLabelPart(text);
    if(text) text = lbl_.c_str();   // "Label##id": only the label is visible
    vec2 tb = bot->textBounds(text, fontSize);
    const bool autoW = size.x==0;
    if(size.x==0) size = vec2(tb.x+2*fontSize, fmax(size.y,2*fontSize));
    vec2 pos = advanceCursor(size, autoW);
    if(outOfLayoutArea(pos, size)) return false;
    ItemState &item = itemStates[ID];

    cColor bgClr = *value ? colors[GUI_BTN_TOGGLE_COLOR] : colors[GUI_BTN_COLOR];   // show hover while open as well
    bool hovered = buttonHovered(pos, size);
    float he = buttonAnim(ID, item, hovered, bgClr, pos);

    bot->roundedFRect(pos, size, btnRadius, bgClr);

    if(text)   bot->text(text, pos+(size-tb)/2, fontSize, mixColor(colors[GUI_TEXT_COLOR], colors[GUI_TEXT_HL_COLOR], he));

    item.set(text, pos+winScroll, size, hovered, ID==focusID, 4);
    bool res = buttonBehavior(item, hovered);
    if(res) *value = !*value;

    if(hovered && hint) setHint(hovered, hint, pos+vec2(size.x/2,0));   // centered (not left-aligned)
    return false;
}

bool axGUI::imageButton(const std::string &name, vec2 size, cColor tint, const char *hint)
{
    genID();
    vec2 pos = advanceCursor(size);
    if(outOfLayoutArea(pos, size)) return false;

    // bool hovered = useInput && winHovered && rect(pos+winScroll, size).isPointInRec(ms) && winClip.isPointInRec(ms);
    bool hovered = buttonHovered(pos, size);

    cColor clr = tint;
    if(msLeftPressed) itemStates[ID].clicked=hovered;
    // bot->roundedFRect(pos.x, pos.y, size.x, size.y, 4, colors[GUI_IDLE_COLOR]);
    if(hovered)
    {
        overGUI = true;
        if(!msLeftAction && !msrightAction)
        {
            bot->roundedFRect(pos.x, pos.y, size.x, size.y, btnRadius, colors[GUI_HOVER_COLOR]);
            hoverID=ID;
        }
        clr = colors[GUI_TEXT_HL_COLOR];
        if(msLeftPressed) clr = colors[GUI_BTN_TOGGLE_COLOR];
    }
    bot->drawIcon(name, pos, size.y, clr);
    // if(text) bot->drawText(text, pos.x + size.x * 0.5f, pos.y + size.y * 0.5f, textSize, textColor, BOT_ALIGN_CENTER|BOT_ALIGN_MIDDLE);
    ItemState &item = itemStates[ID];
    item.set(name, pos+winScroll, size, hovered, ID==focusID, 4);
    if(hovered)
    {
        if(msLeftAction) { proc=true; }
        if(msLeftPressed) { focusID=ID; }
        if(msLeftReleased) { return itemStates[ID].clicked; }
    }
    // if(msLeftReleased)  focusGUI=false;
    return false;
}

bool axGUI::icon(const std::string &name, vec2 size, const char *hint)
{
    genID();
    IconDef *def = bot->iconDef(name);
    const float iw = def ? fabsf(def->irect.size.x) : 0, ih = def ? fabsf(def->irect.size.y) : 0;
    const float aspect = (iw > 0 && ih > 0) ? iw / ih : 1.0f;
    const bool autoW = size.x <= 0;
    if(size.x <= 0 && size.y <= 0) size = (iw > 0 && ih > 0) ? vec2(iw, ih) : vec2(64, 64);
    else if(size.x <= 0) size.x = size.y * aspect;
    else if(size.y <= 0) size.y = size.x / aspect;
    vec2 pos = advanceCursor(size, autoW);
    if(outOfLayoutArea(pos, size)) return false;

    // aspect-fit, centered
    vec2 dsz = size;
    if(dsz.x / dsz.y > aspect) dsz.x = dsz.y * aspect; else dsz.y = dsz.x / aspect;
    vec2 dpos = pos + (size - dsz) * 0.5f;
    if(def) bot->drawImage(def->image < 0 ? bot->iconsImage : def->image, def->irect, dpos, dsz);   // < 0: icon atlas
    else bot->roundedRect(pos, size, 4, 1, colors[GUI_WIN_BORDERCLR]);   // missing image: frame only

    ItemState &item = itemStates[ID];
    bool hovered = buttonHovered(pos, size);
    item.set(name, pos+winScroll, size, hovered, ID==focusID, 0);
    bool res = buttonBehavior(item, hovered);
    if(hovered && hint) setHint(hovered, hint, pos+vec2(size.x/2,0));
    return res;
}

bool axGUI::image(const std::string &path, vec2 size, const char *hint)
{
    const std::string name = "file:" + path;      // registered with axDrawBot under this name
    auto it = imageFiles.find(path);
    if(it == imageFiles.end())
    {
        bool ok = false;
        if(FileExists(path.c_str()))
        {
            Texture2D tex = LoadTexture(path.c_str());
            if(tex.id != 0)
            {
                GenTextureMipmaps(&tex);
                SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
                ok = bot->addIconFromTexture(name, tex);
                if(ok) bot->iconDef(name)->owned = true;   // removeIcon frees it
                else UnloadTexture(tex);
            }
        }
        if(!ok) axLog("Image: cannot load %s", path.c_str());
        imageFiles[path] = ok;
    }
    return icon(name, size, hint);                // missing image: icon() draws a frame
}

void axGUI::unloadImageFiles()
{
    for(auto &[path, ok] : imageFiles) if(ok) bot->removeIcon("file:" + path);
    imageFiles.clear();
}

bool axGUI::imageButton(IconDef &def, vec2 size, cColor tint, const char *hint)
{
    genID();
    vec2 pos = advanceCursor(size);
    if(outOfLayoutArea(pos, size)) return false;
    ItemState &item = itemStates[ID];
    cColor clr = tint;

    bool hovered = buttonHovered(pos, size);
    bot->drawImage(def.image, def.irect, pos, size);
    // if(text) bot->drawText(text, pos.x + size.x * 0.5f, pos.y + size.y * 0.5f, textSize, textColor, BOT_ALIGN_CENTER|BOT_ALIGN_MIDDLE);
    item.set("imgBtn", pos+winScroll, size, hovered, ID==focusID, 4);
    bool res = buttonBehavior(item, hovered);
    return res;
}

bool axGUI::shapeButton(vec2 size, const std::string &shape, vec2 shapeSize, cColor clr, float strk, cColor sClr, const char *hint)
{
    genID();
    vec2 pos = advanceCursor(size);
    if(outOfLayoutArea(pos, size)) return false;
    ItemState &item = itemStates[ID];
    // bool hovered = useInput && winHovered && rect(pos+winScroll, size).isPointInRec(ms) && winClip.isPointInRec(ms);
    bool hovered = buttonHovered(pos, size);

    // bot->roundedFRect(pos.x, pos.y, size.x, size.y, 4, colors[GUI_IDLE_COLOR]);
    if(hovered) {
        overGUI = true;
        // bot->roundedFRect(pos.x, pos.y, size.x, size.y, 4, colors[GUI_HOVER_COLOR]);
        if(msLeftPressed) {
            bot->roundedFRect(pos.x, pos.y, size.x, size.y, btnRadius, colors[GUI_BTN_SEL_COLOR]);
        }
    }

    if(shape=="circle")  {
        bot->ellipse(pos+size/2,shapeSize,clr,strk+2*hovered,sClr);
    }
    if(shape=="rect")
    {
        bot->roundedFRect(pos+0.5f*(size-shapeSize), shapeSize, 0, clr, strk+2*hovered, sClr);
    }
    else if(shape=="gradY+") bot->roundedFRect(pos+0.5f*(size-shapeSize), shapeSize, 0, PaintStyle::LinearAngle(0.5f*M_PI, cBlack, cWhite), strk+2*hovered, sClr);
    else if(shape=="gradY-") bot->roundedFRect(pos+0.5f*(size-shapeSize), shapeSize, 0, PaintStyle::LinearAngle(1.5f*M_PI, cBlack, cWhite), strk+2*hovered, sClr);
    else if(shape=="gradX-") bot->roundedFRect(pos+0.5f*(size-shapeSize), shapeSize, 0, PaintStyle::LinearAngle(0, cBlack, cWhite), strk+2*hovered, sClr);
    else if(shape=="gradX+") bot->roundedFRect(pos+0.5f*(size-shapeSize), shapeSize, 0, PaintStyle::LinearAngle(M_PI, cBlack, cWhite), strk+2*hovered, sClr);

    if(hovered && hint) setHint(hovered, hint, pos+vec2(size.x/2,0));

    item.set(shape, pos+winScroll, size, hovered, ID==focusID, 4);
    if(hovered)
    {
        if(msLeftAction) { proc=true; }
        if(msLeftPressed) { focusID=ID; return true; }
    }
    return false;
}

bool axGUI::colorButton(vec2 size, vec2 shapeSize, cColor *clrPtr, float strk, cColor sClr, const char *hint)
{
    genID();
    vec2 pos = advanceCursor(size);
    if(outOfLayoutArea(pos, size)) return false;
    ItemState &item = itemStates[ID];
    // bool hovered = useInput && winHovered && rect(pos+winScroll, size).isPointInRec(ms) && winClip.isPointInRec(ms);
    bool hovered = buttonHovered(pos, size);

    if(hovered)
    {
        overGUI = true;
        // bot->roundedFRect(pos.x, pos.y, size.x, size.y, 4, colors[GUI_HOVER_COLOR]);
        if(msLeftPressed) {
            bot->roundedFRect(pos.x, pos.y, size.x, size.y, btnRadius, colors[GUI_BTN_SEL_COLOR]);
        }
    }

    bot->ellipse(pos+size/2,shapeSize,*clrPtr,strk+2*hovered,sClr);
    if(hovered && hint) setHint(hovered, hint, pos+vec2(size.x/2,0));

    item.set("clrBtn", pos+winScroll, size, hovered, ID==focusID, 4);

    bool res = buttonBehavior(item, hovered);
    if(res)
    {
        if(msLeftAction) { proc=true; }
        if(msLeftReleased)
        {
            focusID=ID;
            clrPickerPtr = clrPtr;
            clrPickerColor = *clrPtr;
            showClrPicker=true;
            // directly below the button (screen coordinates); layout coordinates would ignore winScroll and open the picker offset by the scroll amount in a scrolled window
            clrPickerPos = pos + winScroll + vec2(0, size.y + 4);
            clrPickerAnchor = rect(pos + winScroll, size);
            auto pit = windowStates.find("ColorPicker");               // if opened from another button while open, move it there
            if(pit != windowStates.end()) pit->second.pos = clrPickerPos;
        }
    }
    if(clrPtr && clrPtr==clrPickerPtr && clrPickerUpdate) { *clrPtr=clrPickerColor; res=clrPickerUpdate; clrPickerUpdate=false; }
    return res;
}

bool axGUI::checkbox(const char *label, bool *checked, const vec2 &size, vec2 box)
{
    genID();
    std::string lbl_ = axLabelPart(label); if(label) label = lbl_.c_str();   // "Label##id": only the label is visible
    float w = size.x, h = size.y;
    vec2 nsz = size;
    const bool autoW = nsz.x==0;

    float bsw  = 2.0f;                 // border stroke width
    if(box==vec2(0,0))   box = vec2(h-bsw, h-bsw);
    else bsw = 0.5f*(h-box.y);

    if(nsz.x==0) {
        vec2 tb = bot->textBounds(label, fontSize);
        nsz = vec2(tb.x, fmax(nsz.y,2*fontSize));
        w = nsz.x + box.x + itemSpaceX + bsw;
        nsz.x = w;
    }
    vec2 pos = advanceCursor(nsz, autoW);
    if(outOfLayoutArea(pos, nsz)) return false;
    // box is right-aligned INSIDE the given width (does not overflow onto the neighboring item)
    const float bx   = roundf(pos.x + nsz.x - box.x - 0.5f*bsw);
    const float by   = roundf(pos.y + 0.5f*(nsz.y - box.y));

    // Hit area = whole row (label + box)
    bool hovered = buttonHovered(pos, nsz);

    // Track capture per-control so drag-out doesn’t toggle
    static int pressedId = -1;
    if(hovered) hoverID=ID;
    if (msLeftPressed && hovered) {
        pressedId = ID;
        focusID   = ID;
    }
    const bool clicked = (pressedId == ID) && msLeftReleased && hovered;

    if (msLeftReleased && pressedId == ID) pressedId = -1;

    // Keyboard toggle (Space/Enter) when focused
    if (focusID == ID && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))) {
        *checked = !*checked;
        proc = true;
    }
    if (clicked) {
        *checked = !*checked;
        proc = true;
    }

    // Visual states: colors from the theme; 1.5 px border; when checked the box uses the accent color and the tick is white.
    const bool pressed = (pressedId == ID) && msLeftDown;
    auto& anim = anims[ID];
    anim.frame = frameCnt;
    const float target = *checked ? 1.f : 0.f;
    anim.t += (target - anim.t) * 0.18f;   // ease
    if (std::fabs(target - anim.t) < 0.001f) anim.t = target;

    cColor accent = colors[GUI_BTN_TOGGLE_COLOR];
    cColor idle   = pressed ? colors[GUI_BTN_HOVER_COLOR] : (hovered ? colors[GUI_HOVER_COLOR] : colors[GUI_BTN_COLOR]);
    cColor border = colors[GUI_TEXT_COLOR]; border.a = hovered ? 170 : 120;
    cColor fill   = idle + (accent - idle) * anim.t;
    if (anim.t > 0.5f) border = accent;

    // Checkbox box
    bot->roundedFRect(bx, by, box.x, box.y, fminf(checkRadius, box.x*0.5f), fill, 1.5f, border);

    // Draw checkmark (two-segment ✓), grow with anim.t
    if (anim.t > 0.001f) {
        const float s = std::min(box.x, box.y);
        const float cx = bx + s*0.2f,    cy = by + s*0.55f;
        const float x1 = cx,  y1 = cy;
        const float x2 = cx + s*0.22f,   y2 = cy + s*0.22f;
        const float x3 = cx + s*0.60f,   y3 = cy - s*0.28f;

        // Split anim across segments
        float t1 = std::min(anim.t*2.0f, 1.0f);
        float t2 = std::max((anim.t-0.5f)*2.0f, 0.0f);

        const float lw = std::max(1.8f, s*0.13f);
        const cColor tick = cWhite;
        // seg1: (x1,y1) -> (x2,y2)
        if (t1 > 0.f) {
            bot->drawLine(vec2(x1,y1), vec2(x1+(x2-x1)*t1, y1+(y2-y1)*t1), tick, lw);
        }
        // seg2: (x2,y2) -> (x3,y3); round join so two thick lines do not leave a notch at the corner
        if (t2 > 0.f) {
            bot->circle(vec2(x2,y2), lw*0.5f, tick);
            bot->drawLine(vec2(x2,y2), vec2(x2+(x3-x2)*t2, y2+(y3-y2)*t2), tick, lw);
        }
    }

    // Label
    bot->text(label, pos.x, pos.y+nsz.y*0.5f, fontSize, hovered?colors[GUI_TEXT_HL_COLOR]:colors[GUI_TEXT_COLOR], BOT_ALIGN_LEFT | BOT_ALIGN_MIDDLE);

    ItemState &item = itemStates[ID];
    item.set(label, pos+winScroll, nsz, hovered, ID==focusID, 2);
    return *checked;
}

bool axGUI::dragVec2(const char *txt, float txSize, int align, vec2 size, int min, int max, vec2 *val1, vec2 def, float speed, const char *frmt, bool buttons, const cString &hint)
{
    bool res=false;
    // label(txt,vec2(txSize,size.y), align);
    // sameline();
    if(dragFloat(size, min, max, &val1->x, def.x, speed, frmt, buttons, (hint+".x").c_str())) res=true;
    sameline();
    if(dragFloat(size, min, max, &val1->y, def.y, speed, frmt, buttons, (hint+".y").c_str())) res=true;
    return res;
}

bool axGUI::dragFloat(vec2 size, Param &prm, float speed, const char *frmt, bool buttons, const char *hint)
{
    float value = prm.value.getFloat();
    float vmax = prm.maxVal.getFloat();
    float vmin = prm.minVal.getFloat();
    float vdef = prm.defVal.getFloat();
    if(fabs(vmax-vmin)<10) { speed=0.01f; frmt="%.2f"; }
    speed = prm.speed;
    if(speed<0.01f) frmt="%.3f";
    bool res = dragFloat(size, vmin, vmax, &value, vdef, speed, frmt, buttons, hint);
    if(res) {prm.setVal(value); }
    return res;
}

bool axGUI::dragFloat(vec2 size, float min, float max, float *value, float def, float speed, const char *frmt, bool buttons, const char *hint)
{
    static float wait=0;
    bool changed = false;
    genID();
    vec2 pos = advanceCursor(size);
    if(outOfLayoutArea(pos, size)) return false;
    float w = size.x, h = size.y;
    float x=pos.x, y=pos.y;
    float range = max - min;
    // bool hovered = winHovered && rect(pos+winScroll, vec2(w,h)).isPointInRec(ms) && winClip.isPointInRec(ms);
    bool hovered = buttonHovered(pos, vec2(w,h));

    bool dragging = msLeftPressed || msLeftDown;
    float bx=16;
    rect b1(x+2,y+1,bx,h-2);
    rect b2(x+w-bx-2,y+1,bx,h-2);
    DragFloatState& st = dragStates[ID];
    int strLen = 0;

    if(hovered) hoverID=ID;
    if(hovered && hint) setHint(hovered, hint, pos+vec2(size.x/2,0));

    // Enter editing mode on double click or when focused and not dragging buttons
    if(msLeftPressed && hovered)
    {
        if(buttons)
        {
            st.isBtn1Down = b1.isPointInRec(ms);
            st.isBtn2Down = b2.isPointInRec(ms);
        }
        if(!st.isBtn1Down && !st.isBtn2Down)
        {
            // Double click to enter edit mode (or single click if already focused)
        }
        st.initialValue = *value;
        st.msDown = ms;
        focusID = ID;
        proc = true;
    }
    if(msAnyPressed && st.isEditing)
    {
        if(!hovered) st.isEditing=false;
        proc = true;
    }

    if(focusID == ID && msLeftDown && !st.isEditing && mouseMoved)
    {
        st.isDragging = true;
    }

    if(msLeftReleased && !st.isDragging)
    {
        if(focusID == ID && !st.isEditing)
        {
            st.isEditing = true;

            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3) << *value;  // 3 digits are enough
            std::string str = oss.str();

            str.erase(str.find_last_not_of('0') + 1);
            str.erase(str.find_last_not_of('.') + 1);

            st.editBuffer = str;
            st.cursor = str.length();
            proc = true;
        }
    }

    // Handle keyboard input when editing
    if(st.isEditing && focusID == ID)
    {
        // Get keyboard input (you'll need to adapt this to your input system)
        // Assuming you have functions like: IsKeyPressed(), GetCharPressed(), etc.
        // Get all characters pressed this frame
        bool updValue=false;
        int key = GetCharPressed();
        while(key > 0)
        {
            char c = (char)key;
            if((c >= '0' && c <= '9') || c == '.' || c == '-')
            {
                // Prevent multiple decimal points
                if(c == '.' && st.editBuffer.find('.') != std::string::npos)
                {
                    key = GetCharPressed();
                    continue;
                }
                // Only allow minus at the beginning
                if(c == '-' && !st.editBuffer.empty())
                {
                    key = GetCharPressed();
                    continue;
                }

                st.editBuffer.insert(st.cursor, axFormat("%c",c));
                st.cursor = std::min((int)st.editBuffer.size(), ++st.cursor);
                proc = true;
                updValue = true;
            }
            key = GetCharPressed();
        }

        // Backspace
        if(IsKeyPressed(KEY_BACKSPACE) && !st.editBuffer.empty()) // Adapt key codes
        {
            st.cursor = std::max(0,--st.cursor);
            st.editBuffer.erase(st.cursor,1);
            // st.editBuffer.pop_back();
            proc = updValue = true;
        }
        if(IsKeyPressed(KEY_DELETE) && !st.editBuffer.empty())
        {
            if(st.cursor < (int)st.editBuffer.size())
            {
                st.editBuffer.erase(st.cursor,1);
                proc = updValue = true;
            }
        }
        // Enter - confirm
        bool enter = false;
        if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) { enter = true; }
        if(IsKeyPressed(KEY_LEFT)) st.cursor = std::max(0,--st.cursor);
        if(IsKeyPressed(KEY_RIGHT)) st.cursor = std::min((int)st.editBuffer.size(), ++st.cursor);
        if(updValue || enter)
        {
            try {
                float newValue = std::stof(st.editBuffer);
                float old = *value;
                *value = Clamp(newValue, (float)min, (float)max);
                changed = (*value != old);
            } catch(...) {
                // Invalid input, keep old value
            }
            if(enter)
            {
                st.isEditing = false;
                st.editBuffer.clear();
            }
            proc = true;
        }

        // Escape - cancel
        if(IsKeyPressed(KEY_ESCAPE))
        {
            st.isEditing = false;
            st.editBuffer.clear();
            proc = true;
        }

        int valueChange=0;
        if(shouldKeyTrigger(KEY_UP)) valueChange=1;
        if(shouldKeyTrigger(KEY_DOWN)) valueChange=-1;
        if(IsKeyDown(KEY_RIGHT_SHIFT)) valueChange*=10;
        if(valueChange!=0)
        {
            float old = *value;
            *value = Clamp(*value + valueChange, (float)min, (float)max);
            changed = (*value != old);
            char buffer[32];
            snprintf(buffer, sizeof(buffer), frmt, *value);
            st.editBuffer = buffer;
            proc = true;
        }

        // Click outside - confirm and exit
        if(msLeftPressed && !hovered)
        {
            try {
                float newValue = std::stof(st.editBuffer);
                float old = *value;
                *value = Clamp(newValue, (float)min, (float)max);
                changed = (*value != old);
            } catch(...) {}
            st.isEditing = false;
            st.editBuffer.clear();
        }
    }

    if(!dragging && !st.isEditing) {
        st.isDragging = false;
        st.isBtn1Down = false;
        st.isBtn2Down = false;
    }

    if(st.isDragging)
    {
        float old = *value;
        focusID=ID;
        float dif = speed * (ms.x - msDown.x - ms.y + msDown.y);
        *value = Clamp(st.initialValue + dif, (float)min, (float)max);
        proc = true;
        changed |= (*value != old);
    }

    // Background color changes based on state
    // while editing: window background color + highlight border; while dragging: blended with the highlight
    cColor bgColor = st.isEditing ? withAlpha(colors[GUI_WIN_BACKCLR], 255)
                   : st.isDragging ? mixColor(colors[GUI_IDLE_COLOR], colors[GUI_BTN_TOGGLE_COLOR], 0.35f) : colors[GUI_IDLE_COLOR];
    bot->roundedFRect(x, y, w, h, frameRadius, bgColor, st.isEditing ? 1.5f : 0, colors[GUI_BTN_TOGGLE_COLOR]);

    int align = BOT_ALIGN_CENTER | BOT_ALIGN_MIDDLE;

    //---- Buttons
    if(buttons)
    {
        if(st.isBtn1Down || st.isBtn2Down)
            if(msLeftPressed || msLeftDown)
            {
                if(msLeftPressed || GetTime()>=wait)
                {
                    bool hit=false;
                    if(b1.isPointInRec(ms)) { *value = *value-1; hit=true; proc = true; }
                    if(b2.isPointInRec(ms)) { *value = *value+1; hit=true; proc = true; }
                    if(hit){
                        if(msLeftPressed) wait=GetTime()+0.5f;
                        else if(msLeftDown) wait=GetTime()+0.1f;
                        changed=true;
                    }
                }
            }
        cColor btnClr = colors[GUI_BTN_HOVER_COLOR], btnDown = colors[GUI_BTN_SEL_COLOR];   // theme colors
        bot->roundedFRect(b1,frameRadius*0.5f,st.isBtn1Down?btnDown:btnClr);
        bot->text(axFormat("-", *value), b1.center(), fontSize, colors[GUI_TEXT_COLOR], align);
        bot->roundedFRect(b2,frameRadius*0.5f,st.isBtn2Down?btnDown:btnClr);
        bot->text(axFormat("+", *value), b2.center(), fontSize, colors[GUI_TEXT_COLOR], align);
    }

    // Display text - show edit buffer when editing, otherwise formatted value
    const char* displayText = st.isEditing ?
                                  (st.editBuffer.empty() ? "" : st.editBuffer.c_str()) :
                                  axFormat(frmt, *value);

    cColor txtColor = st.isEditing ? colors[GUI_TEXT_HL_COLOR] : hoverVisual(hovered, st.isDragging) ? colors[GUI_TEXT_HL_COLOR] : colors[GUI_TEXT_COLOR];

    bot->text(displayText, x+w/2, y+h/2, fontSize, txtColor, align);

    // Optional: Draw blinking cursor when editing
    if(st.isEditing && ((int)(GetTime() * 2) % 2 == 0))
    {
        float textW = bot->textWidth(st.editBuffer, fontSize);
        float cursW = bot->textWidth(st.editBuffer.substr(0,st.cursor), fontSize);
        bot->drawLine(vec2(x+(w-textW)/2+cursW+1, y+3), vec2(x+(w-textW)/2+cursW+1, y+h-3), txtColor);
        // bot->drawLine(vec2(x+w/2+textW/2+2, y+3), vec2(x+w/2+textW/2+2, y+h-3), txtColor);
    }
    if(hovered && msRightPressed && ctrl)
    {
        proc = true;
        *value = def;
        changed = true;
    }
    ItemState &item = itemStates[ID];
    item.set("dragFloat", pos+winScroll, size, hovered, ID==focusID, 2);
    return changed;
}

bool axGUI::dragFloat3(vec2 size, float min, float max, float v[], float speed, const char *frmt, bool buttons, const char *hint)
{
    float def[3] = {0,0,0};
    float sw = (size.x-2*itemSpaceX)/3;
    bool res = false;
    if(dragFloat(vec2(sw,size.y),min,max,(float*)v,def[0],speed,frmt,buttons,hint)) res = true;
    if(dragFloat(vec2(sw,size.y),min,max,(float*)v+1,def[1],speed,frmt,buttons,hint)) res = true;
    if(dragFloat(vec2(sw,size.y),min,max,(float*)v+2,def[2],speed,frmt,buttons,hint)) res = true;
    return res;
}

bool axGUI::dragInt(vec2 size, Param &prm, float speed, bool buttons, const char *hint)
{
    int val0 = prm.getInt();
    bool res = dragInt(size, prm.minVal.getInt(), prm.maxVal.getInt(), prm.value.getIntPtr(), speed, buttons, hint);
    if(prm.getInt()!=val0) { prm.dirty = true; }
    return res;
}

bool axGUI::dragInt(vec2 size, int min, int max, int *value, float speed, bool buttons, const char *hint)
{
    bool changed = false;
    genID();
    vec2 pos = advanceCursor(size);
    if(outOfLayoutArea(pos, size)) return false;
    float w = size.x, h = size.y;
    float x=pos.x, y=pos.y;
    float range = max - min;
    static float wait=0;
    // bool hovered = winHovered && rect(pos+winScroll, vec2(w,h)).isPointInRec(ms) && winClip.isPointInRec(ms);
    bool hovered = buttonHovered(pos, vec2(w,h));

    bool dragging = msLeftPressed || msLeftDown;
    float bx=16;
    rect b1(x+2,y+1,bx,h-2);
    rect b2(x+w-bx-2,y+1,bx,h-2);

    DragFloatState& state = dragStates[ID];
    if(hovered) hoverID=ID;
    if(hovered && hint) setHint(hovered, hint, pos+vec2(size.x/2,0));

    if(msLeftPressed && hovered)
    {
        if(buttons)
        {
            state.isBtn1Down = b1.isPointInRec(ms);
            state.isBtn2Down = b2.isPointInRec(ms);
        }
        if(!state.isBtn1Down && !state.isBtn2Down)
        {
            state.isDragging = true;
            state.initialValue = *value;
        }
        state.msDown = ms;
        proc = true;
    }
    if(!dragging) {
        state.isDragging = false;
        state.isBtn1Down = false;
        state.isBtn2Down = false;
    }

    if(state.isDragging)
    {
        float old = *value;
        focusID=ID;
        float dif = speed * (ms.x - state.msDown.x - ms.y + state.msDown.y);
        float v = state.initialValue + dif;
        if(v<min) v=min;
        if(v>max) v=max;
        *value = v;
        proc = true;
        changed |= (*value != old);
    }

    // Track
    bot->roundedFRect(x, y, w, h, frameRadius, state.isDragging ? mixColor(colors[GUI_IDLE_COLOR], colors[GUI_BTN_TOGGLE_COLOR], 0.35f) : colors[GUI_IDLE_COLOR]);

    int align = BOT_ALIGN_CENTER | BOT_ALIGN_MIDDLE;
    //---- left '-' button
    if(buttons)
    {
        if(state.isBtn1Down || state.isBtn2Down)
            if(msLeftPressed || msLeftDown)
            {
                if(msLeftPressed || GetTime()>=wait)
                {
                    bool hit=false;
                    if(b1.isPointInRec(ms)) { *value = *value-1; hit=true; proc = true; }
                    if(b2.isPointInRec(ms)) { *value = *value+1; hit=true; proc = true; }
                    if(hit){
                        if(msLeftPressed) wait=GetTime()+0.5f;
                        else if(msLeftDown) wait=GetTime()+0.1f;
                        changed=true;
                    }
                }
            }
        cColor btnClr = colors[GUI_BTN_HOVER_COLOR], btnDown = colors[GUI_BTN_SEL_COLOR];   // theme colors
        bot->roundedFRect(b1,frameRadius*0.5f,state.isBtn1Down?btnDown:btnClr);
        bot->text(axFormat("-", *value), b1.center(), fontSize, colors[GUI_TEXT_COLOR], align);

        //---- right '+' button
        // if(leftDown && b2.isPointInRec(ms)) { axLog("+"); *value = *value+1; }
        bot->roundedFRect(b2,frameRadius*0.5f,state.isBtn2Down?btnDown:btnClr);
        bot->text(axFormat("+", *value), b2.center(), fontSize, colors[GUI_TEXT_COLOR], align);
    }

    // Value text
    bot->text(axFormat("%d", *value), x+w/2, y+h/2, fontSize, (hovered&!msLeftAction)?colors[GUI_TEXT_HL_COLOR]:colors[GUI_TEXT_COLOR], align);
    return changed;
}

bool axGUI::dragColor(vec2 size, cColor &clr, float speed, const char *frmt, bool buttons, const char *hint)
{
    float min=0, max=255;
    float sw = (size.x-2*itemSpaceX)/3;
    bool res = false;
    float v[3];
    v[0]=clr.r; v[1]=clr.g; v[2]=clr.b;
    float def[3] = {0,0,0};

    if(dragFloat(vec2(sw,size.y),min,max,(float*)v,def[0],speed,frmt,buttons,hint)) res = true;
    if(dragFloat(vec2(sw,size.y),min,max,(float*)v+1,def[1],speed,frmt,buttons,hint)) res = true;
    if(dragFloat(vec2(sw,size.y),min,max,(float*)v+2,def[2],speed,frmt,buttons,hint)) res = true;
    clr.r = (unsigned char)v[0];
    clr.g = (unsigned char)v[1];
    clr.b = (unsigned char)v[2];
    return res;
}

float axGUI::slider(vec2 off, vec2 size, float min, float max, float *value)
{
    genID();
    vec2 pos = advanceCursor(vec2(size.x+32, size.y));   // value text on the right
    if(outOfLayoutArea(pos, size)) return false;
    ItemState &item = itemStates[ID];
    float w = size.x, h = size.y;
    float radius = 8.0f;
    float knobRadius = 6.0f;

    float x=pos.x, y=pos.y;
    // float x=pos.x+off.x, y=pos.y+off.y;
    float range = max - min;
    float norm = (*value - min) / range;
    float knobX = x + norm * w;

    // Mouse handling
    bool hovered = buttonHovered(pos, size);
    bool dragging = msLeftPressed || msLeftDown;

    bool &active = item.open;

    // Dragging starts only if the press began over the slider; otherwise dragging another field (e.g. dragFloat) over the slider would change it too.
    if(msLeftPressed && hovered && useInput) active = true;
    if(!dragging) active = false;

    if (active) {
        focusID=ID;
        float newNorm = ((float)ms.x - x) / w;
        newNorm = fminf(fmaxf(newNorm, 0.0f), 1.0f);
        *value = min + newNorm * range;
        proc=true;
    }

    // Track
    // bot->roundedFRect(x, y + h / 2 - 2, w, 4, 2, cColor(64, 64, 64));
    // bot->roundedFRect(x, y, w, h, 2, cColor(64, 64, 64));

    // track, filled part and handle use theme colors
    bot->roundedFRect(x, y + h / 2 - 2, w, 4, 2, withAlpha(colors[GUI_TEXT_COLOR], 40));
    bot->roundedFRect(x, y + h / 2 - 2, knobX - x, 4, 2, colors[GUI_BTN_TOGGLE_COLOR]);

    // Knob (tutacak)
    const bool hv = hoverVisual(hovered, active);   // no highlight while another widget is being dragged
    cColor knobClr = active ? colors[GUI_BTN_TOGGLE_COLOR] : hv ? colors[GUI_TEXT_HL_COLOR] : contrast(colors[GUI_BTN_HOVER_COLOR], 0.35f);
    knobClr.a = 255;
    bot->circle(vec2(knobX, y+h/2), knobRadius, knobClr, 1, colors[GUI_WIN_BORDERCLR]);

    // Value text
    bot->text(axFormat("%.2f", *value), x+leftMargin+w, y+h/2, fontSize, (hv || active)?colors[GUI_TEXT_HL_COLOR]:colors[GUI_TEXT_COLOR], BOT_ALIGN_LEFT | BOT_ALIGN_MIDDLE);

    item.set("slider", pos+winScroll, size, hovered, ID==focusID, 4);

    // if(msLeftReleased)  focusGUI=false;
    return *value;
}

bool axGUI::drawColorSlider(vec2 off, vec2 size, float *value, float vmax, cColor barColor)
{
    genID();
    vec2 pos = advanceCursor(size + off);
    if(outOfLayoutArea(pos, size + off)) return false;
    float height = size.y;
    float knobRadius = height * 0.5f; // Scale knob with height
    float norm = Clamp(*value / vmax, 0.0f, 1.0f); // Ensure 0-1 range
    float x = pos.x + off.x, y = pos.y + off.y;
    float w = size.x, h = size.y;
    float knobX = w * norm;

    ColorSliderState& state = colorSliderStates[ID];

    // Collision detection
    bool hovered = useInput && winHovered && rect(pos + winScroll + off, size).isPointInRec(ms) && winClip.isPointInRec(ms);

    bool dragging = msLeftDown;
    bool changed = false;

    if(hovered) {
        hoverID=ID;
        proc = true;
    }

    // Click to drag
    if(msLeftPressed && hovered)
    {
        state.isDragging = true;
        focusID = ID;
        float old = *value;
        *value = vmax * Clamp((ms.x - x) / w, 0.0f, 1.0f);
        changed = (*value != old);
        proc = true;
    }

    // Arrow key support (when focused)
    if(focusID == ID)
    {
        float step = vmax * 0.01f; // 1% steps
        if(IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) step *= 10.0f; // 10x faster
        if(IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) step *= 0.1f; // 10x slower

        float old = *value;
        if(IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_DOWN))
        {
            *value = Clamp(*value - step, 0.0f, vmax);
            changed = (*value != old);
            proc = true;
        }
        if(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_UP))
        {
            *value = Clamp(*value + step, 0.0f, vmax);
            changed = (*value != old);
            proc = true;
        }
    }

    // Mouse wheel support (when hovered)
    if(hovered && msWheel != 0)   // msWheel: keep the window from scrolling
    {
        float wheelStep = vmax * 0.01f * msWheel;
        msWheel = 0;
        if(IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) wheelStep *= 10.0f;
        if(IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) wheelStep *= 0.1f;

        float old = *value;
        *value = Clamp(*value + wheelStep, 0.0f, vmax);
        changed = (*value != old);
        proc = true;
    }

    // Handle dragging
    if(state.isDragging && dragging && focusID == ID)
    {
        float old = *value;
        *value = vmax * Clamp((ms.x - x) / w, 0.0f, 1.0f);
        changed = (*value != old);
        proc = true;
    }

    if(!dragging)
    {
        state.isDragging = false;
    }

    // Recalculate knobX after all modifications
    norm = Clamp(*value / vmax, 0.0f, 1.0f);
    knobX = w * norm;

    // Drawing
    // Background bar
    const bool hv = hoverVisual(hovered, state.isDragging);   // no highlight while another widget is being dragged
    cColor borderColor = state.isDragging ? colors[GUI_BTN_TOGGLE_COLOR] : hv ? contrast(colors[GUI_WIN_BORDERCLR], 0.3f) : colors[GUI_WIN_BORDERCLR];
    bot->roundedFRect(vec2(x, y), vec2(w, h), 6, withAlpha(colors[GUI_IDLE_COLOR], 255), 1, borderColor);

    // Colored fill bar (NOW USING barColor parameter!)
    if(knobX > 4)
    {
        float fillRadius = 6.0f;
        bot->roundedFRect(vec2(x, y), vec2(knobX, h), fillRadius, barColor, 0, cColor(0));
    }

    // Knob (handle)
    cColor knobColor = withAlpha(state.isDragging ? colors[GUI_TEXT_HL_COLOR] : hv ? contrast(colors[GUI_BTN_HOVER_COLOR], 0.5f) : colors[GUI_BTN_HOVER_COLOR], 255);
    cColor knobBorder = state.isDragging ? colors[GUI_BTN_TOGGLE_COLOR] : withAlpha(colors[GUI_TEXT_COLOR], 150);
    bot->circle(vec2(x+knobX, y+h/2), knobRadius, knobColor, 2, knobBorder);

    // Display value text when hovered
    if(hv || state.isDragging)
    {
        char valueText[32];
        snprintf(valueText, sizeof(valueText), "%.3f", *value);
        bot->text(valueText, x + w / 2, y + h + 5, fontSize - 2, colors[GUI_TEXT_COLOR], BOT_ALIGN_CENTER);
    }

    return changed;
}

// ---------------------------------------------------------------- color picker
// HSV square (saturation / brightness), hue and alpha bars, eyedropper (screen color), HEX field (#RRGGBB[AA], applied with
// Enter), alpha %, RGB fields and a preset palette. HSV is stored separately so hue is not lost at gray / black.
namespace {
struct PickerState {
    float h = 0, s = 0, v = 0;
    cColor last = cColor(-1, -1, -1, -1);
    int drag = 0;                 // 1: square, 2: hue, 3: alpha
    TextBoxState hex;
    bool hexInit = false;
};
PickerState g_pick;

void rgbToHsv(const cColor &c, float &h, float &s, float &v)
{
    float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f;
    float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b)), d = mx - mn;
    v = mx;
    s = mx > 0 ? d / mx : 0;
    if (d <= 1e-6f) return;   // gray: keep the hue
    float hh;
    if (mx == r) hh = (g - b) / d + (g < b ? 6 : 0);
    else if (mx == g) hh = (b - r) / d + 2;
    else hh = (r - g) / d + 4;
    h = hh / 6.0f;
}
cColor hsvToRgb(float h, float s, float v, int a)
{
    h = h - floorf(h);
    float i = floorf(h * 6), f = h * 6 - i, p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
    float r, g, b;
    switch (((int)i) % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return cColor((int)lroundf(r * 255), (int)lroundf(g * 255), (int)lroundf(b * 255), a);
}
std::string toHex(const cColor &c)
{
    char buf[16];
    if (c.a >= 255) snprintf(buf, sizeof buf, "#%02X%02X%02X", c.r, c.g, c.b);
    else snprintf(buf, sizeof buf, "#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
    return buf;
}
bool parseHex(std::string t, cColor &out)
{
    std::string h;
    for (char ch : t) if (isxdigit((unsigned char)ch)) h += ch; else if (ch != '#' && ch != ' ') return false;
    auto hx = [&](int i, int n) { return (int)strtol(h.substr(i, n).c_str(), nullptr, 16); };
    if (h.size() == 3) { out = cColor(hx(0, 1) * 17, hx(1, 1) * 17, hx(2, 1) * 17, out.a); return true; }
    if (h.size() == 6) { out = cColor(hx(0, 2), hx(2, 2), hx(4, 2), out.a); return true; }
    if (h.size() == 8) { out = cColor(hx(0, 2), hx(2, 2), hx(4, 2), hx(6, 2)); return true; }
    return false;
}
bool sameRgba(const cColor &a, const cColor &b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }
} // namespace

bool axGUI::drawColorPicker(cColor &clr, vec2 pos, vec2 size, bool &open)
{
    genID();
    if(pos==vec2(-1,-1)) pos = curPos;
    if(!open) { pickerEyedrop = false; return false; }
    PickerState &P = g_pick;
    bool res=false;

    // eyedropper: the click is captured in startFrame; the screen (layers drawn, picker not yet) is read
    if(pickerEyedropClick)
    {
        pickerEyedropClick = false;
        pickerEyedrop = false;
        Image img = LoadImageFromScreen();
        int px = (int)pickerEyedropPos.x, py = (int)pickerEyedropPos.y;
        if(img.data && px >= 0 && py >= 0 && px < img.width && py < img.height)
        {
            Color c = GetImageColor(img, px, py);
            clr = cColor(c.r, c.g, c.b, clr.a);
            res = true;
        }
        UnloadImage(img);
    }

    // update HSV if the color was changed externally
    if(!sameRgba(clr, P.last)) { rgbToHsv(clr, P.h, P.s, P.v); P.last = clr; }

    WindowState &win = beginWindow("ColorPicker", pos, size, WIN_POPUP|WIN_TITLE_BAR|WIN_TOPMOST);
    rect lay = win.layoutArea();
    const float W = lay.size.x;
    const float rowH = fmaxf(24.0f, roundf(fontSize * 1.6f));
    const cColor border = colors[GUI_WIN_BORDERCLR];

    // --- preset palette (two rows)
    {
        setCursor(cursorPos()+vec2(0,4));
        shapeButton(vec2(48, 48), "circle", vec2(40, 40), clr, 1, border);

        const float gw = size.x-48, gpad = 2, headerH = 0;    // group box: width, inner padding, title (0 if untitled)
        int n = 0; while(n < (int)pickerColors.size() && pickerColors[n].r != -1) n++;
        int perRow = (n + 1) / 2;
        float inner = gw - 2 * gpad;
        float sw = fminf(22.0f, floorf((inner - (perRow - 1) * itemSpaceX) / fmaxf(1, perRow)));
        float gh = headerH + 2 * gpad + 2 * sw + itemSpaceY;   // 2 rows
        
        if(beginGroupBox(NULL, vec2(gw, gh), false, 0, {0,false, cBlank, 0, cBlank, gpad}))   // inner padding is in the style
        {
            for(int i = 0; i < n; i++)
            {
                if(i == perRow) newline();
                cColor &c = pickerColors[i];
                if(shapeButton(vec2(sw, sw), "rect", vec2(sw - 0, sw - 0), c, 0.5, border)) { clr = cColor(c.r, c.g, c.b, clr.a); res = true; }
            }
        }
        endGroupBox();
        newline();
    }
    if(res) { rgbToHsv(clr, P.h, P.s, P.v); P.last = clr; }

    // --- saturation / brightness square
    const float svH = fminf(160.0f, W * 0.62f);
    vec2 sp = advanceCursor(vec2(W, svH));
    rect svR(sp + winScroll, vec2(W, svH));
    cColor hueC = hsvToRgb(P.h, 1, 1, 255);
    {
        Color hc = { (unsigned char)hueC.r, (unsigned char)hueC.g, (unsigned char)hueC.b, 255 };
        Rectangle r = { sp.x, sp.y, W, svH };
        bot->deferred([r, hc]() {
            DrawRectangleGradientH((int)r.x, (int)r.y, (int)r.width, (int)r.height, WHITE, hc);
            DrawRectangleGradientV((int)r.x, (int)r.y, (int)r.width, (int)r.height, Color{ 0, 0, 0, 0 }, BLACK);
        });
        bot->roundedRect(sp, vec2(W, svH), 0, 1, border);
    }
    newline(); spacerV(2);

    // --- hue bar
    const float barH = 12;
    const float kin = 8;   // bars are inset by the cursor radius (so the cursor does not overflow at the edge)
    const float BW = W - 2 * kin;
    vec2 hp = advanceCursor(vec2(W, barH)) + vec2(kin, 0);
    rect hueR(hp + winScroll - vec2(0, 4), vec2(BW, barH + 8));
    {
        Rectangle r = { hp.x, hp.y, BW, barH };
        bot->deferred([r]() {
            static const Color st[7] = { {255,0,0,255},{255,255,0,255},{0,255,0,255},{0,255,255,255},{0,0,255,255},{255,0,255,255},{255,0,0,255} };
            for(int k = 0; k < 6; k++) {
                float x0 = r.x + r.width * k / 6.0f, x1 = r.x + r.width * (k + 1) / 6.0f;
                DrawRectangleGradientH((int)x0, (int)r.y, (int)ceilf(x1) - (int)x0, (int)r.height, st[k], st[k + 1]);
            }
        });
        bot->roundedRect(hp, vec2(BW, barH), 0, 1, border);
    }
    newline(); spacerV(4);

    // --- alpha bar (checkerboard background)
    vec2 ap = advanceCursor(vec2(W, barH)) + vec2(kin, 0);
    rect alR(ap + winScroll - vec2(0, 4), vec2(BW, barH + 8));
    {
        Rectangle r = { ap.x, ap.y, BW, barH };
        Color c0 = { (unsigned char)clr.r, (unsigned char)clr.g, (unsigned char)clr.b, 0 }, c1 = c0; c1.a = 255;
        bot->deferred([r, c0, c1]() {
            const int cs = 6;
            for(int yy = 0; yy * cs < (int)r.height; yy++)
                for(int xx = 0; xx * cs < (int)r.width; xx++) {
                    int w = std::min(cs, (int)r.width - xx * cs), h = std::min(cs, (int)r.height - yy * cs);
                    DrawRectangle((int)r.x + xx * cs, (int)r.y + yy * cs, w, h, ((xx + yy) & 1) ? Color{ 200, 200, 200, 255 } : Color{ 255, 255, 255, 255 });
                }
            DrawRectangleGradientH((int)r.x, (int)r.y, (int)r.width, (int)r.height, c0, c1);
        });
        bot->roundedRect(ap, vec2(BW, barH), 0, 1, border);
    }
    newline(); spacerV(4);

    // --- dragging (square / hue / alpha)
    if(useInput && msLeftPressed && !pickerEyedrop)
    {
        if(svR.isPointInRec(ms)) P.drag = 1;
        else if(hueR.isPointInRec(ms)) P.drag = 2;
        else if(alR.isPointInRec(ms)) P.drag = 3;
    }
    if(!msLeftDown) P.drag = 0;
    if(P.drag)
    {
        proc = overGUI = true;
        if(P.drag == 1) {
            P.s = Clamp((ms.x - svR.pos.x) / svR.size.x, 0.0f, 1.0f);
            P.v = 1.0f - Clamp((ms.y - svR.pos.y) / svR.size.y, 0.0f, 1.0f);
        }
        else if(P.drag == 2) P.h = Clamp((ms.x - hueR.pos.x) / hueR.size.x, 0.0f, 0.9999f);
        if(P.drag == 3) clr.a = (int)lroundf(Clamp((ms.x - alR.pos.x) / alR.size.x, 0.0f, 1.0f) * 255);
        else clr = hsvToRgb(P.h, P.s, P.v, clr.a);
        P.last = clr;
        res = true;
        hueC = hsvToRgb(P.h, 1, 1, 255);
    }

    // --- cursors (ring: white + dark outline)
    auto knob = [&](vec2 c, cColor fill) {
        bot->circle(c, 7.5f, cColor(0, 0, 0, 90));
        bot->circle(c, 6.5f, fill, 2.0f, cWhite);
    };
    cColor solid = clr; solid.a = 255;
    knob(sp + vec2(P.s * W, (1 - P.v) * svH), solid);
    knob(hp + vec2(P.h * BW, barH * 0.5f), hueC);
    knob(ap + vec2(clr.a / 255.0f * BW, barH * 0.5f), clr);

    // --- eyedropper | HEX | alpha %
    {
        vec2 bp = advanceCursor(vec2(rowH, rowH));
        // eyedropper button
        bool edHover = rect(bp + winScroll, vec2(rowH, rowH)).isPointInRec(ms);
        cColor eb = pickerEyedrop ? colors[GUI_BTN_TOGGLE_COLOR] : (edHover ? colors[GUI_BTN_HOVER_COLOR] : colors[GUI_BTN_COLOR]);
        bot->roundedFRect(bp, vec2(rowH, rowH), btnRadius, eb);
        {
            vec2 c = bp + vec2(rowH, rowH) * 0.5f;
            float k = rowH * 0.22f;
            cColor ic = pickerEyedrop ? colors[GUI_TEXT_HL_COLOR] : colors[GUI_TEXT_COLOR];
            bot->drawLine(c + vec2(-k, k), c + vec2(k * 0.4f, -k * 0.4f), ic, 2.0f);              // body
            bot->circle(c + vec2(k * 0.65f, -k * 0.65f), k * 0.45f, ic);                           // tip
            bot->circle(c + vec2(-k, k), 1.2f, ic);
        }
        if(edHover && msLeftReleased && useInput) { pickerEyedrop = !pickerEyedrop; proc = true; }
        if(edHover) setHint(true, axTr("Pick a color from the screen", "Ekrandan renk al"), bp + vec2(rowH * 0.5f, 0));

        // HEX
        const float pctW = 64;
        float hexW = W - rowH - pctW - 2 * itemSpaceX;
        if(!P.hexInit) { P.hex.placeholder = "#RRGGBB"; P.hexInit = true; }
        if(!P.hex.focused) P.hex.text = toHex(clr);
        bool wasFocused = P.hex.focused;
        bool submit = textBox(P.hex, vec2(hexW, rowH));
        if(submit || (wasFocused && !P.hex.focused))
        {
            cColor c = clr;
            if(parseHex(P.hex.text, c)) { clr = c; rgbToHsv(clr, P.h, P.s, P.v); P.last = clr; res = true; }
            P.hex.text = toHex(clr);
        }

        // alpha %
        float pct = clr.a / 2.55f;
        if(dragFloat(vec2(pctW, rowH), 0, 100, &pct, 100, 0.5f, "%.0f %%")) { clr.a = (int)lroundf(pct * 2.55f); P.last = clr; res = true; }
        newline();
    }

    // --- RGB
    {
        cColor c = clr;
        if(dragColor(vec2(W, rowH), c, 1.0f, "%.0f")) { clr = cColor(c.r, c.g, c.b, clr.a); rgbToHsv(clr, P.h, P.s, P.v); P.last = clr; res = true; }
        newline();
    }

    if(pickerEyedrop) cursorRequest = MOUSE_CURSOR_CROSSHAIR;
    if(msLeftPressed && hoverWinID!=win.ID && !pickerEyedrop && !P.drag) open=false;
    endWindow("ColorPicker");
    return res;
}

bool axGUI::colorSelector(const char *text, bool &open, vec2 cpos, vec2 size, float itemH, cColor &clr)
{
    // Always reaches endWindow (keeps the bot->restore / window stack balanced so later windows are clipped correctly), honors `open`, and reflects dragColor changes in the return value.
    // The outside-click check uses the window area (layoutArea excludes the title / margins).
    genID();
    if(!open) return false;
    if(outOfLayoutArea(size)) { advanceCursor(size); return false; }
    float w=size.x, h=size.y;
    vec2 csize = vec2(w,h);
    bool res = false;
    WindowState &win = beginWindow("colorSelector",cpos, csize, WIN_POPUP|WIN_TOPMOST|WIN_AUTO_NEWLINE);
    if(text && text[0]) label(text,vec2(60,24));

    if(dragColor(vec2(140,24),clr,1,"%.0f",false,NULL)) res = true;
    newline();

    int i=0;
    while(pickerColors[i].r!=-1)
    {
        cColor &c=pickerColors[i];
        if(shapeButton(vec2(itemH,itemH),"circle",vec2(itemH,itemH),c,1,colors[GUI_WIN_BORDERCLR]))
        {
            clr = c;
            res = true;
        }
        i++;
    }
    if(msLeftPressed && !win.geometry().isPointInRec(ms)) open=false;
    endWindow("colorSelector");
    return res;
}

int axGUI::popupMenu(const char *text, PopupState &st, vec2 pos, vec2 size, float itemH, const std::vector<cString> &items)
{
    if(!st.open && !st.first) return -1;

    const float w = size.x;
    // LayoutStateGuard _guard(winflag, curPos, curPos0, layoutArea);
    genID();

    cString winId="popup_menu";
    static WinFlags flagStore = winflag;
    // WindowState &win = windowStates[text];
    // win.init=false;
    beginWindow(winId.c_str(), pos, size, WIN_AUTO_NEWLINE|WIN_POPUP);
    bool hovered = useInput && rect(pos, size).isPointInRec(ms) && winClip.isPointInRec(ms);
    if(hovered)  overGUI = true;

    if (text && text[0]) { label(text, vec2(60,24)); separator(4); }

    int sel = -1;
    for(int i = 0; i < (int)items.size(); ++i)
    {
        if(selectable(items[i].c_str(), false, true))
        {
            sel  = i;
            proc = true;
        }
    }

    // close on outside click or right click (after buttons so valid clicks aren't eaten)
    if(leftReleased || rightReleased)  proc = true;
    if((!st.first && msLeftPressed && !hovered) || sel!=-1 )
    {
        proc = true;
        st.open = false;
    }

    endWindow(winId.c_str());
    winflag = flagStore; // restore flags
    st.first=false;
    return sel;
}

void axGUI::printGUIStats(int showStates)
{
    float gScl=1.0f;
    if(showStates==0)
    {
        int cnt=0;
        text(axFormat("windowStates: %d",windowStates.size()), 14*gScl, colors[GUI_TEXT_COLOR]);
        newline();
        // if(scrollWinName)
        // {
        //     WindowState &w1 = windowStates[scrollWinName];
        //     text(axFormat("scrollWin:%s, %.0f,%.0f ",scrollWinName, w1.scroll.x, w1.scroll.y), 14*gScl, colors[GUI_TEXT_COLOR]);
        //     newline();
        // }
        text(axFormat("scrollWin:%s, %.0fd,%.0f focusWin:%s",scrollWinName, winScroll.x, winScroll.y, focusWinName), 14*gScl, colors[GUI_TEXT_COLOR]);
        newline();
        for(auto &it:windowStates)
        {
            cString name = it.first;
            WindowState &ws = it.second;
            text(axFormat("ID:%d, name:<%s>,  order:%d,  parent:%s",ws.ID, name.c_str(), ws.zOrder, ws.prWinName), 12*gScl, cColor(0));
        }
    }
    if(showStates==1)
    {
        text(axFormat("itemStates: %d",itemStates.size()), 14, colors[GUI_TEXT_COLOR]);
        newline();
        for (auto& [id, s] : itemStates) {
            bool valid = (s.frame==frameCnt);
            cColor c(0);
            // bot->roundedRect(s.pos, s.size, 0, 2, cColor(250,100,100,130));
            if(s.hovered) c = cColor(200,200,120);
            if(s.focused) c = cColor(100,200,120);
            text(axFormat("ID:%d, %s, hover:%d, focus:%d",id, s.name.c_str(), s.hovered, s.focused), 12*gScl, c);
        }
    }
    if(showStates==2)
    {
        text(axFormat("comboStates: %d",comboStates.size()), 14*gScl, colors[GUI_TEXT_COLOR]);
        newline();
        for (auto& [id, s] : comboStates) {
            bool valid = (s.frame==frameCnt);
            cColor c(0);
            if(s.hovered) c = cColor(200,200,120);
            if(s.focused) c = cColor(100,200,120);
            if(!valid) c = cColor(200,100,100,100);
            text(axFormat("ID:%d, open:%d, selIx:%d",id, s.open, s.selIx), 12*gScl, c);
            newline();
        }
    }
    if(showStates==3)
    {
        text(axFormat("animStates: %d",anims.size()), 14, colors[GUI_TEXT_COLOR]);
        newline();
        for (auto& [id, s] : anims) {
            bool valid = (s.frame==frameCnt);
            cColor c(0);
            text(axFormat("ID:%d",id), 12*gScl, c);
        }
    }
}

bool axGUI::beginTabBar(const char* strId)
{
    uint32_t id = hashTabId(strId);

    TabBarState& state = g_tabBarStates[id];
    // keep scroll state, reset only per-frame data
    state.tabs.clear();
    state.hoveredTabId = 0;
    state.totalTabWidth = 0;
    state.needsLayout = true;

    // Visible width = current layout area
    float availW = getContainerWidth();
    if (availW <= 0) availW = 400;  // fallback
    state.visibleWidth = availW;

    ActiveTabBar atb;
    atb.id              = id;
    atb.origin          = curPos;        // winScroll is already applied via translate
    atb.barHeight       = tabStyle.tabHeight;
    atb.contentY        = curPos.y + tabStyle.tabHeight + tabStyle.underlineHeight + 4.0f;
    atb.anyTabOpen      = false;
    atb.currentTabIndex = -1;
    atb.contentEndY     = atb.contentY;  // no content yet
    atb.savedCurPos     = curPos;
    tabBarStack.push_back(atb);

    return true;
}


bool axGUI::beginTabItem(const char* label, bool* pOpen)
{
    if (tabBarStack.empty()) return false;

    ActiveTabBar& atb   = tabBarStack.back();
    TabBarState&  state  = g_tabBarStates[atb.id];

    uint32_t tabId = combineHash(atb.id, hashTabId(axIdPart(label)));   // "Label###id"

    // --- Tab info ---
    TabItemInfo ti;
    ti.label    = label;
    ti.id       = tabId;
    ti.closable = (pOpen != nullptr);

    const std::string shown = axLabelPart(label);
    float textW = bot->textWidth(shown, fontSize);
    ti.width    = textW + tabStyle.tabPaddingH * 2.0f;
    if (ti.closable)
        ti.width += tabStyle.closeButtonSize + 6.0f;

    // X offset
    float offsetX = 24.0f;
    for (auto& t : state.tabs)
        offsetX += t.width + tabStyle.tabSpacing;
    ti.offsetX = offsetX;

    state.tabs.push_back(ti);
    state.totalTabWidth = offsetX + ti.width;
    int tabIndex = (int)state.tabs.size() - 1;

    // First frame: if there is no active tab, select the first one
    if (state.activeTabId == 0)
        state.activeTabId = tabId;

    bool isActive  = (state.activeTabId == tabId);
    bool isHovered = false;

    // --- Overflow computation ---
    bool hasOverflow = state.hasOverflow();
    float scrollOff  = state.scrollOffset;
    float leftMargin = hasOverflow ? TAB_ARROW_WIDTH : 0.0f;

    // --- Tab rectangle (scroll offset applied) ---
    float tabX = atb.origin.x + leftMargin + ti.offsetX - scrollOff;
    float tabY = atb.origin.y;
    float tabW = ti.width;
    float tabH = atb.barHeight;

    // Visible area bounds (skip drawing if outside the clip)
    float visibleLeft  = atb.origin.x + leftMargin;
    float visibleRight = atb.origin.x + state.visibleWidth - (hasOverflow ? TAB_ARROW_WIDTH : 0.0f);
    bool tabVisible = (tabX + tabW > visibleLeft) && (tabX < visibleRight);

    // --- Mouse hit-test (includes winScroll) ---
    // winScroll is applied via translate, so adjust ms by the scroll
    vec2 msLocal = ms - winScroll;

    if (tabVisible &&
        msLocal.x >= tabX && msLocal.x <= tabX + tabW &&
        msLocal.y >= tabY && msLocal.y <= tabY + tabH &&
        winClip.isPointInRec(ms))  // inside the scissor area
    {
        isHovered = true;
        state.hoveredTabId = tabId;
        hoverID = (int)tabId;
        if(msLeftDown) focusID = (int)tabId;
        overGUI = true;

        if (msLeftPressed && !state.dragging) {
            state.activeTabId = tabId;
            isActive = true;
            proc = true;
            axLog("TAB:%s, overflow:%d",label,state.hasOverflow());
            // Scroll the clicked tab into view
            if(1 || state.hasOverflow()) {
                float leftMargin  = TAB_ARROW_WIDTH;
                float rightMargin = TAB_ARROW_WIDTH;
                float viewW = state.visibleWidth - leftMargin - rightMargin;
                float tabLeft  = ti.offsetX - state.scrollOffset;
                float tabRight = tabLeft + ti.width;
                float maxScr = state.maxScroll();

                axLog("  TAB:%s, left:%.0f,  offsetX:%.0f",label,tabLeft,ti.offsetX);

                if(tabLeft < 24) { state.scrollTarget = fmax(0.0f, ti.offsetX); axLog("111"); }
                else if (tabRight > viewW) { state.scrollTarget = fmin(maxScr, ti.offsetX + ti.width - viewW)+48; axLog("222"); }
            }
        }
    }

    // --- Drawing (visible tabs only) ---
    if(tabVisible)
    {
        cColor bgColor = isActive  ? tabStyle.tabActive
                         : isHovered ? tabStyle.tabHovered
                                     : tabStyle.tabNormal;

        bot->roundedFRect(tabX, tabY, tabW, tabH, tabStyle.tabBorderRadius, bgColor);

        // Label
        cColor textColor = isActive ? tabStyle.tabTextActive : tabStyle.tabTextNormal;
        float textX = tabX + tabStyle.tabPaddingH;
        float textY = tabY + (tabH - fontSize) * 0.5f;
        bot->text(shown, textX, textY, fontSize, textColor);

        // Close butonu
        if (ti.closable && pOpen) {
            float cSize = tabStyle.closeButtonSize;
            float cx = tabX + tabW - tabStyle.tabPaddingH - cSize;
            float cy = tabY + (tabH - cSize) * 0.5f;

            bool closeHovered = (msLocal.x >= cx && msLocal.x <= cx + cSize &&
                                 msLocal.y >= cy && msLocal.y <= cy + cSize &&
                                 winClip.isPointInRec(ms));

            if (closeHovered) {
                bot->circle(vec2(cx + cSize*0.5f, cy + cSize*0.5f), cSize*0.6f, tabStyle.closeHovered);
                overGUI = true;
                if (msLeftPressed) {
                    *pOpen = false;
                    proc = true;
                }
            }

            cColor xColor = closeHovered ? tabStyle.tabTextActive : tabStyle.tabTextNormal;
            float m = 3.0f;
            bot->drawLine(cx + m, cy + m, cx + cSize - m, cy + cSize - m, xColor, 1.5f);
            bot->drawLine(cx + cSize - m, cy + m, cx + m, cy + cSize - m, xColor, 1.5f);
        }

        // Active tab underline
        if (isActive) {
            bot->roundedFRect(tabX, tabY + tabH, tabW, tabStyle.underlineHeight, 0, tabStyle.underlineColor);
        }
    }
    // bot->roundedRect(tabX, tabY, tabW, tabH, 3, 1, cRed);
    // --- Content area ---
    if (isActive) {
        atb.anyTabOpen      = true;
        atb.currentTabIndex = tabIndex;

        // Move the cursor to the content area
        curPos  = vec2(atb.origin.x, atb.contentY);
        lineHMax = 0;
    }

    return isActive;
}


void axGUI::endTabItem()
{
    if (tabBarStack.empty()) return;

    ActiveTabBar& atb = tabBarStack.back();

    // Track how far down the content extends
    float currentBottom = curPos.y + lineHMax;
    if (currentBottom > atb.contentEndY)
        atb.contentEndY = currentBottom;
}


void axGUI::endTabBar()
{
    if (tabBarStack.empty()) return;

    ActiveTabBar& atb   = tabBarStack.back();
    TabBarState&  state  = g_tabBarStates[atb.id];

    bool hasOverflow = state.hasOverflow();

    // --- Overflow: arrow buttons and wheel scroll ---
    if (hasOverflow)
    {
        float barY = atb.origin.y;
        float barH = atb.barHeight;
        vec2 msLocal = ms - winScroll;
        float maxScr = state.maxScroll();

        // Left arrow button
        float leftX = atb.origin.x;
        rect leftArrow(leftX, barY, TAB_ARROW_WIDTH, barH);
        bool leftEnabled = (state.scrollOffset > 0.1f);

        // Background
        cColor leftBg = tabStyle.barBackground;
        bool leftHov = leftArrow.isPointInRec(msLocal) && winClip.isPointInRec(ms);
        if (leftHov && leftEnabled) {
            leftBg = tabStyle.tabHovered;
            overGUI = true;
            hoverID = (int)(atb.id + 1);
            if (msLeftPressed) {
                state.scrollTarget = fmax(0.0f, state.scrollOffset - 80.0f);
                proc = true;
            }
        }

        bot->roundedFRect(leftArrow, 0, leftBg);

        // Draw left arrow  <
        {
            cColor arClr = leftEnabled ? tabStyle.tabTextActive : withAlpha(tabStyle.tabTextNormal, 70);
            float cx = leftX + TAB_ARROW_WIDTH * 0.5f;
            float cy = barY + barH * 0.5f;
            float as = 5.0f;  // arrow size
            bot->drawLine(cx + as*0.3f, cy - as, cx - as*0.5f, cy, arClr, 2.0f);
            bot->drawLine(cx - as*0.5f, cy, cx + as*0.3f, cy + as, arClr, 2.0f);
        }

        // Right arrow button
        float rightX = atb.origin.x + state.visibleWidth - TAB_ARROW_WIDTH;
        rect rightArrow(rightX, barY, TAB_ARROW_WIDTH, barH);
        bool rightEnabled = (state.scrollOffset < maxScr - 0.1f);

        cColor rightBg = tabStyle.barBackground;
        bool rightHov = rightArrow.isPointInRec(msLocal) && winClip.isPointInRec(ms);
        if (rightHov && rightEnabled) {
            rightBg = tabStyle.tabHovered;
            overGUI = true;
            hoverID = (int)(atb.id + 2);
            if (msLeftPressed) {
                state.scrollTarget = fmin(maxScr, state.scrollOffset + 80.0f);
                proc = true;
            }
        }
        bot->roundedFRect(rightArrow, 0, rightBg);

        // Draw right arrow  >
        {
            cColor arClr = rightEnabled ? tabStyle.tabTextActive : withAlpha(tabStyle.tabTextNormal, 70);
            float cx = rightX + TAB_ARROW_WIDTH * 0.5f;
            float cy = barY + barH * 0.5f;
            float as = 5.0f;
            bot->drawLine(cx - as*0.3f, cy - as, cx + as*0.5f, cy, arClr, 2.0f);
            bot->drawLine(cx + as*0.5f, cy, cx - as*0.3f, cy + as, arClr, 2.0f);
        }

        // Mouse wheel: add to velocity
        rect barArea(atb.origin.x, barY, state.visibleWidth, barH);
        vec2 msLocalBar = ms - winScroll;
        bool barHovered = barArea.isPointInRec(msLocalBar) && winClip.isPointInRec(ms);

        if(leftHov || rightHov)  barHovered=false;

        if(barHovered && msWheel != 0 && !scrollWinName)
        {
            state.scrollVelocity -= msWheel * 12.0f;
            msWheel = 0;   // keep the window from scrolling
            overGUI = true;
            proc = true;
        }

        // Left mouse drag: scroll by dragging on the bar
        // msDragging = true once the 10px threshold is exceeded (from the GUI system)
        if (barHovered && msDragging && msLeftDown && !state.dragging)
        {
            state.dragging = true;
            state.dragStartScroll = state.scrollOffset;
            state.dragPrevMs  = ms.x;
            state.scrollVelocity = 0;
            overGUI = true;
            proc = true;

        }

        if(state.dragging)
        {
            if (msLeftDown)
            {
                float dx = ms.x - state.dragPrevMs;
                state.scrollOffset -= dx;
                state.scrollOffset = Clamp(state.scrollOffset, 0.0f, maxScr);
                state.scrollVelocity = -dx;
                state.dragPrevMs = ms.x;
                state.scrollTarget = state.scrollOffset;
                overGUI = true;
                proc = true;
            }
            if (msLeftReleased)
            {
                state.dragging = false;
                // momentum continues, velocity comes from the last dx
            }
        }

        // Velocity-based smooth scroll (drag momentum + wheel momentum + arrow buttons)
        if(!state.dragging)
        {
            // Arrow buttons use scrollTarget
            float targetDiff = state.scrollTarget - state.scrollOffset;
            if (fabs(targetDiff) > 0.5f)
                state.scrollVelocity += targetDiff * 0.015f;

            // Apply velocity
            state.scrollOffset += state.scrollVelocity;
            // Friction
            state.scrollVelocity *= 0.85f;
            // Dur
            if (fabs(state.scrollVelocity) < 0.1f)
                state.scrollVelocity = 0;
        }

        // Clamp and sync target
        state.scrollOffset = Clamp(state.scrollOffset, 0.0f, maxScr);
        state.scrollTarget = Clamp(state.scrollTarget, 0.0f, maxScr);

        // Zero the velocity when hitting the limits
        if (state.scrollOffset <= 0 || state.scrollOffset >= maxScr)
            state.scrollVelocity = 0;
    }

    // Bottom line (separator across the whole bar)
    float barWidth = state.visibleWidth > 0 ? state.visibleWidth : getContainerWidth();
    float sepY = atb.origin.y + atb.barHeight;
    bot->drawLine(atb.origin.x, sepY, atb.origin.x + barWidth, sepY, tabStyle.separatorColor, 1.0f);

    // --- Move the cursor past the content ---
    float finalY = fmax(atb.contentEndY, atb.contentY);
    curPos  = vec2(atb.savedCurPos.x, finalY + itemSpaceY);
    lineHMax = 0;

    // Update window state (parent window cursor tracking)
    if (prWinName) {
        WindowState& win = windowStates[prWinName];
        win.curPos  = curPos;
    }

    tabBarStack.pop_back();
}
