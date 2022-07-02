#ifndef MONGOOSE_JS_H
#define MONGOOSE_JS_H

#include "js-utils.h"

#ifdef JS_SHARED_LIBRARY
#define JS_INIT_MONGOOSE_MODULE js_init_module
#else
#define JS_INIT_MONGOOSE_MODULE js_init_module_mongoose
#endif

JSModuleDef *JS_INIT_MONGOOSE_MODULE(JSContext *ctx, const char *module_name);

#endif