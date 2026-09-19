// Native audit: real screen, theme, fonts, input handling and extracted factories.
// Hardware callbacks count invocations; this executable never accesses a device.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "ui.h"

static unsigned actionCount = 0;
static bool german = false;
static lv_obj_t *dialogBackdrop = nullptr;
static lv_obj_t *activeDialog = nullptr;
static bool lvglLock() { return true; }
static void lvglUnlock() {}
struct TextEntry { const char *key; const char *value; };
#define DIALOG_ELEMENT_HEIGHT UI_TOUCH_TARGET_SIZE
#include "ui_factories.inc"

static uint16_t frame[320][480];
static uint8_t drawBuffer[480 * 20 * 2];
static lv_point_t pointerPosition = {0, 0};
static bool pointerDown = false;
static lv_indev_t *pointerDevice;

static void require(bool condition, const char *message)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

static void flushDisplay(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    auto data = reinterpret_cast<uint16_t *>(pixels);
    for (int y = area->y1; y <= area->y2; ++y)
        for (int x = area->x1; x <= area->x2; ++x) frame[y][x] = *data++;
    lv_display_flush_ready(display);
}

static void readPointer(lv_indev_t *, lv_indev_data_t *data)
{
    data->point = pointerPosition;
    data->state = pointerDown ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void settle()
{
    for (int i = 0; i < 6; ++i) { lv_tick_inc(20); lv_timer_handler(); }
    lv_obj_update_layout(ui_Screen1);
}

static lv_area_t bounds(lv_obj_t *object)
{
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    return area;
}

static void pointerAt(int x, int y, bool down)
{
    pointerPosition = {x, y}; pointerDown = down;
    lv_indev_read(pointerDevice);
    settle();
}

static void savePreview(const char *name)
{
    lv_obj_invalidate(ui_Screen1); settle();
    FILE *file = std::fopen(name, "wb");
    require(file != nullptr, "preview file");
    std::fprintf(file, "P6\n480 320\n255\n");
    for (const auto &row : frame) for (uint16_t pixel : row)
    {
        unsigned char rgb[] = {static_cast<unsigned char>(((pixel >> 11) & 31) * 255 / 31),
                               static_cast<unsigned char>(((pixel >> 5) & 63) * 255 / 63),
                               static_cast<unsigned char>((pixel & 31) * 255 / 31)};
        std::fwrite(rgb, 1, 3, file);
    }
    std::fclose(file);
}

static void collectButtons(lv_obj_t *object, std::vector<lv_obj_t *> &buttons)
{
    if (!lv_obj_is_visible(object)) return;
    if (lv_obj_check_type(object, &lv_button_class)) buttons.push_back(object);
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i)
        collectButtons(lv_obj_get_child(object, i), buttons);
}

static void auditButtons(lv_obj_t *root, bool segmented = false)
{
    settle();
    std::vector<lv_obj_t *> buttons;
    collectButtons(root, buttons);
    for (auto button : buttons)
    {
        auto area = bounds(button);
        require(lv_area_get_width(&area) >= UI_TOUCH_TARGET_SIZE && lv_area_get_height(&area) == UI_TOUCH_TARGET_SIZE, "minimum visible button size");
        require(area.x1 >= 0 && area.y1 >= 0 && area.x2 < 480 && area.y2 < 320, "button outside display");
        for (auto parent = lv_obj_get_parent(button); parent; parent = lv_obj_get_parent(parent))
        {
            auto clip = bounds(parent);
            require(area.x1 >= clip.x1 && area.x2 <= clip.x2 && area.y1 >= clip.y1 && area.y2 <= clip.y2,
                    "button clipped by ancestor");
        }
        require(!lv_obj_has_flag(button, LV_OBJ_FLAG_PRESS_LOCK), "button press lock prevents cancellation");
        for (uint32_t i = 0; i < lv_obj_get_child_count(button); ++i)
        {
            auto label = lv_obj_get_child(button, i);
            if (!lv_obj_check_type(label, &lv_label_class)) continue;
            lv_point_t textSize;
            lv_text_get_size(&textSize, lv_label_get_text(label), lv_obj_get_style_text_font(label, LV_PART_MAIN),
                             0, 0, 10000, LV_TEXT_FLAG_NONE);
            if (textSize.x > lv_obj_get_content_width(button) || textSize.y > lv_obj_get_content_height(button))
                std::fprintf(stderr, "Text: %s, needs %ld x %ld; available %ld x %ld\n", lv_label_get_text(label),
                             (long)textSize.x, (long)textSize.y, (long)lv_obj_get_content_width(button), (long)lv_obj_get_content_height(button));
            require(textSize.x <= lv_obj_get_content_width(button) && textSize.y <= lv_obj_get_content_height(button),
                    "button label does not fit");
        }
    }
    if (!segmented) for (size_t i = 0; i < buttons.size(); ++i) for (size_t j = i + 1; j < buttons.size(); ++j)
    {
        auto a = bounds(buttons[i]), b = bounds(buttons[j]);
        int gapX = a.x1 > b.x2 ? a.x1 - b.x2 - 1 : b.x1 - a.x2 - 1;
        int gapY = a.y1 > b.y2 ? a.y1 - b.y2 - 1 : b.y1 - a.y2 - 1;
        require(gapX >= 8 || gapY >= 8, "buttons overlap or lack 8px separation");
    }
}

static void equalRow(lv_obj_t *a, lv_obj_t *b, bool horizontal)
{
    auto first = bounds(a), second = bounds(b);
    require(lv_area_get_height(&first) == UI_TOUCH_TARGET_SIZE &&
            lv_area_get_height(&second) == UI_TOUCH_TARGET_SIZE, "equal row heights");
    require(horizontal ? (first.y1 == second.y1 && second.x1 - first.x2 - 1 == UI_TOUCH_GAP) :
                         (second.y1 - first.y2 - 1 == UI_TOUCH_GAP), "exact uniform row spacing");
}

static size_t usedMemory()
{
    lv_mem_monitor_t memory; lv_mem_monitor(&memory);
    return memory.total_size - memory.free_size;
}

int main()
{
    lv_init();
    auto display = lv_display_create(480, 320);
    lv_display_set_buffers(display, drawBuffer, nullptr, sizeof(drawBuffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flushDisplay);
    pointerDevice = lv_indev_create();
    lv_indev_set_type(pointerDevice, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointerDevice, readPointer);
    ui_init(); settle();
    auditButtons(lv_tabview_get_tab_bar(ui_TabView), true);
    auditButtons(ui_TabPageTrickler);
    equalRow(ui_PanelTarget, ui_ButtonAddWeightCycle, false);
    equalRow(ui_ButtonAddWeightCycle, ui_PanelTricklerWeight, false);
    equalRow(ui_PanelTricklerWeight, ui_ButtonToggleTrickler, false);
    equalRow(ui_ButtonToggleTrickler, ui_PanelInfo, false);
    equalRow(ui_ButtonIncreaseTargetWeight, ui_ButtonAddWeightCycle, true);
    equalRow(ui_ButtonAddWeightCycle, ui_ButtonDecreaseTargetWeight, true);
    require(lv_obj_get_height(ui_LabelTarget) == UI_TOUCH_TARGET_SIZE &&
            lv_obj_get_height(ui_LabelInfo) == UI_TOUCH_TARGET_SIZE, "standalone label row heights");
    savePreview("trickler.ppm");
    auto plus = bounds(ui_ButtonIncreaseTargetWeight);
    int x = (plus.x1 + plus.x2) / 2, y = (plus.y1 + plus.y2) / 2;
    unsigned before = actionCount;
    pointerAt(x, y, true);
    require(actionCount == before && lv_obj_has_state(ui_ButtonIncreaseTargetWeight, LV_STATE_PRESSED), "press must only show feedback");
    pointerAt(x, y, false);
    require(actionCount == before + 1, "release must activate exactly once");
    before = actionCount;
    pointerAt(x, y, true); pointerAt(plus.x2 + 4, y, true); pointerAt(plus.x2 + 4, y, false);
    require(actionCount == before, "slide into gap must cancel");
    pointerAt(x, y, true); pointerAt(240, y, true); pointerAt(240, y, false);
    require(actionCount == before, "slide into another control must not activate it");
    setProfileTabEnabled(false); settle();
    require(lv_obj_has_state(ui_ButtonIncreaseTargetWeight, LV_STATE_DISABLED) &&
            lv_obj_has_state(ui_ButtonDecreaseTargetWeight, LV_STATE_DISABLED) &&
            lv_obj_has_state(ui_ButtonAddWeightCycle, LV_STATE_DISABLED), "run must visibly disable target editors");
    pointerAt(x, y, true); pointerAt(x, y, false);
    require(actionCount == before, "disabled control must not activate");
    savePreview("trickler_running.ppm");
    setProfileTabEnabled(true);
    size_t minimumTuneFreeBlock = (size_t)-1;
    for (int language = 0; language < 2; ++language)
    {
        german = language != 0;
        lv_tabview_set_active(ui_TabView, 1, LV_ANIM_OFF);
        lv_label_set_text(ui_LabelProfile, "A long profile name for layout verification");
        auditButtons(ui_TabPageProfile);
        equalRow(ui_ButtonProfilePrev, ui_PanelProfile, false);
        equalRow(ui_PanelProfile, ui_ButtonProfileNext, false);
        equalRow(ui_ButtonProfileTune, ui_LabelProfile, true);
        equalRow(ui_LabelProfile, ui_ButtonProfileDelete, true);
        savePreview(german ? "profile_de.ppm" : "profile_en.ppm");
        const char *titles[] = {"msg_tune_profile_title", "msg_tune_limit_factor_title", "msg_tune_measurements_title", "msg_tune_steps_title"};
        for (auto title : titles)
        {
            createProfileTuneDialog();
            lv_label_set_text(profileTuneTitleLabel, langText(title));
            lv_label_set_text(profileTuneValueLabel, "12.345");
            lv_label_set_text(profileTuneEntryLabel, "0.100");
            lv_label_set_text(profileTuneStepSizeLabel, "100");
            if (strcmp(title, "msg_tune_steps_title"))
            {
                lv_obj_add_flag(profileTuneStepSizeButton, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(profileTuneTestButton, LV_OBJ_FLAG_HIDDEN);
            }
            showDialog(ui_PanelProfileTune); auditButtons(ui_PanelProfileTune);
            settle();
            lv_mem_monitor_t tuneMemory; lv_mem_monitor(&tuneMemory);
            if (tuneMemory.free_biggest_size < minimumTuneFreeBlock)
                minimumTuneFreeBlock = tuneMemory.free_biggest_size;
            require(tuneMemory.free_biggest_size >= 2600, "profile tuning draw-memory headroom");
            equalRow(profileTuneTitleLabel, profileTuneValueLabel, false);
            equalRow(profileTuneValueLabel, lv_obj_get_parent(profileTuneEntryLabel), false);
            equalRow(lv_obj_get_parent(profileTuneEntryLabel), lv_obj_get_child(ui_PanelProfileTune, -1), false);
            if (!strcmp(title, "msg_tune_steps_title")) savePreview(german ? "tune_de.ppm" : "tune_en.ppm");
            size_t tuneUsed = usedMemory();
            closeDialog(&ui_PanelProfileTune, true);
            require(ui_PanelProfileTune == nullptr, "profile tuning dialog closes immediately");
            require(usedMemory() + 1000 < tuneUsed, "profile tuning memory is freed before the next dialog");
            settle();
        }
        lv_tabview_set_active(ui_TabView, 2, LV_ANIM_OFF);
        lv_label_set_text(ui_LabelScaleProtocol, german ? "Waage: Sartorius" : "Scale: Sartorius");
        lv_obj_clear_flag(ui_ButtonSyncFlashToSd, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_ButtonSyncSdToFlash, LV_OBJ_FLAG_HIDDEN);
        auditButtons(ui_TabPageInfo);
        savePreview(german ? "info_de.ppm" : "info_en.ppm");
        presentDialog(german ? "Profil wirklich loeschen?\nLanger Profilname\nWeitere Informationen\nZeile 4\nZeile 5\nLetzte Zeile" :
                      "Delete this profile?\nA long profile name\nAdditional details\nLine 4\nLine 5\nLast line",
                      UI_FONT_LARGE, lv_color_white(), true, langText("action_delete"));
        auditButtons(ui_PanelMessages);
        auto fixed = bounds(ui_ButtonMessageOk);
        auto viewport = lv_obj_get_parent(ui_LabelMessages);
        require(lv_obj_get_scroll_bottom(viewport) > 0, "long message must be scrollable");
        lv_obj_scroll_to_y(viewport, 10000, LV_ANIM_OFF); settle();
        require(lv_obj_get_scroll_bottom(viewport) == 0, "last message line must be reachable");
        require(bounds(ui_ButtonMessageOk).y1 == fixed.y1, "message actions must remain fixed");
        before = actionCount;
        pointerAt(8, 8, true); pointerAt(8, 8, false);
        require(actionCount == before, "modal backdrop must block underlying controls");
        savePreview(german ? "confirm_de.ppm" : "confirm_en.ppm");
        closeDialog(&ui_PanelMessages, true); settle();
        ui_ButtonMessageNo = nullptr;
    }
    size_t baseline = usedMemory();
    for (int i = 0; i < 20; ++i)
    {
        presentDialog("Repeated message", UI_FONT_LARGE, lv_color_white(), true, langText("action_copy"));
        settle(); closeDialog(&ui_PanelMessages, true); settle(); ui_ButtonMessageNo = nullptr;
    }
    require(usedMemory() == baseline, "repeated message dialogs must release their allocations");
    lv_mem_monitor_t memory; lv_mem_monitor(&memory);
    std::printf("PASS: tabs, Profile-to-Tune EN/DE dialogs, draw-memory headroom, targets, spacing, text fit, pointer cancellation, disabled state, modal blocking, scrolling and 20 dialog lifecycles.\n");
    std::printf("Minimum tuning free block: %zu bytes.\n", minimumTuneFreeBlock);
    std::printf("Native 32-bit LVGL pool: %zu bytes; peak used: %zu; final used: %zu.\n", memory.total_size, memory.max_used, usedMemory());
}
