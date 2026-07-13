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
		$filename = 'log.txt';
		$lines = file($filename);
		file_put_contents($filename,'');
		sleep(2);
                header("Location: log.php");

	}else{
		echo '<meta http-equiv = "refresh" content = "0; url = index.php" />';

	}
?>
