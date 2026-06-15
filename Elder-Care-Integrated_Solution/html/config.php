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
	
	echo '<!DOCTYPE html><html><head><style>table,th,td{border: 1px solid white;border-collapse: collapse;}</style></head><body>';

	$conn = new mysqli('localhost','userecsys','ecsys123','ecsys');
        if ($conn->connect_error) {
                die("Connection failed: " . $conn->connect_error);
        }else{
		echo '<table><tr>';
		echo '<th style="color:blue">System Configurations</th>';
		echo '<th style="color:red">Application Parameters</th>';
		echo '</tr><tr><td>';
		$sql = "SELECT * FROM syscon";
		$result = $conn->query($sql);
		if ($result->num_rows > 0) {
			echo '<table>';
			echo '<tr>';
			echo '<th bgcolor="Limegreen">ID</th>';
			echo '<th bgcolor="Limegreen">NAME</th>';
			echo '</tr>';
			while($row = $result->fetch_assoc()) {
				echo '<tr width=100%>';
				echo '<td bgcolor="Limegreen">';
				echo $row['id'];
				echo '</td>';
				echo '<td bgcolor="Limegreen">';
				echo $row['name'];
				echo '</tr>';
			}
		}
		echo '</table>';
		echo '</td><td>';
		
		$sql = "SELECT * FROM cfg";
		$result = $conn->query($sql);
		if ($result->num_rows > 0) {
			while($row = $result->fetch_assoc()){
 				$mouse_levela = $row['mouse_levela'];
 				$mouse_levelb = $row['mouse_levelb'];
 				$mouse_bhrs = $row['mouse_bhrs'];
        			$mouse_name = $row['mouse_name'];
        			$mouse_index = $row['mouse_index'];
        			$beacon_timeout = $row['beacon_timeout'];
        			$dir_max = $row['dir_max'];
        			$night = $row['night'];
        			$ac = $row['ac'];
        			$akey = $row['akey'];
				$aip = $row['aip'];
        			$aco = $row['aco'];
        			$voice = $row['voice'];
        			$motion = $row['motion'];
				$sip = $row['sip'];
				$mip = $row['mip'];
				$wip = $row['wip'];
				$bip = $row['bip'];
				$sn = $row['sn'];
        			$mkey = $row['mkey'];
        			$wkey = $row['wkey'];
        			$bkey = $row['bkey'];
        			$whr = $row['whr'];
        			$wlo = $row['wlo'];
        			$whi = $row['whi'];
				$access = $row['access'];
			}
			
			echo '<table><tr>';
			echo '</tr><tr><td>';

			echo '<form action="reconfig.php" method="POST" id="action-form">';
			echo '<table><tr>';
			echo '<tr width=100%><td bgcolor="orange">MOUSE_LEVEL_A[0-5]</td><td><input type="number" style="width: 250px" id="mouse_levela" name="mouse_levela" value="';
			echo $mouse_levela;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="orange">MOUSE_LEVEL_B[0-5]</td><td><input type="number" style="width: 250px" id="mouse_levelb" name="mouse_levelb" value="';
			echo $mouse_levelb;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="orange">MOUSE_LEVEL_B_HOURS[0,1,2...23]</td><td><input type="text" style="width: 250px" id="mouse_bhrs" name="mouse_bhrs" value="';
			echo $mouse_bhrs;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="orange">MOUSE_NAME</td><td><input type="text" style="width: 250px" id="mouse_name" name="mouse_name" value="';
			echo $mouse_name;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="orange">MOUSE_INDEX</td><td><input type="text" style="width: 250px" id="mouse_index" name="mouse_index" value="';
			echo $mouse_index;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="orange">BEACON_TIMEOUT[1-255]</td><td><input type="number" style="width: 250px" id="beacon_timeout" name="beacon_timeout" value="';
			echo $beacon_timeout;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="orange">DIR_MAX[1-255]</td><td><input type="number" style="width: 250px" id="dir_max" name="dir_max" value="';
			echo $dir_max;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="orange">NIGHT MODE HOURS[0,1,2...23]</td><td><input type="text" style="width: 250px" id="night" name="night" value="';
			echo $night;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="orange">AC ON HOURS[0,1,2...23]</td><td><input type="text" style="width: 250px" id="ac" name="ac" value="';
			echo $ac;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="orange">ALARM KEY</td><td><input type="text" style="width: 250px" id="akey" name="akey" value="';
			echo $akey;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="orange">ALARM IP</td><td><input type="text" style="width: 250px" id="aip" name="aip" value="';
			echo $aip;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="orange">AC CUT OFF TEMPERATURE</td><td><input type="text" style="width: 250px" id="aco" name="aco" value="';
			echo $aco;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="gold">VOICE DETECTION HOURS[0,1,2...23]</td><td><input type="text" style="width: 250px" id="voice" name="voice" value="';
			echo $voice;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="gold">MOTION DETECTION HOURS[0,1,2...23]</td><td><input type="text" style="width: 250px" id="motion" name="motion" value="';
			echo $motion;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="gold">SOLAR IP</td><td><input type="text" style="width: 250px" id="sip" name="sip" value="';
			echo $sip;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="gold">MOTOR IP</td><td><input type="text" style="width: 250px" id="mip" name="mip" value="';
                        echo $mip;
                        echo '"></td></tr>';
                        echo '<tr width=100%><td bgcolor="gold">SENSOR IP</td><td><input type="text" style="width: 250px" id="wip" name="wip" value="';
                        echo $wip;
			echo '"></td></tr>';
                        echo '<tr width=100%><td bgcolor="gold">IR BLASTER IP</td><td><input type="text" style="width: 250px" id="bip" name="bip" value="';
                        echo $bip;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="gold">Solar Serial Number</td><td><input type="text" style="width: 250px" id="sn" name="sn" value="';
                        echo $sn;
			echo '"></td></tr>';
                        echo '<tr width=100%><td bgcolor="gold">MOTOR KEY</td><td><input type="text" style="width: 250px" id="mkey" name="mkey" value="';
                        echo $mkey;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="gold">SENSOR KEY</td><td><input type="text" style="width: 250px" id="wkey" name="wkey" value="';
                        echo $wkey;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="gold">IR BLASTER KEY</td><td><input type="text" style="width: 250px" id="bkey" name="bkey" value="';
                        echo $bkey;
			echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="gold">Motor Hour</td><td><input type="text" style="width: 250px" id="phr" name="phr" value="';
                        echo $whr;
                        echo '"></td></tr>';
                        echo '<tr width=100%><td bgcolor="gold">Water Low</td><td><input type="text" style="width: 250px" id="wlo" name="wlo" value="';
                        echo $wlo;
                        echo '"></td></tr>';
                        echo '<tr width=100%><td bgcolor="gold">Water High</td><td><input type="text" style="width: 250px" id="whi" name="whi" value="';
                        echo $whi;
                        echo '"></td></tr>';
			echo '<tr width=100%><td bgcolor="red">ACCESS_PASSWORD</td><td><input type="text" style="width: 250px" id="access" name="access" value="';
			echo $access;
			echo '"></td><td><input type="submit" value="Submit"></td></tr>';
			echo '</table>';
			echo '</form>';
			echo '</td></tr>';
			echo '</td></tr></table>';
		}
		
		echo '</table>';

		echo '<h3 style="color:Blue;">Select the image area for motion detection</h3>';
		$sql = "SELECT * FROM mask";
		$result = $conn->query($sql);
		if ($result->num_rows > 0) {
			echo '<table bgcolor="Limegreen">';
			while($row = $result->fetch_assoc()) {
				echo '<tr><td >ID</td><td>';
				echo $row['id'];
				echo '</td><td>X</td><td>';
				echo $row['x'];
				echo '</td><td>Y</td><td>';
				echo $row['y'];
				echo '</td><td>W</td><td>';
				echo $row['w'];
				echo '</td><td>H</td><td>';
				echo $row['h'];
				echo '</td></tr>';
			}
			echo '</table>';
		}

		echo '<table><tr>';
		echo '<th style="color:red">Mask Add</th>';
		echo '<th style="color:red">Mask Remove</th>';
		echo '</tr><tr><td>';
		echo '<form action="reconfig.php" method="POST" id="action-form">';
		echo '<table>';
		echo '<tr width=100%><td>X</td><td><input type="number" id="x" name="x"></td></tr>';
		echo '<tr width=100%><td>Y</td><td><input type="number" id="y" name="y"></td></tr>';
		echo '<tr width=100%><td>W</td><td><input type="number" id="w" name="w"></td></tr>';
		echo '<tr width=100%><td>H</td><td><input type="number" id="h" name="h"></td>';
		echo '<td><input type="submit" value="Add"></td></tr>';
		echo '</table><td><table>';
		echo '<tr width=100%><td>ID</td><td><input type="number" id="id" name="id"></td>';
		echo '<td><input type="submit" value="Remove"></td></tr>';
		echo '</table>';
		echo '</form>';
		echo '</td></tr></table>';

		echo '<canvas id="myCanvas" width="640" height="480" style="border:1px solid black;background-color: lightblue;">Browser does not support canvas</canvas>';
		echo '<script> const canvas = document.getElementById("myCanvas");';
		echo 'const ctx = canvas.getContext("2d");';

		$sql = "SELECT * FROM mask";
		$result = $conn->query($sql);
		if ($result->num_rows > 0) {
			while($row = $result->fetch_assoc()){
 				$x = $row['x'];
        			$y = $row['y'];
        			$w = $row['w'];
				$h = $row['h'];
				echo 'ctx.fillStyle = "blue";';
				echo 'ctx.fillRect('.$x.','.$y.','.$w.','.$h.');';
			}
		}

                echo 'ctx.beginPath();';
                echo 'ctx.moveTo(0,120);';
                echo 'ctx.lineTo(640,120);';
                echo 'ctx.moveTo(0,240);';
                echo 'ctx.lineTo(640,240);';
                echo 'ctx.moveTo(0,360);';
                echo 'ctx.lineTo(640,360);';
                echo 'ctx.moveTo(160,0);';
                echo 'ctx.lineTo(160,480);';
                echo 'ctx.moveTo(320,0);';
                echo 'ctx.lineTo(320,480);';
                echo 'ctx.moveTo(480,0);';
                echo 'ctx.lineTo(480,480);';
                echo 'ctx.stroke();';

                echo 'ctx.font = "20px Arial ";';
                echo 'ctx.fillStyle = "red";';
                echo 'ctx.fillText("160",120,20,50);';
                echo 'ctx.fillText("320",280,20,50);';
                echo 'ctx.fillText("480",440,20,50);';
                echo 'ctx.fillText("640",600,20,50);';
                echo 'ctx.fillText("120",5,115,50);';
                echo 'ctx.fillText("240",5,235,50);';
                echo 'ctx.fillText("360",5,355,50);';
                echo 'ctx.fillText("480",5,475,50);';
		echo '</script>';

		echo '<form action="reboot.php" method="POST" id="action-form">';
		echo '<table>';
		echo '<td><input type="submit" id="reboot" name="reboot" value="REBOOT"></td></tr>';
		echo '</table>';
		echo '</form>';

		$conn->close();
	}
?>
