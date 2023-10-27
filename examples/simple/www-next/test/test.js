import WebSocket from 'ws';

var ws = new WebSocket('wss://esp.tymek.duckdns.org');
ws.on('open', function() {
    console.log('Open');
    ws.send('{ "sn": "ALR_1" }');
    //ws.send('{ "cmd": 0, "arg": 4 }');
    ws.send('{ "cmd": 0, "arg": 1 }');
});
ws.on('message', function(data, flags) {
    console.log(`Got message: ${data}` );
    // flags.binary will be set if a binary data is received
    // flags.masked will be set if the data was masked
});
