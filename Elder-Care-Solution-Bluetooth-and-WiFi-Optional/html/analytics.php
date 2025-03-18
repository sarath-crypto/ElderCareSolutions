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


function drawBarGraph($cachefilename, $ydata,$color,$title,$ylegend){
	require_once ('jpgraph/jpgraph.php');
	require_once ('jpgraph/jpgraph_utils.inc.php');
	require_once ('jpgraph/jpgraph_bar.php');
	
	$graph = new Graph(1600,200);
	$valid = $graph -> cache -> IsValid($cachefilename);
	if ($valid){
		return;
	}else{
		$graph -> SetupCache($cachefilename, 1);
		$graph->clearTheme();	
		$graph -> SetScale("textlin");
		$graph->SetShadow();
		$bplot = new BarPlot($ydata);
		$bplot->SetFillColor($color);
		$bplot->SetWidth(1.0);
		$graph->Add($bplot);
		$graph->title->Set($title);
		$graph->xaxis->SetTickLabels(array(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23));
		$graph->xaxis->title->Set("Time");
		$graph->yaxis->title->Set($ylegend);
		$graph->title->SetFont(FF_FONT1,FS_BOLD);
		$graph->yaxis->title->SetFont(FF_FONT1,FS_BOLD);
		$graph->xaxis->title->SetFont(FF_FONT1,FS_BOLD);
		$absolutePath = (CACHE_DIR . "" . $cachefilename);
		$graph -> Stroke($absolutePath);
	}
}


function draw2BarGraph($cachefilename, $ydata_a,$ydata_b,$color_a,$color_b,$title,$ylegend){
	require_once ('jpgraph/jpgraph.php');
	require_once ('jpgraph/jpgraph_utils.inc.php');
	require_once ('jpgraph/jpgraph_bar.php');

	$data1y=array(12,8,19,3,10,5);
	$data2y=array(8,2,11,7,14,4);
 
	$graph = new Graph(1600,200);
	$valid = $graph -> cache -> IsValid($cachefilename);
	if ($valid){
		return;
	}else{
		$graph -> SetupCache($cachefilename, 1);
		$graph->clearTheme();	
		$graph -> SetScale("textlin");
		$graph->SetShadow();
		$b1plot = new BarPlot($ydata_a);
		$b1plot->SetFillColor($color_a);
		$b2plot = new BarPlot($ydata_b);
		$b2plot->SetFillColor($color_b);
		$gbplot = new GroupBarPlot(array($b1plot,$b2plot));
		$gbplot->SetWidth(1.0);

		$graph->Add($gbplot);
		$graph->title->Set($title);
		$graph->xaxis->SetTickLabels(array(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23));
		$graph->xaxis->title->Set("Time");
		$graph->yaxis->title->Set($ylegend);
		$graph->title->SetFont(FF_FONT1,FS_BOLD);
		$graph->yaxis->title->SetFont(FF_FONT1,FS_BOLD);
		$graph->xaxis->title->SetFont(FF_FONT1,FS_BOLD);
		$absolutePath = (CACHE_DIR . "" . $cachefilename);
		$graph -> Stroke($absolutePath);
	}
}


	if ($_SERVER["REQUEST_METHOD"] == "POST") {
		$date = htmlspecialchars($_POST['data']);
		if (empty($date) == FALSE){
			$ydata = array_fill($ydata,24,0);
			$files = array();
			$dir_sz = 0;
			$dir = "data/".$date."/*.jpg";
			$f = glob($dir);
			foreach ($f as $e){
				$files[] = $e;
				$dir_sz += filesize($e);
				$hr = substr($e,14,2);
				$ydata[(int)$hr] += 1;
			}
			$files_sz = sizeof($files);
			echo "[Entries:".$files_sz ."] [Directory Size:". round($dir_sz/1000000,2) . "MB] [Date:". $date . "] <a href='summary.php'>Back to summary page</a></br>";


			echo '<table border="4">';
			
			$f_events = 'graph/bar_events.png';
			$graph = drawBarGraph($f_events, $ydata,'blue',"Events Graph","Pictures Count");
			echo '<tr  style="height:200px"><td width="1600px"><img style="vertical-align: bottom;"  width="100%" height="100%" src=';
			echo $f_events; 
			echo "></img></td></tr>";

			unset($ydata);
			$ydata = array_fill($ydata,24,0);
 			$dir = "data/".$date."/*.tmr";
			$f = glob($dir);
			foreach ($f as $e){
				$myfile = fopen($e,"r");
				$ts = strtotime(fread($myfile,filesize($e)));
				fclose($myfile);
				$te = "20" . substr($e,8,2) . "-" . substr($e,10,2). "-" . substr($e,12,2) . " " .  substr($e,14,2) . ":" . substr($e,16,2) . ":" . substr($e,18,2);
				$te = strtotime($te);
				$hr = substr($e,14,2);
				$ydata[(int)$hr] += abs($te-$ts);
			}

			$ytrig = array_fill($ytrig,24,0);
 			$dir = "data/".$date."/*.trg";
			$f = glob($dir);
			foreach ($f as $e){
				$te = "20" . substr($e,8,2) . "-" . substr($e,10,2). "-" . substr($e,12,2) . " " .  substr($e,14,2) . ":" . substr($e,16,2) . ":" . substr($e,18,2);
				$te = strtotime($te);
				$hr = substr($e,14,2);
				$ytrig[(int)$hr] += 10;
			}

			$f_downtime = 'graph/bar_downtime.png';
			$graph = draw2BarGraph($f_downtime, $ydata,$ytrig,'red','blue',"Down Time/Trigger Graph","Seconds");
			echo '<tr  style="height:200px"><td width="1600px"><img style="vertical-align: bottom;"  width="100%" height="100%" src=';
			echo $f_downtime; 
			echo "></img></td></tr>";
			
			echo '</table>';

		}
	}

echo 	'<br><form method="post" action="showdata.php">  
	<label>Select Hour:</label>
 	<select name="hour">
    	<option value="0">0</option>
    	<option value="1">1</option>
    	<option value="2">2</option>
	<option value="3">3</option>
	<option value="4">4</option>
    	<option value="5">5</option>
    	<option value="6">6</option>
    	<option value="7">7</option>
    	<option value="8">8</option>
    	<option value="9">9</option>
    	<option value="10">10</option>
    	<option value="11">11</option>
    	<option value="12">12</option>
    	<option value="13">13</option>
    	<option value="14">14</option>
    	<option value="15">15</option>
	<option value="16">16</option>
	<option value="17">17</option>
    	<option value="18">18</option>
    	<option value="19">19</option>
    	<option value="20">20</option>
    	<option value="21">21</option>
    	<option value="22">22</option>
    	<option value="23">23</option>
  	</select>
	<br><br>
	<input type="hidden" name="date" value="';
	echo $date;
	echo '">
	<input type="submit" />
	</form>';
?>
