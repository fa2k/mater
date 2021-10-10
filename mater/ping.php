<?php
header('Expires: 0');
header('Content-type: text/plain');


$timestamp = date('d/m H:i:s', time());
$body = file_get_contents("php://input");
$jsondata = json_decode($body);

if ($jsondata) {
	$ip = $_SERVER['REMOTE_ADDR'];
	$fh = fopen("ip.txt", 'w');
	fwrite($fh, $ip);
	fclose($fh);

	$served = $jsondata->served_portions;
	$condition = $jsondata->condition;

	$row = "<tr>";
	$row .= "<td>$timestamp</td>";
	$row .= "<td>$ip</td>";
	$row .= "<td>$condition</td>";
	if ($condition == 0 || $condition == 1) {
		$row .= "<td>$served</td>";
	}
	else {
		$row .= "<td>?</td>";
	}
	$row .= "</tr>\n";
	
	$lines = array();
	$file = fopen("reports.txt", "r");
	while(!feof($file)) {
	    $lines[] = fgets($file, 4096);
	}
	fclose ($file);
	
	$fr = fopen("reports.txt", "w");
	fwrite($fr, $row);
	$nrows = min(count($lines), 4);
	for($i = 0; $i < $nrows; $i++) {
		fwrite($fr, $lines[$i]);
	}
	fclose($fr);
}
print ("$ip")
?>
