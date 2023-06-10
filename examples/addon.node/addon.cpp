#include "whisper.h"
#include <napi.h>
#include <mutex>
#include <thread>
#include <iostream>
#include <vector>

whisper_context *g_context;

// Initialize the whisper context from the model file path.
Napi::Boolean Init(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    std::string path_model = info[0].As<Napi::String>();
    if (g_context == nullptr)
    {
        g_context = whisper_init_from_file(path_model.c_str());
        if (g_context != nullptr)
        {
            return Napi::Boolean::New(env, true);
        }
        else
        {
            return Napi::Boolean::New(env, false);
        }
    }
    return Napi::Boolean::New(env, false);
}

// Free the whisper context
void Free(const Napi::CallbackInfo &info)
{
    if (g_context)
    {
        whisper_free(g_context);
        g_context = nullptr;
    }
}

// Complete default function with the provided parameters.
Napi::Promise FullDefault(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    if (g_context == nullptr)
    {
        Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
        deferred.Reject(Napi::String::New(env, "g_context is nullptr"));
        return deferred.Promise();
    }

    void *data;
    size_t length;
    napi_get_buffer_info(env, info[0], &data, &length);

    struct whisper_full_params params = whisper_full_default_params(whisper_sampling_strategy::WHISPER_SAMPLING_GREEDY);
    params.print_realtime = false;
    params.print_progress = false;
    params.print_timestamps = false;
    params.print_special = false;
    params.translate = false;
    params.single_segment = true;
    params.language = "en";
    params.n_threads = std::min(8, (int)std::thread::hardware_concurrency());
    params.offset_ms = 0;

    std::vector<float> pcmf32;
    // Cast the void pointer to an uint8_t pointer
    uint8_t *data_uint8 = static_cast<uint8_t *>(data);
    // Now, convert and normalize the data to a float, taking care to respect the correct data size
    size_t sample_count = length / sizeof(int16_t);
    pcmf32.resize(sample_count);
    for (size_t i = 0; i < sample_count; ++i)
    {
        // Reassemble the little-endian 16-bit sample from the 8-bit data
        int16_t sample = data_uint8[i * 2] | (data_uint8[i * 2 + 1] << 8);
        // Convert and normalize the 16-bit sample to a floating-point sample
        pcmf32[i] = sample / static_cast<float>(INT16_MAX);
    }
    whisper_reset_timings(g_context);
    whisper_full(g_context, params, pcmf32.data(), pcmf32.size());
    // whisper_print_timings(g_context);
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    const char *text = whisper_full_get_segment_text(g_context, 0);
    
    std::string response(text);
    deferred.Resolve(Napi::String::New(env, response));

    // Return the promise to JavaScript
    return deferred.Promise();
}

Napi::Object InitAddon(Napi::Env env, Napi::Object exports)
{
    exports.Set("init", Napi::Function::New(env, Init));
    exports.Set("free", Napi::Function::New(env, Free));
    exports.Set("fullDefault", Napi::Function::New(env, FullDefault));
    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, InitAddon)