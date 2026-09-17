import assert from "node:assert/strict";
import test from "node:test";
import {
  FIRMWARE_BAUD_RATE,
  FORMAT_SD_COMMAND,
  FORMAT_SD_COMMAND_TEXT,
  FORMAT_SD_ERROR,
  FORMAT_SD_OK,
  FORMAT_SD_START,
  firmwareSupportsSdFormat,
} from "../src/serial-protocol.js";

test("SD format serial protocol uses stable newline-delimited tokens", () => {
  assert.equal(FIRMWARE_BAUD_RATE, 115200);
  assert.equal(FORMAT_SD_COMMAND, `${FORMAT_SD_COMMAND_TEXT}\n`);
  assert.equal(FORMAT_SD_START, "RTUI:FORMAT_SD:START");
  assert.equal(FORMAT_SD_OK, "RTUI:FORMAT_SD:OK");
  assert.equal(FORMAT_SD_ERROR, "RTUI:FORMAT_SD:ERROR:");
});

test("detects whether a firmware image supports post-flash SD formatting", () => {
  const encoder = new TextEncoder();
  assert.equal(
    firmwareSupportsSdFormat(encoder.encode(`binary-${FORMAT_SD_COMMAND_TEXT}-data`)),
    true,
  );
  assert.equal(firmwareSupportsSdFormat(encoder.encode("older firmware")), false);
});
