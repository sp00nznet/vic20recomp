# vic20recomp

**A static-recompilation toolkit for the Commodore VIC-20 — turning a
cartridge's MOS 6502 ROM into native C.**

The VIC-20 (1980) is a lovely recompilation target. A cartridge is the whole
program: the 6502 powers on, reads the **cold-start vector** out of the cart at
`$A000`, and runs. And the picture isn't a chip you reverse-engineer — like the
Apple II hi-res scan and the Pac-Man tile grid, the VIC-20's screen is **a region
of RAM the VIC chip scans**. If you've recompiled an Apple II (a 6502 writing a
RAM-scanned screen), the VIC-20 is the same idea with a different — and, for the
self-contained carts, *simpler* — machine around it.

`vic20recomp` is the reusable toolkit. The games brought up on it live in
separate repos that consume this one (via `-DVIC20RECOMP_DIR` or a submodule).

- [`vic20-recomp`](../vic20-recomp) — *Jelly Monsters* (Commodore, 1981), the
  VIC-20 Pac-Man clone, recompiled to native C and **rendering its maze in full
  colour**, ~100% as recompiled code.

> **No ROM data here.** VIC-20 cartridges and system ROMs are `.gitignore`d.
> This repo is the recompiler, the runtime and docs — bring your own dump that
> you legally own.

## Why the VIC-20 is a great target

- **One famously documented CPU.** A stock NMOS MOS 6502 — and this toolkit's
  decoder, the flag-correct ALU, the analyzer, the C emitter and the
  interpreter oracle are the **same battle-tested 6502 front-end** that
  recompiled Apple II games (Choplifter, Oregon Trail). The VIC-20 branch just
  swaps the hardware around it.
- **The cartridge is the program.** No OS image to snapshot: the 6502 boots from
  the cold-start vector at `$A000`. The recompiler discovers code by recursive
  descent from there, following `JSR`/`JMP`/branches and seeded by any
  computed-jump targets a runner captures.
- **No graphics chip to fake.** The screen is RAM the VIC scans: a `cols×rows`
  grid of character codes (default 22×23 at `$1E00`), an 8×8 glyph per cell from
  character memory, and a per-cell colour from colour RAM — all reconstructed by
  a **readout that reads RAM**.
- **Flat memory.** Cartridge ROM, screen/colour RAM and the VIC/VIA registers
  are all just addresses in one 64K array — no banking, no soft switches.

## How it works

```
  cartridge .crt ──► strip $A000 load-addr header ──► 8K ROM image at $A000
                  │
                  ▼
          ┌────────────────────┐  decode 6502, discover blocks by recursive
          │  m6502recomp       │  descent from the cold-start vector (+ captured
          │  decode·analyze·   │  JSR seeds), follow JSR/JMP/branches, emit one
          │  emit              │  C function per basic block: vic_func_<addr>
          └────────────────────┘
                  │  generated/recomp_funcs.c
                  ▼
          ┌────────────────────┐  the runtime the generated C links against:
          │  vic20recomp (lib) │  6502 state + flag ALU · the flat VIC-20 memory
          │  src/, include/    │  map · the VIC register/charset/colour readout
          └────────────────────┘
                  │
                  ▼   (+ a per-game host: load cart, boot, run, present)
          native executable — the recompiled cartridge runs
```

Every inter-block transfer (branch, `JMP`, `JSR`, `RTS`, fall-through) sets the
PC and returns to the host driver, which dispatches the next block — so control
flow is never a nested C call and a never-returning game loop can't overflow the
C stack. A target with no recompiled block falls back to the bundled
interpreter, so the cartridge runs end to end while coverage grows.

## Status — it renders, ~100% recompiled

The 6502 front-end (decoder, analyzer, C emitter, interpreter oracle,
self-modifying-code handling) is the mature code shared with the Apple II
toolkit; this repo wires it to VIC-20 hardware.

- ✅ **6502 decoder / analyzer / emitter / interpreter** — shared, reused.
- ✅ **Cartridge boot** — map a `[A000]` `.crt` at its load address, boot from
  the cold-start vector; `RTS`-fill KERNAL/BASIC space + VIC register defaults
  so even KERNAL-leaning carts come up.
- ✅ **Flat VIC-20 memory model** — the whole machine as one 64K array, with an
  optional I/O hook for raster/timer/keyboard reads.
- ✅ **VIC video readout** — `cols×rows` character grid, 8×8 glyphs from ROM or a
  custom RAM charset, per-cell colour, multicolour cells, the 16-colour palette,
  and a built-in font fallback for the (bring-your-own) CHARGEN ROM.
- ✅ **Jelly Monsters renders** — boots from `$A01F`, runs **~100% as recompiled
  native C**, and draws its attract maze in full colour. See
  [`vic20-recomp`](../vic20-recomp).
- 🚧 **VIA-timer IRQ + input** — model the 60 Hz timer IRQ and the keyboard/
  joystick matrix to drive gameplay (a static frame render needs neither).
- 🚧 **Bring-your-own KERNAL/CHARGEN** — load real system ROMs so KERNAL-heavy
  carts (Gorf, Cosmic Cruncher) run their original setup + glyphs.
- 🚧 **SDL frontend** — a live window with sound.

## Building

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
m6502recomp disbin    cart.bin 0xA000 64                     # disassemble
m6502recomp recompbin cart.bin 0xA000 generated 0xA01F --code 0xA000 0xBFFF
```

`recompbin` discovers blocks from the seed addresses (plus `--seeds file` of
captured JSR targets and `--table ADDR N ADJ` for jump tables) and emits
`generated/recomp_funcs.{c,h}`.

## Repository layout

```
include/vic20recomp/  runtime API (cpu, mem, video, recomp_rt)
src/                  runtime: 6502 flag ALU, flat VIC-20 memory map, VIC readout
tools/m6502recomp/    recompiler: decode · analyze · emit, register naming, CLI
docs/                 hardware + recompiler notes
```

## Credits & references

All code is original. With thanks to the 6502 opcode/flag documentation and the
well-documented VIC-20 hardware (the `$9000` register map, the screen/charset
address translation, the colour palette). **No ROM data ships here** — supply
your own cartridge dump.

## License

MIT — see [`LICENSE`](LICENSE). Independent, non-commercial preservation work;
not affiliated with Commodore.
