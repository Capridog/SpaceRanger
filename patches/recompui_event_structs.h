#ifndef TS2_RECOMPUI_EVENT_STRUCTS_H
#define TS2_RECOMPUI_EVENT_STRUCTS_H

/*
 * TS2 OldNative V87 reconstructed RecompFrontend guest event ABI.
 * Enum values are generated from the pinned local RecompFrontend source.
 * The packet layout follows the canonical N64Recomp/Zelda64Recomp
 * recompui event ABI, extended for the pinned MenuAction payload.
 */

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    UI_EVENT_NONE = 0,
    UI_EVENT_CLICK = 1,
    UI_EVENT_FOCUS = 2,
    UI_EVENT_HOVER = 3,
    UI_EVENT_ENABLE = 4,
    UI_EVENT_DRAG = 5,
    UI_EVENT_TEXT = 6,
    UI_EVENT_UPDATE = 7,
    UI_EVENT_NAVIGATE = 8,
    UI_EVENT_MOUSE_BUTTON = 9,
    UI_EVENT_MENU_ACTION = 10,
    UI_EVENT_COUNT = 11,
} RecompuiEventType;

typedef enum {
    UI_DRAG_NONE = 0,
    UI_DRAG_START = 1,
    UI_DRAG_MOVE = 2,
    UI_DRAG_END = 3,
} RecompuiDragPhase;

typedef enum {
    UI_MENU_ACTION_NONE = 0,
    UI_MENU_ACTION_ACCEPT = 1,
    UI_MENU_ACTION_APPLY = 2,
    UI_MENU_ACTION_BACK = 3,
    UI_MENU_ACTION_TOGGLE = 4,
    UI_MENU_ACTION_TAB_LEFT = 5,
    UI_MENU_ACTION_TAB_RIGHT = 6,
} RecompuiMenuAction;

typedef struct {
    RecompuiEventType type;
    union {
        struct {
            float x;
            float y;
        } click;
        struct {
            bool active;
        } focus;
        struct {
            bool active;
        } hover;
        struct {
            bool active;
        } enable;
        struct {
            float x;
            float y;
            RecompuiDragPhase phase;
        } drag;
        struct {
            RecompuiMenuAction action;
        } menu_action;
    } data;
} RecompuiEventData;

#ifdef __cplusplus
#include <cstddef>
static_assert(sizeof(bool) == 1);
static_assert(sizeof(RecompuiEventType) == 4);
static_assert(sizeof(RecompuiDragPhase) == 4);
static_assert(sizeof(RecompuiMenuAction) == 4);
static_assert(offsetof(RecompuiEventData, type) == 0);
static_assert(offsetof(RecompuiEventData, data) == 4);
static_assert(offsetof(RecompuiEventData, data.click.x) == 4);
static_assert(offsetof(RecompuiEventData, data.click.y) == 8);
static_assert(offsetof(RecompuiEventData, data.drag.x) == 4);
static_assert(offsetof(RecompuiEventData, data.drag.y) == 8);
static_assert(offsetof(RecompuiEventData, data.drag.phase) == 12);
static_assert(offsetof(RecompuiEventData, data.menu_action.action) == 4);
static_assert(sizeof(RecompuiEventData) == 16);
static_assert(static_cast<int>(recompui::EventType::None) == UI_EVENT_NONE);
static_assert(static_cast<int>(recompui::EventType::Click) == UI_EVENT_CLICK);
static_assert(static_cast<int>(recompui::EventType::Focus) == UI_EVENT_FOCUS);
static_assert(static_cast<int>(recompui::EventType::Hover) == UI_EVENT_HOVER);
static_assert(static_cast<int>(recompui::EventType::Enable) == UI_EVENT_ENABLE);
static_assert(static_cast<int>(recompui::EventType::Drag) == UI_EVENT_DRAG);
static_assert(static_cast<int>(recompui::EventType::Text) == UI_EVENT_TEXT);
static_assert(static_cast<int>(recompui::EventType::Update) == UI_EVENT_UPDATE);
static_assert(static_cast<int>(recompui::EventType::Navigate) == UI_EVENT_NAVIGATE);
static_assert(static_cast<int>(recompui::EventType::MouseButton) == UI_EVENT_MOUSE_BUTTON);
static_assert(static_cast<int>(recompui::EventType::MenuAction) == UI_EVENT_MENU_ACTION);
static_assert(static_cast<int>(recompui::EventType::Count) == UI_EVENT_COUNT);
static_assert(static_cast<int>(recompui::DragPhase::None) == UI_DRAG_NONE);
static_assert(static_cast<int>(recompui::DragPhase::Start) == UI_DRAG_START);
static_assert(static_cast<int>(recompui::DragPhase::Move) == UI_DRAG_MOVE);
static_assert(static_cast<int>(recompui::DragPhase::End) == UI_DRAG_END);
static_assert(static_cast<int>(recompui::MenuAction::None) == UI_MENU_ACTION_NONE);
static_assert(static_cast<int>(recompui::MenuAction::Accept) == UI_MENU_ACTION_ACCEPT);
static_assert(static_cast<int>(recompui::MenuAction::Apply) == UI_MENU_ACTION_APPLY);
static_assert(static_cast<int>(recompui::MenuAction::Back) == UI_MENU_ACTION_BACK);
static_assert(static_cast<int>(recompui::MenuAction::Toggle) == UI_MENU_ACTION_TOGGLE);
static_assert(static_cast<int>(recompui::MenuAction::TabLeft) == UI_MENU_ACTION_TAB_LEFT);
static_assert(static_cast<int>(recompui::MenuAction::TabRight) == UI_MENU_ACTION_TAB_RIGHT);
#endif

#endif
