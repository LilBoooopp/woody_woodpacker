# woody_woodpacker

ELF File
├── ELF Header          (metadata: entry point, architecture, offsets to tables)
├── Program Headers     (segments — tells the OS loader what to map into memory)
├── Section Headers     (tells linkers/debuggers about .text, .data, etc.)
└── Raw data            (the actual bytes both views point into)

## Key Segment Types
When the kernel loads an ELF, it reads the program headers and maps segments into memory. The important ones:
* PT_LOAD — the only type that actually gets mapped into memory. Every executable has at least two: one R-X (code) and one RW- (data).
* PT_NOTE — originally meant to hold build metadata (compiler version, OS ABI info). The kernel reads it but doesn't need it at runtime. If you destroyed its contents, the program would still run fine.
* PT_PHDR — points to the program header table itself.

## The Entry Point
The ELF header contains an e_entry field — a virtual memory address telling the kernel: "jump here to start execution." This normally points into .text at main (or more precisely, at _start).
If you change e_entry to point somewhere else... execution starts there instead.

## Injection technique

1) Find PT_NOTE, which is irrelevant for running (metadata, verions, etc)

2) Inject stub into this already allocated section

3) Change program header entry from PT_NOTE to PT_LOAD with R-X perms

4) now change e_entry (program start address) to decrypt stub.
 * kernel loads stub into memory because it thinks it's PT_LOAD
 * execution starts at stub
 * stub decrypts/decompresses and then jumps to original binary entry point.

# Before:
```
┌─────────────┐
│ ELF Header  │ e_entry → 0x401000 (original _start)
├─────────────┤
│ PT_LOAD     │ 0x400000, R-X  (code)
│ PT_LOAD     │ 0x600000, RW-  (data)
│ PT_NOTE     │ (build metadata, unused at runtime)
├─────────────┤
│ .text       │ original code
│ .note       │ useless bytes
└─────────────┘
```

# After:
```
┌─────────────┐
│ ELF Header  │ e_entry → 0xc000000 (YOUR STUB)
├─────────────┤
│ PT_LOAD     │ 0x400000, R-X  (code, now encrypted)
│ PT_LOAD     │ 0x600000, RW-  (data)
│ PT_LOAD ◄───┤ 0xc000000, R-X (was PT_NOTE, now your stub)
├─────────────┤
│ .text       │ encrypted+compressed bytes
│ stub code   │ decrypt → decompress → jump to 0x401000
└─────────────┘
```
