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
			$dir = "./storage/".$date."/*".$selhr."????m*.jpg";
			$f = glob($dir);
			$f_sz = sizeof($f);
			echo "[Entries:".$f_sz ."] [Hour:".$selhr."] [Date:".$date."] <a href='main.php'>Back to main page</a></br>";
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
					echo '<tr style="height:200px;text-align: center; vertical-align: middle;">';
				}
				echo '<td width="200px">';
				echo '<img src=';
				echo $e;
				echo ' alt="" border=1 height=180 width=180>';
				echo '</td>';
				$nc += 1;
				if( $nc == 7){
					echo '</tr>';
					$nc = 0;
				}
			}
			echo '</table>';
		}
	}
?>
