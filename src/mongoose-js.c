#include "mongoose.h"
#include "mongoose-js.h"
#include "MongooseManager-js.h"
#include "MongooseConnection-js.h"
#include "MongooseMqttClient-js.h"
#include "MongooseHttpMessage-js.h"
#include "MongooseWsMessage-js.h"
#include "MongooseMqttMessage-js.h"

static int init(JSContext *ctx, JSModuleDef *m) {
    initFullClass(ctx, m, &mgMgrClass);
    initFullClass(ctx, m, &mgHttpMsgClass);
    initFullClass(ctx, m, &mgWsMsgClass);
    initFullClass(ctx, m, &mgConnClass);
    initFullClass(ctx, m, &mgMqttClientClass);
    initFullClass(ctx, m, &mgMqttMsgClass);
    return 0;
}

JSModuleDef *JS_INIT_MONGOOSE_MODULE(JSContext *ctx, const char *module_name) {
    JSModuleDef *m;
    // mg_log_set("4"); // DEBUG LEVEL
    m = JS_NewCModule(ctx, module_name, init);
    if (!m) return NULL;
    JS_AddModuleExport(ctx, m, mgMgrClass.def.class_name);
    return m;
}