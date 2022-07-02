#include "MongooseConnection-js.h"

typedef struct {
    JSContext *ctx;
    struct mg_connection *conn;
} mgConnObj;

static mgConnObj* getMgConnObj(JSValueConst this_val) 
{
    return JS_GetOpaque(this_val, mgConnClass.id);
}

static void mgConnFinalizer(JSRuntime *rt, JSValue val) 
{
    mgConnObj *state = getMgConnObj(val);
    js_free(state->ctx, state);
}

static void mgConnGcMark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) 
{
    mgConnObj *state = getMgConnObj(val);
    if (state) 
    {
        // NOTHING TO MARK
    }
}

JSValue mgConnCreate(JSContext *ctx, struct mg_connection *conn)
{
    JSValue obj = JS_NewObjectClass(ctx, mgConnClass.id);
    mgConnObj *state;
    state = js_mallocz(ctx, sizeof(*state));
    state->ctx = ctx;
    state->conn = conn;
    JS_SetOpaque(obj, state);
    return obj;
}

static JSValue mgConnGetLabel(JSContext *ctx, JSValueConst this_val)
{
    mgConnObj *state = getMgConnObj(this_val);
    return JS_NewString(ctx, state->conn->label);
}

static JSValue mgConnSetLabel(JSContext *ctx, JSValueConst this_val, JSValueConst value)
{
    mgConnObj *state = getMgConnObj(this_val);
    size_t labelLen;
    char *label = JS_ToCStringLen(ctx, &labelLen, value);
    size_t len = MIN(labelLen, sizeof(state->conn->label) - 1);
    strncpy(state->conn->label, label, len);
    state->conn->label[len] = '\0';
    JS_FreeCString(ctx, label);
    return JS_UNDEFINED;
}

static JSValue mgConnNext(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv, int magic)
{
    mgConnObj *state = getMgConnObj(this_val);
    if (state->conn->next == NULL)
        return JS_NULL;
    else 
        return mgConnCreate(ctx, state->conn->next);
}

static JSValue mgConnSntpRequest(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    mgConnObj *state = getMgConnObj(this_val);
    time_t secs;
    if (argc > 0 && JS_IsNumber(argv[0]))
        JS_ToInt64(ctx, &secs, argv[0]);
    mg_sntp_request(state->conn);
}

static JSValue mgConnWsSend(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv, int magic)
{
    mgConnObj *state = getMgConnObj(this_val);
    char* data;
    size_t len;
    data = magic == WEBSOCKET_OP_TEXT 
        ? JS_ToCStringLen(ctx, &len, argv[0]) 
        : JS_GetArrayBuffer(ctx, &len, argv[0]);
    mg_ws_send(state->conn, data, len, magic);
    if (magic == WEBSOCKET_OP_TEXT) 
        JS_FreeCString(ctx, data);
    return JS_UNDEFINED;
}

static JSCFunctionListEntry mgConnClassFuncs[] = {
    JS_CFUNC_MAGIC_DEF("wsSendBinary", 1, mgConnWsSend, WEBSOCKET_OP_BINARY),
    JS_CFUNC_MAGIC_DEF("wsSendText", 1, mgConnWsSend, WEBSOCKET_OP_TEXT),
    JS_CGETSET_DEF("label", mgConnGetLabel, mgConnSetLabel),
    JS_CFUNC_DEF("next", 0, mgConnNext),
    JS_CFUNC_DEF("sntpRequest", 0, mgConnSntpRequest)
};

JSFullClassDef mgConnClass = {
    .def = {
        .class_name = "MongooseConnection",
        .finalizer = mgConnFinalizer,
        .gc_mark = mgConnGcMark,
    },
    .constructor = { NULL, 0 },
    .funcs_len = sizeof(mgConnClassFuncs),
    .funcs = mgConnClassFuncs
};
