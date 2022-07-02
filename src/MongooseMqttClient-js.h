#ifndef __MONGOOSE_MQTT_CLIENT_JS_H
#define __MONGOOSE_MQTT_CLIENT_JS_H

#include "mongoose.h"
#include "js-utils.h"

extern JSFullClassDef mgMqttClientClass;
JSValue mgMqttClientCreate(JSContext *ctx, struct mg_mgr *mgr, int argc, JSValueConst *argv);

#endif