<?php
/* ======================================================================
   Siraj  -  update_lamp.php            (ONE file does everything)
   ----------------------------------------------------------------------
   The ESP32 sends one reading every 5 seconds to this page.
   This page does two things with it:

     1) SHEET:    adds one row with ALL readings to  siraj_readings.csv
                  (created next to this file, opens directly in Excel)
     2) DATABASE: updates the `siraj` database exactly like before
                    lamp        -> Lux_Value (road light), Status
                    lampreading -> ambientLight, motionDetected
                    area        -> Latitude, Longitude, AreaName

   The sheet is written first, so the sheet keeps working even if the
   database is stopped.

   Setup:
     - put this file in the Siraj/api/ folder
     - check the database settings below (port 8889 if you use MAMP)
     - the folder must allow writing, so the CSV file can be created
     - to start a new sheet: close Excel, rename or delete the CSV file
   ====================================================================== */

header('Content-Type: application/json; charset=utf-8');
date_default_timezone_set('Asia/Riyadh');   // server_time in Riyadh time

// ---- database settings ----
$DB_HOST = "localhost";
$DB_USER = "root";
$DB_PASS = "root";
$DB_NAME = "siraj";
$DB_PORT = 3306;

// ---- sheet file ----
$CSV_FILE = __DIR__ . "/siraj_readings.csv";

// sheet columns: name => type   (i = whole number, f = decimal, s = short text)
$COLUMNS = [
    "lamp_id"         => "i",   // lamp number
    "seq"             => "i",   // reading number since the device started
    "rtc_time"        => "s",   // device clock
    "uptime_s"        => "i",   // seconds since the device started
    "lux_ambient"     => "f",   // ambient light (lux)
    "lux_road"        => "f",   // light on the road under the lamp (lux)
    "sky_hz"          => "f",   // sky brightness sensor (Hz)
    "human"           => "i",   // 1 = person detected
    "vehicle"         => "i",   // 1 = vehicle detected
    "motion"          => "i",   // 1 = person or vehicle
    "human_state"     => "i",   // 0 none, 1 moving, 2 still, 3 both
    "human_dist_cm"   => "i",   // distance to the person (cm)
    "veh_count"       => "i",   // number of vehicles seen
    "veh_dist_m"      => "i",   // distance to the vehicle (m), -1 = none
    "veh_speed_kmh"   => "i",   // vehicle speed (km/h), -1 = none
    "veh_approaching" => "i",   // 1 = vehicle coming toward the lamp
    "lamp_pct"        => "i",   // lamp brightness (%)
    "lamp_duty_pct"   => "f",   // real PWM duty (%)
    "boost"           => "i",   // 1 = +10% boost active
    "battery_v"       => "f",   // battery voltage (V)
    "lamp_ma"         => "f",   // lamp current (mA)
    "lamp_fault"      => "i",   // 1 = lamp fault
    "temp_c"          => "f",   // temperature (C)
    "status"          => "s",   // on / off
];

// ---- lamp id is required ----
$lamp_id = isset($_POST['lamp_id']) ? (int)$_POST['lamp_id'] : 0;
if ($lamp_id <= 0) {
    http_response_code(400);
    die(json_encode(["status" => "error", "msg" => "no lamp_id"]));
}

$result = ["status" => "ok", "lamp_id" => $lamp_id, "sheet" => "", "database" => ""];

/* ======================================================================
   1) SHEET  -  one row with all readings
   ====================================================================== */
// server_time comes from the laptop clock, so it is correct even if the
// device clock was never set
$row = [date("Y-m-d H:i:s")];
foreach ($COLUMNS as $name => $type) {
    $v = $_POST[$name] ?? "";
    if ($type === "i")     $v = ($v === "") ? "" : (string)(int)$v;
    elseif ($type === "f") $v = ($v === "") ? "" : (string)(float)$v;
    else                   $v = preg_replace('/[^A-Za-z0-9 :\-_.]/', '', substr((string)$v, 0, 32));
    $row[] = $v;
}

$isNew = !file_exists($CSV_FILE) || filesize($CSV_FILE) === 0;
$fp = @fopen($CSV_FILE, "a");
if ($fp) {
    flock($fp, LOCK_EX);
    if ($isNew) {
        fwrite($fp, "\xEF\xBB\xBF");   // lets Excel read the file as UTF-8
        fputcsv($fp, array_merge(["server_time"], array_keys($COLUMNS)));
    }
    fputcsv($fp, $row);
    flock($fp, LOCK_UN);
    fclose($fp);
    $result["sheet"] = "ok";
} else {
    // usually: the folder does not allow writing, or the file is open in Excel
    $result["status"] = "error";
    $result["sheet"]  = "cannot write siraj_readings.csv (folder permission, or file open in Excel)";
}

/* ======================================================================
   2) DATABASE  -  same tables and fields as before
   ====================================================================== */
mysqli_report(MYSQLI_REPORT_OFF);
$conn = @new mysqli($DB_HOST, $DB_USER, $DB_PASS, $DB_NAME, $DB_PORT);
if ($conn->connect_error) {
    http_response_code(500);
    $result["status"]   = "error";
    $result["database"] = "connect failed: " . $conn->connect_error;
    die(json_encode($result));
}
$conn->set_charset("utf8mb4");

// inputs for the database
$lux_old     = isset($_POST['lux'])         ? (float)$_POST['lux']         : 0.0;
$lux_ambient = isset($_POST['lux_ambient']) ? (float)$_POST['lux_ambient'] : $lux_old;
$lux_road    = isset($_POST['lux_road'])    ? (float)$_POST['lux_road']    : $lux_old;
if ($lux_ambient < 0) $lux_ambient = 0;
if ($lux_road    < 0) $lux_road    = 0;

$motion = !empty($_POST['motion']) ? 1 : 0;
$status = (isset($_POST['status']) && strtolower(trim($_POST['status'])) === 'on') ? 'on' : 'off';

$lat = isset($_POST['lat']) ? (float)$_POST['lat'] : 0.0;
$lng = isset($_POST['lng']) ? (float)$_POST['lng'] : 0.0;

// lamp row: road sensor = light the lamp puts on the ground
$stmt = $conn->prepare("UPDATE `lamp` SET `Lux_Value` = ?, `Status` = ? WHERE `LampID` = ?");
if ($stmt) {
    $stmt->bind_param("dsi", $lux_road, $status, $lamp_id);
    $stmt->execute();
    $stmt->close();
}

// reading row: ambient sensor + motion
$stmt = $conn->prepare(
    "INSERT INTO `lampreading` (`LampID`, `ambientLight`, `motionDetected`) VALUES (?, ?, ?)"
);
if (!$stmt || !$stmt->bind_param("idi", $lamp_id, $lux_ambient, $motion) || !$stmt->execute()) {
    http_response_code(400);
    $result["status"]   = "error";
    $result["database"] = "insert failed: " . ($stmt ? $stmt->error : $conn->error);
    $conn->close();
    die(json_encode($result));
}
$stmt->close();
$result["database"] = "ok";

// area position + name: only looked up when the position changes
// (the device sends every 5 s; the map service must not be called every time)
if ($lat != 0 && $lng != 0) {
    $same = false;
    $q = $conn->query("SELECT `Latitude`, `Longitude` FROM `area` WHERE `AreaID` = 1");
    if ($q && ($a = $q->fetch_assoc())) {
        $same = abs((float)$a['Latitude'] - $lat) < 0.00001 && abs((float)$a['Longitude'] - $lng) < 0.00001;
    }

    if (!$same) {
        $url  = "https://nominatim.openstreetmap.org/reverse?lat=$lat&lon=$lng&format=json&accept-language=ar";
        $opts = ['http' => ['header' => "User-Agent: Siraj/1.0\r\n", 'timeout' => 3]];
        $response = @file_get_contents($url, false, stream_context_create($opts));

        $areaName = "Unknown";
        if ($response) {
            $data = json_decode($response, true);
            $areaName = $data['address']['suburb']
                     ?? $data['address']['neighbourhood']
                     ?? $data['address']['city_district']
                     ?? $data['address']['city']
                     ?? "Unknown";
        }

        $stmt = $conn->prepare(
            "UPDATE `area` SET `Latitude` = ?, `Longitude` = ?, `AreaName` = ? WHERE `AreaID` = 1"
        );
        if ($stmt) {
            $stmt->bind_param("dds", $lat, $lng, $areaName);
            $stmt->execute();
            $stmt->close();
        }
    }
}

$conn->close();

$result["lux_ambient"] = $lux_ambient;
$result["lux_road"]    = $lux_road;
$result["motion"]      = $motion;
$result["lamp_status"] = $status;
echo json_encode($result);
?>
