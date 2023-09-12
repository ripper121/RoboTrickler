void btnSub_pressAction(void)
{
  if (btnSub.justPressed()) {
    btnSub.drawButton(true);
    targetWeight -= addWeight;
    if (targetWeight < 0) {
      targetWeight = 0.0;
    }
    beep(button);
    labelTarget.drawButton(false, String(targetWeight, 3));
  }
}
void btnSub_ReleaseAction(void)
{
  if (btnSub.justReleased()) {
    btnSub.drawButton(false);
  }
}

void btnAdd_pressAction(void)
{
  if (btnAdd.justPressed()) {
    btnAdd.drawButton(true);
    targetWeight += addWeight;
    if (targetWeight > MAX_TARGET_WEIGHT) {
      targetWeight = MAX_TARGET_WEIGHT;
    }
    beep(button);
    labelTarget.drawButton(false, String(targetWeight, 3));
  }
}
void btnAdd_releaseAction(void)
{
  if (btnAdd.justReleased()) {
    btnAdd.drawButton(false);
  }
}

void btnStart_pressAction(void)
{
  if (btnStart.justPressed()) {
    btnStart.drawButton(true);
    running = true;
    finished = false;
    beep(button);
    if (WIFI_UPDATE && !running) {
      String text = "Start http://";
      text += String(WiFi.localIP());
      labelInfo.drawButton(false, text);
    } else {
      labelInfo.drawButton(false, "Start");
    }
  }
}
void btnStart_releaseAction(void)
{
  if (btnStart.justReleased()) {
    btnStart.drawButton(false);
  }
}

void btnStop_pressAction(void)
{
  if (btnStop.justPressed()) {
    btnStop.drawButton(true);
    running = false;
    beep(button);
    labelInfo.drawButton(false, "Stop");
  }
}
void btnStop_releaseAction(void)
{
  if (btnStop.justReleased()) {
    btnStop.drawButton(false);
  }
}

void btn1_pressAction(void)
{
  if (btn1.justPressed()) {
    btn1.drawButton(true);
    addWeight = 0.001;
    beep(button);
  }
}
void btn1_releaseAction(void)
{
  if (btn1.justReleased()) {
    btn1.drawButton(false);
  }
}

void btn10_pressAction(void)
{
  if (btn10.justPressed()) {
    btn10.drawButton(true);
    addWeight = 0.01;
    beep(button);
  }
}
void btn10_releaseAction(void)
{
  if (btn10.justReleased()) {
    btn10.drawButton(false);
  }
}

void btn100_pressAction(void)
{
  if (btn100.justPressed()) {
    btn100.drawButton(true);
    addWeight = 0.1;
    beep(button);
  }
}
void btn100_releaseAction(void)
{
  if (btn100.justReleased()) {
    btn100.drawButton(false);
  }
}

void btn1000_pressAction(void)
{
  if (btn1000.justPressed()) {
    btn1000.drawButton(true);
    addWeight = 1.0;
    beep(button);
  }
}
void btn1000_releaseAction(void)
{
  if (btn1000.justReleased()) {
    btn1000.drawButton(false);
  }
}

void initButtons() {
  Serial.println("initButtons()");

  byte buttonSpacing = 10;
  byte rowMult = 0;
  byte textOffset = 4;
  int buttonW = 0;

  tft.fillScreen(TFT_BLACK);

  if (config.mode == trickler) {
    buttonW = (LCD_WIDTH / 4) - buttonSpacing;
    btn1.initButtonUL(buttonSpacing, (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_GREEN, TFT_BLACK, "0.001", 3);
    btn1.setPressAction(btn1_pressAction);
    btn1.setReleaseAction(btn1_releaseAction);
    btn1.setLabelDatum(0, textOffset, MC_DATUM);
    btn1.drawButton();

    btn10.initButtonUL(buttonW + (buttonSpacing * 2), (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_GREEN, TFT_BLACK, "0.01", 3);
    btn10.setPressAction(btn10_pressAction);
    btn10.setReleaseAction(btn10_releaseAction);
    btn10.setLabelDatum(0, textOffset, MC_DATUM);
    btn10.drawButton();

    btn100.initButtonUL((buttonW * 2) + (buttonSpacing * 3), (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_GREEN, TFT_BLACK, "0.1", 3);
    btn100.setPressAction(btn100_pressAction);
    btn100.setReleaseAction(btn100_releaseAction);
    btn100.setLabelDatum(0, textOffset, MC_DATUM);
    btn100.drawButton();

    btn1000.initButtonUL((buttonW * 3) + (buttonSpacing * 4), (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_GREEN, TFT_BLACK, "1.0", 3);
    btn1000.setPressAction(btn1000_pressAction);
    btn1000.setReleaseAction(btn1000_releaseAction);
    btn1000.setLabelDatum(0, textOffset, MC_DATUM);
    btn1000.drawButton();
    rowMult++;

    buttonW = (LCD_WIDTH / 3) - buttonSpacing;
    btnSub.initButtonUL(buttonSpacing, (BUTTON_H * rowMult) + (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_RED, TFT_BLACK, "-", 4);
    btnSub.setPressAction(btnSub_pressAction);
    btnSub.setReleaseAction(btnSub_ReleaseAction);
    btnSub.setLabelDatum(0, textOffset, MC_DATUM);
    btnSub.drawButton();

    labelTarget.initButtonUL(buttonW + (buttonSpacing * 2), (BUTTON_H * rowMult) + (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_BLACK, TFT_WHITE, "0.000", 3);
    labelTarget.setLabelDatum(0, textOffset, MC_DATUM);
    labelTarget.drawButton();
    labelTarget.drawButton(false, String(targetWeight, 3));

    btnAdd.initButtonUL((buttonW * 2) + (buttonSpacing * 3), (BUTTON_H * rowMult) + (buttonSpacing * rowMult),  buttonW, BUTTON_H, TFT_WHITE, TFT_RED, TFT_BLACK, "+", 4);
    btnAdd.setPressAction(btnAdd_pressAction);
    btnAdd.setReleaseAction(btnAdd_releaseAction);
    btnAdd.setLabelDatum(0, textOffset, MC_DATUM);
    btnAdd.drawButton();
    rowMult++;
  }

  buttonW = (LCD_WIDTH) - buttonSpacing;
  labelWeight.initButtonUL(buttonSpacing, (BUTTON_H * rowMult) + (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_BLACK, TFT_WHITE, "0.000", 4);
  labelWeight.setLabelDatum(0, textOffset, MC_DATUM);
  labelWeight.drawButton();
  rowMult++;

  buttonW = (LCD_WIDTH / 2) - buttonSpacing;
  btnStart.initButtonUL(buttonSpacing, (BUTTON_H * rowMult) + (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_GREEN, TFT_BLACK, "Start", 4);
  btnStart.setPressAction(btnStart_pressAction);
  btnStart.setReleaseAction(btnStart_releaseAction);
  btnStart.setLabelDatum(0, textOffset, MC_DATUM);
  btnStart.drawButton();

  btnStop.initButtonUL(buttonW + (buttonSpacing * 2),  (BUTTON_H * rowMult) + (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_RED, TFT_BLACK, "Stop", 4);
  btnStop.setPressAction(btnStop_pressAction);
  btnStop.setReleaseAction(btnStop_releaseAction);
  btnStop.setLabelDatum(0, textOffset, MC_DATUM);
  btnStop.drawButton();
  rowMult++;

  buttonW = (LCD_WIDTH) - buttonSpacing;
  labelInfo.initButtonUL(buttonSpacing, (BUTTON_H * rowMult) + (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_BLACK, TFT_WHITE, "Loading ...", 2);
  labelInfo.setLabelDatum(0, textOffset, MC_DATUM);
  labelInfo.drawButton();
  rowMult++;

  buttonW = (LCD_WIDTH) - buttonSpacing;
  labelBanner.initButtonUL(buttonSpacing, LCD_HEIGHT - BUTTON_H, buttonW, BUTTON_H, TFT_WHITE, TFT_BLACK, TFT_WHITE, "RoboTrickler-1.15 ripper121.com", 2);
  labelBanner.setLabelDatum(0, textOffset, MC_DATUM);
  labelBanner.drawButton();
}
