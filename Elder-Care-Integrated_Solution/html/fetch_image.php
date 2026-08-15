<?php
// Database configuration
$host = 'localhost';
$db   = 'ecsys';
$user = 'userecsys';
$pass = 'ecsys123';
$charset = 'utf8mb4';

$dsn = "mysql:host=$host;dbname=$db;charset=$charset";
$options = [
	PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
	PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
	PDO::ATTR_EMULATE_PREPARES   => false,
];

try {
	$pdo = new PDO($dsn, $user, $pass, $options);

	// SQL query to get the most recent image BLOB
	// Replace 'your_table', 'image_blob_column', and 'id' with your actual column names
	$stmt = $pdo->query('SELECT data FROM out_img');
	$row = $stmt->fetch();

	if ($row && !empty($row['data'])) {
		// Set header to image/jpeg (change to image/png if applicable)
		header("Content-Type: image/jpeg");
		echo $row['data'];
	} else {
		// Fallback placeholder if no image is found
		http_response_code(404);
	}
} catch (\PDOException $e) {
	http_response_code(500);
}
?>

