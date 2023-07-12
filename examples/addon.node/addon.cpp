#include "whisper.h"
#include "common.h"
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

// Convert int buffer to float32
std::vector<float> cast_audio_buffer(void *data, size_t length)
{
    std::vector<float> pcmf32;
    uint8_t *data_uint8 = static_cast<uint8_t *>(data);
    size_t sample_count = length / sizeof(int16_t);
    pcmf32.resize(sample_count);
    for (size_t i = 0; i < sample_count; ++i)
    {
        int16_t sample = data_uint8[i * 2] | (data_uint8[i * 2 + 1] << 8);
        pcmf32[i] = sample / static_cast<float>(INT16_MAX);
    }
    return pcmf32;
}

class WhisperAsyncWorker : public Napi::AsyncWorker {
public:
    WhisperAsyncWorker(Napi::Env env, void *data, size_t length, Napi::Promise::Deferred deferred) : 
        AsyncWorker(env), _data(data), _length(length), _deferred(deferred) {}

    void Execute() override {
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
        params.suppress_non_speech_tokens = true;
        params.suppress_blank = true;

        std::vector<float> pcmf32 = cast_audio_buffer(_data, _length);
        whisper_reset_timings(g_context);
        whisper_full(g_context, params, pcmf32.data(), pcmf32.size());
        _response = std::string(whisper_full_get_segment_text(g_context, 0));
    }

    void OnOK() override {
        Napi::HandleScope scope(Env());
        _deferred.Resolve(Napi::String::New(Env(), _response));
    }

    void OnError(const Napi::Error& e) override {
        Napi::HandleScope scope(Env());
        _deferred.Reject(e.Value());
    }

private:
    void *_data;
    size_t _length;
    std::string _response;
    Napi::Promise::Deferred _deferred;
};

Napi::Promise whisperInferenceOnBytes(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (g_context == nullptr)
    {
        throw Napi::Error::New(env, "g_context is nullptr");
    }

    void *data;
    size_t length;
    napi_get_buffer_info(env, info[0], &data, &length);

    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    WhisperAsyncWorker* worker = new WhisperAsyncWorker(env, data, length, deferred);
    worker->Queue();

    return deferred.Promise();
}
/**
 *  VAD which triggers on changes in sound activity
 *  Returns true if the user was speaking and is now finished
 *
 *  CallbackInfo arguments:
 *    0: Audio buffer (Buffer)
 *    1: Sample size in ms (int)
 *    2: VAD multiplier. Lower is more sensitive (float)
 *    3: Noise threshold (float)
 */
Napi::Boolean finishedVoiceActivity(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (g_context == nullptr)
    {
        throw Napi::Error::New(env, "g_context is nullptr");
    }

    void *data;
    int64_t last_ms_int;
    double vad_thold_dbl;
    double energy_thold_dbl;
    size_t length;
    float freq_thold = 100.0f;

    napi_get_buffer_info(env, info[0], &data, &length);
    std::vector<float> pcmf32 = cast_audio_buffer(data, length);
    napi_get_value_int64(env, info[1], &last_ms_int);
    size_t last_ms = static_cast<size_t>(last_ms_int);
    napi_get_value_double(env, info[2], &vad_thold_dbl);
    float vad_thold = static_cast<float>(vad_thold_dbl);
    napi_get_value_double(env, info[3], &energy_thold_dbl);
    float energy_thold = static_cast<float>(energy_thold_dbl);

    bool finished = vad_simple(pcmf32, WHISPER_SAMPLE_RATE, last_ms, vad_thold, energy_thold, freq_thold, false);

    return Napi::Boolean::New(env, finished);
}


// // Complete default function with the provided parameters.
// Napi::Promise whisperInferenceOnBytes(const Napi::CallbackInfo &info)
// {
//     Napi::Env env = info.Env();
//     if (g_context == nullptr)
//     {
//         Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
//         deferred.Reject(Napi::String::New(env, "g_context is nullptr"));
//         return deferred.Promise();
//     }

//     void *data;
//     size_t length;
//     napi_get_buffer_info(env, info[0], &data, &length);

//     struct whisper_full_params params = whisper_full_default_params(whisper_sampling_strategy::WHISPER_SAMPLING_GREEDY);
//     params.print_realtime = false;
//     params.print_progress = false;
//     params.print_timestamps = false;
//     params.print_special = false;
//     params.translate = false;
//     params.single_segment = true;
//     params.language = "en";
//     params.n_threads = std::min(8, (int)std::thread::hardware_concurrency());
//     params.offset_ms = 0;

//     std::vector<float> pcmf32;
//     // Cast the void pointer to an uint8_t pointer
//     uint8_t *data_uint8 = static_cast<uint8_t *>(data);
//     // Now, convert and normalize the data to a float, taking care to respect the correct data size
//     size_t sample_count = length / sizeof(int16_t);
//     pcmf32.resize(sample_count);
//     for (size_t i = 0; i < sample_count; ++i)
//     {
//         // Reassemble the little-endian 16-bit sample from the 8-bit data
//         int16_t sample = data_uint8[i * 2] | (data_uint8[i * 2 + 1] << 8);
//         // Convert and normalize the 16-bit sample to a floating-point sample
//         pcmf32[i] = sample / static_cast<float>(INT16_MAX);
//     }
//     whisper_reset_timings(g_context);
//     whisper_full(g_context, params, pcmf32.data(), pcmf32.size());
//     // whisper_print_timings(g_context);
//     Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
//     const char *text = whisper_full_get_segment_text(g_context, 0);
    
//     std::string response(text);
//     deferred.Resolve(Napi::String::New(env, response));

//     // Return the promise to JavaScript
//     return deferred.Promise();
// }

Napi::Object InitAddon(Napi::Env env, Napi::Object exports)
{
    exports.Set("init", Napi::Function::New(env, Init));
    exports.Set("free", Napi::Function::New(env, Free));
    exports.Set("whisperInferenceOnBytes", Napi::Function::New(env, whisperInferenceOnBytes));
    exports.Set("finishedVoiceActivity", Napi::Function::New(env, finishedVoiceActivity));
    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, InitAddon)
