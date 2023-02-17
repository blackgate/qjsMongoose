import { MongooseManager } from '../build/libqjsMongoose.so';
import { setInterval, clearInterval } from './utils.js';
import { match } from './pathToRegEx.js';

const SNTP_UPDATE_INTERVAL = 3600 * 1000; // 1 hour

function httpRequest(msg) {
    let queryParams = null;
    return {
        get uri() { return msg.uri },
        get method() { return msg.method },
        get query() { return msg.query },
        get body() { return msg.body },
        get headers() { return msg.headers },
        get queryParams() {
            if (queryParams === null) {
                const kvPairs = msg.query.split("&").filter(i => i.length > 0).map(i => {
                    const [k, v] = i.split("=");
                    return [k, decodeURIComponent(v)];
                });
                queryParams = Object.fromEntries(kvPairs);
            }
            return queryParams;
        },
        getQueryParam(name) {
            return this.queryParams[name];
        },
        getHeader(name) {
            return msg.getHeaderValue(name);
        },
        getJson() {
            return JSON.parse(msg.body);
        }
    }
}

function httpResponse(msg) {
    const res = { 
        headers: {}, 
        status: 200, 
        headersSent: false,
        get headersText() {
            return Object.entries(this.headers)
                .map(([name, val]) => `${name}: ${val}\r\n`)
                .join("");
        }
    };

    return {
        setHeader(name, val) {
            res.headers[name] = val;
            return this;
        },
        status(statusCode) {
            res.status = statusCode;
            return this;
        },
        send(body) {
            msg.httpReply(res.status, res.headersText, body);
            res.headersSent = true;
        },
        write(body) {
            if (!res.headersSent) this.send("");
            msg.httpWrite(body);
        },
        sendJson(json) {
            return this.setHeader("Content-Type", "application/json").send(JSON.stringify(json));
        },
        serveDir(dir, mimeTypes = null) {
            msg.httpServeDir(dir, res.headersText, mimeTypes)
        },
        serveFile(file, mimeTypes = null) {
            msg.httpServeFile(file, res.headersText, mimeTypes)
        },
        wsUpgrade(label) {
            msg.connection.label = label;
            msg.wsUpgrade();
            return wsResponse(msg.connection);
        }
    }
}

function wsRequest(conn, msg) {
    return {
        get connectionId() { 
            return conn.label; 
        },
        get message() {
            return msg.message;
        },
        getMessageAsString() {
            return String.fromCharCode.apply(null, new Uint8Array(msg.message));
        },
        getMessageAsJson() {
            return JSON.parse(getMessageAsString());
        }
    }
}

function wsResponse(conn) {
    return {
        get connectionId() { 
            return conn.label; 
        },
        sendText: (data) => conn.wsSendText(data),
        sendBinary: (data) => conn.wsSendBinary(data)
    }
}

function mqttClient(srv, url, opts) {
    let { autoReconnect = true, ...otherOpts } = opts;

    const handlers = {
        onOpen: [],
        onClose: [],
        onMessage: []
    }

    let mqttClient = null;

    function initMqttClient() {
        mqttClient = srv.createMqttClient(url, otherOpts);

        mqttClient.onOpen = () => {
            for (let h of handlers.onOpen) h();
        }

        mqttClient.onClose = () => {
            for (let h of handlers.onClose) h();
            if (autoReconnect) initMqttClient();
        }

        mqttClient.onMessage = (m) => {
            for (let h of handlers.onMessage) h(m);
        }
    }

    initMqttClient();

    const pingTimer = setInterval(() => {
        mqttClient.ping();
    }, 15000);

    return {
        onOpen(fn) {
            handlers.onOpen.push(fn);
        },
        onClose(fn) {
            handlers.onClose.push(fn);
        },
        onMessage(fn) {
            handlers.onMessage.push(fn);
        },
        subscribe: (topic) => mqttClient.subscribe(topic),
        publish: (topic, message) => mqttClient.publish(topic, message),
        disconnect: () => {
            autoReconnect = false;
            mqttClient.disconnect();
            clearInterval(pingTimer);
        }
    }
}

function createMongooseInstance() {
    const srv = new MongooseManager();
    const handlers = [];
    const wsHandlers = {};
    const wsConnections = {};
    const sntpHandlers = [];
    let wsHandlerId = 0;
    let wsConnectionId = 0;
    let sntpConnection = null;
    let staticFilesRoot = null;

    const regHandler = (method, urlPattern, callback) => {
        const matchUrl = match(urlPattern, { decode: decodeURIComponent });
        handlers.push({ matchFn: (req) => req.method === method && matchUrl(req.uri), callback });
    }

    const regCustomHandler = (method) => (urlPattern, callback) => {
        regHandler(method, urlPattern, callback);
    }

    const iterateHandlers = (req, res) => {
        let shouldStop = false;
        const next = () => shouldStop = false;
        for (let h of handlers) {
            const match = h.matchFn(req);
            if (match) {
                shouldStop = true;
                h.callback(req, res, match.params, next);
                if (shouldStop) return;
            }
        }
        if (staticFilesRoot) res.serveDir(staticFilesRoot);
    }

    srv.onHttpMessage = (msg) => {
        const req = httpRequest(msg);
        const res = httpResponse(msg);
        iterateHandlers(req, res);
    }

    srv.onWsMessage = (msg) => {
        const wsHandlerId = wsConnections[msg.connection.label];
        if (wsHandlerId && wsHandlerId in wsHandlers) {
            const wsHandler = wsHandlers[wsHandlerId];
            const req = wsRequest(msg.connection, msg);
            const res = wsResponse(msg.connection);
            for (let h of wsHandler.onMessage) h(req, res);
        }
    }

    srv.onHttpClose = (c) => {
        if (c.label in wsConnections) {
            const wsHandlerId = wsConnections[c.label];
            const wsHandler = wsHandlers[wsHandlerId];
            const res = wsResponse(c);
            for (let h of wsHandler.onClose) h(res);
            wsHandler.connections.delete(c.label);
            delete wsConnections[c.label];
        }
    }

    srv.onSntpMessage = (time) => {
        for (const h of sntpHandlers) {
            h(time);
        }
    }

    setInterval(() => srv.poll(10), 50);

    return {
        httpGet: regCustomHandler("GET"),
        httpPost: regCustomHandler("POST"),
        httpPut: regCustomHandler("PUT"),
        httpDelete: regCustomHandler("DELETE"),
        httpMiddleware: (callback) => handlers.push({ 
            matchFn: () => true, 
            callback: (req, res, _, next) => callback(req, res, next) 
        }),
        websocketHandler: (urlPattern) => {
            const handlerId = (wsHandlerId++).toString();
            const handler = { 
                onOpen: [],
                onMessage: [], 
                onClose: [],
                connections: new Set() 
            };
            wsHandlers[handlerId] = handler;
            regHandler("GET", urlPattern, (req, res) => {
                const connId = (wsConnectionId++).toString();
                wsConnections[connId] = handlerId;
                handler.connections.add(connId);
                const conn = res.wsUpgrade(connId);
                for (let h of handler.onOpen) h(req, conn);
            });
            return {
                onOpen: (cb) => handler.onOpen.push(cb),
                onMessage: (cb) => handler.onMessage.push(cb),
                onClose: (cb) => handler.onClose.push(cb),
                getConnections() {
                    let conn = srv.getConnections();
                    const res = [];
                    while (conn !== null) { 
                      if (handler.connections.has(conn.label)) res.push(wsResponse(conn));
                      conn = conn.next();
                    }
                    return res;
                }
            };
        },
        setStaticFilesRoot: (filesRoot) => { staticFilesRoot = filesRoot },
        httpListen: (listenUrl) => srv.httpListen(listenUrl),
        onSntpTime: (fn) => {
            if (!sntpConnection) {
                sntpConnection = srv.sntpConnect();
                setInterval(() => sntpConnection.sntpRequest(), SNTP_UPDATE_INTERVAL);
            }
            sntpHandlers.push(fn);
        },
        createMqttClient: (url, opts = {}) => {
            return mqttClient(srv, url, opts);
        }
    }
}

const mongoose = createMongooseInstance();

export default mongoose;