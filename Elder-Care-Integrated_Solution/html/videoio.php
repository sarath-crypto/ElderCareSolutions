<?php
function drawPieGraph($cachefilename, $ydata){
        require_once ('jpgraph/jpgraph.php');
        require_once ('jpgraph/jpgraph_pie.php');
        $graph = new PieGraph(200,200);
        $valid = $graph -> cache -> IsValid($cachefilename);
        if ($valid){
                return;
        }else{
                $graph -> SetupCache($cachefilename, 1);
                $graph->clearTheme();
                $graph->SetColor('black');
                $graph->title->Set("Disk Usage");
                $graph->title->SetColor('green');
                $pie = new PiePlot($ydata);
                $colors = array('#FF0000', '#00FF00');
                $pie->SetSliceColors($colors);
                $graph->Add($pie);
                $absolutePath = (CACHE_DIR . "" . $cachefilename);
                $graph -> Stroke($absolutePath);
        }
}

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
                      background-color: #000000;
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
		echo '</tr>';
		echo '</table>';
		$conn->close();
	}
       	echo '</body></html>';
?>
