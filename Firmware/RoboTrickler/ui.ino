void btnSub_pressAction(void)
{
  if (btnSub.justPressed()) {
    btnSub.drawButton(true);
    targetWeight -= addWeight;
    if (targetWeight < 0) {
      targetWeight = 0.0;
    }
    beep(button);
    labelTarget.drawButton(false, String(targetWeight, 3) + " g");
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
    if (targetWeight > 100) {
      targetWeight = 100.0;
    }
    beep(button);
    labelTarget.drawButton(false, String(targetWeight, 3) + " g");
  }
}
void btnAdd_releaseAction(void)
{
  if (btnAdd.justReleased()) {
    btnAdd.drawButton(false);
  }
}

void btnDown_pressAction(void)
{
  if (btnDown.justPressed()) {
    btnDown.drawButton(true);
    stepperSpeed -= addSpeed;
    if (stepperSpeed < 10) {
      stepperSpeed = 10;
    }
    beep(button);
    setStepperSpeed(stepperSpeed);
    labelSpeed.drawButton(false, String(stepperSpeed) + " rpm");
  }
}
void btnDown_releaseAction(void)
{
  if (btnDown.justReleased()) {
    btnDown.drawButton(false);
  }
}

void btnUp_pressAction(void)
{
  if (btnUp.justPressed()) {
    btnUp.drawButton(true);
    stepperSpeed += addSpeed;
    if (stepperSpeed > 300) {
      stepperSpeed = 300;
    }
    beep(button);
    setStepperSpeed(stepperSpeed);
    labelSpeed.drawButton(false, String(stepperSpeed) + " rpm");
  }
}
void btnUp_releaseAction(void)
{
  if (btnUp.justReleased()) {
    btnUp.drawButton(false);
  }
}



void btnStart_pressAction(void)
{
  if (btnStart.justPressed()) {
    btnStart.drawButton(true);
    running = true;
    finished = false;
    beep(button);
    labelInfo.drawButton(false, "Stopped");
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
    addSpeed = 1;
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
    addSpeed = 10;
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
    addSpeed = 100;
    beep(button);
  }
}
void btn100_releaseAction(void)
{
  if (btn100.justReleased()) {
    btn100.drawButton(false);
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
    buttonW = (LCD_WIDTH / 3) - buttonSpacing;
    btn1.initButtonUL(buttonSpacing, (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_GREEN, TFT_BLACK, "1", 2);
    btn1.setPressAction(btn1_pressAction);
    btn1.setReleaseAction(btn1_releaseAction);
    btn1.setLabelDatum(0, textOffset, MC_DATUM);
    btn1.drawButton();

    btn10.initButtonUL(buttonW + (buttonSpacing * 2), (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_GREEN, TFT_BLACK, "10", 2);
    btn10.setPressAction(btn10_pressAction);
    btn10.setReleaseAction(btn10_releaseAction);
    btn10.setLabelDatum(0, textOffset, MC_DATUM);
    btn10.drawButton();

    btn100.initButtonUL((buttonW * 2) + (buttonSpacing * 3), (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_GREEN, TFT_BLACK, "100", 2);
    btn100.setPressAction(btn100_pressAction);
    btn100.setReleaseAction(btn100_releaseAction);
    btn100.setLabelDatum(0, textOffset, MC_DATUM);
    btn100.drawButton();
    rowMult++;

    buttonW = (LCD_WIDTH / 3) - buttonSpacing;
    btnSub.initButtonUL(buttonSpacing, (BUTTON_H * rowMult) + (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_RED, TFT_BLACK, "-", 4);
    btnSub.setPressAction(btnSub_pressAction);
    btnSub.setReleaseAction(btnSub_ReleaseAction);
    btnSub.setLabelDatum(0, textOffset, MC_DATUM);
    btnSub.drawButton();

    labelTarget.initButtonUL(buttonW + (buttonSpacing * 2), (BUTTON_H * rowMult) + (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_BLACK, TFT_GREEN, "0.000 g", 2);
    labelTarget.setLabelDatum(0, textOffset, MC_DATUM);
    labelTarget.drawButton();
    labelTarget.drawButton(false, String(targetWeight) + " g");

    btnAdd.initButtonUL((buttonW * 2) + (buttonSpacing * 3), (BUTTON_H * rowMult) + (buttonSpacing * rowMult),  buttonW, BUTTON_H, TFT_WHITE, TFT_RED, TFT_BLACK, "+", 4);
    btnAdd.setPressAction(btnAdd_pressAction);
    btnAdd.setReleaseAction(btnAdd_releaseAction);
    btnAdd.setLabelDatum(0, textOffset, MC_DATUM);
    btnAdd.drawButton();
    rowMult++;
  }

  buttonW = (LCD_WIDTH) - buttonSpacing;
  labelWeight.initButtonUL(buttonSpacing, (BUTTON_H * rowMult) + (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_BLACK, TFT_GREEN, "0.000 g", 4);
  labelWeight.setLabelDatum(0, textOffset, MC_DATUM);
  labelWeight.drawButton();
  rowMult++;

  if (config.mode == trickler) {
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
    labelInfo.initButtonUL(buttonSpacing, (BUTTON_H * rowMult) + (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_BLACK, TFT_GREEN, "Loading ...", 2);
    labelInfo.setLabelDatum(0, textOffset, MC_DATUM);
    labelInfo.drawButton();
    rowMult++;
  }

  buttonW = (LCD_WIDTH) - buttonSpacing;
  labelBanner.initButtonUL(buttonSpacing, (BUTTON_H * rowMult) + (buttonSpacing * rowMult), buttonW, BUTTON_H, TFT_WHITE, TFT_BLACK, TFT_GREEN, "RoboTrickler-1.1 ripper121.com", 2);
  labelBanner.setLabelDatum(0, textOffset, MC_DATUM);
  labelBanner.drawButton();
  rowMult++;
}
