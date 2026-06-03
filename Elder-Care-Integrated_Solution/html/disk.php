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

       	echo '<!DOCTYPE html><html><head><title>DISK</title>';
       	echo '</head><body bgcolor="black">';
	echo '<style>
              table, th, td {
              border: 1px solid black;
              }
              th, td {
              background-color: #000000;
              }</style>';

	echo '<table>';
	echo '<tr style="height:200px;text-align: center; valign: "top;">';
	echo '<td width="480px">';

	$str = @file_get_contents('/proc/uptime');
	$num = floatval($str);
	$mins = intdiv($num, 60) % 60;
	$hours = intdiv($num, 3600) % 24;
	$days = intdiv($num, 86400);

        echo "<br><h5 style='color :#00FF00;'>"."[".$days."]Days [".$hours."]Hours [".$mins."]Minutes</h5>";

        $df = disk_free_space("/");
        $ds = disk_total_space("/");
        $du = ($df/$ds)*100;
        $ydata = array(100-$du,$du);
        $f_pie = 'graph/pie.png';
        $graph = drawPieGraph($f_pie, $ydata);

        echo '<img style="vertical-align: bottom;"  width="100%" height="100%" src=';
        echo $f_pie;
        echo '></img></td>';
		
	echo '</tr>';
	echo '</table>';
       	echo '</body></html>';
?>
