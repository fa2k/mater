
var buttons = ["#feed", "#checkin", "#reset", "#restart_server"]

function successUpdateStatus(json) {
	$( "#machine_status" ).val(json.status)
	if (json.condition == 0 || json.condition == 1) { // not fatal
		var por_string = "" + json.served_portions + " av " 
		por_string += json.portion_capacity + " servert"
		$( "#portions" ).val(por_string)
		var af_string = "";
		if (json.autofeeding) {
			var hours = Math.floor(json.autofeeding_next_event / 3600000)
			if (hours == 0) {
				hours = "mindre enn en"
			}
			af_string = "på, mating om " + hours + " time(r)"
		}
		else {
			af_string = "av"
		}
		$( "#autofeeding" ).val(af_string)
	}
	else {
		$( "#portions" ).val("")
		$( "#autofeeding" ).val("")
	}
	$( ".connection_status" ).html("")
}

function reportAjaxError(request, errorText) {
	$("#machine_status").val("Ukjent")
	$( "#portions" ).val("")
	$( "#autofeeding" ).val("")
	showError("Får ikke kontakt med PC: " + errorText)
}

function showIndicator(statusText) {
	$( "#loading_indicator" ).show()
	$( "span.connection_status" ).html(statusText)
}

function hideIndicator() {
	$( "#loading_indicator" ).hide()
	$( "span.connection_status" ).html("")
}

function startRequestUi(statusText) {
	for (var i=0; i<buttons.length; ++i) {
		$( buttons[i] ).prop("disabled", true)
	}
	showIndicator(statusText)
	$("#notification").text("")
}

function finishRequestUi() {
	hideIndicator()
	for (var i=0; i<buttons.length; ++i) {
		$( buttons[i] ).removeAttr("disabled")
	}
}

function requestUrl(path) {
	return "http://" + $( "#ip" ).val() + "/" + path
}

function showSuccess(message) {
	var notification = $("#notification")
	notification.removeClass("msg_error")
	notification.addClass("msg_success")
	notification.text(message)
}

function showError(message) {
	var notification = $("#notification")
	notification.addClass("msg_error")
	notification.removeClass("msg_success")
	notification.text(message)
}

function generalCompletion(json) {
	if (json.condition != 2) {
		$("#notification").text(json.operation)
		if (json.condition == 1) {
			showError(json.operation)
		}
		else if (json.condition == 0) {
			showSuccess(json.operation)
		}
	}
	else {
		showError("Kritisk feil")
	}
}

function updateStatusFields() {
	$( "#machine_status" ).val("")
	$( "#portions" ).val("")
	$( "#autofeeding" ).val("")
	startRequestUi("henter...")
	$.ajax({
		url: requestUrl("status"),
		type: "GET",
		dataType : "json",

		success: successUpdateStatus,
		error: reportAjaxError,
		complete: finishRequestUi
	})
}

function generalPostRequest(path, message) {
	startRequestUi(message)

	$.ajax({
		url: requestUrl(path),
		type: "POST",
		dataType : "json",
		timeout: 30000,
		success: [successUpdateStatus, generalCompletion],
		error: reportAjaxError,
		complete: finishRequestUi
	})
}

function feed() {
	$("#machine_status").val("Mater")
	generalPostRequest("feed", "venter på resultat...")
}

function feedForce() {
	$("#machine_status").val("Mater")
	generalPostRequest("feed-force", "venter på resultat...")
}

function checkin() {
	generalPostRequest("checkin", "sjekker inn...")
}

function reset() {
	generalPostRequest("reset", "nullstiller...")
}


function basicCommandCompletion() {
	showSuccess("OK")
}

function reportErrorOtherButtons() {
	showError("En feil oppstod")
}

function basicPostRequest(path) {
	$.ajax({
		url: requestUrl(path),
		type: "POST",
		timeout: 30000,
		success: basicCommandCompletion,
		error: reportErrorOtherButtons,
		complete: hideIndicator
	})
}

function restartServer() {
	showIndicator("reboot", "starter på nytt...")
	basicPostRequest("reboot")
}

function activateScreen() {
	showIndicator("skrur på skjerm...")
	basicPostRequest("turn-on-screen")
}

function restartSkype() {
	showIndicator("starter skype på nytt...")
	basicPostRequest("restart-skype")
}

function setAutoFeed() {
	id = $('input:radio[name=automat_on_off]:checked').val()
	generalPostRequest("auto/" + id, "setter...")
	
}


// on load
function init() {

	// Setting up a loading indicator using Ajax Events
	$( "#loading_indicator" ).ajaxStart(function() {
			$( this ).show()
		}).ajaxStop(function() {
			$( this ).hide()
	})

	// set up events
	$( "#feed" ).click(feed)
	$( "#feed_force" ).click(feedForce)
	$( "#checkin" ).click(checkin)
	$( "#refresh" ).click(updateStatusFields)
	$( "#reset" ).click(reset)
	$( "#restart_server" ).click(restartServer)

	$("#activate_screen").click(activateScreen)
	$("#restart_skype").click(restartSkype)
	
	$("#automater").click(setAutoFeed)
	updateStatusFields()
}


$( document ).ready(init)

