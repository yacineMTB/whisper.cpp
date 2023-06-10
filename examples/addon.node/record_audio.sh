#!/bin/bash
# ffmpeg -ss 0 -t 2:00 -i samhyde.wav -f wav -ac 1 -ar 16000 -sample_fmt s16 pipe:1
# ffmpeg -f pulse -ac 1 -ar 16000 -sample_fmt s16 -i default -f s16le pipe:1

ffmpeg -re -ss 0 -t 2:00 -i samhyde.wav -f wav -ac 1 -ar 16000 -sample_fmt s16 pipe:1

# rec -r 16000 -t raw -e signed-integer -b 16 -c 1 -
