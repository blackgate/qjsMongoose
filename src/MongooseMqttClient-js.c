#include "MongooseMqttClient-js.h"
#include "MongooseMqttMessage-js.h"
#include "mongoose.h"

enum {
    MG_MQTT_CLIENT_EVENT_ON_OPEN,
    MG_MQTT_CLIENT_EVENT_ON_CLOSE,
    MG_MQTT_CLIENT_EVENT_ON_MESSAGE,
    MG_MQTT_CLIENT_EVENT_MAX,
};

typedef struct {
    JSContext *ctx;
    char *url;
    struct mg_connection *conn;
    JSValue events[MG_MQTT_CLIENT_EVENT_MAX];
} mgMqttClientObj;

static mgMqttClientObj* getMgMqttClientObj(JSValueConst this_val) 
{
    return JS_GetOpaque(this_val, mgMqttClientClass.id);
}

static void mgMqttClientFinalizer(JSRuntime *rt, JSValue val) 
{
    mgMqttClientObj *state = getMgMqttClientObj(val);
    js_free(state->ctx, state);
}

static void mgMqttClientGcMark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) 
{
    mgMqttClientObj *state = getMgMqttClientObj(val);
    if (state) {
        // Nothing to mark
    }
}

static void mgMqttClientCb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    mgMqttClientObj *state = fn_data;
    if (ev == MG_EV_ERROR) {
        // On error, log error message
        MG_ERROR(("%p %s", c->fd, (char *) ev_data));
    } else if (ev == MG_EV_CONNECT) {
        // If target URL is SSL/TLS, command client connection to use TLS
        if (mg_url_is_ssl(state->url)) {
            struct mg_tls_opts opts;
            memset(&opts, 0, sizeof(opts));
            mg_tls_init(c, &opts);
        }
    } else if (ev == MG_EV_MQTT_OPEN) {
        JSValue fn = state->events[MG_MQTT_CLIENT_EVENT_ON_OPEN];
        if (JS_IsFunction(state->ctx, fn))
            JS_Call(state->ctx, fn, JS_UNDEFINED, 0, NULL);
    } else if (ev == MG_EV_MQTT_MSG) {
        // When we get echo response, print it
        struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
        JSValue jsMsg = mgMqttMsgCreate(state->ctx, c, mm);
        JSValue fn = state->events[MG_MQTT_CLIENT_EVENT_ON_MESSAGE];
        if (JS_IsFunction(state->ctx, fn))
            JS_Call(state->ctx, fn, JS_UNDEFINED, 1, &jsMsg);
        JS_FreeValue(state->ctx, jsMsg);
    } else if (ev == MG_EV_CLOSE) {
        JSValue fn = state->events[MG_MQTT_CLIENT_EVENT_ON_CLOSE];
        if (JS_IsFunction(state->ctx, fn))
            JS_Call(state->ctx, fn, JS_UNDEFINED, 0, NULL);
    }
}

static int JS_GetInt32Prop(JSContext *ctx, JSValueConst thisObj, const char *prop, int *res) {
    JSValue val = JS_GetPropertyStr(ctx, thisObj, prop);
    int status = JS_ToInt32(ctx, res, val);
    JS_FreeValue(ctx, val);
    return status;
}

static const char* JS_GetCStringProp(JSContext *ctx, JSValueConst thisObj, const char *prop) {
    JSValue val = JS_GetPropertyStr(ctx, thisObj, prop);
    const char *str = JS_ToCString(ctx, val);
    JS_FreeValue(ctx, val);
    return str;
}

static void parseOptions(JSContext *ctx, JSValue jsOpts, struct mg_mqtt_opts *opts) {
    if (!JS_IsObject(jsOpts)) return;
    opts->user = mg_str_s(JS_GetCStringProp(ctx, jsOpts, "username"));
    opts->pass = mg_str_s(JS_GetCStringProp(ctx, jsOpts, "password"));
}

JSValue mgMqttClientCreate(JSContext *ctx, struct mg_mgr *mgr, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowReferenceError(ctx, "url argument required");
    JSValue obj = JS_NewObjectClass(ctx, mgMqttClientClass.id);
    mgMqttClientObj *state;
    struct mg_mqtt_opts opts;
    memset(&opts, 0, sizeof(opts));
    state = js_mallocz(ctx, sizeof(*state));
    state->url = JS_ToCString(ctx, argv[0]);
    if (argv > 1) parseOptions(ctx, argv[1], &opts);
    state->ctx = ctx;
    state->conn = mg_mqtt_connect(mgr, state->url, &opts, mgMqttClientCb, state);
    JS_SetOpaque(obj, state);
    return obj;
}

static JSValue mgMqttClientEventGet(
    JSContext *ctx, JSValueConst this_val, int magic)
{
    mgMqttClientObj *state = getMgMqttClientObj(this_val);
    return JS_DupValue(ctx, state->events[magic]);
}

static JSValue mgMqttClientEventSet(
    JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
    mgMqttClientObj *state = getMgMqttClientObj(this_val);
    JSValue ev = state->events[magic];
    if (!JS_IsUndefined(ev)) JS_FreeValue(ctx, ev);
    if (JS_IsFunction(ctx, value))
        state->events[magic] = JS_DupValue(ctx, value);
    else
        state->events[magic] = JS_UNDEFINED;
    return JS_UNDEFINED;
}

static JSValue mgMqttClientSubscribe(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    mgMqttClientObj *state = getMgMqttClientObj(this_val);
    if (argc < 1 || !JS_IsString(argv[0])) return JS_ThrowReferenceError(ctx, "invalid topic");
    char *topic = JS_ToCString(ctx, argv[0]);
    int qos = 0;
    struct mg_str mgStrTopic = mg_str_s(topic);
    if (argc > 1 && JS_IsNumber(argv[1])) JS_ToInt32(ctx, &qos, argv[1]);
    mg_mqtt_sub(state->conn, mgStrTopic, qos);
    JS_FreeCString(ctx, topic);
    return JS_UNDEFINED;
}

static JSValue mgMqttClientPublish(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    mgMqttClientObj *state = getMgMqttClientObj(this_val);
    if (argc < 2) return JS_ThrowReferenceError(ctx, "topic and message required");
    if (!JS_IsString(argv[0])) return JS_ThrowReferenceError(ctx, "invalid topic");
    if (!JS_IsString(argv[1])) return JS_ThrowReferenceError(ctx, "invalid message");
    char *topic = JS_ToCString(ctx, argv[0]);
    char *data = JS_ToCString(ctx, argv[1]);
    int qos = 0;
    int retain = 0;
    if (argc > 2 && JS_IsNumber(argv[2])) JS_ToInt32(ctx, &qos, argv[2]);
    if (argc > 3 && JS_IsBool(argv[3])) retain = JS_ToBool(ctx, argv[3]);
    struct mg_str mgStrTopic = mg_str_s(topic);
    struct mg_str mgStrData = mg_str_s(data);
    mg_mqtt_pub(state->conn, mgStrTopic, mgStrData, qos, retain);
    JS_FreeCString(ctx, topic);
    JS_FreeCString(ctx, data);
    return JS_UNDEFINED;
}

static JSCFunctionListEntry mgMqttClientClassFuncs[] = {
    JS_CGETSET_MAGIC_DEF("onOpen", mgMqttClientEventGet, mgMqttClientEventSet, MG_MQTT_CLIENT_EVENT_ON_OPEN),
    JS_CGETSET_MAGIC_DEF("onClose", mgMqttClientEventGet, mgMqttClientEventSet, MG_MQTT_CLIENT_EVENT_ON_OPEN),
    JS_CGETSET_MAGIC_DEF("onMessage", mgMqttClientEventGet, mgMqttClientEventSet, MG_MQTT_CLIENT_EVENT_ON_MESSAGE),
    JS_CFUNC_DEF("subscribe", 1, mgMqttClientSubscribe),
    JS_CFUNC_DEF("publish", 2, mgMqttClientPublish)
};

JSFullClassDef mgMqttClientClass = {
    .def = {
        .class_name = "MongooseMqttClient",
        .finalizer = mgMqttClientFinalizer,
        .gc_mark = mgMqttClientGcMark,
    },
    .constructor = { NULL, 0 },
    .funcs_len = sizeof(mgMqttClientClassFuncs),
    .funcs = mgMqttClientClassFuncs
};
