#ifndef __MONGOOSE_MQTT_MESSAGE_JS_H
#define __MONGOOSE_MQTT_MESSAGE_JS_H

#include "mongoose.h"
#include "js-utils.h"

extern JSFullClassDef mgMqttMsgClass;
JSValue mgMqttMsgCreate(JSContext *ctx, struct mg_connection *conn, struct mg_mqtt_message *msg);

#endif