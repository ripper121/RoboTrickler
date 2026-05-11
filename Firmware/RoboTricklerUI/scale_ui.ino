void updateWeightLabel(lv_obj_t *label)
{
  char text[32];
  int decimals = decimalPlaces;
  if (decimals < 0)
  {
    decimals = 0;
  }
  else if (decimals > 6)
  {
    decimals = 6;
  }
  snprintf(text, sizeof(text), "%.*f%s", decimals, weight, unit.c_str());
  setLabelText(label, text);
}
