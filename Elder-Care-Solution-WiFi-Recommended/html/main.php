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
		header("Refresh: 1");
		$conn = new mysqli('localhost','userecsys','ecsys123','ecsys');
		if ($conn->connect_error) {
			die("Connection failed: " . $conn->connect_error);
		}else{
			echo    '<style>
                                table, th, td {
                                border: 1px solid black;
                                }
                                th, td {
                                background-color: #96D4D4;
                                }</style>';

                        echo '<table>';
		
			echo '<tr style="height:200px;text-align: center; vertical-align: middle;">';
                        echo '<td width="200px">';
			$sql = "SELECT ts,data FROM out_img";	
			$result = $conn->query($sql);
			if ($result->num_rows > 0) {
				while($row = $result->fetch_assoc()) {
					echo '<div class="caption"><h3><img src="data:image/jpeg;base64,'.base64_encode($row['data']).'"/></br>'. $row['ts']. '</h3></div>';
				}						
			}
                        echo '</td>';
			
                        echo '<td width="200px">';
			$sql = "SELECT ts,data FROM in_img";	
			$result = $conn->query($sql);
			if ($result->num_rows > 0) {
				while($row = $result->fetch_assoc()) {
					echo '<div class="caption"><h3><img src="data:image/jpeg;base64,'.base64_encode($row['data']).'"/></br>'. $row['ts']. '</h3></div>';
				}						
			}
                        echo '</td>';
                        echo '</tr>';
			
			echo '</table>';
			$conn->close();
		}
		echo '<a href="summary.php">Show Summary</a>';
		echo '<br><a href="config.php">Show Configuration</a>';
	}else{
		echo '<meta http-equiv = "refresh" content = "0; url = index.php" />';

	}
?>
