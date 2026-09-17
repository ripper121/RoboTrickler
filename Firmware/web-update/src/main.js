import { ESPLoader, Transport } from "esptool-js";
import { loadFlashableReleases } from "./releases.js";
import "./styles.css";

const APP_OFFSET = 0x10000;
const LITTLEFS_OFFSET = 0x670000;
const FLASH_BAUD_RATE = 921600;

const elements = {
  browserWarning: document.querySelector("#browser-warning"),
  firmwareSelect: document.querySelector("#firmware-select"),
  installButton: document.querySelector("#install-button"),
  progressPanel: document.querySelector("#progress-panel"),
  progress: document.querySelector("#progress"),
  progressPercent: document.querySelector("#progress-percent"),
  releaseDetails: document.querySelector("#release-details"),
  resultMessage: document.querySelector("#result-message"),
  statusText: document.querySelector("#status-text"),
  statusDetail: document.querySelector("#status-detail"),
  terminal: document.querySelector("#terminal"),
};

let releases = [];
let transport;

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
  return new Intl.DateTimeFormat(undefined, {
    year: "numeric",
    month: "long",
    day: "numeric",
  }).format(new Date(date));
}

function setProgress(value, label, detail = "Keep the device connected.") {
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
    ? `Published ${formatDate(release.publishedAt)}${release.prerelease ? " · Pre-release" : ""}`
    : "";
}

function populateReleases() {
  elements.firmwareSelect.replaceChildren();

  for (const release of releases) {
    const option = document.createElement("option");
    option.value = release.id;
    option.textContent = `${release.name}${release.prerelease ? " (pre-release)" : ""}`;
    elements.firmwareSelect.append(option);
  }

  elements.firmwareSelect.disabled = false;
  elements.installButton.disabled = !navigator.serial;
  updateReleaseDetails();
}

async function downloadBinary(asset, label) {
  setProgress(4, `Downloading ${label}…`);
  const response = await fetch(asset.url, { cache: "no-store" });
  if (!response.ok) {
    throw new Error(`Could not download ${label} (${response.status}).`);
  }

  const data = new Uint8Array(await response.arrayBuffer());
  if (data.length !== asset.size) {
    throw new Error(
      `${label} download is incomplete (received ${data.length} of ${asset.size} bytes).`,
    );
  }
  const digest = Array.from(
    new Uint8Array(await crypto.subtle.digest("SHA-256", data)),
    (byte) => byte.toString(16).padStart(2, "0"),
  ).join("");
  if (digest !== asset.sha256) {
    throw new Error(`${label} failed its integrity check.`);
  }
  return data;
}

async function disconnect() {
  if (!transport) return;
  try {
    await transport.disconnect();
  } catch (error) {
    appendLog(`Disconnect warning: ${error.message}\n`);
  } finally {
    transport = undefined;
  }
}

async function installSelectedRelease() {
  const release = selectedRelease();
  if (!release) return;

  elements.installButton.disabled = true;
  elements.firmwareSelect.disabled = true;
  elements.resultMessage.hidden = true;
  elements.progressPanel.hidden = false;
  terminal.clean();
  setProgress(0, "Choose the Robo-Trickler USB port…");

  try {
    const port = await navigator.serial.requestPort();
    transport = new Transport(port, true);
    const loader = new ESPLoader({
      transport,
      baudrate: FLASH_BAUD_RATE,
      terminal,
      debugLogging: false,
    });

    setProgress(2, "Connecting to the device…");
    const chipName = await loader.main();
    if (!/^ESP32(?:$|[-\s])/i.test(chipName)) {
      throw new Error(`Unsupported device detected: ${chipName}. Expected an ESP32.`);
    }

    const firmware = await downloadBinary(release.firmware, "firmware");
    const littlefs = await downloadBinary(release.littlefs, "LittleFS");
    const totalBytes = firmware.length + littlefs.length;
    const fileSizes = [firmware.length, littlefs.length];

    setProgress(5, `Installing ${release.name}…`);
    await loader.writeFlash({
      fileArray: [
        { data: firmware, address: APP_OFFSET },
        { data: littlefs, address: LITTLEFS_OFFSET },
      ],
      flashMode: "dio",
      flashFreq: "80m",
      flashSize: "8MB",
      eraseAll: false,
      compress: true,
      reportProgress(fileIndex, written) {
        const completedBytes = fileSizes
          .slice(0, fileIndex)
          .reduce((sum, size) => sum + size, 0);
        const percent = 5 + ((completedBytes + written) / totalBytes) * 93;
        const currentFile = fileIndex === 0 ? "firmware" : "LittleFS";
        setProgress(percent, `Installing ${currentFile}…`);
      },
    });

    setProgress(99, "Restarting the Robo-Trickler…");
    await loader.after("hard_reset");
    setProgress(100, "Installation complete", "You can unplug the USB cable.");
    showResult(`${release.name} was installed successfully.`, "success");
  } catch (error) {
    const cancelled = error?.name === "NotFoundError";
    setProgress(0, cancelled ? "No USB port selected" : "Installation stopped");
    showResult(
      cancelled
        ? "No device was selected. Connect the Robo-Trickler and try again."
        : `Installation failed: ${error.message || error}`,
      "error",
    );
    appendLog(`\nError: ${error.stack || error}\n`);
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
      throw new Error("No release contains both firmware.bin and littlefs.bin.");
    }
    populateReleases();
  } catch (error) {
    elements.firmwareSelect.replaceChildren(new Option("Releases unavailable"));
    showResult(`Could not load firmware releases: ${error.message}`, "error");
  }
}

elements.firmwareSelect.addEventListener("change", updateReleaseDetails);
elements.installButton.addEventListener("click", installSelectedRelease);
start();
