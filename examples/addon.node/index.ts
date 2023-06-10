import { spawn } from 'child_process';
// import { writeFileSync } from 'fs';
// import { addWavByteHeaders } from './src/audioUtil';
const audioProcess = spawn('bash', ['record_audio.sh']);
// CPP addon; this is whisper
const whisper = require('../../build/Release/whisper-addon');
console.log(whisper.init('/Users/kache/Repos/whisper.cpp/models/ggml-tiny.bin'));

// import { ModelLoad, LLama, Generate } from '@llama-node/llama-cpp';
// console.log(LLama)

// const config = {
//     modelPath: '/Users/kache/Downloads/llama-13b-ggml-v2-q4_0.bin',
// };
// LLama.load(config, true)

const SAMPLING_RATE = 16000;
const CHANNELS = 1;
const BIT_DEPTH = 16;

class Conversation {
    private audioBuffer: Buffer;
    public transcriptions: { startByte: number, text: string }[];
    constructor() {
        this.audioBuffer = Buffer.alloc(0);
        this.transcriptions = [];
    }

    addTranscription(startByte: number, text: string) {
        const lastTranscription = this.transcriptions[this.transcriptions.length - 1];
        if (lastTranscription && lastTranscription.startByte === startByte) {
            return lastTranscription.text = text;
        }
        if (!lastTranscription || lastTranscription.text !== text) {
            return this.transcriptions.push({ startByte, text });
        }
    }

    // I could follow along it with a pointer and time
    audioDataEvent(data: Buffer) {
        this.audioBuffer = Buffer.concat([this.audioBuffer, data]);
    }

    getBuffer() {
        return this.audioBuffer;
    }
}
const conversation = new Conversation();

async function whisperLoop() {
    const ONE_SECOND = SAMPLING_RATE * (BIT_DEPTH / 8) * CHANNELS;
    const BUFFER_LENGTH_SECONDS = 28;
    const BUFFER_LENGTH = ONE_SECOND * BUFFER_LENGTH_SECONDS;
    await new Promise(r => setTimeout(r, 2000)); // give the thread a break
    const processAudioSlice = async () => {
        const total_buffer_length = conversation.getBuffer().length;
        let start = total_buffer_length - BUFFER_LENGTH;
        if (start < 0) start = 0;
        const audioSlice = conversation.getBuffer().slice(start, total_buffer_length);

        console.time('Inference Time');
        let result = await whisper.whisperInferenceOnBytes(audioSlice);
        console.timeEnd('Inference Time');

        conversation.addTranscription(start, result);
        if (conversation.transcriptions[conversation.transcriptions.length - 1]) {
            console.log(conversation.transcriptions[conversation.transcriptions.length - 1].text);
            // await infer(llama, `Here is some text from some audio. Come up with one extremely short thought, yet amusing, about what they're saying. 5 words only!. Audio so far: ${conversation.transcriptions[conversation.transcriptions.length - 1].text}`);
        }
        // Schedule the next audio slice to be processed
        setImmediate(processAudioSlice);
    };
    await new Promise(r => setTimeout(r, 10)); // give the thread a break
    setImmediate(processAudioSlice);
}
whisperLoop();

audioProcess.stdout.on('readable', () => {
    let data;
    while (data = audioProcess.stdout.read()) {
        conversation.audioDataEvent(data);
    }
});
audioProcess.stderr.on('data', (data: Buffer) => {
    // console.log(`${data}`);
});
