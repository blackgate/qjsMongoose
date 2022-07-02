#ifndef __MONGOOSE_WS_MESSAGE_JS_H
#define __MONGOOSE_WS_MESSAGE_JS_H

#include "mongoose.h"
#include "js-utils.h"

extern JSFullClassDef mgWsMsgClass;
JSValue mgWsMsgCreate(JSContext *ctx, struct mg_connection *conn, struct mg_ws_message *msg);

#endif