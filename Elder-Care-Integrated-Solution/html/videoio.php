<?php
       	echo '<!DOCTYPE html><html><head><title>VIDEO IO</title>';
       	echo '</head><body bgcolor="black">';
	header("Refresh: 1");
	$conn = new mysqli('localhost','userecsys','ecsys123','ecsys');
	if ($conn->connect_error) {
		die("Connection failed: " . $conn->connect_error);
	}else{
		echo '<style>
                      table, th, td {
                      border: 1px solid black;
                      }
                      th, td {
                      background-color: #00008b;
                      }</style>';

                echo '<table>';
		echo '<tr style="height:200px;text-align: center; valign: "top;">';
                echo '<td width="200px">';
		$sql = "SELECT ts,data FROM out_img";	
		$result = $conn->query($sql);
		if ($result->num_rows > 0) {
			while($row = $result->fetch_assoc()) {
				echo '<div class="caption"><h3 style="color: #FFFFFF"><img src="data:image/jpeg;base64,'.base64_encode($row['data']).'"/></br>'. $row['ts']. '</h3></div>';
			}						
		}
                echo '</td>';
			
                echo '<td width="200px">';
		$sql = "SELECT ts,data FROM in_img";	
		$result = $conn->query($sql);
		if ($result->num_rows > 0) {
			while($row = $result->fetch_assoc()) {
				echo '<div class="caption"><h3 style="color: #FFFFFF"><img src="data:image/jpeg;base64,'.base64_encode($row['data']).'"/></br>'. $row['ts']. '</h3></div>';
			}						
		}
                echo '</td>';
                echo '</tr>';
		echo '</table>';
		$conn->close();
	}
       	echo '</body></html>';
?>
