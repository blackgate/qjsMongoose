#include "MongooseHttpMessage-js.h"
#include "MongooseConnection-js.h"

static char *mg_str_to_cstr(struct mg_str *src, char *dest, size_t dest_size) {
  int bytesToCopy = MIN(src->len, dest_size - 1);
  strncpy(dest, src->ptr, bytesToCopy);
  dest[bytesToCopy] = '\0';
  return dest;
}

enum {
    MG_MSG_PROP_URI,
    MG_MSG_PROP_METHOD,
    MG_MSG_PROP_BODY,
    MG_MSG_PROP_QUERY,
    MG_MSG_PROP_MESSAGE
};

typedef struct {
    JSContext *ctx;
    struct mg_connection *conn;
    struct mg_http_message *msg;
    JSValue jsConnection;
    JSValue jsHeaders;
} mgHttpMsgObj;

static mgHttpMsgObj* getMgHttpMsgObj(JSValueConst this_val) 
{
    return JS_GetOpaque(this_val, mgHttpMsgClass.id);
}

static void mgHttpMsgFinalizer(JSRuntime *rt, JSValue val) 
{
    mgHttpMsgObj *state = getMgHttpMsgObj(val);
    JS_FreeValueRT(rt, state->jsConnection);
    JS_FreeValueRT(rt, state->jsHeaders);
    js_free(state->ctx, state);
}

static void mgHttpMsgGcMark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) 
{
    mgHttpMsgObj *state = getMgHttpMsgObj(val);
    if (state) 
    {
        JS_MarkValue(rt, state->jsConnection, mark_func);
        JS_MarkValue(rt, state->jsHeaders, mark_func);
    }
}

JSValue mgHttpMsgCreate(JSContext *ctx, struct mg_connection *conn, struct mg_http_message *msg)
{
    JSValue obj = JS_NewObjectClass(ctx, mgHttpMsgClass.id);
    mgHttpMsgObj *state;
    state = js_mallocz(ctx, sizeof(*state));
    state->ctx = ctx;
    state->conn = conn;
    state->msg = msg;
    state->jsConnection = JS_UNDEFINED;
    state->jsHeaders = JS_UNDEFINED;
    JS_SetOpaque(obj, state);
    return obj;
}

static JSValue mgHttpMsgHttpServe(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv, int magic)
{
    mgHttpMsgObj *state = getMgHttpMsgObj(this_val);
    const char *path = JS_ToCString(ctx, argv[0]);
    const char *extraHeaders = NULL; 
    const char *mineTypes = NULL; 
    if (argc > 1)
        extraHeaders = JS_ToCString(ctx, argv[1]);
    if (argc > 2)
        mineTypes = JS_ToCString(ctx, argv[2]);
    struct mg_http_message *msg = state->msg;
    struct mg_http_serve_opts opts = { 
        .root_dir = path, 
        .extra_headers = extraHeaders, 
        .mime_types = mineTypes 
    };
    if (magic == 0)
        mg_http_serve_dir(state->conn, msg, &opts);
    else
        mg_http_serve_file(state->conn, msg, path, &opts);
    JS_FreeCString(ctx, path);
    JS_FreeCString(ctx, extraHeaders);
    JS_FreeCString(ctx, mineTypes);
    return JS_UNDEFINED;
}

static JSValue mgHttpMsgWsUpgrade(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    mgHttpMsgObj *state = getMgHttpMsgObj(this_val);
    struct mg_http_message *msg = state->msg;
    mg_ws_upgrade(state->conn, msg, NULL);
    return JS_UNDEFINED;
}

static JSValue mgHttpMsgHttpReply(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    mgHttpMsgObj *state = getMgHttpMsgObj(this_val);
    int status;
    char *headers;
    char *body;
    if (JS_ToInt32(ctx, &status, argv[0]) != 0)
        return JS_ThrowTypeError(ctx, "status code in not a number");
    headers = JS_ToCString(ctx, argv[1]);
    body = JS_ToCString(ctx, argv[2]);
    mg_http_reply(state->conn, status, headers, "%s", body);
    JS_FreeCString(ctx, headers);
    JS_FreeCString(ctx, body);
    return JS_UNDEFINED;
}

static JSValue mgHttpMsgHttpWrite(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    mgHttpMsgObj *state = getMgHttpMsgObj(this_val);
    size_t len;
    char *body = JS_ToCStringLen(ctx, &len, argv[0]);
    mg_http_write_chunk(state->conn, body, len);
    JS_FreeCString(ctx, body);
    return JS_UNDEFINED;
}

static struct mg_str *getHttpMessageStrProp(struct mg_http_message *msg, int prop) {
    switch (prop)
    {
        case MG_MSG_PROP_BODY:
            return &msg->body;
        case MG_MSG_PROP_URI:
            return &msg->uri;
        case MG_MSG_PROP_METHOD:
            return &msg->method;
        case MG_MSG_PROP_QUERY:
            return &msg->query;
        case MG_MSG_PROP_MESSAGE:
            return &msg->message;
        default:
            return NULL;
    }
}

static JSValue mgHttpMsgGetProp(JSContext *ctx, JSValueConst this_val, int magic)
{
    mgHttpMsgObj *state = getMgHttpMsgObj(this_val);
    struct mg_http_message *msg = state->msg;
    struct mg_str *prop = getHttpMessageStrProp(msg, magic);
    if (prop == NULL) return JS_ThrowInternalError(ctx, "unknown property");
    return JS_NewStringLen(ctx, prop->ptr, prop->len);
}

static JSValue mgHttpMsgGetHeaders(JSContext *ctx, JSValueConst this_val)
{
    mgHttpMsgObj *state = getMgHttpMsgObj(this_val);
    if (JS_IsUndefined(state->jsHeaders)) 
    {
        struct mg_http_message *msg = state->msg;
        state->jsHeaders = JS_NewObject(ctx);
        for (int i = 0; i < MG_MAX_HTTP_HEADERS; i++) {
            struct mg_http_header h = msg->headers[i];
            if (h.name.ptr == NULL) break;
            char name[h.name.len + 1];
            JSValue val = JS_NewStringLen(ctx, h.value.ptr, h.value.len);
            mg_str_to_cstr(&h.name, name, sizeof(name));
            JS_SetPropertyStr(ctx, state->jsHeaders, name, val);
        }
    }
    return JS_DupValue(ctx, state->jsHeaders);
}

static JSValue mgHttpMsgGetHeaderValue(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    mgHttpMsgObj *state = getMgHttpMsgObj(this_val);
    struct mg_http_message *msg = state->msg;
    char *name = JS_ToCString(ctx, argv[0]);
    struct mg_str *val;
    if (name == NULL)
        return JS_ThrowTypeError(ctx, "invalid header name");
    val = mg_http_get_header(msg, name);
    JS_FreeCString(ctx, name);
    return val == NULL ? JS_NULL : JS_NewStringLen(ctx, val->ptr, val->len);
}

static JSValue mgHttpMsgGetConnection(JSContext *ctx, JSValueConst this_val)
{
    mgHttpMsgObj *state = getMgHttpMsgObj(this_val);
    if (JS_IsUndefined(state->jsConnection)) 
        state->jsConnection = mgConnCreate(ctx, state->conn);
    return JS_DupValue(ctx, state->jsConnection);
}

static JSCFunctionListEntry mgHttpMsgClassFuncs[] = {
    JS_CFUNC_MAGIC_DEF("httpServeDir", 1, mgHttpMsgHttpServe, 0),
    JS_CFUNC_MAGIC_DEF("httpServeFile", 1, mgHttpMsgHttpServe, 1),
    JS_CFUNC_DEF("wsUpgrade", 0, mgHttpMsgWsUpgrade),
    JS_CFUNC_DEF("httpReply", 3, mgHttpMsgHttpReply),
    JS_CFUNC_DEF("httpWrite", 3, mgHttpMsgHttpWrite),
    JS_CFUNC_DEF("getHeaderValue", 1, mgHttpMsgGetHeaderValue),
    JS_CGETSET_MAGIC_DEF("uri", mgHttpMsgGetProp, NULL, MG_MSG_PROP_URI),
    JS_CGETSET_MAGIC_DEF("query", mgHttpMsgGetProp, NULL, MG_MSG_PROP_QUERY),
    JS_CGETSET_MAGIC_DEF("method", mgHttpMsgGetProp, NULL, MG_MSG_PROP_METHOD),
    JS_CGETSET_MAGIC_DEF("body", mgHttpMsgGetProp, NULL, MG_MSG_PROP_BODY),
    JS_CGETSET_MAGIC_DEF("message", mgHttpMsgGetProp, NULL, MG_MSG_PROP_MESSAGE),
    JS_CGETSET_DEF("headers", mgHttpMsgGetHeaders, NULL),
    JS_CGETSET_DEF("connection", mgHttpMsgGetConnection, NULL)
};

JSFullClassDef mgHttpMsgClass = {
    .def = {
        .class_name = "MongooseHttpMessage",
        .finalizer = mgHttpMsgFinalizer,
        .gc_mark = mgHttpMsgGcMark,
    },
    .constructor = { NULL, 0 },
    .funcs_len = sizeof(mgHttpMsgClassFuncs),
    .funcs = mgHttpMsgClassFuncs
};