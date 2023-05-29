#include <napi.h>
#include <thread>
#include "whisper.h"
#include <vector>

struct whisper_context * g_context;

// functions to be implemented using whisper library
Napi::Boolean Init(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    // Check for number of arguments
    if (info.Length() < 1) {
        Napi::TypeError::New(env, "Expected model file path").ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }

    // Check argument types
    if (!info[0].IsString()) {
        Napi::TypeError::New(env, "Expected model file path to be a string").ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }

    std::string modelPath = info[0].As<Napi::String>();
    
    if (g_context == nullptr) {
        g_context = whisper_init_from_file(modelPath.c_str());
        if (g_context != nullptr) {
            return Napi::Boolean::New(env, true);
        } else {
            return Napi::Boolean::New(env, false);
        }
    }

    return Napi::Boolean::New(env, false);
}

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
    exports.Set("init", Napi::Function::New(env, Init));
    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, InitAll)
