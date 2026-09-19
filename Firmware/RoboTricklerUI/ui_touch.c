#include "ui_touch.h"

// One shared style avoids storing four padding properties on every button.
static lv_style_t touchButtonStyle;
static bool touchButtonStyleInitialized = false;

void prepareTouchButton(lv_obj_t *button)
{
    if (!touchButtonStyleInitialized)
    {
        lv_style_init(&touchButtonStyle);
        lv_style_set_pad_hor(&touchButtonStyle, 4);
        lv_style_set_pad_ver(&touchButtonStyle, 0);
        touchButtonStyleInitialized = true;
    }
    // Match the LVGL example: sliding outside cancels the original click.
    lv_obj_clear_flag(button, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_style(button, &touchButtonStyle, LV_PART_MAIN);
}

// Labels already have a local style for their font/alignment. Add the two row
// properties there instead of attaching another style entry to every label.
void prepareTouchLabel(lv_obj_t *label)
{
    const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    lv_obj_set_height(label, UI_TOUCH_TARGET_SIZE);
    lv_obj_set_style_pad_top(label,
                             (UI_TOUCH_TARGET_SIZE - font->line_height) / 2,
                             LV_PART_MAIN);
    lv_obj_set_y(label, 0);
}

void prepareTouchControls(lv_obj_t *parent)
{
    if (lv_obj_check_type(parent, &lv_button_class))
    {
        prepareTouchButton(parent);
    }
    uint32_t count = lv_obj_get_child_count(parent);
    for (uint32_t i = 0; i < count; ++i)
    {
        prepareTouchControls(lv_obj_get_child(parent, i));
    }
}
