<?php echo '<?xml version="1.0" encoding="UTF-8" ?>' . "\n"; ?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<link rel="stylesheet" type="text/css" href="style.css" />
<script type="text/javascript" src="jquery-2.0.3.js"></script>
<script type="text/javascript" src="todemater.js"></script>
<script type="text/javascript" src="http://www.skypeassets.com/i/scom/js/skype-uri.js"></script>
<title>skilpaddemater v. 1</title>
</head>
<body>
<div class="main">
<h1>todemater!</h1>
<div class="indicator">
<span class="connection_status">&nbsp;</span>
<span id="loading_indicator">
<img src="turtle.gif" alt="laster" />
</span>
</div>
<div class="notification" id="notification"></div>
<form>
<h2>maskin</h2>
<table>
<tr>
<td>Maskinstatus:</td>
<td><input type="text" id="machine_status" readonly="readonly" value="javascript is required" /></td>
</tr>
<tr>
<td>Porsjoner:</td>
<td><input type="text" id="portions" readonly="readonly" /></td>
</tr>
<tr>
<td>Automating ved inaktivitet:</td>
<td><input type="text" id="autofeeding" readonly="readonly" /></td>
</tr>
</table>
<p>
<input type="button" id="feed" value="Mat!"/>
<input type="button" id="checkin" value="Sjekk inn"/>
(sjekk inn for å utsette automating)
<input type="button" id="feed_force" value="Mat!!!"/> (se bort i fra feil)
</p>
<p> automatisk mating
<input type="radio" checked="checked" name="automat_on_off" value="enable">på</input>
<input type="radio" name="automat_on_off" value="disable">av</input>
<input type="button" id="automater" value="sett"/>
</p>
<p>
Server: 
<input type="text" id="ip" value="<?php echo file_get_contents("ip.txt") . ":5001"; ?>" />
<input type="button" id="refresh" value="Oppdater" /> 
</p>
<p> 
<input type="button" id="reset" value="Nullstill teller" />
<input type="button" id="restart_server" value="Restart PC" /> (ikke bruk)
</p>
</form>

<h2>Webcam</h2>
<p>Direkte video av Poppe og Tode: <a href="http://<?php echo file_get_contents("ip.txt"); ?>:5003/" target="_blank">klikk her</a></p>

<h2>automatisk rapportering (feilsøking)</h2>
<table class="reporting">
<tr><th>dato/tid</th><th>adresse</th><th>tilstand</th><th>porsjoner</th></tr>
<?php echo file_get_contents("reports.txt"); ?></table>
</div>
<p>password: +ugl5i6S2QIQ</p>
</body>
</html>

