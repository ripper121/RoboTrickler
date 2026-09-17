import { ESPLoader, Transport } from "esptool-js";
import { FLASH_FILES } from "./flash-layout.js";
import { createTranslator, detectLanguage } from "./i18n.js";
import { loadFlashableReleases } from "./releases.js";
import "./styles.css";

const FLASH_BAUD_RATE = 921600;
// Keep GPIO0 released while pulsing EN low/high. esptool-js 0.6.1's built-in
// hard_reset only releases RTS, which is not a reset edge when RTS is already
// released after flashing (observed with the CH340 1a86:7523 adapter).
const POST_FLASH_RESET_SEQUENCE = "D0|R1|W100|R0|W500";
const language = detectLanguage();
const locale = language === "de" ? "de-DE" : "en";
const t = createTranslator(language);

const elements = {
  browserWarning: document.querySelector("#browser-warning"),
  firmwareSelect: document.querySelector("#firmware-select"),
  installButton: document.querySelector("#install-button"),
  progressPanel: document.querySelector("#progress-panel"),
  progress: document.querySelector("#progress"),
  progressPercent: document.querySelector("#progress-percent"),
  releaseDetails: document.querySelector("#release-details"),
  resultMessage: document.querySelector("#result-message"),
  pageShell: document.querySelector(".page-shell"),
  sdCardOverlay: document.querySelector("#sd-card-overlay"),
  sdCardOverlayClose: document.querySelector("#sd-card-overlay-close"),
  sdCardOverlayMessage: document.querySelector("#sd-card-overlay-message"),
  sdFilesDownload: document.querySelector("#sd-files-download"),
  statusText: document.querySelector("#status-text"),
  statusDetail: document.querySelector("#status-detail"),
  terminal: document.querySelector("#terminal"),
};

let releases = [];
let transport;
let overlayReturnFocus;

function translatePage() {
  document.documentElement.lang = language;
  document.title = t("pageTitle");
  document.querySelector("#page-description").content = t("pageDescription");
  document.querySelectorAll("[data-i18n]").forEach((element) => {
    element.textContent = t(element.dataset.i18n);
  });
  document.querySelectorAll("[data-i18n-aria-label]").forEach((element) => {
    element.setAttribute("aria-label", t(element.dataset.i18nAriaLabel));
  });
}

const terminal = {
  clean() {
    elements.terminal.textContent = "";
  },
  writeLine(data) {
    appendLog(`${data}\n`);
  },
  write(data) {
    appendLog(data);
  },
};

function appendLog(data) {
  elements.terminal.textContent += data;
  elements.terminal.scrollTop = elements.terminal.scrollHeight;
}

function formatDate(date) {
  return new Intl.DateTimeFormat(locale, {
    year: "numeric",
    month: "long",
    day: "numeric",
  }).format(new Date(date));
}

function setProgress(value, label, detail = t("keepConnected")) {
  const rounded = Math.max(0, Math.min(100, Math.round(value)));
  elements.progress.value = rounded;
  elements.progress.textContent = `${rounded}%`;
  elements.progressPercent.textContent = `${rounded}%`;
  elements.statusText.textContent = label;
  elements.statusDetail.textContent = detail;
}

function showResult(message, kind) {
  elements.resultMessage.textContent = message;
  elements.resultMessage.className = `notice notice-${kind}`;
  elements.resultMessage.hidden = false;
}

function selectedRelease() {
  return releases.find(
    (release) => String(release.id) === elements.firmwareSelect.value,
  );
}

function updateReleaseDetails() {
  const release = selectedRelease();
  elements.releaseDetails.textContent = release
    ? `${t("published", { date: formatDate(release.publishedAt) })}${release.prerelease ? ` · ${t("prerelease")}` : ""}`
    : "";
}

function populateReleases() {
  elements.firmwareSelect.replaceChildren();

  for (const release of releases) {
    const option = document.createElement("option");
    option.value = release.id;
    option.textContent = `${release.name}${release.prerelease ? ` (${t("prerelease")})` : ""}`;
    elements.firmwareSelect.append(option);
  }

  elements.firmwareSelect.disabled = false;
  elements.installButton.disabled = !navigator.serial;
  updateReleaseDetails();
}

async function downloadBinary(asset, label, progress) {
  setProgress(progress, t("downloading", { file: label }));
  const response = await fetch(asset.url, { cache: "no-store" });
  if (!response.ok) {
    throw new Error(t("downloadFailed", { file: label, status: response.status }));
  }

  const data = new Uint8Array(await response.arrayBuffer());
  if (data.length !== asset.size) {
    throw new Error(
      t("downloadIncomplete", {
        file: label,
        received: data.length,
        expected: asset.size,
      }),
    );
  }
  const digest = Array.from(
    new Uint8Array(await crypto.subtle.digest("SHA-256", data)),
    (byte) => byte.toString(16).padStart(2, "0"),
  ).join("");
  if (digest !== asset.sha256) {
    throw new Error(t("integrityFailed", { file: label }));
  }
  return data;
}

async function disconnect() {
  if (!transport) return;
  try {
    await transport.disconnect();
  } catch (error) {
    appendLog(`${t("disconnectWarning", { message: error.message })}\n`);
  } finally {
    transport = undefined;
  }
}

function showSdCardInstructions(release) {
  overlayReturnFocus = document.activeElement;
  elements.sdCardOverlayMessage.textContent = t("sdCardFilesManual");
  elements.sdFilesDownload.href = release.sdFilesUrl;
  elements.sdCardOverlay.hidden = false;
  elements.pageShell.inert = true;
  document.body.classList.add("overlay-open");
  elements.sdFilesDownload.focus();
}

function closeSdCardInstructions() {
  elements.sdCardOverlay.hidden = true;
  elements.pageShell.inert = false;
  document.body.classList.remove("overlay-open");
  overlayReturnFocus?.focus();
}

async function installSelectedRelease() {
  const release = selectedRelease();
  if (!release) return;

  elements.installButton.disabled = true;
  elements.firmwareSelect.disabled = true;
  elements.resultMessage.hidden = true;
  elements.progressPanel.hidden = false;
  terminal.clean();
  setProgress(0, t("choosePort"));

  try {
    const port = await navigator.serial.requestPort();
    transport = new Transport(port, true);
    const loader = new ESPLoader({
      transport,
      baudrate: FLASH_BAUD_RATE,
      terminal,
      debugLogging: false,
    });

    setProgress(2, t("connecting"));
    const chipName = await loader.main();
    if (!/^ESP32(?:$|[-\s])/i.test(chipName)) {
      throw new Error(t("unsupportedDevice", { chip: chipName }));
    }

    const downloadedFiles = [];
    for (const [index, layout] of FLASH_FILES.entries()) {
      const asset = release.files.find((file) => file.role === layout.role);
      if (!asset) throw new Error(t("missingFlashFile", { file: layout.archiveName }));
      downloadedFiles.push({
        data: await downloadBinary(asset, t(layout.role), 3 + index),
        address: layout.address,
        role: layout.role,
      });
    }
    const totalBytes = downloadedFiles.reduce((sum, file) => sum + file.data.length, 0);
    const fileSizes = downloadedFiles.map((file) => file.data.length);

    setProgress(8, t("erasingFlash"), t("doNotDisconnect"));
    await loader.eraseFlash();
    setProgress(12, t("installingRelease", { release: release.name }));
    await loader.writeFlash({
      fileArray: downloadedFiles.map(({ data, address }) => ({ data, address })),
      flashMode: "dio",
      flashFreq: "80m",
      flashSize: "8MB",
      eraseAll: false,
      compress: true,
      reportProgress(fileIndex, written) {
        const completedBytes = fileSizes
          .slice(0, fileIndex)
          .reduce((sum, size) => sum + size, 0);
        const percent = 12 + ((completedBytes + written) / totalBytes) * 86;
        const currentFile = t(downloadedFiles[fileIndex].role);
        setProgress(percent, t("installingFile", { file: currentFile }));
      },
    });

    setProgress(99, t("restarting"));
    await loader.after("custom_reset", false, POST_FLASH_RESET_SEQUENCE);
    await disconnect();
    setProgress(100, t("installationComplete"), t("unplugCable"));
    showResult(t("installedSuccessfully", { release: release.name }), "success");
    showSdCardInstructions(release);
  } catch (error) {
    const cancelled = error?.name === "NotFoundError";
    setProgress(
      0,
      cancelled ? t("noPortSelected") : t("installationStopped"),
    );
    const message = cancelled
      ? t("noDeviceSelected")
      : t("installationFailed", { message: error.message || error });
    showResult(message, "error");
    appendLog(`\n${t("errorLog", { message: error.stack || error })}\n`);
  } finally {
    await disconnect();
    elements.firmwareSelect.disabled = false;
    elements.installButton.disabled = !navigator.serial;
  }
}

async function start() {
  if (!navigator.serial) {
    elements.browserWarning.hidden = false;
  }

  try {
    releases = await loadFlashableReleases();
    if (releases.length === 0) {
      throw new Error(t("noCompatibleRelease"));
    }
    populateReleases();
  } catch (error) {
    elements.firmwareSelect.replaceChildren(new Option(t("releasesUnavailable")));
    showResult(t("releaseLoadFailed", { message: error.message }), "error");
  }
}

elements.firmwareSelect.addEventListener("change", updateReleaseDetails);
elements.installButton.addEventListener("click", installSelectedRelease);
elements.sdCardOverlayClose.addEventListener("click", closeSdCardInstructions);
elements.sdCardOverlay.addEventListener("click", (event) => {
  if (event.target === elements.sdCardOverlay) closeSdCardInstructions();
});
document.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && !elements.sdCardOverlay.hidden) {
    closeSdCardInstructions();
  }
});
translatePage();
start();
