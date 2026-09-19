#include "ui_touch.h"

// One shared style avoids storing four padding properties on every button.
static lv_style_t touchButtonStyle;
static bool touchButtonStyleInitialized = false;

void prepareTouchButton(lv_obj_t *button)
{
    if (!touchButtonStyleInitialized)
    {
        lv_style_init(&touchButtonStyle);
        lv_style_set_pad_all(&touchButtonStyle, 4);
        touchButtonStyleInitialized = true;
    }
    // Match the LVGL example: sliding outside cancels the original click.
    lv_obj_clear_flag(button, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_style(button, &touchButtonStyle, LV_PART_MAIN);
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
