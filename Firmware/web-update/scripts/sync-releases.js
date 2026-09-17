import { createHash } from "node:crypto";
import { mkdir, rename, rm, writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { RELEASES_API, selectFlashableReleases } from "../src/releases.js";

const APP_PARTITION_SIZE = 0x330000;
const LITTLEFS_PARTITION_SIZE = 0x180000;
const projectDirectory = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  "..",
);
const publicDirectory = path.join(projectDirectory, "public");
const firmwareDirectory = path.join(publicDirectory, "firmware");
const stagingDirectory = path.join(publicDirectory, ".firmware-staging");

function validateGeneratedPath(target) {
  if (path.dirname(target) !== publicDirectory) {
    throw new Error(`Refusing to replace a directory outside ${publicDirectory}.`);
  }
}

async function downloadAsset(asset, maximumSize) {
  if (!Number.isSafeInteger(asset.size) || asset.size < 1 || asset.size > maximumSize) {
    throw new Error(`${asset.name} has an invalid size: ${asset.size}.`);
  }

  const response = await fetch(asset.browser_download_url, {
    headers: { "User-Agent": "RoboTrickler-Web-Flasher-Build" },
  });
  if (!response.ok) {
    throw new Error(`Could not download ${asset.name}: HTTP ${response.status}.`);
  }

  const data = new Uint8Array(await response.arrayBuffer());
  if (data.length !== asset.size) {
    throw new Error(
      `${asset.name} is incomplete (received ${data.length} of ${asset.size} bytes).`,
    );
  }
  return data;
}

function assetManifest(asset, data, url) {
  return {
    name: asset.name,
    size: data.length,
    sha256: createHash("sha256").update(data).digest("hex"),
    url,
  };
}

async function main() {
  const apiHeaders = {
    Accept: "application/vnd.github+json",
    "User-Agent": "RoboTrickler-Web-Flasher-Build",
  };
  if (process.env.GITHUB_TOKEN) {
    apiHeaders.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
  }
  const response = await fetch(RELEASES_API, {
    headers: apiHeaders,
  });
  if (!response.ok) {
    throw new Error(`GitHub returned ${response.status} while loading releases.`);
  }

  const releases = selectFlashableReleases(await response.json());
  if (releases.length === 0) {
    throw new Error("No release contains both firmware.bin and littlefs.bin.");
  }

  validateGeneratedPath(firmwareDirectory);
  validateGeneratedPath(stagingDirectory);
  await mkdir(publicDirectory, { recursive: true });
  await rm(stagingDirectory, { recursive: true, force: true });
  await mkdir(stagingDirectory);

  const manifest = [];
  for (const release of releases) {
    process.stdout.write(`Synchronizing ${release.tag}...\n`);
    const releaseDirectory = path.join(stagingDirectory, String(release.id));
    await mkdir(releaseDirectory);

    const [firmware, littlefs] = await Promise.all([
      downloadAsset(release.firmware, APP_PARTITION_SIZE),
      downloadAsset(release.littlefs, LITTLEFS_PARTITION_SIZE),
    ]);
    if (firmware[0] !== 0xe9) {
      throw new Error(`${release.tag} firmware.bin is not an ESP application image.`);
    }

    await Promise.all([
      writeFile(path.join(releaseDirectory, "firmware.bin"), firmware),
      writeFile(path.join(releaseDirectory, "littlefs.bin"), littlefs),
    ]);

    manifest.push({
      id: release.id,
      name: release.name,
      tag: release.tag,
      prerelease: release.prerelease,
      publishedAt: release.publishedAt,
      firmware: assetManifest(
        release.firmware,
        firmware,
        `./firmware/${release.id}/firmware.bin`,
      ),
      littlefs: assetManifest(
        release.littlefs,
        littlefs,
        `./firmware/${release.id}/littlefs.bin`,
      ),
    });
  }

  await rm(firmwareDirectory, { recursive: true, force: true });
  await rename(stagingDirectory, firmwareDirectory);
  await writeFile(
    path.join(publicDirectory, "releases.json"),
    `${JSON.stringify(manifest, null, 2)}\n`,
    "utf8",
  );
  process.stdout.write(`Synchronized ${manifest.length} release(s).\n`);
}

main().catch(async (error) => {
  await rm(stagingDirectory, { recursive: true, force: true });
  console.error(error);
  process.exitCode = 1;
});
