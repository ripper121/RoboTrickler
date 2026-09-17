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
    reconnectingForFormat: "Reconnecting to format the SD card…",
    formattingSd: "Formatting the SD card as FAT32…",
    serialReconnectFailed: "Could not reconnect to the firmware serial port. {message}",
    formatResponseLost: "The serial connection closed before formatting completed.",
    formatDeviceError: "The device could not format the SD card (code {code}).",
    formatTimedOut: "The SD card format operation timed out.",
    formattingFailed: "SD card formatting failed",
    formatFailedAfterInstall: "Firmware was installed, but SD formatting failed: {message}",
    installationComplete: "Installation complete",
    unplugCable: "You can unplug the USB cable.",
    installedSuccessfully: "{release} was installed successfully.",
    installedAndFormatted: "{release} was installed and the SD card was formatted successfully.",
    installedWithoutFormat:
      "{release} was installed successfully. This older firmware does not support automatic SD formatting.",
    sdCardFilesAfterFormat:
      "The SD card was formatted as FAT32. Extract SD-Files.zip and copy its contents directly into the root of the SD card.",
    sdCardFilesManual:
      "Format the SD card as FAT32 using the default allocation size. Then extract SD-Files.zip and copy its contents directly into the root of the SD card.",
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
    reconnectingForFormat: "Verbindung zum Formatieren der SD-Karte wird hergestellt…",
    formattingSd: "Die SD-Karte wird als FAT32 formatiert…",
    serialReconnectFailed: "Die serielle Verbindung zur Firmware konnte nicht hergestellt werden. {message}",
    formatResponseLost: "Die serielle Verbindung wurde vor Abschluss der Formatierung getrennt.",
    formatDeviceError: "Das Gerät konnte die SD-Karte nicht formatieren (Code {code}).",
    formatTimedOut: "Zeitüberschreitung beim Formatieren der SD-Karte.",
    formattingFailed: "Formatierung der SD-Karte fehlgeschlagen",
    formatFailedAfterInstall: "Die Firmware wurde installiert, aber die SD-Formatierung ist fehlgeschlagen: {message}",
    installationComplete: "Installation abgeschlossen",
    unplugCable: "Sie können das USB-Kabel abziehen.",
    installedSuccessfully: "{release} wurde erfolgreich installiert.",
    installedAndFormatted: "{release} wurde installiert und die SD-Karte erfolgreich formatiert.",
    installedWithoutFormat:
      "{release} wurde erfolgreich installiert. Diese ältere Firmware unterstützt die automatische SD-Formatierung noch nicht.",
    sdCardFilesAfterFormat:
      "Die SD-Karte wurde als FAT32 formatiert. Entpacken Sie SD-Files.zip und kopieren Sie den Inhalt direkt in das Hauptverzeichnis der SD-Karte.",
    sdCardFilesManual:
      "Formatieren Sie die SD-Karte als FAT32 mit der Standardgröße für Zuordnungseinheiten. Entpacken Sie anschließend SD-Files.zip und kopieren Sie den Inhalt direkt in das Hauptverzeichnis der SD-Karte.",
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
