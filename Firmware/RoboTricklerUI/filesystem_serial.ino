#include <ff.h>

static const char FORMAT_SD_COMMAND[] = "RTUI:FORMAT_SD:FAT32";
static const char FORMAT_SD_RESPONSE_PREFIX[] = "RTUI:FORMAT_SD:";
static const size_t SERIAL_COMMAND_LENGTH = 64;

enum SdFormatResult
{
  SD_FORMAT_OK = 0,
  SD_FORMAT_NO_MEMORY = -1,
  SD_FORMAT_INIT_FAILED = -2
};

static int formatSdFat32()
{
  if (activeFsIsSd)
  {
    activeFs = NULL;
    activeFsIsSd = false;
    filesystemActive = false;
#if ENABLE_LITTLEFS
    if (littleFsMounted)
    {
      activeFs = &LittleFS;
      filesystemActive = true;
    }
#endif
  }

  if (sdMounted)
  {
    SD.end();
    sdMounted = false;
  }

  if (sdSpi == NULL)
  {
    sdSpi = new (std::nothrow) SPIClass(HSPI);
    if (sdSpi == NULL)
    {
      return SD_FORMAT_NO_MEMORY;
    }
    sdSpi->begin(GRBL_SPI_SCK, GRBL_SPI_MISO, GRBL_SPI_MOSI, GRBL_SPI_SS);
  }

  uint8_t driveNumber = sdcard_init(GRBL_SPI_SS, sdSpi, SD_SPI_FREQ);
  if (driveNumber == 0xFF)
  {
    return SD_FORMAT_INIT_FAILED;
  }

  char drive[3] = {(char)('0' + driveNumber), ':', '\0'};
  BYTE *workBuffer = (BYTE *)malloc(FF_MAX_SS);
  if (workBuffer == NULL)
  {
    sdcard_uninit(driveNumber);
    return SD_FORMAT_NO_MEMORY;
  }
  MKFS_PARM options = {};
  options.fmt = FM_FAT32;
  options.au_size = 0; // Let FatFs choose the default allocation-unit size.

  FRESULT result = f_mkfs(drive, &options, workBuffer, FF_MAX_SS);
  free(workBuffer);
  sdcard_uninit(driveNumber);
  return (int)result;
}

static void runFormatSdCommand()
{
  Serial.print(FORMAT_SD_RESPONSE_PREFIX);
  Serial.println("START");
  Serial.flush();

  int result = formatSdFat32();
  Serial.print(FORMAT_SD_RESPONSE_PREFIX);
  if (result == SD_FORMAT_OK)
  {
    Serial.println("OK");
  }
  else
  {
    Serial.print("ERROR:");
    Serial.println(result);
  }
  Serial.flush();
  delay(250);
  ESP.restart();
}

void handleSerialCommands()
{
  static char command[SERIAL_COMMAND_LENGTH];
  static size_t commandLength = 0;

  while (Serial.available() > 0)
  {
    char input = (char)Serial.read();
    if (input == '\r')
    {
      continue;
    }
    if (input != '\n')
    {
      if (commandLength < sizeof(command) - 1)
      {
        command[commandLength++] = input;
      }
      else
      {
        commandLength = 0;
      }
      continue;
    }

    command[commandLength] = '\0';
    commandLength = 0;
    if (strcmp(command, FORMAT_SD_COMMAND) == 0)
    {
      runFormatSdCommand();
      return;
    }
  }
}
