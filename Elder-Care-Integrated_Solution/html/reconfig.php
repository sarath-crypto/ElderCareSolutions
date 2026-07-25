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
	$night = 0;
	$ac = 0;
	$akey = 0;
	$aip = 0;
	$aco = 0;
	$voice = 0;
	$motion = 0;
	$sip = 0;
	$bip = 0;
	$sn = 0;
	$bkey = 0;
	$access = 0;

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
		$night = htmlspecialchars($_POST['night']);
		$ac = htmlspecialchars($_POST['ac']);
		$akey = htmlspecialchars($_POST['akey']);
		$aip = htmlspecialchars($_POST['aip']);
		$aco = htmlspecialchars($_POST['aco']);
		$voice = htmlspecialchars($_POST['voice']);
		$motion = htmlspecialchars($_POST['motion']);
		$sip = htmlspecialchars($_POST['sip']);
		$bip = htmlspecialchars($_POST['bip']);
		$sn = htmlspecialchars($_POST['sn']);
		$bkey = htmlspecialchars($_POST['bkey']);
		$access = htmlspecialchars($_POST['access']);
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
			if($night)$sql = $sql."night='".$night."',";	
			if($ac)$sql = $sql."ac='".$ac."',";	
			if($akey)$sql = $sql."akey='".$akey."',";	
			if($aip)$sql = $sql."aip='".$aip."',";	
			if($aco)$sql = $sql."aco=".$aco.",";	
			if($voice)$sql = $sql."voice='".$voice."',";	
			if($motion)$sql = $sql."motion='".$motion."',";	
			if($sip)$sql = $sql."sip='".$sip."',";	
			if($bip)$sql = $sql."bip='".$bip."',";	
			if($sn)$sql = $sql."sn='".$sn."',";	
			if($bkey)$sql = $sql."bkey='".$bkey."',";	
			if($access)$sql = $sql."access='".$access."',";	
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
			sleep(2);
			header("Location: config.php");
		}
	}
?>
