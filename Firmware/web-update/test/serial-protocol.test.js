import assert from "node:assert/strict";
import test from "node:test";
import {
  FIRMWARE_BAUD_RATE,
  FORMAT_SD_COMMAND,
  FORMAT_SD_COMMAND_TEXT,
  FORMAT_SD_ERROR,
  FORMAT_SD_OK,
  FORMAT_SD_START,
} from "../src/serial-protocol.js";

test("SD format serial protocol uses stable newline-delimited tokens", () => {
  assert.equal(FIRMWARE_BAUD_RATE, 115200);
  assert.equal(FORMAT_SD_COMMAND, `${FORMAT_SD_COMMAND_TEXT}\n`);
  assert.equal(FORMAT_SD_START, "RTUI:FORMAT_SD:START");
  assert.equal(FORMAT_SD_OK, "RTUI:FORMAT_SD:OK");
  assert.equal(FORMAT_SD_ERROR, "RTUI:FORMAT_SD:ERROR:");
});
