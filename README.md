*This project has been created as part of the 42 curriculum by cbopp, ilyanar*
# woody_woodpacker

Packs an ELF64 executable by compressing and encrypting its `.text` section, then injecting a self-unpacking stub. The packed binary prints `....WOODY....` on startup, decrypts and decompresses itself at runtime, then resumes normal execution — transparently.

## How it works

### Injection technique

Standard ELF binaries contain a `PT_NOTE` segment — build metadata that the kernel loads but never uses at runtime. `woody_woodpacker` hijacks it:

1. **Compress** `.text` with LZSS
2. **Encrypt** the compressed bytes with RC4 (128-bit key, random or custom)
3. **Append** the unpacking stub to the file
4. **Repurpose** `PT_NOTE` → `PT_LOAD` at `0xc000000` (R-X)
5. **Redirect** `e_entry` to point at the stub

```
Before:                              After:
┌─────────────┐                      ┌─────────────┐
│ ELF Header  │ e_entry → 0x401000   │ ELF Header  │ e_entry → 0xc000000
├─────────────┤                      ├─────────────┤
│ PT_LOAD R-X │ 0x400000 (code)      │ PT_LOAD R-X │ 0x400000 (encrypted .text)
│ PT_LOAD RW- │ 0x600000 (data)      │ PT_LOAD RW- │ 0x600000 (data)
│ PT_NOTE     │ (unused metadata)    │ PT_LOAD R-X │ 0xc000000 (stub ← new)
├─────────────┤                      ├─────────────┤
│ .text       │ original code        │ .text       │ LZSS-compressed + RC4-encrypted bytes
│ .note       │ useless bytes        │ stub        │ decrypt → decompress → jump → _start
└─────────────┘                      └─────────────┘
```

### Stub execution (stub.asm)

The injected x86_64 assembly stub runs before the original binary:

1. Print `....WOODY....` via `write(1, ...)`
2. Walk the auxiliary vector (`AT_PHDR`) to compute the runtime load base (PIE-safe)
3. `mprotect` the `.text` region to `RWX`
4. RC4-decrypt `.text` in-place using the embedded key
5. `mmap` an anonymous buffer, LZSS-decompress into it, copy back over `.text`
6. `mprotect` `.text` back to `R-X`
7. Restore `rdx` (`rtld_fini`) — required by glibc `_start`
8. Jump to the original entry point

### Compression

LZSS (Lempel-Ziv-Storer-Szymanski) — a sliding-window compression algorithm. Encodes repeated byte sequences as `(offset, length)` back-references into a 4096-byte history buffer. Applied to `.text` before encryption; the stub decompresses into a temporary `mmap` buffer at runtime then copies back in-place.

### Encryption

RC4 stream cipher, 16-byte (128-bit) key. Key is generated from `/dev/urandom` or supplied by the caller. Encrypts the already-compressed `.text` bytes. The key is embedded in the stub at pack time using a magic-byte placeholder pattern.

## ELF structure reference

```
ELF File
├── ELF Header       — metadata: entry point, architecture, offsets to tables
├── Program Headers  — segments: tells the OS loader what to map into memory
│   ├── PT_LOAD      — actually mapped into memory (R-X code, RW- data)
│   ├── PT_NOTE      — build metadata, ignored at runtime (← we hijack this)
│   └── PT_PHDR      — points to the program header table itself
└── Section Headers  — tells linkers/debuggers about .text, .data, etc.
```

## Usage

```bash
# Build
cmake -B build && cmake --build build

# Pack with random key (printed to stdout)
./woody_woodpacker <elf_binary>

# Pack with custom 128-bit key (32 hex chars)
./woody_woodpacker <elf_binary> DEADBEEFDEADBEEFDEADBEEFDEADBEEF

# Run packed binary
./woody
```

Output: `woody` — the packed binary, ready to run.

## Requirements

- Linux x86_64
- NASM (stub assembly)
- CMake ≥ 3.10

## Notes

- Supports `ET_EXEC` (static executables) and `ET_DYN` (PIE binaries)
- Requires a `PT_NOTE` segment in the target binary
- Stub computes load base at runtime via `AT_PHDR` auxv entry — no hardcoded addresses for PIE
