<?php 

function drawLineGraph_solar($cachefilename,$pdprod,$pdload,$pdbuy,$psoc,$puload,$pgload,$pprod,$pgvolt,$pgdexp,$pgexp,$px){
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
	$graph->tabtitle->Set('SOLAR SYSTEM' );
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

		$p4 = new LinePlot($psoc);
		$graph->Add($p4);
		$p4->SetStyle("dashed");
		$p4->SetWeight(3);
		$p4->SetFillFromYMin(false);
		$p4->SetColor("#f1c40f");
		$val = end($psoc);
		$p4->SetLegend('SOC '.$val.'%');

		$p5 = new LinePlot($puload);
		$graph->Add($p5);
		$p5->SetStyle("dashed");
		$p5->SetWeight(3);
		$p5->SetFillFromYMin(false);
		$p5->SetColor("#00008B");
		$val = end($puload)*10;
		$p5->SetLegend('Load x100['.$val.'W]');
	
		$p6 = new LinePlot($pgload);
		$graph->Add($p6);
		$p6->SetStyle("dashed");
		$p6->SetWeight(3);
		$p6->SetFillFromYMin(false);
		$p6->SetColor("#800080");
		$val = end($pgload)*10;
		$p6->SetLegend('Grid Load x100['.$val.'W]');

		$p7 = new LinePlot($pprod);
		$graph->Add($p7);
		$p7->SetStyle("dashed");
		$p7->SetWeight(3);
		$p7->SetFillFromYMin(false);
		$p7->SetColor("#7fff00");
		$val = end($pprod)*10;
		$p7->SetLegend('Production x10['.$val.'W]');

		$p8 = new LinePlot($pgvolt);
		$graph->Add($p8);
		$p8->SetStyle("dashed");
		$p8->SetWeight(3);
		$p8->SetFillFromYMin(false);
		$p8->SetColor("#ff0000");
		$val = end($pgvolt);
		$p8->SetLegend('Grid['.$val.'v]');

		$p9 = new LinePlot($pgdexp);
		$graph->Add($p9);
		$p9->SetWeight(3);
		$p9->SetFillFromYMin(false);
		$p9->SetColor("#008000");
		$p9->SetLegend('Daily Export Grid');

		$p10 = new LinePlot($pgexp);
		$graph->Add($p10);
		$p10->SetStyle("dashed");
		$p10->SetWeight(3);
		$p10->SetFillFromYMin(false);
		$p10->SetColor("#808000");
		$p10->SetLegend('Export Grid');

		$graph->legend->SetFrameWeight(1);
		$absolutePath = (CACHE_DIR . "" . $cachefilename);
		$graph -> Stroke($absolutePath);

	}
}

function drawLineGraph_sensor($cachefilename,$ptemp,$phumd,$pnoise,$px){
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
	$graph->tabtitle->Set('SENSOR SYSTEM' );
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

		$min = min($ptemp);
		$max = max($ptemp);
		$swing = $max-$min;
		foreach ($ptemp as $val){
			$mtemp[] = ($val-$min)*($swing*10);
		}
		$p1 = new LinePlot($mtemp);
		$graph->Add($p1);
		$p1->SetWeight(3);
		$p1->SetColor("#f44336");
		$val = end($ptemp);
		$p1->SetLegend('Temperature ['.$val.'c] [Max:'.$max.']'.'[Min:'.$min.']');
			
		$min = min($pnoise);
		$max = max($pnoise);
		$swing = $max-$min;
		foreach ($pnoise as $val){
			$mnoise[] = ($val-$min)*($swing);
		}
		$p3 = new LinePlot($mnoise);
		$graph->Add($p3);
		$p3->SetWeight(3);
		$p3->SetColor("#888E96");
		$val = end($mnoise);
		$p3->SetLegend('Sound Noise Level ['.$val.']');

		$p4 = new LinePlot($phumd);
		$graph->Add($p4);
		$p4->SetWeight(3);
		$p4->SetColor("#00D100");
		$val = end($phumd);
		$p4->SetLegend('Humidity ['.$val.']');

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
				$temp[] = $row["temp"]/100; 
				$humd[] = $row["humd"]/100; 
				$noise[] = $row["noise"]/10; 
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
		$f_solar = 'graph/solar.png';
		$graph = drawLineGraph_solar($f_solar,$dprod,$dload,$dbuy,$soc,$uload,$gload,$prod,$gvolt,$gdexp,$gexp,$ts);
		echo '<table><tr>';
		echo '<td><img style="vertical-align: bottom;" src=';
		echo $f_solar;
		echo '></img></td></tr>';
		
		
		$f_sensor = 'graph/sensor.png';
		$graph = drawLineGraph_sensor($f_sensor,$temp,$humd,$noise,$ts);
		echo '<tr><td>';
		echo '<img style="vertical-align: bottom;" src=';
		echo $f_sensor;
		echo '></img></td></tr>';

		echo '</table>';
		echo '</body>';
		$conn->close();
	}
?>
