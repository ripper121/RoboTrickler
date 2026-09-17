export const FLASH_SIZE = 8 * 1024 * 1024;

export const FLASH_FILES = [
  {
    role: "bootloader",
    archiveName: "RoboTricklerUI.ino.bootloader.bin",
    outputName: "bootloader.bin",
    address: 0x1000,
    maximumSize: 0x7000,
  },
  {
    role: "partitions",
    archiveName: "RoboTricklerUI.ino.partitions.bin",
    outputName: "partitions.bin",
    address: 0x8000,
    maximumSize: 0x1000,
  },
  {
    role: "bootApp",
    archiveName: "boot_app0.bin",
    outputName: "boot_app0.bin",
    address: 0xe000,
    maximumSize: 0x2000,
  },
  {
    role: "firmware",
    archiveName: "RoboTricklerUI.ino.bin",
    outputName: "firmware.bin",
    address: 0x10000,
    maximumSize: 0x330000,
  },
  {
    role: "littlefs",
    archiveName: "littlefs.bin",
    outputName: "littlefs.bin",
    address: 0x670000,
    maximumSize: 0x180000,
  },
];
