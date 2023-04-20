const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Bird Box Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
  html {
    font-family: Arial, Helvetica, sans-serif;
    text-align: center;
  }
  img {  
    width: auto; 
    max-width: 400px; 
    height: auto; 
    padding-top: 20px; 
  }
  h1 {
    font-size: 1.8rem;
    color: white;
  }
  h2{
    font-size: 1.5rem;
    font-weight: bold;
    color: #143642;
  }
  .topnav {
    overflow: hidden;
    background-color: #143642;
  }
  body {
    margin: 0;
  }
  .content {
    padding: 30px;
    max-width: 600px;
    margin: 0 auto;
    display: flex;
    justify-content: space-between;
  }
  .card {
    background-color: #F8F7F9;;
    box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);
    padding-top:10px;
    padding-bottom:20px;
    margin-right: 20px;
    margin-left: 20px;
  }
  .button {
    padding: 15px 50px;
    font-size: 24px;
    text-align: center;
    outline: none;
    color: #fff;
    background-color: #c90411;
    border: none;
    border-radius: 5px;
    -webkit-touch-callout: none;
    -webkit-user-select: none;
    -khtml-user-select: none;
    -moz-user-select: none;
    -ms-user-select: none;
    user-select: none;
    -webkit-tap-highlight-color: rgba(0,0,0,0);
  }
  .button:active {
    background-color: #c90411;
    box-shadow: 2 2px #CDCDCD;
    transform: translateY(2px);
  }
  .state {
    font-size: 1.5rem;
    color:#8c8c8c;
    font-weight: bold;
  }
  </style>
</head>
<body>
  <div class="topnav">
    <h1>Bird Box</h1>
  </div>
  <div class="content">  
    <div class="card">
      <h2>NIGHT MODE</h2>
      <p class="state">state: <span id="night_state">%NIGHT_STATE%</span></p>
      <p><button id="night_button" class="button">Night</button></p>
    </div>
    <div class="card">
      <h2>Feeder Fill Status</h2>
      <p class="state">state: <span id="fill_state">%FILL_STATE%</span></p>
      <p><button id="fill_button" class="button">Fill</button></p>
    </div>
  </div>
  <img src="" id="photo">
  <div class="content">
    <div class="card">
      <h2>Camera</h2>
      <p class="state">state: <span id="cam_state">%CAM_STATE%</span></p>
      <p><button id="cam_button" class="button">Toggle Camera</button></p>
    </div>
    <div class="card">
      <h2>Flash</h2>
      <p class="state">state: <span id="led_state">%LED_STATE%</span></p>
      <p><button id="led_button" class="button">Toggle LED</button></p>
    </div>
    <div class="card">
      <h2>GPIO 13</h2>
      <p class="state">state: <span id="door_state">%DOOR_STATE%</span></p>
      <p><button id="door_button" class="button">Toggle Door</button></p>
    </div>
  </div>
<script>
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  window.addEventListener('load', onLoad);
  function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage; // <-- add this line
  }
  function onOpen(event) {
    console.log('Connection opened');
  }
  function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
  }

  function onMessage(event) {
    switch(event.data)
    {
      case '0': document.getElementById("cam_state").innerHTML = "OFF"; document.getElementById('cam_button').style.backgroundColor = "#c90411"; document.getElementById("photo").src = ""; break;
      case '1': document.getElementById("cam_state").innerHTML = "ON &nbsp;"; document.getElementById('cam_button').style.backgroundColor = "#04b50a"; document.getElementById("photo").src = window.location.href + "stream" + "?width=500&height=500"; break;
      case '2': document.getElementById("led_state").innerHTML = "OFF"; document.getElementById('led_button').style.backgroundColor = "#c90411"; break;
      case '3': document.getElementById("led_state").innerHTML = "ON &nbsp;"; document.getElementById('led_button').style.backgroundColor = "#04b50a"; break;
      case '4': document.getElementById("door_state").innerHTML = "OPEN &nbsp;"; document.getElementById('door_button').style.backgroundColor = "#04b50a"; break;
      case '5': document.getElementById("door_state").innerHTML = "CLOSED"; document.getElementById('door_button').style.backgroundColor = "#c90411"; break;
      case '6': document.getElementById("night_state").innerHTML = "OFF"; document.getElementById('night_button').style.backgroundColor = "#c90411"; break;
      case '7': document.getElementById("night_state").innerHTML = "ON &nbsp;"; document.getElementById('night_button').style.backgroundColor = "#04b50a"; break;
      case '8': document.getElementById("fill_state").innerHTML = "FULL"; document.getElementById('fill_button').style.backgroundColor = "#04b50a"; break;
      case '9': document.getElementById("fill_state").innerHTML = "EMPTY &nbsp;"; document.getElementById('fill_button').style.backgroundColor = "#c90411"; break;
    }
  }

  function onLoad(event) {
    initWebSocket();
    initButton();

  }
  function initButton() {
    document.getElementById('cam_button').addEventListener('click', toggle_cam);
    document.getElementById('led_button').addEventListener('click', toggle_led);
    document.getElementById('door_button').addEventListener('click', toggle_door);
  }
 
  function toggle_cam(){
    websocket.send('toggle_cam');
  }
  function toggle_led(){
    websocket.send('toggle_led');
  }
  function toggle_door(){
    websocket.send('toggle_door');
  }

</script>
</body>
</html>
)rawliteral";