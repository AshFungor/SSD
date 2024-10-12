# Sound back-end design

To handle audio input/output, this daemon uses **RtAudio** library with system
**ALSA** back-end (It is important to supply **RtAudio** with system **ALSA** since
it should be already configured and ready-to-use).

**RtAudio** runs its own thread to provide **ALSA** with sound samples and request them from SSD clients.
Async callbacks (*readCallback* and *writeCallback*) should be as fast as possible, since long sound-stream borrowing
times may impact stream time and ruin playback/capture. That is why all greedy work is handled by async jobs in-between
callbacks, for example bass filtering.

All SSD clients have a seperate sound buffer, which is then squashed into mono-channel signed 32-bit wide integer stream and balanced with all other active streams, each sample is balanced individually:

```
bool isFirst = true;
for (auto& buffer : buffers) {
    if (isFirst) {
        isFirst = false;
        squashed[frame] = buffer[frame];
    } else {
        squashed[frame] = 2 * ((std::int64_t) buffer[frame] + squashed[frame]) 
            - (std::int64_t) buffer[frame] * squashed[frame] / (INT32_MAX / 2) - INT32_MAX;
    }
}
```

First buffer goes unchanged, then each other is balanced with the result from a previous iteration.

To support testing, SSD is able to pick pulseaudio alsa sink in case it is present in system:

```

```