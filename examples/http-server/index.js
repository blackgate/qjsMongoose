import mongoose from "../../js/mongoose.js";

const $log = (msg) => console.log(`[${new Date().toISOString()}] ${msg}`)

mongoose.httpGet("/api/hello/:name", (req, res, params) => {
    res.sendJson({ 
        result: `Hello ${params.name}!` 
    })
});

const wsHandler = mongoose.websocketHandler("/websocket");

function logNumOfWebSocketConnections() {
    const c = wsHandler.getConnections();
    $log(`Number of websocket connections: ${c.length}`);
}

wsHandler.onOpen((httpReq, conn) => {
    $log(`Opening connection for #${conn.connectionId}`);
    logNumOfWebSocketConnections();
});

wsHandler.onMessage((req, res) => {
    const msg = req.getMessageAsString();
    $log(`Got ws message from #${req.connectionId}: ${msg}`);
    res.sendText(msg); // echo message back
});

wsHandler.onClose((conn) => {
    $log(`Closing connection for #${conn.connectionId}`);
    logNumOfWebSocketConnections();
});

mongoose.setStaticFilesRoot("./public");

mongoose.httpListen("http://0.0.0.0:8080");

console.log("Open http://localhost:8080 on your browser");