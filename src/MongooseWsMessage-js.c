#include "MongooseWsMessage-js.h"
#include "MongooseConnection-js.h"

typedef struct {
    JSContext *ctx;
    struct mg_connection *conn;
    struct mg_ws_message *msg;
    JSValue jsConnection;
} mgWsMsgObj;

static mgWsMsgObj* getMgWsMsgObj(JSValueConst this_val) 
{
    return JS_GetOpaque(this_val, mgWsMsgClass.id);
}

static void mgWsMsgFinalizer(JSRuntime *rt, JSValue val) 
{
    mgWsMsgObj *state = getMgWsMsgObj(val);
    JS_FreeValueRT(rt, state->jsConnection);
    js_free(state->ctx, state);
}

static void mgWsMsgGcMark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) 
{
    mgWsMsgObj *state = getMgWsMsgObj(val);
    if (state) 
        JS_MarkValue(rt, state->jsConnection, mark_func);
}

JSValue mgWsMsgCreate(JSContext *ctx, struct mg_connection *conn, struct mg_ws_message *msg)
{
    JSValue obj = JS_NewObjectClass(ctx, mgWsMsgClass.id);
    mgWsMsgObj *state;
    state = js_mallocz(ctx, sizeof(*state));
    state->ctx = ctx;
    state->conn = conn;
    state->msg = msg;
    state->jsConnection = JS_UNDEFINED;
    JS_SetOpaque(obj, state);
    return obj;
}

static JSValue mgWsMsgGetMessage(JSContext *ctx, JSValueConst this_val)
{
    mgWsMsgObj *state = getMgWsMsgObj(this_val);
    return JS_NewArrayBuffer(ctx, state->msg->data.ptr, state->msg->data.len, NULL, NULL, false);
}

static JSValue mgWsMsgGetConnection(JSContext *ctx, JSValueConst this_val)
{
    mgWsMsgObj *state = getMgWsMsgObj(this_val);
    if (JS_IsUndefined(state->jsConnection)) 
        state->jsConnection = mgConnCreate(ctx, state->conn);
    return JS_DupValue(ctx, state->jsConnection);
}

static JSCFunctionListEntry mgWsMsgClassFuncs[] = {
    JS_CGETSET_DEF("message", mgWsMsgGetMessage, NULL),
    JS_CGETSET_DEF("connection", mgWsMsgGetConnection, NULL)
};

JSFullClassDef mgWsMsgClass = {
    .def = {
        .class_name = "MongooseWsMessage",
        .finalizer = mgWsMsgFinalizer,
        .gc_mark = mgWsMsgGcMark,
    },
    .constructor = { NULL, 0 },
    .funcs_len = sizeof(mgWsMsgClassFuncs),
    .funcs = mgWsMsgClassFuncs
};
