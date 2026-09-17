const translations = {
  en: {
    pageTitle: "Robo-Trickler Firmware Installer",
    pageDescription: "Install an official Robo-Trickler firmware release over USB.",
    heading: "Firmware installer",
    intro: "Update your device directly from the browser. No files or tools to download.",
    browserWarning:
      "Web Serial is not available. Open this page in Google Chrome or Microsoft Edge on a desktop computer.",
    chooseFirmware: "Choose a firmware",
    loadingReleases: "Loading releases…",
    connectDevice: "Connect the Robo-Trickler with a USB data cable.",
    closeSerialTools: "Close Arduino IDE and any serial monitor first.",
    connectAndInstall: "Erase and install",
    selectPortHint: "Select the USB serial port when your browser asks.",
    preparing: "Preparing…",
    keepConnected: "Keep the device connected.",
    technicalDetails: "Technical details",
    installationLog: "Installation log",
    littlefsWarning:
      "This performs a full flash erase. All device settings and internal files will be replaced.",
    requirements: "Requires Chrome or Edge and a USB data cable.",
    published: "Published {date}",
    prerelease: "Pre-release",
    downloading: "Downloading {file}…",
    downloadFailed: "Could not download {file} ({status}).",
    downloadIncomplete:
      "{file} download is incomplete (received {received} of {expected} bytes).",
    integrityFailed: "{file} failed its integrity check.",
    disconnectWarning: "Disconnect warning: {message}",
    choosePort: "Choose the Robo-Trickler USB port…",
    connecting: "Connecting to the device…",
    unsupportedDevice: "Unsupported device detected: {chip}. Expected an ESP32.",
    firmware: "firmware",
    bootloader: "bootloader",
    partitions: "partition table",
    bootApp: "boot data",
    littlefs: "LittleFS",
    missingFlashFile: "The release is missing {file}.",
    erasingFlash: "Erasing the entire flash…",
    doNotDisconnect: "Do not disconnect the device.",
    installingRelease: "Installing {release}…",
    installingFile: "Installing {file}…",
    restarting: "Restarting the Robo-Trickler…",
    installationComplete: "Installation complete",
    unplugCable: "You can unplug the USB cable.",
    installedSuccessfully: "{release} was installed successfully.",
    noPortSelected: "No USB port selected",
    installationStopped: "Installation stopped",
    noDeviceSelected: "No device was selected. Connect the Robo-Trickler and try again.",
    installationFailed: "Installation failed: {message}",
    errorLog: "Error: {message}",
    noCompatibleRelease: "No release contains a complete USB flash package.",
    releasesUnavailable: "Releases unavailable",
    releaseLoadFailed: "Could not load firmware releases: {message}",
  },
  de: {
    pageTitle: "Robo-Trickler Firmware-Installer",
    pageDescription: "Installieren Sie eine offizielle Robo-Trickler-Firmware über USB.",
    heading: "Firmware-Installer",
    intro: "Aktualisieren Sie Ihr Gerät direkt im Browser – ohne Downloads oder zusätzliche Tools.",
    browserWarning:
      "Web Serial ist nicht verfügbar. Öffnen Sie diese Seite auf einem Desktop-Computer in Google Chrome oder Microsoft Edge.",
    chooseFirmware: "Firmware auswählen",
    loadingReleases: "Versionen werden geladen…",
    connectDevice: "Verbinden Sie den Robo-Trickler mit einem USB-Datenkabel.",
    closeSerialTools: "Schließen Sie zuerst die Arduino IDE und alle seriellen Monitore.",
    connectAndInstall: "Löschen und installieren",
    selectPortHint: "Wählen Sie den seriellen USB-Anschluss aus, wenn Ihr Browser danach fragt.",
    preparing: "Vorbereitung…",
    keepConnected: "Lassen Sie das Gerät angeschlossen.",
    technicalDetails: "Technische Details",
    installationLog: "Installationsprotokoll",
    littlefsWarning:
      "Der gesamte Flash-Speicher wird gelöscht. Alle Geräteeinstellungen und internen Dateien werden ersetzt.",
    requirements: "Benötigt Chrome oder Edge und ein USB-Datenkabel.",
    published: "Veröffentlicht am {date}",
    prerelease: "Vorabversion",
    downloading: "{file} wird heruntergeladen…",
    downloadFailed: "{file} konnte nicht heruntergeladen werden ({status}).",
    downloadIncomplete:
      "Der Download von {file} ist unvollständig ({received} von {expected} Bytes empfangen).",
    integrityFailed: "Die Integritätsprüfung für {file} ist fehlgeschlagen.",
    disconnectWarning: "Warnung beim Trennen: {message}",
    choosePort: "Wählen Sie den USB-Anschluss des Robo-Tricklers aus…",
    connecting: "Verbindung zum Gerät wird hergestellt…",
    unsupportedDevice: "Nicht unterstütztes Gerät erkannt: {chip}. Erwartet wird ein ESP32.",
    firmware: "Firmware",
    bootloader: "Bootloader",
    partitions: "Partitionstabelle",
    bootApp: "Boot-Daten",
    littlefs: "LittleFS",
    missingFlashFile: "In der Version fehlt {file}.",
    erasingFlash: "Der gesamte Flash-Speicher wird gelöscht…",
    doNotDisconnect: "Trennen Sie das Gerät nicht.",
    installingRelease: "{release} wird installiert…",
    installingFile: "{file} wird installiert…",
    restarting: "Der Robo-Trickler wird neu gestartet…",
    installationComplete: "Installation abgeschlossen",
    unplugCable: "Sie können das USB-Kabel abziehen.",
    installedSuccessfully: "{release} wurde erfolgreich installiert.",
    noPortSelected: "Kein USB-Anschluss ausgewählt",
    installationStopped: "Installation angehalten",
    noDeviceSelected: "Es wurde kein Gerät ausgewählt. Verbinden Sie den Robo-Trickler und versuchen Sie es erneut.",
    installationFailed: "Installation fehlgeschlagen: {message}",
    errorLog: "Fehler: {message}",
    noCompatibleRelease: "Keine Version enthält ein vollständiges USB-Flash-Paket.",
    releasesUnavailable: "Versionen nicht verfügbar",
    releaseLoadFailed: "Firmware-Versionen konnten nicht geladen werden: {message}",
  },
};

export function detectLanguage(
  languages = globalThis.navigator?.languages ?? [globalThis.navigator?.language],
) {
  for (const locale of languages ?? []) {
    const language = String(locale ?? "").toLowerCase().split("-")[0];
    if (language === "de" || language === "en") return language;
  }
  return "en";
}

export function createTranslator(language) {
  const dictionary = translations[language] ?? translations.en;
  return (key, values = {}) => {
    const template = dictionary[key] ?? translations.en[key] ?? key;
    return template.replace(/\{(\w+)\}/g, (match, name) =>
      Object.hasOwn(values, name) ? String(values[name]) : match,
    );
  };
}
