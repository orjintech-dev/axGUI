// axGUITable.h - axGUI table widget (immediate mode, data supplied by the caller)
//
// Usage:
//   static TableState ts;                       // persistent state held by the caller (like TextBoxState)
//   std::vector<TableColumn> cols = { {"Ad", 0}, {"Adet", 80, BOT_ALIGN_RIGHT}, {"Fiyat", 100, BOT_ALIGN_RIGHT} };
//   if (gui.table(ts, vec2(0, 300), cols, (int)rows.size(),
//                 [&](int r, int c) { return rows[r][c]; })) {
//       if (ts.activatedRow >= 0) open(ts.activatedRow);   // double click / Enter
//   }
//
// - Row indices are always DATA indices (sorting only changes the display order).
// - Call ts.invalidate() when data changes (a changed row count is detected automatically).
// - Only visible rows are drawn: fast with thousands of rows.
// - Mouse: click header = sort, drag header edge = column width, click = select, double click = activate,
//   right click = rightClickedRow, wheel = scroll (Shift: horizontal), scrollbar is draggable.
// - Touch: drag = scroll (inertia), touch released without scrolling = select, double tap = activate.
// - Keyboard (table focused): up/down, PageUp/PageDown, Home/End, Enter; Shift / Ctrl for multi-select, Ctrl+A.
#ifndef AXGUITABLE_H
#define AXGUITABLE_H

#include <string>
#include <vector>
#include <functional>
#include "axDefs.h"

enum TableFlags {
    TBL_HEADER      = 1,    // header row
    TBL_SORTABLE    = 2,    // click header to sort (can be disabled per column)
    TBL_RESIZABLE   = 4,    // column width via header edge
    TBL_STRIPED     = 8,    // zebra rows
    TBL_GRID        = 16,   // vertical cell lines
    TBL_MULTISELECT = 32,   // multi-select with Ctrl / Shift
    TBL_NOSELECT    = 64,   // no selection (display only)
    TBL_DEFAULT     = TBL_HEADER | TBL_SORTABLE | TBL_RESIZABLE | TBL_STRIPED
};

struct TableColumn
{
    std::string title;
    float width = 0;            // 0: shares the remaining width with other 0 columns
    int align = 0;              // BOT_ALIGN_LEFT / CENTER / RIGHT (horizontal part)
    float minWidth = 40;
    bool sortable = true;
    TableColumn() {}
    TableColumn(const std::string &t, float w = 0, int a = 0, float minW = 40, bool s = true)
        : title(t), width(w), align(a), minWidth(minW), sortable(s) {}
};

struct TableStyle
{
    float rowH = 0;             // 0: font size * 1.9
    float headerH = 0;          // 0: rowH
    float fontSize = 0;         // 0: gui.fontSize
    float padX = 8;
    float radius = 6;
    float scrollbarW = 6;
    // colors: taken from the theme (gui.colors) if a == 0
    cColor bg{ 0, 0, 0, 0 }, border{ 0, 0, 0, 0 }, header{ 0, 0, 0, 0 }, headerText{ 0, 0, 0, 0 };
    cColor text{ 0, 0, 0, 0 }, stripe{ 0, 0, 0, 0 }, hover{ 0, 0, 0, 0 }, selected{ 0, 0, 0, 0 }, grid{ 0, 0, 0, 0 };
    const char *emptyText = nullptr;   // centered when there are no rows
};

struct TableState
{
    // --- persistent (managed by the widget; caller may also set)
    std::vector<float> widths;   // user-set widths (0: the column's own value)
    int sortCol = -1;            // -1: data order
    bool sortAsc = true;
    int selRow = -1;             // selected data row (multi-select: last clicked)
    std::vector<int> selected;   // selected data rows in multi-select ({selRow} for single selection)
    vec2 scroll = vec2(0, 0);    // scroll offset (px, positive)
    SmoothScroll wheelX, wheelY; // wheel scrolling (gui.wheelSmooth)

    // --- rewritten every frame (events)
    int clickedRow = -1;         // clicked this frame
    int activatedRow = -1;       // double click / Enter
    int rightClickedRow = -1;    // right click / long press: right click (position: gui.ms)
    bool selectionChanged = false;
    bool sortChanged = false;
    int hoverRow = -1, hoverCol = -1;

    void invalidate() { orderDirty = true; }   // data changed: re-sort
    bool isSelected(int row) const;
    void select(int row);                      // single selection (and scroll into view)
    void clearSelection() { selRow = -1; selected.clear(); }

    // --- internal state
    std::vector<int> order;      // display order -> data row
    bool orderDirty = true;
    int lastRows = -1, lastSortCol = -2;
    bool lastSortAsc = true;
    bool pressed = false, dragScroll = false, thumbDragV = false, thumbDragH = false;
    int pressZone = 0, pressRow = -1, pressCol = -1, resizeCol = -1;
    float resizeStartW = 0, thumbGrab = 0;
    vec2 speed = vec2(0, 0);
    double lastClickTime = 0;
    int lastClickRow = -1, anchorDisp = -1;
    int ensureVisibleRow = -1;   // requested by select(): scroll into view on next draw
};

using TableCellFn = std::function<std::string(int row, int col)>;
// Custom cell drawing: if it returns true, the default text is skipped. r: cell rect on screen (draw coordinates).
using TableDrawFn = std::function<bool(int row, int col, const rect &r, bool selected)>;

#endif // AXGUITABLE_H
