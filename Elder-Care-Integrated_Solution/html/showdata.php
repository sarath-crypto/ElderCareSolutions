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

	if ($_SERVER["REQUEST_METHOD"] == "POST") {
		$selhr = htmlspecialchars($_POST['hour']);
		$date = htmlspecialchars($_POST['date']);
		if (strlen($selhr) > 0){
                	if(strlen($selhr) == 1)$selhr = "0".$selhr;
			$dir = "./storage/".$date."/*".$selhr."????m*.mkv";
			$f = glob($dir);
			$f_sz = sizeof($f);
			echo '</head><body bgcolor="black">';
			echo '<h3 style="color: white;">';
			echo "[Entries:".$f_sz ."] [Hour:".$selhr."] [Date:".$date."] <a href='summary.php'>Back to summary page</a></br>";
			echo '</h3>';
			echo '<style>
                		table, th, td {
                		border: 1px solid black;
                		}
                		th, td {
                		background-color: #96D4D4;
                		}</style>';
			echo '<table>';
			foreach ($f as $e){
				if( $nc == 0){
					echo '<tr style="height:120px;text-align: center; vertical-align: middle;">';
				}
				echo '<td width="160px">';
				echo '<video width="160" height="120" controls>';
				echo '<source src="';
				echo $e;
				echo '" type="video/mp4"> Your browser does not support the video tag.</video>';
				$fn = substr($e,13);
				$fn = substr($fn,0,-4);
				echo $fn;
				echo '</td>';
				$nc += 1;
				if( $nc == 10){
					echo '</tr>';
					$nc = 0;
				}
			}
			echo '</table>';
                	echo '</body></html>';
		}
	}
?>
