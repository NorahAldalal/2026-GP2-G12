<?php
$conn = new mysqli("localhost", "root", "root", "siraj", 3306);

if ($conn->connect_error) {
    die("Connection failed: " . $conn->connect_error);
}

$lamp_id        = (int)$_POST['lamp_id'];
$ambientLight   = (float)$_POST['ambientLight'];
$motionDetected = (int)$_POST['motionDetected'];

$stmt = $conn->prepare("INSERT INTO LampReading (LampID, ambientLight, motionDetected, readingTime)
                        VALUES (?, ?, ?, NOW())");
$stmt->bind_param("idi", $lamp_id, $ambientLight, $motionDetected);

if ($stmt->execute()) {
    echo "Reading inserted OK";
} else {
    echo "Error: " . $stmt->error;
}

$stmt->close();
$conn->close();
?>