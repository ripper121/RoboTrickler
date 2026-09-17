import assert from "node:assert/strict";
import test from "node:test";
import { createTranslator, detectLanguage } from "../src/i18n.js";

test("detects German browser locales", () => {
  assert.equal(detectLanguage(["de-DE", "en-US"]), "de");
  assert.equal(detectLanguage(["de-AT"]), "de");
});

test("uses English for unsupported browser locales", () => {
  assert.equal(detectLanguage(["fr-FR"]), "en");
  assert.equal(detectLanguage([]), "en");
});

test("translates messages and replaces variables", () => {
  const translate = createTranslator("de");

  assert.equal(translate("heading"), "Firmware-Installer");
  assert.equal(
    translate("published", { date: "6. September 2026" }),
    "Veröffentlicht am 6. September 2026",
  );
});
