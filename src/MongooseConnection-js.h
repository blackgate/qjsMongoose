#ifndef __MONGOOSE_CONNECTION_JS_H
#define __MONGOOSE_CONNECTION_JS_H

#include "mongoose.h"
#include "js-utils.h"

extern JSFullClassDef mgConnClass;
JSValue mgConnCreate(JSContext *ctx, struct mg_connection *conn);

#endif