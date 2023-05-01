#include "widget_element_i.h"
#include <gui/elements.h>

typedef struct {
    uint8_t x;
    uint8_t y;
    Align horizontal;
    Align vertical;
    Font font;
    FurryString* text;
} GuiStringMultiLineModel;

static void gui_string_multiline_draw(Canvas* canvas, WidgetElement* element) {
    furry_assert(canvas);
    furry_assert(element);
    GuiStringMultiLineModel* model = element->model;

    if(furry_string_size(model->text)) {
        canvas_set_font(canvas, model->font);
        elements_multiline_text_aligned(
            canvas,
            model->x,
            model->y,
            model->horizontal,
            model->vertical,
            furry_string_get_cstr(model->text));
    }
}

static void gui_string_multiline_free(WidgetElement* gui_string) {
    furry_assert(gui_string);

    GuiStringMultiLineModel* model = gui_string->model;
    furry_string_free(model->text);
    free(gui_string->model);
    free(gui_string);
}

WidgetElement* widget_element_string_multiline_create(
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    Font font,
    const char* text) {
    furry_assert(text);

    // Allocate and init model
    GuiStringMultiLineModel* model = malloc(sizeof(GuiStringMultiLineModel));
    model->x = x;
    model->y = y;
    model->horizontal = horizontal;
    model->vertical = vertical;
    model->font = font;
    model->text = furry_string_alloc_set(text);

    // Allocate and init Element
    WidgetElement* gui_string = malloc(sizeof(WidgetElement));
    gui_string->parent = NULL;
    gui_string->input = NULL;
    gui_string->draw = gui_string_multiline_draw;
    gui_string->free = gui_string_multiline_free;
    gui_string->model = model;

    return gui_string;
} //-V773
