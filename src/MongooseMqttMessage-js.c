#include "MongooseMqttMessage-js.h"
#include "MongooseConnection-js.h"

typedef struct {
    JSContext *ctx;
    struct mg_connection *conn;
    struct mg_mqtt_message *msg;
    JSValue jsConnection;
} mgMqttMsgObj;

static mgMqttMsgObj* getMgMqttMsgObj(JSValueConst this_val) 
{
    return JS_GetOpaque(this_val, mgMqttMsgClass.id);
}

static void mgMqttMsgFinalizer(JSRuntime *rt, JSValue val) 
{
    mgMqttMsgObj *state = getMgMqttMsgObj(val);
    JS_FreeValueRT(rt, state->jsConnection);
    js_free(state->ctx, state);
}

static void mgMqttMsgGcMark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) 
{
    mgMqttMsgObj *state = getMgMqttMsgObj(val);
    if (state) 
        JS_MarkValue(rt, state->jsConnection, mark_func);
}

JSValue mgMqttMsgCreate(JSContext *ctx, struct mg_connection *conn, struct mg_mqtt_message *msg)
{
    JSValue obj = JS_NewObjectClass(ctx, mgMqttMsgClass.id);
    mgMqttMsgObj *state;
    state = js_mallocz(ctx, sizeof(*state));
    state->ctx = ctx;
    state->conn = conn;
    state->msg = msg;
    state->jsConnection = JS_UNDEFINED;
    JS_SetOpaque(obj, state);
    return obj;
}

static JSValue mgMqttMsgGetMessage(JSContext *ctx, JSValueConst this_val)
{
    mgMqttMsgObj *state = getMgMqttMsgObj(this_val);
    return JS_NewStringLen(ctx, state->msg->data.ptr, state->msg->data.len);
}

static JSValue mgMqttMsgGetTopic(JSContext *ctx, JSValueConst this_val)
{
    mgMqttMsgObj *state = getMgMqttMsgObj(this_val);
    return JS_NewStringLen(ctx, state->msg->topic.ptr, state->msg->topic.len);
}

static JSValue mgMqttMsgGetConnection(JSContext *ctx, JSValueConst this_val)
{
    mgMqttMsgObj *state = getMgMqttMsgObj(this_val);
    if (JS_IsUndefined(state->jsConnection)) 
        state->jsConnection = mgConnCreate(ctx, state->conn);
    return JS_DupValue(ctx, state->jsConnection);
}

static JSCFunctionListEntry mgMqttMsgClassFuncs[] = {
    JS_CGETSET_DEF("message", mgMqttMsgGetMessage, NULL),
    JS_CGETSET_DEF("topic", mgMqttMsgGetTopic, NULL),
    JS_CGETSET_DEF("connection", mgMqttMsgGetConnection, NULL)
};

JSFullClassDef mgMqttMsgClass = {
    .def = {
        .class_name = "MongooseMqttMessage",
        .finalizer = mgMqttMsgFinalizer,
        .gc_mark = mgMqttMsgGcMark,
    },
    .constructor = { NULL, 0 },
    .funcs_len = sizeof(mgMqttMsgClassFuncs),
    .funcs = mgMqttMsgClassFuncs
};
