/*
 * uilib.h - User Interface Library
 *
 * v1.0.8 / 2022-01-29 / (c) Io Engineering / Terje
 *
 */

/*
 * Copyright (c) 2015-2022, Io Engineering
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * * Neither the name of the copyright holder nor the names of its contributors may
 *   be used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __UILIB__H_
#define __UILIB__H_

#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "../LCD/graphics.h"
#include "../LCD/colorRGB.h"

#define font_23x16 (Font*)font_23x16_data
extern const uint8_t font_23x16_data[];

#ifdef _REMOTE_
#include "rc5recv.h"
#endif

#ifdef _TOUCH_
#include "LCD/Touch/touch.h"
#endif

#ifdef _KEYPAD_
#include "../keypad.h"
#endif

#define WIDGET_MSG_PTR_DOWN   0x00000002
#define WIDGET_MSG_PTR_MOVE   0x00000003
#define WIDGET_MSG_PTR_UP     0x00000004
#define WIDGET_MSG_KEY_UP     0x00000005
#define WIDGET_MSG_KEY_DOWN   0x00000006
#define WIDGET_MSG_KEY_LEFT   0x00000007
#define WIDGET_MSG_KEY_RIGHT  0x00000008
#define WIDGET_MSG_KEY_SELECT 0x00000009

#define RCDATA ((remote_cmd_t *)event->data)

typedef enum {
    DataTypeString          = 0x00,
    DataTypeUnsignedInteger = 0x01,
    DataTypeInteger         = 0x02,
    DataTypeFloat           = 0x04,
    DataTypeLogical         = 0x08
} DataType;

typedef enum {
    WidgetCanvas      = 0x00,
    WidgetButton      = 0x01,
    WidgetList        = 0x02,
    WidgetListElement = 0x04,
    WidgetFrame       = 0x08,
    WidgetImage       = 0x10,
    WidgetLabel       = 0x20,
    WidgetTextBox     = 0x40,
    WidgetCheckBox    = 0x80,
    WidgetAll         = 0xFF
} WidgetType;

typedef enum {
    EventNullEvent = 0,
    EventPointerDown = 100,
    EventPointerUp,
    EventPrePointerChange,
    EventPointerChanged,
    EventPointerEnter,
    EventPointerLeave,
    EventListOffStart,
    EventListOffEnd,
    EventWidgetPainted,
    EventWidgetClose,
    EventWidgetClosed,
    EventKeyDown,
    EventKeyUp,
    EventValidate,
    EventRemote,
    EvenUserDefined
} EventReason;

typedef union {
    uint32_t value;
    struct {
        uint32_t selected    :1,
                 highlighted :1,
                 noBox       :1,
                 hidden      :1,
                 visible     :1,
                 disabled    :1,
                 opaque      :1,
                 allocated   :1,
                 alignment   :2;
    };
} WidgetFlags;

typedef struct {
    int16_t x;
    int16_t y;
} position_t;

typedef struct Event {
    EventReason reason;
    uint16_t x;
    uint16_t y;
    void *data;
    bool claimed;
} Event;

typedef struct Widget {
    WidgetType type;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint16_t xMax;
    uint16_t yMax;
    RGBColor_t fgColor;
    RGBColor_t bgColor;
    RGBColor_t disabledColor;
    WidgetFlags flags;
    void *privateData;
    void (*eventHandler)(struct Widget *self, Event *event);
    struct Widget *parent;
    struct Widget *firstChild;
    struct Widget *lastChild;
    struct Widget *prevSibling;
    struct Widget *nextSibling;
} Widget;

typedef struct Canvas {
    Widget widget;
    int8_t tabNav;
//  RGBColor_t bgColor;
} Canvas;

typedef struct List {
    Widget widget;
    Widget *currentElement;
    uint8_t group;
} List;

typedef struct Button {
    Widget widget;
    RGBColor_t hltColor;
    RGBColor_t movColor;
    RGBColor_t curColor;
    char *label;
    Font *font;
    uint16_t labelx;
    uint16_t labely;
    uint8_t group;
} Button;

typedef struct {
    Widget widget;
    void *image;
} Image;

typedef struct {
    Widget widget;
    Font *font;
    char *string;
} Label;

typedef struct {
    Widget widget;
    Font *font;
    char *label;
    bool *value;
    bool checked;
} CheckBox;

typedef struct {
    Widget widget;
    Font *font;
    RGBColor_t borderColor;
    char *string;
    void *value;
    uint8_t maxLength;
    DataType dataType;
    char *format;
} TextBox;

typedef struct {
    TextBox *textbox;
    uint8_t cpos;
    uint8_t xpos;
    bool insert;
    uint16_t state;
} Caret;

typedef struct {
    Caret *caret;
    char key;
    bool down;
} keypress_t;

typedef Widget Frame;
typedef Button ListElement;

extern const position_t currPos;

bool UILibInit (void);
void UILibSetTabnav (uint8_t delta);

Canvas *UILibCanvasCreate (uint16_t x, uint16_t y, uint16_t width, uint16_t height, void (*eventHandler)(Widget *self, Event *event));
void UILibCanvasDisplay (Canvas *canvas);
Canvas *UILibCanvasGetCurrent (void);

Frame *UILibFrameCreate (Widget *parent, uint16_t x, uint16_t y, uint16_t width, uint16_t height, void (*eventHandler)(Widget *self, Event *event));
void UILibFrameClear (Frame *frame);

Image *UILibImageCreate (Widget *parent, uint16_t x, uint16_t y, uint16_t width, uint16_t height, void *pImage, void (*eventHandler)(Widget *self, Event *event));

List *UILibListCreate (Widget *parent, uint16_t x, uint16_t y, uint16_t width, uint16_t rows, void (*eventHandler)(Widget *self, Event *event));
ListElement *UILibListCreateElement (List *list, uint16_t row, const char *label, void (*eventHandler)(Widget *self, Event *event));

Button *UILibButtonCreate (Widget *parent, uint16_t x, uint16_t y, const char *label, void (*eventHandler)(Widget *self, Event *event));
Button *UILibButtonGetSelected (uint32_t group);
Font *UILibButtonSetDefaultFont (Font *font);
void UILibButtonFlash (Button *button);
void UILibButtonSetLabel (Button *button, const char *label);

Label *UILibLabelCreate(Widget *parent, Font *font, RGBColor_t fgColor, uint16_t x, uint16_t y, uint16_t width, void (*eventHandler)(Widget *self, Event *event));
void UILibLabelClear (Label *textbox);
bool UILibLabelDisplay (Label *textbox, const char *string);

CheckBox *UILibCheckBoxCreate(Widget *parent, RGBColor_t fgColor, uint16_t x, uint16_t y, char *label, bool *value, void (*eventHandler)(Widget *self, Event *event));
void UILibCheckBoxDisplay (CheckBox *checkbox, const bool checked);

TextBox *UILibTextBoxCreate(Widget *parent, Font *font, RGBColor_t fgColor, uint16_t x, uint16_t y, uint16_t width, void (*eventHandler)(Widget *self, Event *event));
void UILibTextBoxClear (TextBox *textbox);
bool UILibTextBoxDisplay (TextBox *textbox);
bool UILibTextBoxBindValue(TextBox *textbox, void *value, DataType dataType, char *format, const uint8_t maxLength);

void UILibWidgetDisplay (Widget *widget);
bool UILibWidgetHide (Widget *widget, bool hidden);
void UILibWidgetEnable (Widget *widget, bool enable);
void UILibWidgetSelect (Widget *widget, uint32_t group);
void UILibWidgetDeselect (Widget *widget);
void UILibApplyEnter (Widget *widget);
Widget *UILibWidgetSetWidth (Widget *widget, uint16_t width);
Widget *UILibWidgetSetHeight (Widget *widget, uint16_t height);
void UILibSetEventHandler (Widget *widget, void (*eventHandler)(Widget *self, Event *event));

bool UILib_ValidateKeypress (TextBox *textbox, char c);
void UILibProcessEvents (void);
void UILibClaimInputDevice (void);
bool UILibPublishEvent (Widget *widget, EventReason reason, position_t pos, bool propagate, void *eventdata);

typedef int32_t (*on_navigator_event_ptr)(uint32_t ulMessage, int32_t lX, int32_t lY);

extern bool NavigatorInit (uint32_t xSize, uint32_t ySize);
extern bool NavigatorSetPosition (uint32_t xPos, uint32_t yPos, bool callback);
extern uint32_t NavigatorGetYPosition (void);
extern void NavigatorSetEventHandler (on_navigator_event_ptr on_navigator_event);

#endif
