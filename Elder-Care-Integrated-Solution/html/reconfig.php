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

	$mouse_levela = 0;
	$mouse_levelb = 0;
	$mouse_bhrs = 0;
	$mouse_name = 0;
	$mouse_index = 0;
	$beacon_timeout = 0;
	$dir_max = 0;
	$voice_th = 0;
	$voice = 0;
	$ac = 0;
	$cam = 0;
	$skey = 0;
	$aip = 0;
	$sip = 0;
	$access = 0;
	$night = 0;
	$motion = 0;
	$aco = 0;
	$photo = 0;
	$x = -1;
	$y = -1;
	$w = -1;
	$h = -1;
	$id = 0;
	
        if ($_SERVER["REQUEST_METHOD"] == "POST") {
		$mouse_levela = htmlspecialchars($_POST['mouse_levela']);
		$mouse_levelb = htmlspecialchars($_POST['mouse_levelb']);
		$mouse_bhrs = htmlspecialchars($_POST['mouse_bhrs']);
		$mouse_name = htmlspecialchars($_POST['mouse_name']);
		$mouse_index = htmlspecialchars($_POST['mouse_index']);
		$beacon_timeout = htmlspecialchars($_POST['beacon_timeout']);
		$dir_max = htmlspecialchars($_POST['dir_max']);
		$voice_th = htmlspecialchars($_POST['voice_th']);
		$voice = htmlspecialchars($_POST['voice']);
		$ac = htmlspecialchars($_POST['ac']);
		$cam = htmlspecialchars($_POST['cam']);
		$skey = htmlspecialchars($_POST['skey']);
		$aip = htmlspecialchars($_POST['aip']);
		$sip = htmlspecialchars($_POST['sip']);
		$access = htmlspecialchars($_POST['access']);
		$night = htmlspecialchars($_POST['night']);
		$motion = htmlspecialchars($_POST['motion']);
		$aco = htmlspecialchars($_POST['aco']);
		$photo = htmlspecialchars($_POST['photo']);
		$x =  htmlspecialchars($_POST['x']);
		$y =  htmlspecialchars($_POST['y']);
		$w =  htmlspecialchars($_POST['w']);
		$h =  htmlspecialchars($_POST['h']);
		$id =  htmlspecialchars($_POST['id']);
	}

        if($_SESSION['auth']){
                $conn = new mysqli('localhost','userecsys','ecsys123','ecsys');
                if ($conn->connect_error) {
                        die("Connection failed: " . $conn->connect_error);
		}else{
			$sql = "UPDATE cfg SET ";
			if($mouse_levela >= 0)$sql = $sql."mouse_levela=".$mouse_levela.",";	
			if($mouse_levelb >= 0)$sql = $sql."mouse_levelb=".$mouse_levelb.",";	
			if($mouse_bhrs)$sql = $sql."mouse_bhrs='".$mouse_bhrs."',";	
			if($mouse_name)$sql = $sql."mouse_name='".$mouse_name."',";	
			if($mouse_index >= 0)$sql = $sql."mouse_index='".$mouse_index."',";	
			if($beacon_timeout >= 0)$sql = $sql."beacon_timeout=".$beacon_timeout.",";	
			if($dir_max >= 0)$sql = $sql."dir_max=".$dir_max.",";	
			if($voice_th >= 0)$sql = $sql."voice_th=".$voice_th.",";	
			if($voice)$sql = $sql."voice='".$voice."',";	
			if($ac)$sql = $sql."ac='".$ac."',";	
			if($cam)$sql = $sql."cam='".$cam."',";	
			if($skey)$sql = $sql."skey='".$skey."',";	
			if($aip)$sql = $sql."aip='".$aip."',";	
			if($sip)$sql = $sql."sip='".$sip."',";	
			if($access)$sql = $sql."access='".$access."',";	
			if($night)$sql = $sql."night='".$night."',";	
			if($motion)$sql = $sql."motion='".$motion."',";	
			if($aco)$sql = $sql."aco=".$aco.",";	
			if($photo)$sql = $sql."photo='".$photo."',";	
			$sql = rtrim($sql,",");
			if($sql != "UPDATE cfg SET "){
				$result = $conn->query($sql);
			}
			if(($x >= 0) && ($y >= 0) && ($w >= 0) && ($h >= 0)){
				$sql = "INSERT INTO mask (x,y,w,h) values(".$x.",".$y.",".$w.",".$h.")";
                        	$result = $conn->query($sql);
			}
			if($id){
				$sql = "DELETE FROM mask WHERE id=$id";
                        	$result = $conn->query($sql);
			}
			sleep(5);
			header("Location: config.php");
		}
	}
?>
