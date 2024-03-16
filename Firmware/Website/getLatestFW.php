<?php
// Database credentials
$servername = "";
$username = "";
$password = "";
$dbname = "";
$latestVersion = "2.01";

echo $latestVersion;

// Function to get the client IP address
function getClientIP() {
    $ipaddress = '';
    if (isset($_SERVER['HTTP_CLIENT_IP']))
        $ipaddress = $_SERVER['HTTP_CLIENT_IP'];
    else if(isset($_SERVER['HTTP_X_FORWARDED_FOR']))
        $ipaddress = $_SERVER['HTTP_X_FORWARDED_FOR'];
    else if(isset($_SERVER['HTTP_X_FORWARDED']))
        $ipaddress = $_SERVER['HTTP_X_FORWARDED'];
    else if(isset($_SERVER['HTTP_FORWARDED_FOR']))
        $ipaddress = $_SERVER['HTTP_FORWARDED_FOR'];
    else if(isset($_SERVER['HTTP_FORWARDED']))
        $ipaddress = $_SERVER['HTTP_FORWARDED'];
    else if(isset($_SERVER['REMOTE_ADDR']))
        $ipaddress = $_SERVER['REMOTE_ADDR'];
    else
        $ipaddress = 'UNKNOWN';
    return $ipaddress;
}

// Set parameters and execute
$id = NULL; // Assuming ID is auto-increment

// Retrieve MAC and IP
$mac = isset($_GET['mac']) ? $_GET['mac'] : 'UNKNOWN';
$ip = getClientIP();


// Use a geolocation service
$url = "http://ip-api.com/json/$ip";
$ch = curl_init($url);
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
$result = curl_exec($ch);
curl_close($ch);

// Decode the JSON response
$data = json_decode($result, true);

// Check if the request was successful
if ($data['status'] == 'success') {
    $city = $data['city'];
    $country= $data['country'];
    // You can also echo other information like country, region, etc.
}
// Create connection
$conn = new mysqli($servername, $username, $password, $dbname);

// Check connection
if ($conn->connect_error) {
    die("Connection failed: " . $conn->connect_error);
}

// Prepare and bind
$stmt = $conn->prepare("INSERT INTO userTracker (id, mac, ip, city,country) VALUES (?, ?, ?, ?, ?)");
$stmt->bind_param("issss", $id, $mac, $ip, $city, $country);

$stmt->execute();

$stmt->close();
$conn->close();
?>