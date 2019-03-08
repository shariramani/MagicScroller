const char MAIN_page[] PROGMEM = R"=====(

<!DOCTYPE html>
<html>
<head>
<meta name='mobile-web-app-capable' content='yes' />
<meta name='viewport' content='width=device-width' />
</head>

<body>

<p>what would you like to display?</p>

<form action="/">
<input type="checkbox" name="scroll" value="2Zone" >2-Zone Mode<br>
<input type="checkbox" name="scroll" value="timeDisplay" >Only Time Display<br>
<input type="checkbox" name="scroll" value="welcome message" >welcome message<br>
<input type="checkbox" name="scroll" value="temperature" >temperature<br>
<input type="checkbox" name="scroll" value="calendar events" >calendar events<br>
<input type="checkbox" name="scroll" value="spiff_quote" >Random Quotes from spiff<br>
<input type="checkbox" name="scroll" value="spiff_facts" >Random facts from spiff<br>
<input type="checkbox" name="scroll" value="spiff_taglines" >Random tagline from spiff<br>
<input type="checkbox" name="scroll" value="cricket" >live cricket scores<br>
<input type="checkbox" name="scroll" value="qod" >qoutes of the day<br>
<input type="checkbox" name="scroll" value="bbc" >BBC news<br>
<input type="checkbox" name="scroll" value="zee" >ZEE news<br>
<input type="checkbox" name="scroll" value="bsnews" >Business Standard news<br>
<input type="checkbox" name="scroll" value="finexp" >Financial Express<br>
<input type="checkbox" name="scroll" value="ITnews" >India Today news<br>


<p>INTENSITY</p>
<left><input type="range" min="0" max="15" value=intensity step="1" onchange="showIntensity(this.value)" size="80" align="left" > </input></left>
<span id="range"></span>
<script type="text/javascript">function showIntensity(newValue){document.getElementById("range").innerHTML=newValue;var req = new XMLHttpRequest();req.open('GET', '?' + 'intensity=' + newValue, true);req.send();}</script>


<p>Scroll Speed</p>
<left><input type="range" min="0" max="100" value=speedTime step="5" onchange="showSpeed(this.value)" size="80" align="left" > </input></left>
<span id="range"></span>
<script type="text/javascript">function showSpeed(newValue){document.getElementById("range").innerHTML=newValue;var req = new XMLHttpRequest();req.open('GET', '?' + 'speedTime=' + newValue, true);req.send();}</script>


<br>Welcome Message:  <br>
<input type="text" name="WelcomeMsg" value="Welcome to home of creators"><br>

<input type="submit" value="Submit">
</form>

</body></html>

)=====";
