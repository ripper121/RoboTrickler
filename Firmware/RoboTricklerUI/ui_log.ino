void insertLine(String newLine)
{
  for (int i = 0; i < (int)ARRAY_COUNT(infoMessageBuffer) - 1; i++)
  {
    infoMessageBuffer[i] = infoMessageBuffer[i + 1];
  }
  infoMessageBuffer[ARRAY_COUNT(infoMessageBuffer) - 1] = newLine;
}

void updateDisplayLog(String logOutput, bool noLog)
{
  setLabelText(ui_LabelInfo, logOutput.c_str());
  logOutput += "\n";

  if (!noLog)
  {
    insertLine(logOutput);
    String temp = "";
    for (int i = 0; i < (int)ARRAY_COUNT(infoMessageBuffer); i++)
    {
      temp += infoMessageBuffer[i];
    }
    setLabelText(ui_LabelLog, temp.c_str());
  }

  DEBUG_PRINT(logOutput);
}
