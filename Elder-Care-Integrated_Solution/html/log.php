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
        	date_default_timezone_set('Asia/Kolkata');
		echo '<!DOCTYPE html><html><head><title>Logs</title>';
		echo '</head><body>';
		$lines = file('log.txt');
		$count = count($lines);
		for ($i = $count - 1; $i >= 0; $i--) {
    			echo htmlspecialchars($lines[$i]) . "<br>";
		}

		echo '<form action="dlog.php" method="POST" id="action-form">';
                echo '<input type="submit" value="Delete last 10 lines">';
                echo '</form>';

        	echo '</body></html>';
	}else{
		echo '<meta http-equiv = "refresh" content = "0; url = index.php" />';

	}
?>
