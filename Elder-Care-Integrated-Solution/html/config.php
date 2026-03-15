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
	
	echo '<!DOCTYPE html><html><head><style>table,th,td{border: 1px solid white;border-collapse: collapse;}th,td{background-color: #96D4D4;}</style></head><body>';
	echo "<a href='main.php'>Back to main page</a>";

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
			echo '<th>ID</th>';
			echo '<th>NAME</th>';
			echo '</tr>';
			while($row = $result->fetch_assoc()) {
				echo '<tr width=100%>';
				echo '<td>';
				echo $row['id'];
				echo '</td>';
				echo '<td>';
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
 				$mouse_level = $row['mouse_level'];
        			$mouse_name = $row['mouse_name'];
        			$mouse_index = $row['mouse_index'];
        			$beacon_timeout = $row['beacon_timeout'];
        			$dir_max = $row['dir_max'];
        			$voice_th = $row['voice_th'];
        			$voice = $row['voice'];
        			$ac = $row['ac'];
        			$cam = $row['cam'];
        			$skey = $row['skey'];
				$access = $row['access'];
				$aip = $row['aip'];
				$sip = $row['sip'];
        			$night = $row['night'];
        			$motion = $row['motion'];
				$photo = $row['photo'];
			}
			
			echo '<table><tr>';
			echo '</tr><tr><td>';

			echo '<form action="reconfig.php" method="POST" id="action-form">';
			echo '<table><tr>';
			echo '<tr width=100%><td>MOUSE_LEVEL[0-5]</td><td><input type="number" style="width: 250px" id="mouse_level" name="mouse_level" value="';
			echo $mouse_level;
			echo '"></td></tr>';
			echo '<tr width=100%><td>MOUSE_NAME</td><td><input type="text" style="width: 250px" id="mouse_name" name="mouse_name" value="';
			echo $mouse_name;
			echo '"></td></tr>';
			echo '<tr width=100%><td>MOUSE_INDEX</td><td><input type="text" style="width: 250px" id="mouse_index" name="mouse_index" value="';
			echo $mouse_index;
			echo '"></td></tr>';
			echo '<tr width=100%><td>BEACON_TIMEOUT[1-255]</td><td><input type="number" style="width: 250px" id="beacon_timeout" name="beacon_timeout" value="';
			echo $beacon_timeout;
			echo '"></td></tr>';
			echo '<tr width=100%><td>DIR_MAX[1-255]</td><td><input type="number" style="width: 250px" id="dir_max" name="dir_max" value="';
			echo $dir_max;
			echo '"></td></tr>';
			echo '<tr width=100%><td>VOICE_THRESHOLD[>13100]</td><td><input type="number" style="width: 250px" id="voice_th" name="voice_th" value="';
			echo $voice_th;
			echo '"></td></tr>';
			echo '<tr width=100%><td>VOICE DETECTION HOURS[0,1,2...23]</td><td><input type="text" style="width: 250px" id="voice" name="voice" value="';
			echo $voice;
			echo '"></td></tr>';
			echo '<tr width=100%><td>NIGHT MODE HOURS[0,1,2...23]</td><td><input type="text" style="width: 250px" id="night" name="night" value="';
			echo $night;
			echo '"></td></tr>';
			echo '<tr width=100%><td>MOTION DETECTION HOURS[0,1,2...23]</td><td><input type="text" style="width: 250px" id="motion" name="motion" value="';
			echo $motion;
			echo '"></td></tr>';
			echo '<tr width=100%><td>AC ON HOURS[0,1,2...23]</td><td><input type="text" style="width: 250px" id="ac" name="ac" value="';
			echo $ac;
			echo '"></td></tr>';
			echo '<tr width=100%><td>CAMERA NAME(no)</td><td><input type="text" style="width: 250px" id="cam" name="cam" value="';
			echo $cam;
			echo '"></td></tr>';
			echo '<tr width=100%><td>SECURITY_KEY</td><td><input type="text" style="width: 250px" id="skey" name="skey" value="';
			echo $skey;
			echo '"></td></tr>';
			echo '<tr width=100%><td>ALARM IP</td><td><input type="text" style="width: 250px" id="aip" name="aip" value="';
			echo $aip;
			echo '"></td></tr>';
			echo '<tr width=100%><td>SOLAR IP</td><td><input type="text" style="width: 250px" id="sip" name="sip" value="';
			echo $sip;
			echo '"></td></tr>';
			echo '<tr width=100%><td>ACCESS_PASSWORD</td><td><input type="text" style="width: 250px" id="access" name="access" value="';
			echo $access;
			echo '"></td></tr>';
			echo '<tr width=100%><td>PHOTO_MODE[yes/no]</td><td><input type="text" style="width: 250px" id="photo" name="photo" value="';
			echo $photo;
			echo '"></td><td><input type="submit" value="Submit"></td></tr>';
			echo '</table>';
			echo '</form>';
			echo '</td></tr>';
			echo '</td></tr></table>';
		}
		
		echo '</table>';

		$sql = "SELECT * FROM mask";
		$result = $conn->query($sql);
		if ($result->num_rows > 0) {
			echo '<table>';
			echo '<th style="color:blue">MASKS</th>';
			while($row = $result->fetch_assoc()) {
				echo '<tr><td>ID</td><td>';
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

		echo '<canvas id="myCanvas" width="640" height="360" style="border:1px solid black">Browser does not support canvas</canvas>';
		echo '<script> const canvas = document.getElementById("myCanvas");';
		echo 'const ctx = canvas.getContext("2d");';

		$sql = "SELECT * FROM mask";
		$result = $conn->query($sql);
		if ($result->num_rows > 0) {
			while($row = $result->fetch_assoc()){
 				$x = $row['x']/2;
        			$y = $row['y']/2;
        			$w = $row['w']/2;
				$h = $row['h']/2;
				echo 'ctx.fillStyle = "blue";';
				echo 'ctx.fillRect('.$x.','.$y.','.$w.','.$h.');';
			}
		}

                echo 'ctx.beginPath();';
                echo 'ctx.moveTo(0,90);';
                echo 'ctx.lineTo(640,90);';
                echo 'ctx.moveTo(0,180);';
                echo 'ctx.lineTo(640,180);';
                echo 'ctx.moveTo(0,270);';
                echo 'ctx.lineTo(640,270);';
                echo 'ctx.moveTo(160,0);';
                echo 'ctx.lineTo(160,384);';
                echo 'ctx.moveTo(320,0);';
                echo 'ctx.lineTo(320,384);';
                echo 'ctx.moveTo(480,0);';
                echo 'ctx.lineTo(480,384);';
                echo 'ctx.stroke();';

                echo 'ctx.font = "20px Arial ";';
                echo 'ctx.fillStyle = "red";';
                echo 'ctx.fillText("320",120,20,50);';
                echo 'ctx.fillText("640",280,20,50);';
                echo 'ctx.fillText("960",440,20,50);';
                echo 'ctx.fillText("180",5,85,50);';
                echo 'ctx.fillText("360",5,175,50);';
                echo 'ctx.fillText("540",5,265,50);';
		echo '</script>';

		echo '<form action="reboot.php" method="POST" id="action-form">';
		echo '<table>';
		echo '<td><input type="submit" id="reboot" name="reboot" value="REBOOT"></td></tr>';
		echo '</table>';
		echo '</form>';

		$conn->close();
	}
?>
