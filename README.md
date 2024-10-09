# TIG

Reimplementation of the engine behind Arcanum: Of Steamworks and Magick Obscura, and The Temple of Elemental Evil.

## Introduction

See [Re-introducing TIG](https://medium.com/@alex.batalov/re-introducing-tig-0b1e4da8e656) story on Medium.

## Status

About 94% of the code is ready. Most of the code is either tested with unit tests or can be verified manually with example.

- The main blitting function (`art_blit`) is huge and very hard to test without actual game.
- In many cases blitting and other rendering routines only support 32 bpp mode. This is intentional. From Fallouts porting experience blitting will be handed to SDL.
- The networking code is mostly untested and rather obscure.
- Some symbols might not be accurate.
- What's missing is the DAT files packing code (used by Arcanum's WorldEd), and some minor stuff (which is not required for core game).

## Contributing

Just like in other porting projects, there are two major pain points - Windows API/DirectX and x64 compatibility.

Additionally, TIG uses Bink for playing movies and Miles Sound System for sound rendering. For now I've created tiny wrappers around appropriate DLLs that come with Arcanum. In order to make the engine cross-platform these two need to be replaced with custom implementations.

Preparing for x64 is mostly about dealing with serialization anti-patterns where the original code reads/writes structs with pointers. An example of this approach can be found in `art.c`.

Roadmap:
 - [ ] Provide platform-specific API (e.g. path resolution, timers, etc.)
 - [ ] Replace DirectX with SDL
 - [ ] Replace WinSock with SDL_Net
 - [ ] Replace Bink (not sure if there is an obvious replacement)
 - [ ] Replace Miles Sound System with OpenAL + minimp3
 - [ ] x64

You're welcome to help with any of these. Keep it C and match surrounding code style.

## Example

There is an `example` application that is very rough approximation of the Arcanum boot sequence. It's just one window with main menu art and it doesn't make much sense on its own, but it's enough to quickly check some features.

`example` uses Arcanum resources so you have to run it from Arcanum directory. If you're using Visual Studio specify `cwd` in your `launch.vs.json`. For Visual Studio Code (with CMake Tools extension installed) specify `cwd` for CMake's Debug Config.

## Tests

TIG comes with some tests. I haven't invested much time in testing because many TIG modules are dependent on platform-specific window, which make testing them a little bit difficult. If you're on board consider adding a few tests if it makes sense.

## License

The source code in this repository is available under the [Sustainable Use License](LICENSE.md).
