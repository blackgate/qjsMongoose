#ifndef __MONGOOSE_HTTP_MESSAGE_JS_H
#define __MONGOOSE_HTTP_MESSAGE_JS_H

#include "mongoose.h"
#include "js-utils.h"

extern JSFullClassDef mgHttpMsgClass;
JSValue mgHttpMsgCreate(JSContext *ctx, struct mg_connection *conn, struct mg_http_message *msg);

#endif