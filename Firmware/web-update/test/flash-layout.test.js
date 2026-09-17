import assert from "node:assert/strict";
import test from "node:test";
import { FLASH_FILES, FLASH_SIZE } from "../src/flash-layout.js";

test("full-flash files fit inside the 8 MiB layout without overlapping", () => {
  const sorted = [...FLASH_FILES].sort((a, b) => a.address - b.address);

  for (const [index, file] of sorted.entries()) {
    assert.ok(file.address + file.maximumSize <= FLASH_SIZE);
    if (index > 0) {
      const previous = sorted[index - 1];
      assert.ok(previous.address + previous.maximumSize <= file.address);
    }
  }
});

test("full-flash layout contains every required image", () => {
  assert.deepEqual(
    FLASH_FILES.map((file) => file.role),
    ["bootloader", "partitions", "bootApp", "firmware", "littlefs"],
  );
});
