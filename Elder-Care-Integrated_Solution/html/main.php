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
        	echo '</head><body bgcolor="black" style="color: white;">';

        	echo '<h3 style="color: red;">This is a private web site, any unautherized attempt to access will be prosecuted to the maximum permitted by the law.'."[".$ip.']</h3>';
        	echo '<iframe src="videoio.php" frameborder=no scrolling="no" width="640" height="540"></iframe>';
        	echo '<iframe src="disk.php" frameborder=no scrolling="no" width="640" height="540"></iframe>';
        	echo '<iframe src="management.php" frameborder=no scrolling="yes" width="150" height="540"></iframe>';
                echo '<h4 style="color: #0000FF";>Statistics</h4>';
                echo '<iframe src="charts.php" frameborder=no scrolling="no" width="1510" height="820"></iframe>';
        	echo '</body></html>';
	}else{
		echo '<meta http-equiv = "refresh" content = "0; url = index.php" />';

	}
?>
