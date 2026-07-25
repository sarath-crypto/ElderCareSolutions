<?php
	session_start();
	
	if ($_SERVER["REQUEST_METHOD"] == "POST") {
		$pass = htmlspecialchars($_POST['pass_word']);
		if (empty($pass) == FALSE){
			$conn = new mysqli('localhost','userecsys','ecsys123','ecsys');
			if ($conn->connect_error) {
				die("Connection failed: " . $conn->connect_error);
			}else{
				$sql = "SELECT access FROM cfg";	
				$result = $conn->query($sql);
				$dbpass = "";
				if ($result->num_rows > 0) {
					$row = $result->fetch_assoc();
					$dbpass = $row['access'];
				}
				if($pass == $dbpass){
					$_SESSION['auth'] = true;
				}
				$conn->close();
			}
		}
	}
	if($_SESSION['auth']){
		$ip = $_SERVER["REMOTE_ADDR"];
        	date_default_timezone_set('Asia/Kolkata');
        	echo '<!DOCTYPE html><html><head><title>Elder Care System</title>';
        	echo '</head><body bgcolor="black">';
		echo '<br><a href="summary.php" target="_blank">Show Summary</a>';
		echo '<br><a href="config.php" target="_blank">Show Configuration</a>';
		echo '<br><a href="log.php" target="_blank">Show Logs</a>';
        	echo '</body></html>';
	}else{
		echo '<meta http-equiv = "refresh" content = "0; url = index.php" />';

	}
?>
