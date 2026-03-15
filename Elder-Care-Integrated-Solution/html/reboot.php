<?php
	session_start();
        if(!isset($_SESSION['auth'])){
                header("Location: index.php");
                exit();
        }
        if($_SESSION['auth'] == 0){
                header("Location: index.php");
                exit();
	}
	$reboot = 0;
        if ($_SERVER["REQUEST_METHOD"] == "POST") {
		$reboot =  htmlspecialchars($_POST['reboot']);
	}
        if($_SESSION['auth']){
                $conn = new mysqli('localhost','userecsys','ecsys123','ecsys');
                if ($conn->connect_error) {
                        die("Connection failed: " . $conn->connect_error);
		}else{
			$sql = "UPDATE cfg SET ";
			if($reboot >= 0)$sql = $sql."reboot=1";	
			if($sql != "UPDATE cfg SET "){
				echo $sql;
				$result = $conn->query($sql);
			}
			sleep(5);
			header("Location: config.php");
		}
	}
?>
