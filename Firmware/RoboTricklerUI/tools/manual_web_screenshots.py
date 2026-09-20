"""Capture the actual SD web pages in headless Edge using illustrative API data.

Requires selenium and installed Microsoft Edge. No device is contacted and the
local preview server accepts only GET requests. Run from any working directory.
"""
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from threading import Thread
from urllib.parse import parse_qs, urlsplit
import base64
import json
import gzip

from selenium import webdriver
from selenium.webdriver.support.ui import WebDriverWait

ROOT = Path(__file__).resolve().parent.parent
SD = ROOT / "SD-Files"
OUTPUT = ROOT / "docs/screenshots"


class PreviewHandler(SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        url = urlsplit(self.path)
        data = {
            "/getLanguage": "de",
            "/getProfile": "avg",
            "/getProfileList": ["avg", "calibrate", "min"],
            "/getTarget": "40.000",
            "/getTricklerState": {"weight": 39.6 if self.server.demoRunning else 0.0,
                                  "running": self.server.demoRunning,
                                  "trickle": 1 if self.server.demoRunning else 0},
            "/api/wifi/scan": [{"ssid": "Beispiel-WLAN", "rssi": -45, "channel": 6, "secure": True}],
        }
        if url.path == "/config.txt":
            # Public, synthetic settings; never read a user's network credentials.
            data[url.path] = {
                "wifi": {"enabled": True, "ssid": "Beispiel-WLAN", "psk": "",
                         "ipStatic": "", "ipGateway": "", "ipSubnet": "", "ipDns": ""},
                "scale": {"protocol": "GG", "customCode": "", "baud": 9600},
                "stepper": {"stepsPerRev": 200}, "activeProfile": "avg", "language": "de",
                "beeper": "done", "totalCounter": {"enable": False, "count": 0},
                "firmwareUpdate": {"check": True},
            }
        if url.path == "/list":
            directory = parse_qs(url.query).get("dir", ["/"])[0]
            folder = (SD / directory.lstrip("/")).resolve()
            if not folder.is_relative_to(SD) or not folder.is_dir():
                self.send_error(404)
                return
            data[url.path] = [{"type": "dir" if p.is_dir() else "file", "name": p.name}
                              for p in sorted(folder.iterdir())]
        if url.path in data:
            value = data[url.path]
            body = (value if isinstance(value, str) else json.dumps(value)).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if url.path == "/":
            self.path = "/system/index.html"
        elif url.path == "/system/ap":
            self.path = "/system/ap/index.html"
        compressed = Path(str(SD / url.path.lstrip("/")) + ".gz")
        if compressed.resolve().is_relative_to(SD) and compressed.is_file():
            body = gzip.decompress(compressed.read_bytes())
            self.send_response(200)
            self.send_header("Content-Type", self.guess_type(url.path))
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        super().do_GET()


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    # 127.0.0.2 is loopback, but avoids the pages' localhost/offline mode.
    server = ThreadingHTTPServer(("127.0.0.2", 0), partial(PreviewHandler, directory=str(SD)))
    server.demoRunning = False
    Thread(target=server.serve_forever, daemon=True).start()
    options = webdriver.EdgeOptions()
    options.add_argument("--headless=new")
    options.add_argument("--window-size=1000,900")
    driver = None
    try:
        driver = webdriver.Edge(options=options)
        driver.execute_cdp_cmd("Emulation.setDeviceMetricsOverride",
                               {"width": 1000, "height": 900, "deviceScaleFactor": 1, "mobile": False})
        pages = {"web_home": "/system/index.html", "web_settings": "/system/settings.html",
                 "web_profile": "/system/profile_editor.html", "web_trickler": "/system/trickler.html",
                 "web_wifi": "/system/ap", "web_files": "/system/resources/edit/index.html?file=/profiles/min.txt",
                 "web_profile_min": "/system/profile_editor.html",
                 "web_trickler_running": "/system/trickler.html"}
        for name, path in pages.items():
            server.demoRunning = name == "web_trickler_running"
            driver.get(f"http://127.0.0.2:{server.server_port}{path}")
            WebDriverWait(driver, 15).until(lambda d: d.execute_script(
                "return document.documentElement.lang === 'de'"))
            if name in ("web_profile", "web_profile_min"):
                WebDriverWait(driver, 15).until(lambda d: d.execute_script(
                    "return document.getElementById('profileSelect').options.length > 1"))
                profile = "min" if name == "web_profile_min" else "avg"
                driver.execute_script("document.getElementById('profileSelect').value = arguments[0]; loadSelectedProfile()", profile)
                WebDriverWait(driver, 15).until(lambda d: d.execute_script(
                    "return document.getElementById('profileName').value === arguments[0]", profile))
            if name == "web_trickler_running":
                WebDriverWait(driver, 15).until(lambda d: d.execute_script(
                    "return document.getElementById('start-stop-btn').classList.contains('button-stop')"))
            if name == "web_wifi":
                WebDriverWait(driver, 15).until(lambda d: d.execute_script(
                    "return document.getElementById('ssid').options.length > 1"))
                driver.execute_script("document.getElementById('ssid').value = 'Beispiel-WLAN'")
            if name == "web_files":
                WebDriverWait(driver, 15).until(lambda d: d.execute_script(
                    "return document.querySelectorAll('#uploader button').length > 0 && "
                    "document.getElementById('editor').innerText.includes('targetWeight')"))
            driver.execute_async_script("const done = arguments[0]; setTimeout(done, 700)")
            if name == "web_trickler_running":
                WebDriverWait(driver, 15).until(lambda d: d.execute_script(
                    "return document.getElementById('start-stop-btn').innerText === 'Stopp'"))
            screenshot = driver.execute_cdp_cmd("Page.captureScreenshot", {"captureBeyondViewport": name != "web_profile"})
            (OUTPUT / f"{name}.png").write_bytes(base64.b64decode(screenshot["data"]))
            if name == "web_profile":
                sections = driver.find_elements("css selector", "#container > fieldset")
                for section, suffix in zip(sections[:2], ("general", "steppers")):
                    captureSection(driver, section, f"web_profile_{suffix}")
                captureSection(driver, driver.find_element("css selector", "#container > fieldset:last-child fieldset"),
                               "web_profile_map_entry")
            print(f"Captured {name}", flush=True)
    finally:
        if driver:
            driver.quit()
        server.shutdown()
        server.server_close()


def captureSection(driver, element, name):
    rect = driver.execute_script(
        "const r = arguments[0].getBoundingClientRect(); "
        "return {x:r.x + scrollX, y:r.y + scrollY, width:r.width, height:r.height, scale:1}", element)
    screenshot = driver.execute_cdp_cmd("Page.captureScreenshot", {"captureBeyondViewport": True, "clip": rect})
    (OUTPUT / f"{name}.png").write_bytes(base64.b64decode(screenshot["data"]))


if __name__ == "__main__":
    main()
