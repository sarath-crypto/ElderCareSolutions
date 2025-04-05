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
		echo '<th style="color:blue">Network Configuration</th>';
		echo '<th style="color:blue">Mouse Configuration</th>';
		echo '<th style="color:blue">Camera Configuration</th>';
		echo '<th style="color:blue">Microphone Configuration</th>';
		echo '</tr><tr><td>';
                $sql = "SELECT name,status FROM net";
		$result = $conn->query($sql);
		if ($result->num_rows > 0) {
			echo '<table>';
			echo '<tr>';
			echo '<th>NAME</th>';
			echo '<th>STATUS</th>';
			echo '</tr>';
			while($row = $result->fetch_assoc()) {
				echo '<tr width=100%>';
				echo '<td>';
				echo $row['name'];
				echo '</td>';
				echo '<td>';
				echo $row['status'];
				echo '</td>';
				echo '</tr>';
			}
			echo '</table>';
		}
		echo '</td><td>';
		$sql = "SELECT * FROM mouse";
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
			echo '</table>';
		}
		echo '</td><td>';
		$sql = "SELECT * FROM camera";
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
			echo '</table>';
		}
		echo '</td><td>';
		$sql = "SELECT * FROM micro";
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
			echo '</table>';
		}
		echo '</td></tr></table>';

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

		$sql = "SELECT * FROM cfg";
		$result = $conn->query($sql);
		if ($result->num_rows > 0) {
			while($row = $result->fetch_assoc()){
 				$mouse_level = $row['mouse_level'];
        			$mouse_name = $row['mouse_name'];
        			$mouse_index = $row['mouse_index'];
        			$beacon_timeout = $row['beacon_timeout'];
        			$dir_max = $row['dir_max'];
        			$camera = $row['camera'];
        			$micro = $row['micro'];
        			$voice_threshold = $row['voice_threshold'];
        			$voice_start = $row['voice_start'];
        			$voice_duration = $row['voice_duration'];
        			$sec = $row['sec'];
				$access = $row['access'];
				$res_reboot = $row['res_reboot'];
			}
			
			echo '<br><table><tr>';
			echo '<th style="color:red">Change Application Parameters</th>';
			echo '<th style="color:red">Change Application Parameters</th>';
			echo '<th style="color:red">Change Network Configuration</th>';
			echo '</tr><tr><td>';

			echo '<form action="reconfig.php" method="POST" id="action-form">';
			echo '<table><tr>';
			echo '<tr width=100%><td>MOUSE_LEVEL[0-5]</td><td><input type="number" id="mouse_level" name="mouse_level" value="';
			echo $mouse_level;
			echo '"></td></tr>';
			echo '<tr width=100%><td>MOUSE_NAME</td><td><input type="text" id="mouse_name" name="mouse_name" value="';
			echo $mouse_name;
			echo '"></td></tr>';
			echo '<tr width=100%><td>MOUSE_INDEX</td><td><input type="text" id="mouse_index" name="mouse_index" value="';
			echo $mouse_index;
			echo '"></td></tr>';
			echo '<tr width=100%><td>BEACON_TIMEOUT[1-255]</td><td><input type="number" id="beacon_timeout" name="beacon_timeout" value="';
			echo $beacon_timeout;
			echo '"></td></tr>';
			echo '<tr width=100%><td>DIR_MAX[1-255]</td><td><input type="number" id="dir_max" name="dir_max" value="';
			echo $dir_max;
			echo '"></td></tr>';
			echo '<tr width=100%><td>CAMERA</td><td><input type="text" id="camera" name="camera" value="';
			echo $camera;
			echo '"></td></tr>';
			echo '<tr width=100%><td>MICRO</td><td><input type="text" id="micro" name="micro" value="';
			echo $micro;
			echo '"></td></tr>';
			echo '</table></td><td><table>';
			echo '<tr width=100%><td>VOICE_THRESHOLD[>90]</td><td><input type="number" id="voice_threshold" name="voice_threshold" value="';
			echo $voice_threshold;
			echo '"></td></tr>';
			echo '<tr width=100%><td>VOICE_START[0-23]</td><td><input type="number" id="voice_start" name="voice_start" value="';
			echo $voice_start;
			echo '"></td></tr>';
			echo '<tr width=100%><td>VOICE_DURATION[0-24]</td><td><input type="number" id="voice_duration" name="voice_duration" value="';
			echo $voice_duration;
			echo '"></td></tr>';
			echo '<tr width=100%><td>SEC</td><td><input type="text" id="sec" name="sec" value="';
			echo $sec;
			echo '"></td></tr>';
			echo '<tr width=100%><td>ACCESS</td><td><input type="text" id="access" name="access" value="';
			echo $access;
			echo '"></td></tr>';
			echo '<tr width=100%><td>REBOOT_THRESHOLD[60-100]</td><td><input type="text" id="res_reboot" name="res_reboot" value="';
			echo $res_reboot;
			echo '"></td><td><input type="submit" value="Submit"></td></tr>';
			echo '</table>';
			echo '</form>';
			echo '</td><td>';
			echo '<form action="reconfig.php" method="POST" id="action-form">';
			echo '<table>';
			echo '<tr width=100%><td>SSID</td><td><input type="text" id="ssid" name="ssid"></td></tr>';
			echo '<tr width=100%><td>PASSWORD</td><td><input type="text" id="password" name="password"></td></tr>';
			echo '<tr width=100%><td>WiFi mode</td><td><select name="mode" id="mode"><option value="STA">STA</option><option value="AP">AP</option></select>';
			echo '<select name="chng" id="chng"><option value="ADD">ADD</option><option value="REMOVE">REMOVE</option></select></td><td>';
			echo '<input type="submit" value="Submit"></td></tr>';
			echo '</table>';
			echo '</form>';
			echo '</td></tr>';
			echo '</td></tr></table>';
		}

		echo '<table><tr>';
		echo '<th style="color:red">Mask Add</th>';
		echo '<th style="color:red">Mask Remove</th>';
		echo '<th style="color:red">System Reboot</th>';
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
		echo '</table><td><table>';
		echo '<td><input type="submit" id="reboot" name="reboot" value="REBOOT"></td></tr>';
		echo '<td></table>';
		echo '</form>';
		echo '</td></tr></table>';

		echo '<canvas id="myCanvas" width="960" height="540" style="border:1px solid black">Browser does not support canvas</canvas>';
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
                echo 'ctx.moveTo(0,135);';
                echo 'ctx.lineTo(960,135);';
                echo 'ctx.moveTo(0,270);';
                echo 'ctx.lineTo(960,270);';
                echo 'ctx.moveTo(0,405);';
                echo 'ctx.lineTo(960,405);';
                echo 'ctx.moveTo(240,0);';
                echo 'ctx.lineTo(240,540);';
                echo 'ctx.moveTo(480,0);';
                echo 'ctx.lineTo(480,540);';
                echo 'ctx.moveTo(720,0);';
                echo 'ctx.lineTo(720,540);';
                echo 'ctx.stroke();';

                echo 'ctx.font = "20px Arial ";';
                echo 'ctx.fillStyle = "red";';
                echo 'ctx.fillText("480",245,20,50);';
                echo 'ctx.fillText("960",485,20,50);';
                echo 'ctx.fillText("1440",725,20,50);';

                echo 'ctx.fillText("270",5,130,50);';
                echo 'ctx.fillText("540",5,260,50);';
                echo 'ctx.fillText("810",5,390,50);';
		
		echo '</script>';
		$conn->close();
	}
?>
