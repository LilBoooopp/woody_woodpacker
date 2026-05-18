# woody_woodpacker

*42 school project — cbopp, ilyanar*

Packs an ELF64 executable by compressing and encrypting its `.text` section, then injecting a self-unpacking stub. The packed binary prints `....WOODY....` on startup, decrypts and decompresses itself at runtime, then resumes normal execution — transparently.

---

## How it works

### Injection technique

Standard ELF binaries contain a `PT_NOTE` segment — build metadata that the kernel loads but never executes. `woody_woodpacker` hijacks it:

1. **Compress** `.text` with LZSS
2. **Encrypt** the compressed bytes with RC4 (128-bit key)
3. **Append** the unpacking stub to the end of the file
4. **Repurpose** the `PT_NOTE` segment → `PT_LOAD` at virtual address `0xc000000` (R-X)
5. **Redirect** `e_entry` to the stub's address

```
Before:                                   After:
┌─────────────┐                           ┌─────────────┐
│ ELF Header  │  e_entry → 0x401000       │ ELF Header  │  e_entry → 0xc000000
├─────────────┤                           ├─────────────┤
│ PT_LOAD R-X │  0x400000  (code)         │ PT_LOAD R-X │  0x400000  (encrypted .text)
│ PT_LOAD RW- │  0x600000  (data)         │ PT_LOAD RW- │  0x600000  (data)
│ PT_NOTE     │  (unused metadata)        │ PT_LOAD R-X │  0xc000000 (stub ← new)
├─────────────┤                           ├─────────────┤
│ .text       │  original code            │ .text       │  LZSS-compressed + RC4-encrypted
│ .note       │  useless bytes            │ stub        │  decrypt → decompress → jmp _start
└─────────────┘                           └─────────────┘
```

See `elf64_file_layout.svg` for a detailed diagram of ELF segment structure.

---

### Stub execution (`stub.asm`)

The injected x86_64 assembly stub runs before the original binary entry point:

1. Save `rdx` (`rtld_fini`) — required by glibc `_start`, clobbered by the `write` syscall
2. Print `....WOODY....` via `write(1, ...)`
3. Walk the auxiliary vector (`AT_PHDR`) to compute the runtime load base — PIE-safe, no hardcoded addresses
4. `mprotect` the `.text` region to `RWX`
5. RC4-decrypt `.text` in-place using the key embedded in the stub
6. `mmap` an anonymous buffer, LZSS-decompress into it, `rep movsb` copy back over `.text`
7. `munmap` the temporary buffer
8. `mprotect` `.text` back to `R-X`
9. Restore `rdx` and jump to the original entry point

#### Placeholder patching

The stub is compiled to a flat binary (`stub.bin`) and embedded as a C header via `xxd`. At pack time, `inject_stub()` patches magic constants inside the stub binary using `memmem`:

| Placeholder              | Replaced with                              |
|--------------------------|--------------------------------------------|
| `0xDEADBEEFDEADBEEF`    | `.text` virtual address (link-time)        |
| `0xCAFEBABECAFEBABE`    | Compressed `.text` size                    |
| `0xBEEFCAFEBEEFCAFE`    | Uncompressed `.text` size                  |
| `0xAAAAAAAAAAAAAAAA`    | Original `e_entry`                         |
| `0xFEEDFACEFEEDFACE`    | `PT_PHDR` link-time `p_vaddr` (for PIE base) |
| `DE AD BE EF` × 4 (16 B) | RC4 key                                   |

---

### Compression — LZSS

LZSS (Lempel-Ziv-Storer-Szymanski) is a sliding-window algorithm. It encodes repeated byte sequences as `(offset, length)` back-references into a 4096-byte history buffer, emitting raw literal bytes otherwise.

- Window size: 4096 bytes
- Max match length: 18 bytes
- Match threshold: > 2 bytes (otherwise emit literal)
- Encoded format: 8-token flag byte followed by up to 8 literal bytes or 2-byte match tokens

The compressor uses a binary search tree for O(log n) match finding. Applied to `.text` before encryption; the stub decompresses into a temporary `mmap` buffer at runtime, then copies back in-place.

---

### Encryption — RC4

RC4 stream cipher with a 16-byte (128-bit) key. Encrypts the already-compressed `.text` bytes. Standard KSA + PRGA implementation. The same RC4 logic runs in both the packer (C) and the stub (x86_64 assembly) for symmetry.

Key source:
- **Random**: read from `/dev/urandom`, printed to stdout in hex at pack time
- **Custom**: 32 hex characters passed as `argv[2]`

---

## ELF structure reference

```
ELF File
├── ELF Header       — metadata: entry point, architecture, offsets to tables
├── Program Headers  — segments: what the OS loader maps into memory
│   ├── PT_LOAD      — mapped into memory (R-X code, RW- data)
│   ├── PT_NOTE      — build metadata, ignored at runtime  ← we hijack this
│   └── PT_PHDR      — points to the program header table itself
└── Section Headers  — linker/debugger view: .text, .data, .bss, etc.
```

---

## Building

Requires: **Linux x86_64**, **NASM**, **CMake ≥ 3.16**, **xxd**, **zlib**

```bash
# Configure + build
cmake -B build -S .
cmake --build build

# Or use the helper script
sh build.sh run
cmake --build build

# Clean
sh build.sh clear
```

The build system:
1. Assembles `stub.asm` → `stub.bin` via NASM
2. Converts `stub.bin` → `stub.h` via `xxd -i` (embeds raw bytes as a C array)
3. Compiles `main.c` with `stub.h` on the include path

---

## Usage

```bash
# Pack with a random key (key printed to stdout in hex)
./woody_woodpacker <elf_binary>

# Pack with a custom 128-bit key (exactly 32 hex characters)
./woody_woodpacker <elf_binary> DEADBEEFDEADBEEFDEADBEEFDEADBEEF

# Run the packed binary
./woody
```

Output is always written to `./woody`.

---

## Limitations

- Linux x86_64 only (`ET_EXEC` and `ET_DYN`/PIE supported)
- Target binary must have a `PT_NOTE` segment (standard for GCC/Clang-compiled binaries)
- Stub maps at fixed virtual address `0xc000000` — conflicts with targets that already use that range
- No support for stripped binaries without a `.text` section header
- Compressed output is bounded to the original `.text` size; pathological inputs (random data) may not compress
