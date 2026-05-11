bool lvglLock()
{
  if (lvglMutex == NULL)
  {
    return true;
  }
  return xSemaphoreTakeRecursive(lvglMutex, portMAX_DELAY) == pdTRUE;
}

void lvglUnlock()
{
  if (lvglMutex != NULL)
  {
    xSemaphoreGiveRecursive(lvglMutex);
  }
}

void setLabelText(lv_obj_t *label, const char *text)
{
  if (lvglLock())
  {
    lv_label_set_text(label, text);
    lvglUnlock();
  }
}

void setObjBgColor(lv_obj_t *obj, uint32_t colorHex)
{
  if (lvglLock())
  {
    lv_obj_set_style_bg_color(obj, lv_color_hex(colorHex), LV_PART_MAIN);
    lvglUnlock();
  }
}

void setLabelTextColor(lv_obj_t *label, uint32_t colorHex)
{
  if (lvglLock())
  {
    lv_obj_set_style_text_color(label, lv_color_hex(colorHex), LV_PART_MAIN);
    lvglUnlock();
  }
}
