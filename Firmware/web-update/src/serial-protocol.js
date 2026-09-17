export const FIRMWARE_BAUD_RATE = 115200;
export const FORMAT_SD_COMMAND_TEXT = "RTUI:FORMAT_SD:FAT32";
export const FORMAT_SD_COMMAND = `${FORMAT_SD_COMMAND_TEXT}\n`;
export const FORMAT_SD_START = "RTUI:FORMAT_SD:START";
export const FORMAT_SD_OK = "RTUI:FORMAT_SD:OK";
export const FORMAT_SD_ERROR = "RTUI:FORMAT_SD:ERROR:";

export function firmwareSupportsSdFormat(data) {
  const marker = new TextEncoder().encode(FORMAT_SD_COMMAND_TEXT);
  outer: for (let offset = 0; offset <= data.length - marker.length; offset += 1) {
    for (let index = 0; index < marker.length; index += 1) {
      if (data[offset + index] !== marker[index]) continue outer;
    }
    return true;
  }
  return false;
}
