<?php
// Database credentials
$servername = "";
$username = "";
$password = "";
$dbname = "";

$latestVersion = "2.12";

$requestScheme = "http";
$requestHost = isset($_SERVER['HTTP_HOST']) ? $_SERVER['HTTP_HOST'] : "strenuous.dev";
$requestPath = isset($_SERVER['SCRIPT_NAME']) ? dirname($_SERVER['SCRIPT_NAME']) : "/roboTrickler";
$requestPath = str_replace("\\", "/", $requestPath);
$requestPath = rtrim($requestPath, "/");
$sdPackageUrl = $requestScheme . "://" . $requestHost . $requestPath . "/SD-Files.tar";

function getClientIP() {
    $ipaddress = '';
    if (isset($_SERVER['HTTP_CLIENT_IP']))
        $ipaddress = $_SERVER['HTTP_CLIENT_IP'];
    else if (isset($_SERVER['HTTP_X_FORWARDED_FOR']))
        $ipaddress = $_SERVER['HTTP_X_FORWARDED_FOR'];
    else if (isset($_SERVER['HTTP_X_FORWARDED']))
        $ipaddress = $_SERVER['HTTP_X_FORWARDED'];
    else if (isset($_SERVER['HTTP_FORWARDED_FOR']))
        $ipaddress = $_SERVER['HTTP_FORWARDED_FOR'];
    else if (isset($_SERVER['HTTP_FORWARDED']))
        $ipaddress = $_SERVER['HTTP_FORWARDED'];
    else if (isset($_SERVER['REMOTE_ADDR']))
        $ipaddress = $_SERVER['REMOTE_ADDR'];
    else
        $ipaddress = 'UNKNOWN';

    $parts = explode(',', $ipaddress);
    return trim($parts[0]);
}

function trackRequest($servername, $username, $password, $dbname) {
    if ($servername === "" || $username === "" || $dbname === "") {
        return;
    }

    $version = isset($_GET['version']) ? $_GET['version'] : '0.00';
    $mac = isset($_GET['mac']) ? $_GET['mac'] : 'UNKNOWN';
    $ip = getClientIP();
    $city = "";
    $country = "";

    $url = "http://ip-api.com/json/" . urlencode($ip);
    $ch = curl_init($url);
    if ($ch) {
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($ch, CURLOPT_TIMEOUT, 3);
        $result = curl_exec($ch);
        curl_close($ch);

        $data = json_decode($result, true);
        if (is_array($data) && isset($data['status']) && $data['status'] === 'success') {
            $city = isset($data['city']) ? $data['city'] : "";
            $country = isset($data['country']) ? $data['country'] : "";
        }
    }

    $conn = @new mysqli($servername, $username, $password, $dbname);
    if ($conn->connect_error) {
        return;
    }

    $stmt = $conn->prepare("INSERT INTO userTracker (version, mac, ip, city, country) VALUES (?, ?, ?, ?, ?)");
    if ($stmt) {
        $versionNumber = (float)$version;
        $stmt->bind_param("dssss", $versionNumber, $mac, $ip, $city, $country);
        $stmt->execute();
        $stmt->close();
    }

    $conn->close();
}

trackRequest($servername, $username, $password, $dbname);

$format = isset($_GET['format']) ? strtolower($_GET['format']) : "";
if ($format === "json" || $format === "manifest") {
    header('Content-Type: application/json');
    echo json_encode([
        "version" => $latestVersion,
        "sdPackageUrl" => $sdPackageUrl
    ]);
    exit;
}

header('Content-Type: text/plain');
echo $latestVersion;
?>
