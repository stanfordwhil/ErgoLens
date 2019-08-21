function displayStatus(info) {
    var message = document.createElement('div');
    message.setAttribute('style', 'color: red;');
    message.innerHTML = info;
    var Out = document.getElementById('status');
    while(Out.childNodes.length >= 1)
	Out.removeChild(Out.firstChild);
    Out.appendChild(message);
}

function displayJSON(JSONString) {
    var div = document.createElement('pre');
    div.innerHTML = JSONString;

    var Out = document.getElementById('JSON');
    while(Out.childNodes.length >= 1)
	Out.removeChild(Out.firstChild);
    Out.appendChild(div);
}

function receiveData(JSONString) {
    JSONObj = JSON.parse(JSONString);    
}

window.onload = function()
{
    var url = 'ws://' + window.location.host;
    websocket = new WebSocket(url);
    websocket.onopen = function(ev) {
	websocket.send('Hello server!');
    };
    websocket.onclose = function(ev) {
	displayStatus('INFO: Connection closed.');
    };
    websocket.onmessage = function(ev) {
	websocket.send('Thanks!');
	displayStatus('Receiving data.');
	// displayJSON(ev.data);
	receiveData(ev.data);
    };
    websocket.onerror = function(ev) {
	displayStatus('ERROR:' + ev.data);
    };
}