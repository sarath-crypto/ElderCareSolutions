<?php 

function drawLineGraph_solar($cachefilename,$psoc,$puload,$pgload,$pprod,$pgvolt,$pgdexp,$pgexp,$px){
	require_once ('jpgraph/jpgraph.php');
	require_once ('jpgraph/jpgraph_line.php');
	
	$graph = new Graph(1480,400);
	$graph->SetScale("textlin");
	$graph->ygrid->Show(false);
	$graph->SetColor('black');

	$graph->SetMargin(30,10,20,70);
	$graph->SetFrame(true,'black',0);
	$graph->SetMarginColor('black');


	$graph->SetImgFormat('jpeg',80);
	$graph->tabtitle->Set('REAL TIME' );
	$graph->tabtitle->SetColor('white','blue','white');
	$graph->tabtitle->SetFont(FF_ARIAL,FS_BOLD,10);

	$graph->yaxis->HideZeroLabel();
	$graph->yaxis->HideLine(false);
	$graph->yaxis->HideTicks(false,false);
	$graph->yaxis->Setcolor('white');

	$graph->xgrid->Show(false);
	$graph->xgrid->SetLineStyle("solid");
	$graph->xaxis->SetTickLabels($px);
        $graph->xaxis->SetLabelAngle(90);
	$graph->xaxis->Setcolor('white');
	$graph->xgrid->SetColor('#E3E3E3');

	$valid = $graph -> cache -> IsValid($cachefilename);
	if ($valid){
                return;
        }else{
		$graph->SetupCache($cachefilename, 1);
		$graph->legend->SetPos(0.33,0,'centre','top');
		$graph->legend->SetColor('white','navy');
		$graph->legend->SetFillColor('navy@0.25');
		$graph->legend->SetFont(FF_ARIAL,FS_BOLD,8);

		$p1 = new LinePlot($psoc);
		$graph->Add($p1);
		$p1->SetStyle("dashed");
		$p1->SetWeight(3);
		$p1->SetFillFromYMin(false);
		$p1->SetColor("#f1c40f");
		$val = end($psoc);
		$p1->SetLegend('SOC '.$val.'%');
		
		$p2 = new LinePlot($puload);
		$graph->Add($p2);
		$p2->SetStyle("dashed");
		$p2->SetWeight(3);
		$p2->SetFillFromYMin(false);
		$p2->SetColor("#FF8000");
		$val = end($puload)*10;
		$p2->SetLegend('Load x100['.$val.'W]');
	
		$p3 = new LinePlot($pgload);
		$graph->Add($p3);
		$p3->SetStyle("dashed");
		$p3->SetWeight(3);
		$p3->SetFillFromYMin(false);
		$p3->SetColor("#ff0000");
		$val = end($pgload)*10;
		$p3->SetLegend('Grid Load x100['.$val.'W]');
		
		$p4 = new LinePlot($pprod);
		$graph->Add($p4);
		$p4->SetStyle("dashed");
		$p4->SetWeight(3);
		$p4->SetFillFromYMin(false);
		$p4->SetColor("#7fff00");
		$val = end($pprod)*10;
		$p4->SetLegend('Production x10['.$val.'W]');
		
		$p5 = new LinePlot($pgvolt);
		$graph->Add($p5);
		$p5->SetStyle("dashed");
		$p5->SetWeight(3);
		$p5->SetFillFromYMin(false);
		$p5->SetColor("#800080");
		$val = end($pgvolt);
		$p5->SetLegend('Grid['.$val.'v]');
		
		$p6 = new LinePlot($pgdexp);
		$graph->Add($p6);
		$p6->SetWeight(3);
		$p6->SetFillFromYMin(false);
		$p6->SetColor("#0080ff");
		$p6->SetLegend('Daily Export Grid');

		$p7 = new LinePlot($pgexp);
		$graph->Add($p7);
		$p7->SetStyle("dashed");
		$p7->SetWeight(3);
		$p7->SetFillFromYMin(false);
		$p7->SetColor("#00808B");
		$p7->SetLegend('Export Grid');
		
		$graph->legend->SetFrameWeight(1);
		$absolutePath = (CACHE_DIR . "" . $cachefilename);
		$graph -> Stroke($absolutePath);

	}
}

function drawLineGraph_sensor($cachefilename,$pdprod,$pdload,$pdbuy,$pnoise,$px){
	require_once ('jpgraph/jpgraph.php');
	require_once ('jpgraph/jpgraph_line.php');
	
	$graph = new Graph(1480,400);
	$graph->SetScale("textlin");
	$graph->ygrid->Show(false);
	$graph->SetColor('black');

	$graph->SetMargin(30,10,20,70);
	$graph->SetFrame(true,'black',0);
	$graph->SetMarginColor('black');


	$graph->SetImgFormat('jpeg',80);
	$graph->tabtitle->Set('AVERAGED' );
	$graph->tabtitle->SetColor('white','blue','white');
	$graph->tabtitle->SetFont(FF_ARIAL,FS_BOLD,10);

	$graph->yaxis->HideZeroLabel();
	$graph->yaxis->HideLine(false);
	$graph->yaxis->Setcolor('white');
	$graph->yaxis->HideTicks(false,false);

	$graph->xgrid->Show(false);
	$graph->xgrid->SetLineStyle("solid");
	$graph->xaxis->SetTickLabels($px);
	$graph->xaxis->Setcolor('white');
        $graph->xaxis->SetLabelAngle(90);
	$graph->xgrid->SetColor('#E3E3E3');

	$valid = $graph -> cache -> IsValid($cachefilename);
	if ($valid){
                return;
        }else{
		$graph -> SetupCache($cachefilename, 1);
		$graph->legend->SetPos(0.33,0,'centre','top');
		$graph->legend->SetColor('white','navy');
		$graph->legend->SetFillColor('navy@0.25');
		$graph->legend->SetFont(FF_ARIAL,FS_BOLD,8);

		$p1 = new LinePlot($pdprod);
		$graph->Add($p1);
		$p1->SetWeight(3);
		$p1->SetFillFromYMin(false);
		$p1->SetColor("#00FFFF");
		$val = end($pdprod)*100;
		$p1->SetLegend('Daily Production x100['.$val.'Wh]');
		
		$p2 = new LinePlot($pdload);
		$graph->Add($p2);
		$p2->SetWeight(3);
		$p2->SetFillFromYMin(false);
		$p2->SetColor("#800000");
		$val = end($pdload)*100;
		$p2->SetLegend('Daily Load x100['.$val.'Wh]');

		$p3 = new LinePlot($pdbuy);
		$graph->Add($p3);
		$p3->SetWeight(3);
		$p3->SetFillFromYMin(false);
		$p3->SetColor("#FF00FF");
		$val = end($pdbuy)*100;
		$p3->SetLegend('Daily Buy x100['.$val.'Wh]');

		$min = min($pnoise);
		$max = max($pnoise);
		$swing = $max-$min;
		foreach ($pnoise as $val){
			$mnoise[] = ($val-$min);
		}

		$p4 = new LinePlot($mnoise);
		$graph->Add($p4);
		$p4->SetWeight(3);
		$p4->SetColor("#888E96");
		$val = end($mnoise);
		$p4->SetLegend('Sound Noise Level ['.$val.']');

		$graph->legend->SetFrameWeight(1);
                $absolutePath = (CACHE_DIR . "" . $cachefilename);
		$graph -> Stroke($absolutePath);
	}
}

 	header("Refresh: 300");
	$conn = new mysqli('localhost','userecsys','ecsys123','ecsys');
	if ($conn->connect_error) {
		die("Connection failed: " . $conn->connect_error);
	}else{
		echo '<html><body>';
		date_default_timezone_set('Asia/Kolkata');
		$timezone = date_default_timezone_get();
		$sql = "select * from hour";
		$result = $conn->query($sql);
		if ($result->num_rows > 0) {
			while($row = $result->fetch_assoc()) {
				$noise[] = $row["noise"]; 
				$dprod[] = $row["dprod"];
				$dload[] = $row["dload"];
				$dbuy[] = $row["dbuy"];
				$soc[] = $row["soc"];
				$uload[] = $row["uload"]/10;
				$gload[] = $row["gload"]/10;
				$prod[] = $row["prod"]/10;
				$gvolt[] = $row["gvolt"]/10;
				$gdexp[] = $row["gdexp"];
				$gexp[] = $row["gexp"];
				$datetime = explode(" ",$row["ts"]); 
				$ts[] = $datetime[1];
			}						
		}
		$old_value = 0;
		$new_value = max($noise);
		foreach ($noise as &$item){
			if ($item == $old_value){
				$item = $new_value; 
			}
		}
		unset($item);

		$f_solar = 'graph/solar.png';
		$graph = drawLineGraph_solar($f_solar,$soc,$uload,$gload,$prod,$gvolt,$gdexp,$gexp,$ts);
		echo '<table><tr>';
		echo '<td><img style="vertical-align: bottom;" src=';
		echo $f_solar;
		echo '></img></td></tr>';
		
		
		$f_sensor = 'graph/sensor.png';
		$graph = drawLineGraph_sensor($f_sensor,$dprod,$dload,$dbuy,$noise,$ts);
		echo '<tr><td>';
		echo '<img style="vertical-align: bottom;" src=';
		echo $f_sensor;
		echo '></img></td></tr>';

		echo '</table>';
		echo '</body>';
		$conn->close();
	}
?>
