# osborne1-cpm

An Osborne 1 clone: a Z80 machine emulator that boots **CP/M 2.2**.

The Osborne 1 (1981) was the first mass-market portable computer: Z80A @ 4 MHz,
64 KB RAM, dual single-sided 5.25" floppy drives, 80x24 CRT, and CP/M 2.2 in ROM
disk. This project emulates the machine and its CP/M operating system layer.

## Layers

```
  frontend (terminal / UI)
        |  serial + keyboard + video events
  machine  (Osborne-1 hardware: RAM, ROM, FDC, CRT, KBD)
        |  memory bus, I/O ports
  cpu      (Z80 instruction set)
        |
  cpm      (BDOS / BIOS, CCP, .COM loader, disk format)
```

## Layout

| Path                | Contents                                        |
| ------------------- | ----------------------------------------------- |
| `src/cpu/`          | Z80 CPU core (registers, decode, execute, flags)|
| `src/machine/`      | Osborne-1 board: bus, memory, video, keyboard   |
| `src/cpm/`          | CP/M 2.2 BDOS, BIOS, CCP, FCB/disk layer        |
| `src/sound/`        | 8253 PIT / PC-speaker tone generator + effects  |
| `src/frontend/`     | Terminal front end, input handling, main()      |
| `include/`          | Public headers                                  |
| `disks/`            | Disk images (`boot.cpm`, blank `.dsk`)          |
| `roms/`             | BIOS/CCP ROM images                             |
| `tests/`            | CPU opcode tests, CP/M API tests                |
| `tools/`            | Disk image builder, hex dumper                  |
| `docs/`             | Hardware notes, CP/M call reference             |

## Build

```
make            # build the emulator
make run        # build and boot CP/M
make test       # run the test suite
make demo       # render the 2600 Hz tone + Pong effects to build/*.wav
```

## Sound

The Osborne 1 has no sound chip: it drives a one-bit speaker from a
counter/timer, and that is enough for every noise the era made. `src/sound/`
models that speaker — one square-wave channel, gated by the speaker port, and
rendered to 8-bit unsigned PCM so it can be pushed at an audio device or dumped
as a `.wav`.

Two effects are built on it:

| Effect    | Frequency    | Shape                       |
| --------- | ------------ | --------------------------- |
| `2600 Hz` | 2600 Hz, 1 s | the classic continuous tone |
| `pong`    | see below    | the arcade game's blips     |

The Pong effect has a note per event:

| Event      | Tone                       |
| ---------- | -------------------------- |
| paddle hit | 1000 Hz, 30 ms             |
| wall hit   | 450 Hz, 30 ms              |
| score/miss | 160 Hz, 250 ms (falling)   |
| serve      | 880 Hz, 120 ms             |
| game over  | 160 Hz, 600 ms (three-note)|

```
osborne1 --sound-demo    # write build/tone-2600.wav and build/pong-*.wav
osborne1 --sound-test    # verify the generator and effects, then exit
```

## Status

Skeleton / scaffold. Every module has a header contract and a stub
implementation to be filled in.
