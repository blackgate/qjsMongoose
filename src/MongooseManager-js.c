#include "MongooseManager-js.h"
#include "mongoose.h"
#include "MongooseConnection-js.h"
#include "MongooseHttpMessage-js.h"
#include "MongooseWsMessage-js.h"
#include "MongooseMqttClient-js.h"

enum {
    MG_MGR_EVENT_HTTP_MESSAGE,
    MG_MGR_EVENT_HTTP_CLOSE,
    MG_MGR_EVENT_WS_MESSAGE,
    MG_MGR_EVENT_WS_OPEN,
    MG_MGR_EVENT_SNTP_MESSAGE,
    MG_MGR_EVENT_MAX,
};

typedef struct {
    JSContext *ctx;
    struct mg_mgr mgr;
    JSValue events[MG_MGR_EVENT_MAX];
} mgMgrObj;

static mgMgrObj* getMgMgrObj(JSValueConst this_val) 
{
    return JS_GetOpaque(this_val, mgMgrClass.id);
}

static JSValue mgMgrContructor(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    JSValue obj = JS_NewObjectClass(ctx, mgMgrClass.id);
    mgMgrObj *state;
    state = js_mallocz(ctx, sizeof(*state));
    state->ctx = ctx;
    for (int i = 0 ; i < MG_MGR_EVENT_MAX; i++) 
        state->events[i] = JS_UNDEFINED;
    mg_mgr_init(&state->mgr);
    JS_SetOpaque(obj, state);
    return obj;
}

static void mgMgrHttpCallback(struct mg_connection *c, int ev, void *ev_data, void *fn_data) 
{
    mgMgrObj *state = fn_data;
    if (ev == MG_EV_HTTP_MSG) 
    {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        JSValue fn = state->events[MG_MGR_EVENT_HTTP_MESSAGE];
        JSValue msgObj = mgHttpMsgCreate(state->ctx, c, hm);
        if (JS_IsFunction(state->ctx, fn))
            JS_Call(state->ctx, fn, JS_UNDEFINED, 1, &msgObj);
        JS_FreeValue(state->ctx, msgObj);
    } 
    else if (ev == MG_EV_WS_OPEN) 
    {
        JSValue fn = state->events[MG_MGR_EVENT_WS_OPEN];
        JSValue connObj = mgConnCreate(state->ctx, c); 
        if (JS_IsFunction(state->ctx, fn))
            JS_Call(state->ctx, fn, JS_UNDEFINED, 1, &connObj);
        JS_FreeValue(state->ctx, connObj);
    }
    else if (ev == MG_EV_WS_MSG) 
    {
        struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
        JSValue fn = state->events[MG_MGR_EVENT_WS_MESSAGE];
        JSValue msgObj = mgWsMsgCreate(state->ctx, c, wm); 
        if (JS_IsFunction(state->ctx, fn))
            JS_Call(state->ctx, fn, JS_UNDEFINED, 1, &msgObj);
        JS_FreeValue(state->ctx, msgObj);
    }
    else if (ev == MG_EV_CLOSE) 
    {
        JSValue fn = state->events[MG_MGR_EVENT_HTTP_CLOSE];
        JSValue connObj = mgConnCreate(state->ctx, c); 
        if (JS_IsFunction(state->ctx, fn))
            JS_Call(state->ctx, fn, JS_UNDEFINED, 1, &connObj);
        JS_FreeValue(state->ctx, connObj);
    }
}

static JSValue mgMgrHttpListen(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    mgMgrObj *state = getMgMgrObj(this_val);
    const char *url = JS_ToCString(ctx, argv[0]);
    mg_http_listen(&state->mgr, url, mgMgrHttpCallback, state);
    JS_FreeCString(ctx, url);
    return JS_UNDEFINED;
}

static JSValue mgMgrPoll(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    mgMgrObj *state = getMgMgrObj(this_val);
    int ms;
    if (JS_ToInt32(ctx, &ms, argv[0]) != 0)
        return JS_ThrowTypeError(ctx, "The ms value should be an integer");
    mg_mgr_poll(&state->mgr, ms);
    return JS_UNDEFINED;
}

static void mgMgrSntpCb(struct mg_connection *c, int ev, void *evd, void *fnd) {
  mgMgrObj *state = fnd;
  if (ev == MG_EV_SNTP_TIME) {
    int64_t t = *(int64_t *) evd;
    JSValue fn = state->events[MG_MGR_EVENT_SNTP_MESSAGE];
    JSValue secs = JS_NewInt64(state->ctx, t);
    if (JS_IsFunction(state->ctx, fn))
      JS_Call(state->ctx, fn, JS_UNDEFINED, 1, &secs);
  }
}

static JSValue mgMgrSntpConnect(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    mgMgrObj *state = getMgMgrObj(this_val);
    struct mg_connection *c = mg_sntp_connect(
        &state->mgr, NULL /* connect to time.google.com */, mgMgrSntpCb, state);
    return mgConnCreate(ctx, c);
}

static JSValue mgMgrEventGet(
    JSContext *ctx, JSValueConst this_val, int magic)
{
    mgMgrObj *state = getMgMgrObj(this_val);
    return JS_DupValue(ctx, state->events[magic]);
}

static JSValue mgMgrEventSet(
    JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
    mgMgrObj *state = getMgMgrObj(this_val);
    JSValue ev = state->events[magic];
    if (!JS_IsUndefined(ev)) JS_FreeValue(ctx, ev);
    if (JS_IsFunction(ctx, value))
        state->events[magic] = JS_DupValue(ctx, value);
    else
        state->events[magic] = JS_UNDEFINED;
    return JS_UNDEFINED;
}

static void mgMgrFinalizer(JSRuntime *rt, JSValue val) 
{
    mgMgrObj *state = getMgMgrObj(val);
    mg_mgr_free(&state->mgr);
    for (int i = 0 ; i < MG_MGR_EVENT_MAX; i++)
        JS_FreeValueRT(rt, state->events[i]);
    js_free(state->ctx, state);
}

static void mgMgrGcMark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) 
{
    mgMgrObj *state = getMgMgrObj(val);
    if (state) 
    {
        for (int i = 0 ; i < MG_MGR_EVENT_MAX; i++)
            JS_MarkValue(rt, state->events[i], mark_func);
    }
}

static JSValue mgMgrGetConnections(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    mgMgrObj *state = getMgMgrObj(this_val);
    return mgConnCreate(ctx, state->mgr.conns);
}

static JSValue mgMgrCreateMqttClient(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    mgMgrObj *state = getMgMgrObj(this_val);
    return mgMqttClientCreate(ctx, &state->mgr, argc, argv);
}

static JSCFunctionListEntry mgMgrClassFuncs[] = {
    JS_CFUNC_DEF("httpListen", 1, mgMgrHttpListen),
    JS_CFUNC_DEF("poll", 1, mgMgrPoll),
    JS_CFUNC_DEF("sntpConnect", 0, mgMgrSntpConnect),
    JS_CGETSET_MAGIC_DEF("onHttpMessage", mgMgrEventGet, mgMgrEventSet, MG_MGR_EVENT_HTTP_MESSAGE),
    JS_CGETSET_MAGIC_DEF("onHttpClose", mgMgrEventGet, mgMgrEventSet, MG_MGR_EVENT_HTTP_CLOSE),
    JS_CGETSET_MAGIC_DEF("onWsOpen", mgMgrEventGet, mgMgrEventSet, MG_MGR_EVENT_WS_OPEN),
    JS_CGETSET_MAGIC_DEF("onWsMessage", mgMgrEventGet, mgMgrEventSet, MG_MGR_EVENT_WS_MESSAGE),
    JS_CGETSET_MAGIC_DEF("onSntpMessage", mgMgrEventGet, mgMgrEventSet, MG_MGR_EVENT_SNTP_MESSAGE),
    JS_CFUNC_DEF("getConnections", 0, mgMgrGetConnections),
    JS_CFUNC_DEF("createMqttClient", 0, mgMgrCreateMqttClient)
};

JSFullClassDef mgMgrClass = {
    .def = {
        .class_name = "MongooseManager",
        .finalizer = mgMgrFinalizer,
        .gc_mark = mgMgrGcMark,
    },
    .constructor = { mgMgrContructor, .args_count = 1 },
    .funcs_len = sizeof(mgMgrClassFuncs),
    .funcs = mgMgrClassFuncs
};