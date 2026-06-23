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

IRAM_ATTR void lvglDisplayTask(void *parg)
{
    // LVGL and the web server both need frequent service calls. Keeping them on
    // one pinned task avoids touching LVGL from multiple cores at once.
    while (1)
    {
        if (lvglLock())
        {
            lv_timer_handler();
            lvglUnlock();
        }
        if (webServerActive && ((WiFi.status() == WL_CONNECTED) || wifiSetupApActive))
        {
            server.handleClient();
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

IRAM_ATTR void initDisplayTask(void)
{
    if (lvglMutex == NULL)
    {
        lvglMutex = xSemaphoreCreateRecursiveMutex();
    }

    xTaskCreatePinnedToCore(lvglDisplayTask,
                            "lvglTask",
                            DISP_TASK_STACK,
                            NULL,
                            DISP_TASK_PRO,
                            &lvDisplayTaskHandle,
                            DISP_TASK_CORE // Keep LVGL and web servicing on one core.
    );
}

