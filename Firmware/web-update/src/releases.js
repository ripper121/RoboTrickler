export const RELEASES_API =
  "https://api.github.com/repos/ripper121/RoboTrickler/releases?per_page=100";
export const RELEASES_MANIFEST = "./releases.json";

const REQUIRED_ASSETS = ["firmware.bin", "littlefs.bin", "usb-flash.zip"];

function findAsset(assets, expectedName) {
  return assets.find(
    (asset) => asset.name.toLocaleLowerCase() === expectedName.toLocaleLowerCase(),
  );
}

export function selectFlashableReleases(releases) {
  return releases
    .filter((release) => !release.draft)
    .map((release) => {
      const assets = Object.fromEntries(
        REQUIRED_ASSETS.map((name) => [name, findAsset(release.assets ?? [], name)]),
      );

      if (Object.values(assets).some((asset) => !asset)) {
        return null;
      }

      return {
        id: release.id,
        name: release.name || release.tag_name,
        tag: release.tag_name,
        prerelease: release.prerelease,
        publishedAt: release.published_at,
        firmware: assets["firmware.bin"],
        littlefs: assets["littlefs.bin"],
        usbFlash: assets["usb-flash.zip"],
      };
    })
    .filter(Boolean)
    .sort((a, b) => new Date(b.publishedAt) - new Date(a.publishedAt));
}

export async function loadFlashableReleases(fetchImpl = fetch) {
  const response = await fetchImpl(RELEASES_MANIFEST, { cache: "no-store" });

  if (!response.ok) {
    throw new Error(`The firmware list returned ${response.status}.`);
  }

  return response.json();
}
