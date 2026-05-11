void printLoopDiagnostics()
{
    static uint32_t writeETime = millis();

    if (millis() - writeETime < DIAGNOSTICS_INTERVAL_MS)
    {
        return;
    }
    writeETime = millis();

#if DEBUG
    char temp[300];
    snprintf(temp,
             sizeof(temp),
             "Heap: Free:%lu, Min:%lu, Size:%lu, Alloc:%lu",
             (unsigned long)ESP.getFreeHeap(),
             (unsigned long)ESP.getMinFreeHeap(),
             (unsigned long)ESP.getHeapSize(),
             (unsigned long)ESP.getMaxAllocHeap());
    DEBUG_PRINTLN(temp);

    printf("lv_disp_tcb stackHWM: %d / %d\n",
           (DISP_TASK_STACK - uxTaskGetStackHighWaterMark(lv_disp_tcb)),
           DISP_TASK_STACK);
    printf("loop stackHWM: %d / %d\n",
           (getArduinoLoopTaskStackSize() - uxTaskGetStackHighWaterMark(NULL)),
           getArduinoLoopTaskStackSize());
#endif
}
