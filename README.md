# GenericMidiParser - Generic, platform independent, Midi File Parser
## by SkyWodd

This library is a generic, platform independent midi file parser.

This version can handle only one track at the time.
So with non midi type 0 files the parser will execute track 0, when track 1, ...

A new version who handle simultaneous tracks parsing (but require lot of memory fetching) is also available on my github.

---

This library is released with two examples of usage :
* one for arduino board (see ArduinoMidiParser directory)
* one for pc (see main.cpp)