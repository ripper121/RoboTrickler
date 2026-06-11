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

IRAM_ATTR void lvgl_disp_task(void *parg)
{
    while (1)
    {
        if (lvglLock())
        {
            lv_timer_handler();
            lvglUnlock();
        }
        if (WEB_SERVER_ACTIVE && (WiFi.status() == WL_CONNECTED))
        {
            server.handleClient();
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

IRAM_ATTR void disp_task_init(void)
{
    if (lvglMutex == NULL)
    {
        lvglMutex = xSemaphoreCreateRecursiveMutex();
    }

    xTaskCreatePinnedToCore(lvgl_disp_task,  // task
                            "lvglTask",      // name for task
                            DISP_TASK_STACK, // size of task stack
                            NULL,            // parameters
                            DISP_TASK_PRO,   // priority
                            // nullptr,
                            &lv_disp_tcb,
                            DISP_TASK_CORE // must run the task on same core
                                           // core
    );
}

