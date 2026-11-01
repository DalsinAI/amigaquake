# AmigaOS 3 first-pass build

This lane builds the original GPL Quake engine's portable C software renderer
with its upstream null system, video, audio, input, CD, and network backends.
It targets AmigaOS 3.x and Motorola 68040 + FPU with OpenAmigaGCC open3.

Run:

```sh
ports/amigaos3/build-first-pass.sh
```

The output is `ports/amigaos3/build/quake-null`. It proves that the portable
engine closes and links as an AmigaOS loadseg executable without Roadshow,
BSD sockets, proprietary AHI glue, or x86 assembly. It is intentionally
headless and is not yet playable.

The next port gates are OpenGPU presentation, OpenInput events, OpenAudio / 
OpenAHI sound, OpenSocket networking, Amiga timing and filesystem semantics,
and AC090 runtime qualification. Original Quake `id1` data remains external
and must not be included in source or release archives.
