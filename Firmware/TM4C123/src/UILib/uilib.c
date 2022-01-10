/*
 * uilib.c - User Interface Library
 *
 * v1.0.7 / 2021-03-03/ (c) Io Engineering / Terje
 *
 */

 /* Copyright (c) 2015-2021, Io Engineering
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "uilib.h"

#include "../LCD/graphics.h"
#include "../LCD/colorRGB.h"
#include "../fonts/font_23x16.h"

#define POINTER_CHANGED  0x01
#define POINTER_MOVED    0x02
#define POINTER_SCAN     0x10
#define REMOTE_COMMAND   0x04
#define KEY_CHANGED      0x08
#define TOUCH_PENDING    0x20

typedef struct {
    Canvas *canvas;
    Widget *widget;
    uint32_t x;
    uint32_t y;
} current_t;

typedef struct {
    uint32_t type;
    position_t pos;
    uint8_t tabNav;
} raw_event_t;

typedef struct event_element_t {
    struct event_element_t *next;
    raw_event_t event;
    keypress_t keypress;
#ifdef _REMOTE_
    remote_cmd_t remote;
#endif
} event_element_t;

static char const *const data_format[] = {
    "%s",
    "%ld",
    "%ld",
    "%.2f",
    "%d"
};

static bool hasNavigator;

static volatile bool spin_lock = false;
static uint8_t buttonHeight = 22, listGroup = 255;
static Font *buttonFont = font_23x16;
static volatile uint32_t pointerEvent = 0, systicks = 0, systickLimit = 10;

static volatile bool pointerDown = false;
static Caret caret = {
    .textbox = NULL,
    .cpos    = 0,
    .xpos    = 0,
    .state   = 0,
    .insert  = true
};
static current_t current = {
    .canvas = NULL,
    .widget = NULL,
    .x = 0,
    .y = 0
};

const position_t currPos = {
    .x = -1,
    .y = -1
};

static event_element_t events[16];
static volatile event_element_t *event_tail, *event_head;

static uint8_t tabNav = 0;
static Widget *widgetGetNext (Widget * widget, uint8_t type, bool all);
static Widget *widgetGetPrev (Widget * widget, uint8_t type, bool all);

bool UILibInit (void)
{
    uint32_t i = (sizeof(events) / sizeof(event_element_t)) - 1;

    event_head = event_tail = &events[0];

    events[i].next = &events[0];

    do {
        i--;
        events[i].next = &events[i + 1];
    } while(i);

    lcd_display_t *screen = getDisplayDescriptor();

    hasNavigator = NavigatorInit(screen->Width, screen->Height);

    return true;
}

// private functions

static Widget *widgetCreate (Widget *parent, WidgetType type, size_t size, uint16_t x, uint16_t y, uint16_t width, uint16_t height, void (*eventHandler)(Widget *self, Event *event))
{
    Widget *widget = malloc(size);

    if(widget) {

        memset(widget, 0, size);

        widget->type   = type;
        widget->parent = parent;
        widget->x      = x;
        widget->y      = y;
        widget->width  = width;
        widget->height = height;

        widget->x     = x;
        widget->xMax  = x + width - 1;
        widget->y     = y;
        widget->yMax  = y + height - 1;
        widget->disabledColor = DarkGray;
        widget->eventHandler = eventHandler;

        if(parent) {

            widget->bgColor = parent->bgColor;
            widget->fgColor = parent->fgColor;

            widget->prevSibling = parent->lastChild;

            if(parent->lastChild)
                parent->lastChild->nextSibling = widget;

            if(!parent->firstChild)
                parent->firstChild = widget;

            parent->lastChild = widget;

        }
    }

    return widget;
}

inline uint8_t mixColor (uint8_t fg, uint8_t bg, uint8_t alpha)
{
    return ((fg * alpha) + (bg * (255  - alpha))) >> 8;
}

RGBColor_t mixColors (RGBColor_t fg, RGBColor_t bg, float alpha)
{
    RGBColor_t res;

    res.A = (uint8_t)(255.0f * alpha);
    res.R = mixColor(fg.R, bg.R, res.A);
    res.G = mixColor(fg.G, bg.G, res.A);
    res.B = mixColor(fg.B, bg.B, res.A);

    return res;
}

static void buttonPaint (Button *button, bool force)
{
    RGBColor_t color = button->widget.flags.disabled ?
                        button->widget.disabledColor :
                         (button->widget.flags.highlighted ?
                          mixColors(button->movColor, button->widget.flags.selected ? button->hltColor : button->widget.bgColor, .5f) :
                           (button->widget.flags.selected ? button->hltColor : button->widget.bgColor));

    if(force || color.value != button->curColor.value) {
        setColor(color);
        fillRect(button->widget.x + 1, button->widget.y + 1, button->widget.xMax - 1 , button->widget.yMax - 1);
        setColor(button->widget.fgColor);
        drawString(buttonFont, button->labelx, button->labely - 2, button->label, false);
        if(!button->widget.flags.noBox)
            drawRect(button->widget.x, button->widget.y, button->widget.xMax, button->widget.yMax);
        button->curColor = color;
    }
}

static void widgetPaint (Widget *parent, bool single, bool forceRepaint)
{
    Widget *widget = parent;

    while(widget) {

        if(!widget->flags.hidden) {

            if(forceRepaint)
                widget->flags.visible = false;

            switch(widget->type) {

                case WidgetCanvas:
                    current.canvas = (Canvas *)widget;
                    setColor(current.canvas->widget.bgColor);
                    fillRect(widget->x, widget->y, widget->xMax, widget->yMax);
                    break;

                case WidgetFrame:
                    if(!widget->flags.visible) {
                        setColor(widget->bgColor);
                        fillRect(widget->x, widget->y, widget->xMax, widget->yMax);
                        if(!widget->flags.noBox) {
                            setColor(widget->fgColor);
                            drawRect(widget->x, widget->y, widget->xMax, widget->yMax);
                        }
                    }
                    break;

                case WidgetLabel:
                    if(!widget->flags.visible)
                        UILibLabelDisplay((Label *)widget, ((Label *)widget)->string);
                    break;

                case WidgetTextBox:
                    if(!widget->flags.visible)
                        UILibTextBoxDisplay((TextBox *)widget);
                    break;

                case WidgetCheckBox:
                    if(!widget->flags.visible)
                        UILibCheckBoxDisplay((CheckBox *)widget, *((CheckBox *)widget)->value);
                    break;

                case WidgetList:
                    if(!widget->flags.visible) {
                        setColor(current.canvas->widget.bgColor);
                        fillRect(widget->x, widget->y, widget->xMax , widget->yMax);
                    }
                    break;

                case WidgetButton:
                case WidgetListElement:
                    buttonPaint((Button *)widget, forceRepaint);
                    break;

                case WidgetImage:
                    setColor(widget->fgColor);
                    setBackgroundColor(widget->bgColor);
                    drawImageMono(widget->x, widget->y, widget->width, widget->height, ((Image *)widget)->image);
                    break;

                default:
                    break;
            }

            widget->flags.visible = true;

            if(widget->firstChild)
                widgetPaint(widget->firstChild, false, forceRepaint);

            if(widget->eventHandler)
                UILibPublishEvent(widget, EventWidgetPainted, (position_t){widget->x, widget->y}, false, NULL);

        }

        widget = single ? NULL : widget->nextSibling;
    }

    if(!current.widget && parent->type == WidgetCanvas) {
        if(!(widget = widgetGetNext(parent, WidgetAll, false)))
            widget = widgetGetNext(parent, WidgetAll, true);
        if(widget)
            UILibApplyEnter(widget);
    }
}

static position_t widgetGetCPos (Widget *widget)
{
    position_t pos = {.x = widget->x + (widget->width >> 1), .y = widget->y + (widget->height >> 1)};
    return pos;
}

static bool value2String (TextBox *textbox)
{
    int len = textbox->maxLength + 1;

    if(textbox->string && textbox->value) switch(textbox->dataType) {

        case DataTypeString:
            len = strlen((const char *)textbox->value);
            break;

        case DataTypeFloat:;
            len = snprintf(textbox->string, textbox->maxLength + 1, textbox->format, *(float *)textbox->value);
            break;

        case DataTypeInteger:;
            len = snprintf(textbox->string, textbox->maxLength + 1, textbox->format, *(int32_t *)textbox->value);
            break;

        case DataTypeUnsignedInteger:;
            len = snprintf(textbox->string, textbox->maxLength + 1, textbox->format, *(uint32_t *)textbox->value);
            break;

        default:
            break;
    }

    return len < textbox->maxLength;
}

static void caretPaint (bool visible)
{
    if(caret.xpos > 0) {
        setColor(visible ? caret.textbox->widget.fgColor : caret.textbox->widget.bgColor);
        drawLine(caret.xpos - 1, caret.textbox->widget.y - 1, caret.xpos - 1, caret.textbox->widget.yMax + 3);
        drawLine(caret.xpos, caret.textbox->widget.y - 1, caret.xpos, caret.textbox->widget.yMax + 3);
    }
}

static void caretRender (void)
{
    // Blinks every 900 ms
    if(caret.state == 45) {
        caretPaint(false);
    } else if(caret.state == 90) {
        caretPaint(true);
        caret.state = 0;
    }
    caret.state++;
}

static void caretSetXPos (uint8_t pos)
{
    caret.cpos = strlen(caret.textbox->string);

    caretPaint(false);

    if(pos < caret.cpos)
        caret.cpos = pos;

    if(caret.cpos == 0)
        caret.xpos = caret.textbox->widget.x;
    else {
        char c = caret.textbox->string[caret.cpos];
        caret.textbox->string[caret.cpos] = '\0';
        caret.xpos = caret.textbox->widget.x + getStringWidth(caret.textbox->font, caret.textbox->string);
        caret.textbox->string[caret.cpos] = c;
    }
}

static bool caretParse (TextBox *textbox)
{
    char *end = NULL;

    switch(textbox->dataType) {

        case DataTypeFloat:
            *(float *)textbox->value = strtof(textbox->string, &end);
            break;

        case DataTypeInteger:
            *(int32_t *)textbox->value = strtol(textbox->string, &end, 10);
            break;

        case DataTypeUnsignedInteger:
            *(uint32_t *)textbox->value = strtoul(textbox->string, &end, 10);
            break;

        default:
            break;
    }

    return end == NULL;
}

static void addValue (DataType dataType, void *value, float spin)
{
    switch(dataType) {

        case DataTypeFloat:
            *(float *)value += spin;
            break;

        case DataTypeInteger:
            *(int32_t *)value += (int32_t)spin;
            break;

        case DataTypeUnsignedInteger:;
            if(spin > 0.0f || -spin <= (float)*(uint32_t *)value)
                *(uint32_t *)value += (int32_t)spin;
            break;

        default:
            break;
    }
}

static inline bool isNumericType (DataType dataType)
{
    return (dataType & (DataTypeFloat|DataTypeInteger|DataTypeUnsignedInteger)) != 0;
}

static inline bool isLead (char c)
{
    return c == '-' || c == '.' || c == '+';
}

static void caretSpinValue (bool add)
{
    if(isNumericType(caret.textbox->dataType) && (caret.cpos > 0 || !isLead(caret.textbox->string[0]))) {
        char *dp = strchr(caret.textbox->string, caret.textbox->dataType == DataTypeFloat ? '.' : '\0');
        int_fast8_t dppos = dp == NULL ? (caret.textbox->dataType == DataTypeFloat ? 0 : 1) : dp - caret.textbox->string;
        if(caret.cpos != dppos) {
            float spinvalue = powf(10.0f, (float)(dppos - caret.cpos - (dppos > caret.cpos ? 1 : 0)));
            if(!add)
                spinvalue = -spinvalue;
            addValue(caret.textbox->dataType, caret.textbox->value, spinvalue);
            dppos = strlen(caret.textbox->string);
            if(value2String(caret.textbox) && UILibPublishEvent((Widget *)caret.textbox, EventValidate, (position_t){current.x, current.y}, false, NULL)) {
                UILibTextBoxDisplay(caret.textbox);
                if((dppos = strlen(caret.textbox->string) - dppos))
                    caretSetXPos(caret.cpos + dppos);
            } else
                addValue(caret.textbox->dataType, caret.textbox->value, -spinvalue);
        }
    }
}

static void caretEdit (keypress_t keypress)
{
    uint8_t len;
    Widget *textbox;

    if(!UILib_ValidateKeypress(caret.textbox, keypress.key))
        return;

    caret.insert = !(keypress.key >= '0' && keypress.key <= '9');

    if(keypress.down)
      switch(keypress.key) {

      case 0x09: // HT
          caretSetXPos(caret.cpos + 1);
          break;

      case 0x08: // BS
          caretSetXPos(caret.cpos == 0 ? 0 : caret.cpos - 1);
          break;

      case 0x0A: // LF
          textbox = widgetGetNext((Widget *)caret.textbox, WidgetTextBox, false);
          if(textbox)
              UILibApplyEnter(textbox);
          break;

      case 0x0B: // VT
          textbox = widgetGetPrev((Widget *)caret.textbox, WidgetTextBox, false);
          if(textbox)
              UILibApplyEnter(textbox);
          break;

      case 128: // Spin up
          caretSpinValue(true);
          break;

      case 129: // Spin down
          caretSpinValue(false);
          break;

      case 0x7F: // DEL
          if(caret.cpos > 0 && (len = strlen(caret.textbox->string)) > 0) {
              uint8_t pos = caret.cpos - 1;
              while(pos < len) {
                  caret.textbox->string[pos] = caret.textbox->string[pos + 1];
                  pos++;
              }
              caretParse(caret.textbox);
              caretSetXPos(caret.cpos - 1);
              UILibTextBoxDisplay(caret.textbox);
          }
          break;

      default:
          if(keypress.key > ' ') {
              bool atDp = caret.textbox->dataType == DataTypeFloat && caret.textbox->string[caret.cpos] == '.';
              if((len = strlen(caret.textbox->string)) < caret.textbox->maxLength) {
                  if(caret.insert || atDp) while(len > caret.cpos) {
                      caret.textbox->string[len] = caret.textbox->string[len - 1];
                      len--;
                  }
                  caret.textbox->string[caret.cpos] = keypress.key;
                  caretParse(caret.textbox);
                  caretSetXPos(caret.cpos + (caret.insert && !atDp ? 1 : 0));
                  UILibTextBoxDisplay(caret.textbox);
              }
          }
          break;
    }
}

static inline Widget *_next (Widget *widget)
{
    return widget->firstChild ? widget->firstChild : (widget->nextSibling ? widget->nextSibling : (widget->parent ? widget->parent->nextSibling : NULL));
}

static inline Widget *_prev (Widget *widget)
{
    return widget->lastChild ? widget->lastChild : (widget->prevSibling ? widget->prevSibling : (widget->parent ? widget->parent->prevSibling : NULL));
}

static Widget *widgetGetNext (Widget *widget, uint8_t type, bool all)
{
    widget = widget ? _next(widget) : NULL;

    while(widget && !((widget->type & type) && (!widget->flags.disabled || all)))
        widget = _next(widget);

    return widget && (widget->type & type) ? widget : NULL;
}

static Widget *widgetGetPrev (Widget *widget, uint8_t type, bool all)
{
    widget = widget ? _prev(widget) : NULL;

    while(widget && !((widget->type & type) && (!widget->flags.disabled || all)))
        widget = _prev(widget);

    return widget && (widget->type & type) ? widget : NULL;
}

static Canvas *widgetGetCanvas (Widget *widget)
{
    while(widget && widget->type != WidgetCanvas)
        widget = widget->parent;

    return (Canvas *)widget;
}

static Widget *widgetGetFirst (Widget *parent, uint8_t type, bool all)
{
    return widgetGetNext(parent, type, all);
}

static Widget *widgetGetLast (Widget *parent, uint8_t type, bool all)
{
    return widgetGetPrev(parent, type, all);
}

static void positionNavigator (Widget *widget, bool callback)
{
    if(widget && widget->type == WidgetList)
        widget = widgetGetFirst(widget, WidgetListElement, false);

    if(widget && hasNavigator) {
        position_t pos = widgetGetCPos(widget);
        NavigatorSetPosition((uint32_t)pos.x, (uint32_t)pos.y, callback);
    }
}

static bool widgetPointerInside (Widget *widget, position_t pos)
{
    bool ok = widget && (widget->flags.noBox ?
                (pos.y > widget->y && pos.y < widget->yMax && pos.x > widget->x && pos.x < widget->xMax) :
                 (pos.y >= widget->y && pos.y <= widget->yMax && pos.x >= widget->x && pos.x <= widget->xMax));

    return ok;
}

// For single axis navigator input
void UILibSetTabnav (uint8_t delta)
{
    tabNav = delta;
    if(current.canvas)
        current.canvas->tabNav = tabNav;
}

static bool widgetEnter (uint16_t xPos, uint16_t yPos)
{
    Widget *match = NULL, *widget = widgetGetFirst((Widget *)current.canvas, WidgetButton|WidgetListElement|WidgetTextBox|WidgetCheckBox, false);

    while(widget && !match) {

        if(widgetPointerInside(widget, (position_t){xPos, yPos}))
            match = widget;
        else
            widget = widgetGetNext(widget, WidgetButton|WidgetListElement|WidgetTextBox|WidgetCheckBox, false);
    }

    if(match) {

        if(match->type == WidgetTextBox) {
            caret.textbox = (TextBox *)match;
            caret.xpos = 0;
            caret.cpos = 0;
            caretSetXPos(0);
        }

        if((match != current.widget || !match->flags.highlighted) && (match->type & (WidgetButton|WidgetListElement))) {
            match->flags.highlighted = true;
            widgetPaint(match, true, true);
        }

        if(match->parent && match->parent->type == WidgetList)
            ((List *)match->parent)->currentElement = match;

    }

    current.widget = match;

    return current.widget != NULL;
}
/*
static Widget *GetLastSibling (Widget *parent, uint8_t type) {

    Widget *widget = parent ? parent->firstChild : NULL, *last = NULL;

    while(widget) {

        if(type == 0 || widget->type == type)
            last = widget;

        widget = widget->nextSibling;

    }

    return last;

}
*/
// public functions

Canvas *UILibCanvasCreate (uint16_t x, uint16_t y, uint16_t width, uint16_t height, void (*eventHandler)(Widget *self, Event *event))
{
    Canvas *canvas = (Canvas *)widgetCreate(NULL, WidgetCanvas, sizeof(Canvas), x, y, width, height, eventHandler);

    if(canvas) {
        canvas->tabNav         = tabNav;
        canvas->widget.bgColor = LightSlateGray;
        canvas->widget.fgColor = White;
    }

    return canvas;
}

void UILibCanvasDisplay (Canvas *canvas)
{
    if(canvas && canvas->widget.type == WidgetCanvas) {

        if(current.canvas && current.canvas != canvas && !UILibPublishEvent((Widget *)current.canvas, EventWidgetClose, (position_t){current.x, current.y}, false, NULL))
            return;

#ifdef _TOUCH_
        TOUCH_SetEventHandler(0);
#endif

        NavigatorSetEventHandler(NULL);

        if(current.canvas) {

            Widget *widget = (Widget *)current.canvas;

            if(current.canvas != canvas)
                UILibPublishEvent(widget, EventWidgetClosed, currPos, false, NULL);

            while(widget) {
                widget->flags.visible = false;
                widget = widgetGetNext(widget, WidgetAll, true);
            }

            current.canvas = NULL;
            current.widget = NULL;
        }

        UILibClaimInputDevice();

        widgetPaint((Widget *)canvas, false, true);
    }
}

Canvas *UILibCanvasGetCurrent (void)
{
    return current.canvas;
}

void UILibSetEventHandler (Widget *widget, void (*eventHandler)(Widget *self, Event *event))
{
    if(widget)
        widget->eventHandler = eventHandler;
}

void UILibWidgetDisplay (Widget *widget)
{
    if(widget->flags.hidden)
        UILibWidgetHide(widget, false);
    else
        widgetPaint(widget, true, true);
}

void UILibWidgetEnable (Widget *widget, bool enable)
{
    if(!widget->flags.hidden) {
        widget->flags.visible = true;
        widget->flags.disabled = !enable;
        widgetPaint(widget, true, true);
    }
}

void UILibWidgetDeselect (Widget *widget)
{
    if(widget->flags.visible) {
        widget->flags.selected    = false;
        widget->flags.highlighted = false;

        widgetPaint(widget, true, true);
    }
}

Frame *UILibFrameCreate (Widget *parent, uint16_t x, uint16_t y, uint16_t width, uint16_t height, void (*eventHandler)(Widget *self, Event *event))
{
    Frame *frame = (Frame *)widgetCreate(parent, WidgetFrame, sizeof(Frame), x, y, width, height, eventHandler);

    if(frame)
        frame->flags.disabled = true;

    return frame;
}

void UILibFrameClear (Frame *frame)
{
    if(frame->flags.visible) {
        setColor(frame->bgColor);
        fillRect(frame->x + 1, frame->y + 1, frame->xMax - 1, frame->yMax - 1);
    }
}

Label *UILibLabelCreate (Widget *parent, Font *font, RGBColor_t fgColor, uint16_t x, uint16_t y, uint16_t width, void (*eventHandler)(Widget *self, Event *event))
{
    uint16_t fh = getFontHeight(font);

    Label *label = (Label *)widgetCreate(parent, WidgetLabel, sizeof(Label), x, y - fh, width, fh, eventHandler);

    if(label) {
        label->font = font;
        label->widget.fgColor = fgColor;
        label->widget.flags.noBox = true;
        label->widget.flags.disabled = true;
    }

    return label;
}

void UILibLabelClear (Label *label)
{
    if(label->widget.flags.visible) {
        setColor(label->widget.bgColor);
        fillRect(label->widget.x, label->widget.y, label->widget.xMax, label->widget.yMax);
    }
}

bool UILibLabelDisplay (Label *label, const char *string)
{
    bool ok = false;

    if(!label->widget.flags.hidden) {

        label->string = (char *)string;
        label->widget.flags.visible = true;

        UILibLabelClear(label);

        setColor(label->widget.fgColor);
        ok = label->string && drawStringAligned(label->font, label->widget.x, label->widget.yMax + 1, string, (align_t)label->widget.flags.alignment, label->widget.width, label->widget.flags.opaque);
        setColor(current.canvas->widget.fgColor);
    }

    return ok;
}

CheckBox *UILibCheckBoxCreate (Widget *parent, RGBColor_t fgColor, uint16_t x, uint16_t y, char *label, bool *value, void (*eventHandler)(Widget *self, Event *event))
{
    uint16_t fh = getFontHeight(buttonFont), fw = getStringWidth(buttonFont, label);

    CheckBox *checkbox = (CheckBox *)widgetCreate(parent, WidgetCheckBox, sizeof(CheckBox), x, y - fh - 1, fw + fh + 8, fh + 2, eventHandler);

    if(checkbox) {
        checkbox->font = buttonFont;
        checkbox->label = label;
        checkbox->widget.flags.noBox = false;
        checkbox->widget.flags.disabled = false;
        checkbox->widget.fgColor = fgColor;
        checkbox->value = value == NULL ? &checkbox->checked : value;
  //      checkbox->widget.flags.opaque = true;
 //       checkbox->borderColor = DarkGray;
    }

    return checkbox;
}

void UILibCheckBoxDisplay (CheckBox *checkbox, const bool checked)
{
    checkbox->checked = checked; // TODO: remove?
    *checkbox->value = checked;

    if(!checkbox->widget.flags.hidden) {

        setColor(checkbox->widget.fgColor);

        if(!checkbox->widget.flags.visible) {
            drawString(checkbox->font, checkbox->widget.x + checkbox->widget.height + 5, checkbox->widget.yMax, checkbox->label, checkbox->widget.flags.opaque);
            setColor(checkbox->widget.flags.highlighted ? LightGray : checkbox->widget.parent->bgColor);
            drawRect(checkbox->widget.x + checkbox->widget.height + 4, checkbox->widget.y, checkbox->widget.xMax, checkbox->widget.yMax);
            setColor(SlateGray);
            drawRect(checkbox->widget.x, checkbox->widget.y, checkbox->widget.x + checkbox->widget.height, checkbox->widget.yMax);
            drawRect(checkbox->widget.x + 1, checkbox->widget.y + 1, checkbox->widget.x + checkbox->widget.height - 1, checkbox->widget.yMax - 1);
            setColor(checkbox->widget.fgColor);
        }

        fillRect(checkbox->widget.x + 2, checkbox->widget.y + 2, checkbox->widget.x + checkbox->widget.height - 2, checkbox->widget.yMax - 2);
        if(*checkbox->value) {
            setColor(LimeGreen);
            fillRect(checkbox->widget.x + 4, checkbox->widget.y + 4, checkbox->widget.x + checkbox->widget.height - 4, checkbox->widget.yMax - 4);
        }

        checkbox->widget.flags.visible = true;
    }
}

TextBox *UILibTextBoxCreate (Widget *parent, Font *font, RGBColor_t fgColor, uint16_t x, uint16_t y, uint16_t width, void (*eventHandler)(Widget *self, Event *event))
{
    uint16_t fh = getFontHeight(font);

    TextBox *textbox = (TextBox *)widgetCreate(parent, WidgetTextBox, sizeof(TextBox), x, y - fh, width, fh, eventHandler);

    if(textbox) {
        textbox->font = font;
        textbox->widget.flags.noBox = false;
        textbox->widget.flags.disabled = false;
        textbox->widget.bgColor = White;
        textbox->widget.fgColor = Black;
        textbox->borderColor = DarkGray;
    }

    return textbox;
}

void UILibTextBoxClear (TextBox *textbox)
{
    if(textbox->widget.flags.visible) {
        setColor(textbox->widget.bgColor);
        fillRect(textbox->widget.x - 1, textbox->widget.y - 1, textbox->widget.xMax + 3, textbox->widget.yMax + 3);
        setColor(textbox->borderColor);
        drawRect(textbox->widget.x - 2, textbox->widget.y - 2, textbox->widget.xMax + 4, textbox->widget.yMax + 4);
        setColor(textbox->widget.fgColor);
    }
}

bool UILibTextBoxDisplay (TextBox *textbox)
{
    bool ok = false;

    if(!textbox->widget.flags.hidden) {

        textbox->widget.flags.visible = true;

        UILibTextBoxClear(textbox);

        if(!value2String(textbox)) {

            uint_fast8_t i = textbox->maxLength;

            textbox->string[i--] = '\0';

            while(i)
                textbox->string[i--] = '?';
        }

        ok = textbox->string && drawStringAligned(textbox->font, textbox->widget.x, textbox->widget.yMax + 1, textbox->string, (align_t)textbox->widget.flags.alignment, textbox->widget.width, textbox->widget.flags.opaque);
    }

    return ok;
}

bool UILibTextBoxBindValue (TextBox *textbox, void *value, DataType dataType, char *format, const uint8_t maxLength)
{
    if(dataType != DataTypeString) {

        if(textbox->string && textbox->maxLength != maxLength) {
            free(textbox->string);
            textbox->string = NULL;
        }

        if(!textbox->string) {
            textbox->string = calloc(sizeof(char), maxLength + 1);
            if(textbox->string)
                textbox->string[0] = '\0';
        }
    } else
        textbox->string = value;

    if(textbox->string) {
        textbox->value = value;
        textbox->dataType = dataType;
        textbox->format   = format == NULL ? (char *)data_format[dataType] : format;
        textbox->widget.flags.noBox = false;
        textbox->widget.flags.disabled = false;
        textbox->maxLength = maxLength;
        textbox->borderColor = DarkGray;
    }

    return !textbox->widget.flags.disabled;
}

void UILibButtonFlash (Button *button)
{
    button->widget.flags.visible = true;
//  button->flags.selected = true;
    button->widget.flags.highlighted = false;
    buttonPaint(button, false);
//  button->flags.selected = true;
    button->widget.flags.highlighted = true;
    delay(50);
    buttonPaint(button, false);
}

void UILibButtonSetLabel (Button *button, const char *label)
{
    if(label) {
        button->label  = (char *)label;
        button->labelx = button->widget.x + (button->widget.flags.alignment == Align_Left ? 3 : (button->widget.width - getStringWidth(buttonFont, button->label)) / 2);
        button->labely = button->widget.yMax - ((button->widget.height - getFontHeight(buttonFont)) >> 1) + 4;
        if(button->widget.flags.visible)
            buttonPaint(button, true);
    }
}

Font *UILibButtonSetDefaultFont (Font *font)
{
    if(font) {
        buttonFont   = font;
        buttonHeight = getFontHeight(font) + 6;
    }

    return buttonFont;
}

Button *UILibButtonCreate (Widget *parent, uint16_t x, uint16_t y, const char *label, void (*eventHandler)(Widget *self, Event *event))
{
    Button *button = NULL;

    if(parent && (button = (Button *)widgetCreate(parent, WidgetButton, sizeof(Button), x, y, 82, buttonHeight, eventHandler))) {

        button->widget.bgColor         = Peru;
        button->widget.flags.alignment = Align_Center;
        button->hltColor               = IndianRed;
        button->movColor               = NavajoWhite;

        UILibButtonSetLabel(button, label);
    }

    return button;
}

Button *UILibButtonGetSelected (uint32_t group)
{
    Button *btn = (Button *)widgetGetFirst((Widget *)current.canvas, WidgetButton, true);

    while(btn && !(btn->widget.flags.selected && (group == 0 || btn->group == group)))
        btn = (Button *)widgetGetNext((Widget *)btn, WidgetButton, true);

    return btn;
}

void UILibWidgetSelect (Widget *select, uint32_t group)
{
    bool claimed = false;
    position_t pos = widgetGetCPos(select);
    Widget *widget = widgetGetFirst((Widget *)current.canvas, WidgetButton|WidgetListElement, false);
    WidgetFlags flags;

    while(widget) {
        if((!group || ((Button *)widget)->group == group) && widget != select && widget->parent == select->parent) {
            flags = widget->flags;
            if(widget->flags.selected) {
                widget->flags.highlighted = widget == current.widget;
                widget->flags.selected    = false;
                if(widget != current.widget)
                    widgetPaint(widget, true, true);
                else if((claimed = !UILibPublishEvent(widget, EventPointerLeave, pos, false, NULL)))
                    widget->flags = flags;
            }
        }
        widget = widgetGetNext(widget, WidgetButton|WidgetListElement, false);
    }

    if(!claimed && (select->type & (WidgetButton|WidgetListElement))) {
        if(UILibPublishEvent(select, EventPointerUp, pos, true, NULL))
            current.widget = select;
    }
}

Button *UILibGetHighlightedButton (void)
{
    Button *btn = (Button *)widgetGetFirst((Widget *)current.canvas, WidgetButton, true);

    while(btn && !btn->widget.flags.highlighted)
        btn = (Button *)widgetGetNext((Widget *)btn, WidgetButton, true);

    return btn;
}

Image *UILibImageCreate (Widget *parent, uint16_t x, uint16_t y, uint16_t width, uint16_t height, void *pImage, void (*eventHandler)(Widget *self, Event *event))
{
    Image *image = (Image *)widgetCreate(parent, WidgetImage, sizeof(Image), x, y, width, height, eventHandler);

    if(image) {
        image->image                 = pImage;
        image->widget.flags.disabled = true; // no events per default
    }

    return image;
}

List *UILibListCreate (Widget *parent, uint16_t x, uint16_t y, uint16_t width, uint16_t rows, void (*eventHandler)(Widget *self, Event *event))
{
    List *list = (List *)widgetCreate(parent, WidgetList, sizeof(List), x, y, width, rows * 21, eventHandler);

    if(list)
        list->group = listGroup--;

    return list;
}

ListElement *UILibListCreateElement (List *list, uint16_t row, const char *label, void (*eventHandler)(Widget *self, Event *event))
{
    ListElement *element = NULL;

    if(list && list->widget.type == WidgetList && (element = (ListElement *)UILibButtonCreate((Widget *)list, list->widget.x, list->widget.y + 21 * row, label, eventHandler))) {

        element->widget.xMax            = list->widget.xMax;
        element->widget.width           = list->widget.width;
        element->widget.type            = WidgetListElement;
        element->widget.flags.alignment = Align_Left;
        element->widget.fgColor         = Black;
        element->widget.bgColor         = /*(row &0x01) ? LightSteelBlue :*/ Lavender;
        element->hltColor               = MediumSeaGreen;
        element->movColor               = Gold;
        element->group                  = list->group;
        element->widget.flags.noBox     = true;
    }

    return element;
}

Widget *UILibWidgetSetWidth (Widget *widget, uint16_t width)
{
    if(widget) {

        widget->width = width;
        widget->xMax  = widget->x + width - 1;

        if(widget->type == WidgetButton)
            UILibButtonSetLabel ((Button *)widget, ((Button *)widget)->label);

        if(widget->flags.visible) {
            //! repaint bg if smaller
            widgetPaint(widget, true, true);
        }
    }

    return widget;
}

Widget *UILibWidgetSetHeight (Widget *widget, uint16_t height)
{
    if(widget) {

        widget->height = height;
        widget->yMax   = widget->y + height - 1;

    if(widget->type == WidgetButton)
        UILibButtonSetLabel ((Button *)widget, ((Button *)widget)->label);

        if(widget->flags.visible) {
            //! repaint bg if smaller
            widgetPaint(widget, true, true);
        }
    }

    return widget;
}

bool UILibWidgetHide (Widget *widget, bool hidden)
{
    bool hiddenChanged = false;

    if(widget) {

        Widget *child = widget->firstChild;

        hiddenChanged = widget->flags.hidden != hidden;

        widget->flags.hidden = hidden;

        while(child) {
            child->flags.hidden = hidden;
            child = child->nextSibling;
        }

        if(hiddenChanged) {
            if(hidden) {
                widget->flags.visible = false;
                setColor(widget->parent ? widget->parent->bgColor : current.canvas->widget.bgColor);
                fillRect(widget->x, widget->y, widget->xMax , widget->yMax);
                //publish event?
            } else
                widgetPaint(widget, true, true);
        }

    }

    return hiddenChanged;
}

bool UILibPublishEvent (Widget *widget, EventReason reason, position_t pos, bool propagate, void *eventdata)
{
    Event event;

    event.claimed = false;
    event.x = pos.x >= 0 ? (int16_t)pos.x : current.x;
    event.y = pos.y >= 0 ? (int16_t)pos.y : current.y;
    event.data = eventdata;
    event.reason = reason;

    if(widget) do {

        if(widget->flags.visible) {

            if(widget->eventHandler)
                widget->eventHandler(widget, &event);

            // Stop propagation if canvas changed
            event.claimed = event.claimed || widgetGetCanvas(widget) != current.canvas;

            if(!event.claimed)
              switch(event.reason) {

                case EventListOffStart:
                case EventListOffEnd:
                case EventPointerLeave:

                    switch(widget->type) {

                        case WidgetTextBox:
                             if(widget == (Widget *)caret.textbox) {
                                 caretPaint(false);
                                 caret.textbox = NULL;
                             }
                             break;

                        case WidgetButton:
                        case WidgetCheckBox:
                        case WidgetListElement:
                             if(widget->flags.highlighted) {
                                 widget->flags.highlighted = false;
                                 widgetPaint(widget, true, true);
                             }
                             break;

                        default:
                            break;
                    }
                    current.widget = NULL;
                    break;

                case EventPointerEnter:

                    switch(widget->type) {

                        case WidgetCheckBox:
                             if(!widget->flags.highlighted) {
                                 widget->flags.highlighted = true;
                                 widgetPaint(widget, true, true);
                             }
                             break;

                        default:
                            break;
                    }
                    break;

                case EventKeyDown:

                    switch(widget->type) {

                        case WidgetTextBox:
                             if(eventdata && widget == (Widget *)caret.textbox && !caret.textbox->widget.flags.disabled) {
                                 caretEdit(*(keypress_t *)eventdata);
                                 event.claimed = true;
                             }
                             break;

                        default:
                            break;
                    }
                    break;

                case EventKeyUp:

                    switch(widget->type) {

                        case WidgetCheckBox:
                             if(eventdata && ((keypress_t *)eventdata)->key == '.') {
                                 UILibCheckBoxDisplay((CheckBox *)widget, !*((CheckBox *)widget)->value);
                                 event.claimed = true;
                             }
                             break;

                        default:
                            break;
                    }
                    break;

                case EventPointerDown:
                case EventPointerUp:

                    switch(widget->type) {

                        case WidgetListElement:
                        case WidgetButton:
                            if(event.reason == EventPointerUp)
                                widget->flags.selected = true;
                            widget->flags.highlighted = event.reason == EventPointerDown;
                            widgetPaint(widget, true, true);
                            break;

                        case WidgetCheckBox:
                             if(event.reason == EventPointerUp) {
                                 UILibCheckBoxDisplay((CheckBox *)widget, !*((CheckBox *)widget)->value);
                                 event.claimed = true;
                             }
                             break;

                        default:
                            break;
                    }
                    break;

                case EventWidgetClosed:
                    event.claimed = true;
                    break;

                default:
                    break;
            }
        }

        propagate = propagate && !event.claimed;
        widget = propagate ? widget->parent : NULL;

    } while(widget && propagate);

    if(!event.claimed && reason != EventWidgetPainted) {
        current.x = event.x;
        current.y = event.y;
    }

    return !event.claimed;
}

event_element_t *addEvent (uint32_t type, int16_t x, int16_t y, bool down)
{
    event_element_t *event = NULL;

    while(spin_lock);
    spin_lock = true;

    if(event_head->next != event_tail) {
        event = (event_element_t *)event_head;
        event->event.type = type;
        event->event.tabNav = current.canvas->tabNav;
        event->keypress.down = down;
        event->keypress.key = 0;
        event->event.pos.x = x;
        event->event.pos.y = y;
        event_head = event_head->next;
    }

    spin_lock = false;

    return event;
}

void UILibApplyEnter (Widget *widget)
{
    static bool lock = false;

    if(lock)
        return;

    lock = true;

    if(widget && (!current.widget || current.widget != widget)) {
        position_t pos = widgetGetCPos(widget);
        addEvent(POINTER_MOVED, pos.x, pos.y, false);
    }

    if(widget /*&& (!current.widget || current.widget != widget)*/) {
        position_t pos = widgetGetCPos(widget);
        if(!current.widget || current.widget == widget || !UILibPublishEvent(current.widget, EventPointerLeave, pos, false, NULL)) {
            current.x = pos.x;
            current.y = pos.y;
            widgetEnter(current.x, current.y);
            positionNavigator(widget, true);
        }
    }
    lock = false;
}

bool UILib_ValidateKeypress (TextBox *textbox, char c)
{
    bool ok = textbox->dataType == DataTypeString || (c >= '0' && c <= '9')  || c < ' ' || (c >= 0x7F && c <= 0x9F);

    if(!ok) switch(textbox->dataType) {

        case DataTypeFloat:
            ok = (c == '+' || c == '-' || c == '.') && strchr(textbox->string, c) == NULL;
            if(caret.textbox == textbox && (c == '+' || c == '-'))
                ok = caret.cpos == 0;
            break;

        case DataTypeInteger:
            if(caret.textbox == textbox && (c == '+' || c == '-'))
                ok = caret.cpos == 0;
            else
                ok = (c == '+' || c == '-') && strchr(textbox->string, c) == NULL;
            break;

        default:
            break;
    }

    return ok;
}

// event handler functions

static void keypressEventHandler (bool keyDown, char key)
{
    if(key > 0) {
        event_element_t *event = addEvent(KEY_CHANGED, current.x, current.y, keyDown);
        if(event) {
            event->keypress.key  = key;
            event->keypress.down = keyDown;
        }
    }
}

static int32_t pointerEventHandler (uint32_t message, int32_t x, int32_t y)
{
    switch(message) {

        case WIDGET_MSG_PTR_DOWN:
            addEvent(x == -1 || y == -1 ? POINTER_SCAN : POINTER_CHANGED, x, y, true);
            break;

        case WIDGET_MSG_PTR_MOVE:
            addEvent(POINTER_MOVED, x, y, false);
            break;

        case WIDGET_MSG_PTR_UP:
            addEvent(POINTER_CHANGED, x, y, false);
            break;

        case WIDGET_MSG_KEY_UP:
        case WIDGET_MSG_KEY_DOWN:;
            event_element_t *event = addEvent(KEY_CHANGED, x, y, message == WIDGET_MSG_KEY_DOWN);
            if(event)
                event->keypress.key = 0xFF;
            break;
    }

    return 1;
}

#ifdef _REMOTE_

void remoteEventHandler (remote_cmd_t command)
{
    position_t pos = current.widget ? widgetGetCPos(current.widget) : (position_t){current.x, current.y};

    event_element_t *event = addEvent(REMOTE_COMMAND, pos.x, pos.y, false);
    memcpy(&event->remote, &command, sizeof(remote_cmd_t));

    switch(event->remote.command) {

        case ProgramUp:
            event->event.tabNav = 1;
            event->event.pos.y += 1;
            break;

        case ProgramDown:
            event->event.tabNav = 1;
            event->event.pos.y -= 1;
            break;
    }
}

#endif

void systickHandler (void)
{
    systicks++;
}

void UILibClaimInputDevice (void)
{
    while(spin_lock);
    spin_lock = true;
    pointerDown = false;
    spin_lock = false;

#ifdef _TOUCH_
    TOUCH_SetEventHandler(pointerEventHandler);
#endif

    NavigatorSetEventHandler(pointerEventHandler);

#ifdef _KEYPAD_
    setKeyclickCallback(keypressEventHandler, true);
#endif

#ifdef _REMOTE_
    RemoteSetEventHandler(remoteEventHandler);
#endif
}

void UILibProcessEvents (void)
{
    static bool initOk = false, skipMoves = false;

    bool claimed = false;
    event_element_t *head = (event_element_t *)event_head;

    if(!initOk) {
        initOk = true;
        setSysTickCallback(systickHandler);
    }

    if(event_tail != head) do {

        int16_t chg;
        event_element_t *event = (event_element_t *)event_tail;
        Widget *widget, *nextWidget;

        claimed     = false;
        event_tail  = event_tail->next;
        pointerDown = event->keypress.down;
        widget      = current.widget ? current.widget : (Widget *)current.canvas;

        switch(event->event.type) {

            case POINTER_CHANGED:
                if(!pointerDown && (widget->type & (WidgetButton|WidgetListElement)))
                    UILibWidgetSelect(widget, ((Button *)widget)->group);
                else
                    UILibPublishEvent(widget, pointerDown ? EventPointerDown : EventPointerUp, event->event.pos, true, NULL);
                break;

#ifdef _REMOTE_
            case REMOTE_COMMAND:
                claimed = !UILibPublishEvent(widget, EventRemote, event->event.pos, true, (void *)&event->remote);
                if(!claimed && event->remote.command == Select && event->remote.toggled && (widget->type & (WidgetButton|WidgetListElement)))
                    UILibWidgetSelect(widget, ((Button *)widget)->group);
                if(!(event->remote.toggled && (event->remote.command == ProgramUp || event->remote.command == ProgramDown)))
                    break;
                // no break
#endif
            case POINTER_MOVED:

                if(!skipMoves) { // skip for flushing "old" pointer events...

                    position_t pos;
                    systicks = chg = 0;
                    systickLimit = 200;
                    nextWidget = NULL;

                    if(widget->type != WidgetCanvas && event->event.tabNav) {

                        pos = widgetGetCPos(widget);
                        chg = (int16_t)event->event.pos.y - pos.y;

                        if(chg >= event->event.tabNav) {
                            if((nextWidget = widgetGetNext(widget, WidgetAll, false)))
                                event->event.pos = widgetGetCPos(nextWidget);
                        } else if(chg <= -event->event.tabNav) {
                            if((nextWidget = widgetGetPrev(widget, WidgetAll, false)))
                                event->event.pos = widgetGetCPos(nextWidget);
                        } else
                            chg = 0;
                    }

                    skipMoves = nextWidget != NULL;

                    if(event->event.type == REMOTE_COMMAND || UILibPublishEvent(widget, EventPrePointerChange, event->event.pos, true, NULL)) {

                        if(widget->parent->type == WidgetList) {

                            if(widget == widget->parent->lastChild && (event->event.tabNav ? chg > 0 : event->event.pos.y > widget->yMax))
                                claimed = !UILibPublishEvent(widget->parent, EventListOffEnd, event->event.pos, false, NULL);

                            else if(widget == widget->parent->firstChild && (event->event.tabNav ? chg < 0 : event->event.pos.y < widget->y))
                                claimed = !UILibPublishEvent(widget->parent, EventListOffStart, event->event.pos, false, NULL);

                            if(claimed) {
                                if(chg == 0) {
                                    pos = widgetGetCPos(widget);
                                    chg = 1;
                                }
                                widget->flags.highlighted = true;
                                event->event.pos.x = pos.x;
                                event->event.pos.y = pos.y;
                                widgetPaint(widget, true, true);
                            }
                        }

                        if(!claimed) {

                            if(widget && !widgetPointerInside(widget, event->event.pos))
                                claimed = !UILibPublishEvent(widget, EventPointerLeave, event->event.pos, false, NULL);

                            if(!claimed && widgetEnter(event->event.pos.x, event->event.pos.y))
                                UILibPublishEvent(current.widget, EventPointerEnter, event->event.pos, true, NULL);
                        }

                        if(chg) {
                            current.y = event->event.pos.y;
                            NavigatorSetPosition(event->event.pos.x, event->event.pos.y, false);
                        }

                        /* special event for volume control */
                        if(!claimed)
                            UILibPublishEvent((Widget *)current.canvas, EventPointerChanged, event->event.pos, false, NULL);
                    }
                }
                break;

#ifdef _TOUCH_
            case POINTER_SCAN:
                TOUCH_GetPosition();
                break;
#endif

            case KEY_CHANGED:
                pointerDown = false;
                while(widget) {
                    event->keypress.caret = widget == (Widget *)caret.textbox ? &caret : NULL;
                    claimed = !UILibPublishEvent(widget, event->keypress.down ? EventKeyDown : EventKeyUp, event->event.pos, false, (void *)&event->keypress);
                    widget = claimed ? NULL : widget->parent;
                }
                break;
        }
    } while(event_tail != head);

    if(systicks > systickLimit) {

        systicks     = 0;
        systickLimit = 10;
        skipMoves    = false;

        if(caret.textbox)
            caretRender();

        if(!pointerDown && current.canvas && current.canvas->widget.eventHandler)
            UILibPublishEvent((Widget *)current.canvas, EventNullEvent, (position_t){current.x, current.y}, false, NULL);
    }
}

__attribute__((weak)) bool NavigatorInit (uint32_t xSize, uint32_t ySize) { return false; }
__attribute__((weak)) bool NavigatorSetPosition (uint32_t xPos, uint32_t yPos, bool callback) { return true; }
__attribute__((weak)) uint32_t NavigatorGetYPosition (void) { return 0; }
__attribute__((weak)) void NavigatorSetEventHandler (on_navigator_event_ptr on_navigator_event) {}
