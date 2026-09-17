import { createHash } from "node:crypto";
import { mkdir, rename, rm, writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { unzipSync } from "fflate";
import { FLASH_FILES, FLASH_SIZE } from "../src/flash-layout.js";
import { RELEASES_API, selectFlashableReleases } from "../src/releases.js";
import { firmwareSupportsSdFormat } from "../src/serial-protocol.js";

const MAXIMUM_USB_PACKAGE_SIZE = 32 * 1024 * 1024;
const MERGED_IMAGE_NAME = "RoboTricklerUI.ino.merged.bin";
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

function findArchiveFile(archive, expectedName) {
  const matches = Object.entries(archive).filter(([name]) => {
    const basename = name.replaceAll("\\", "/").split("/").at(-1);
    return basename.toLocaleLowerCase() === expectedName.toLocaleLowerCase();
  });
  if (matches.length !== 1) {
    throw new Error(`Expected exactly one ${expectedName} in USB-Flash.zip.`);
  }
  return matches[0][1];
}

function bytesEqual(first, second) {
  return first.length === second.length && first.every((byte, index) => byte === second[index]);
}

function validatePackage(release, archive, releaseFirmware, releaseLittlefs) {
  const merged = findArchiveFile(archive, MERGED_IMAGE_NAME);
  if (merged.length !== FLASH_SIZE) {
    throw new Error(`${release.tag} merged image is not exactly 8 MiB.`);
  }

  const files = FLASH_FILES.map((layout) => {
    const data = findArchiveFile(archive, layout.archiveName);
    if (data.length < 1 || data.length > layout.maximumSize) {
      throw new Error(`${layout.archiveName} has an invalid size: ${data.length}.`);
    }
    if (!bytesEqual(merged.subarray(layout.address, layout.address + data.length), data)) {
      throw new Error(`${layout.archiveName} does not match the merged image.`);
    }
    return { ...layout, data };
  });

  const firmware = files.find((file) => file.role === "firmware").data;
  const littlefs = files.find((file) => file.role === "littlefs").data;
  if (!bytesEqual(firmware, releaseFirmware)) {
    throw new Error(`${release.tag} firmware.bin differs from USB-Flash.zip.`);
  }
  if (!bytesEqual(littlefs, releaseLittlefs)) {
    throw new Error(`${release.tag} littlefs.bin differs from USB-Flash.zip.`);
  }
  if (firmware[0] !== 0xe9 || files[0].data[0] !== 0xe9) {
    throw new Error(`${release.tag} contains an invalid ESP32 image.`);
  }
  return files;
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

    const [firmware, littlefs, usbPackage] = await Promise.all([
      downloadAsset(release.firmware, FLASH_FILES.find((file) => file.role === "firmware").maximumSize),
      downloadAsset(release.littlefs, FLASH_FILES.find((file) => file.role === "littlefs").maximumSize),
      downloadAsset(release.usbFlash, MAXIMUM_USB_PACKAGE_SIZE),
    ]);
    const files = validatePackage(
      release,
      unzipSync(usbPackage),
      firmware,
      littlefs,
    );
    const supportsSdFormat = firmwareSupportsSdFormat(firmware);
    process.stdout.write(
      `${release.tag}: post-flash SD formatting ${supportsSdFormat ? "enabled" : "not supported"}.\n`,
    );

    await Promise.all(
      files.map((file) => writeFile(path.join(releaseDirectory, file.outputName), file.data)),
    );

    manifest.push({
      id: release.id,
      name: release.name,
      tag: release.tag,
      prerelease: release.prerelease,
      publishedAt: release.publishedAt,
      supportsSdFormat,
      files: files.map((file) => ({
        ...assetManifest(
          { name: file.outputName },
          file.data,
          `./firmware/${release.id}/${file.outputName}`,
        ),
        role: file.role,
      })),
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
