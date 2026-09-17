import assert from "node:assert/strict";
import test from "node:test";
import { selectFlashableReleases } from "../src/releases.js";

function release(overrides = {}) {
  return {
    id: 1,
    name: "2.14 Release",
    tag_name: "v2.14_Release",
    draft: false,
    prerelease: false,
    published_at: "2026-09-06T14:46:39Z",
    assets: [
      { name: "firmware.bin", size: 10, browser_download_url: "firmware" },
      { name: "littlefs.bin", size: 20, browser_download_url: "littlefs" },
    ],
    ...overrides,
  };
}

test("keeps releases containing both required binaries", () => {
  const result = selectFlashableReleases([release()]);

  assert.equal(result.length, 1);
  assert.equal(result[0].firmware.browser_download_url, "firmware");
  assert.equal(result[0].littlefs.browser_download_url, "littlefs");
});

test("accepts the historical littleFS.bin capitalization", () => {
  const result = selectFlashableReleases([
    release({
      assets: [
        { name: "firmware.bin", size: 10, browser_download_url: "firmware" },
        { name: "littleFS.bin", size: 20, browser_download_url: "littlefs" },
      ],
    }),
  ]);

  assert.equal(result.length, 1);
});

test("removes drafts and incomplete releases and sorts newest first", () => {
  const result = selectFlashableReleases([
    release({ id: 1, published_at: "2025-01-01T00:00:00Z" }),
    release({ id: 2, draft: true }),
    release({ id: 3, assets: [{ name: "firmware.bin" }] }),
    release({ id: 4, published_at: "2026-01-01T00:00:00Z" }),
  ]);

  assert.deepEqual(
    result.map((item) => item.id),
    [4, 1],
  );
});
