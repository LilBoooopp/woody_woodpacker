BITS 64
global stub

; ────────────────────────────────────────────────────────────────────────────
; Stable register map (safe across syscalls; all are callee-saved):
;   rbp = load_base
;   r13 = uncompressed_len
;   r14 = compressed_len
;   r15 = text_start (runtime .text virtual address)
;   rbx = decrypted_buf base (anonymous mmap; RC4 plaintext / LZSS input)
;
; NOTE: the `syscall` instruction clobbers rcx (← RIP) and r11 (← RFLAGS).
;       Do NOT use r11 or rcx to hold values that must survive a syscall.
;
; Stack frame (after sub rsp, 272):
;   [rsp +   0 .. 255]  RC4 S-box (256 bytes)
;   [rsp + 256 .. 263]  saved rtld_fini
;   [rsp + 264 .. 271]  decomp_buf base (must survive multiple syscalls)
; ────────────────────────────────────────────────────────────────────────────

stub:
  mov r12, rdx          ; stash rtld_fini (rdx) before any syscall touches it
  mov rbp, rsp          ; anchor for auxv walk

  ; ── print "....WOODY...." ────────────────────────────────────────────────
  call .after_woody_str
  db "....WOODY....", 10
.after_woody_str:
  pop rsi
  mov rax, 1            ; sys_write
  mov rdi, 1
  mov rdx, 14
  syscall

  ; ── capture the RC4 key ─────────────────────────────────────────────────
  call .after_key
  db 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF  ; 16-byte placeholder
  db 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF  ; patched at pack time
.after_key:
  pop rcx               ; rcx = pointer to 16-byte key

  ; ── load patched constants ───────────────────────────────────────────────
  mov r15, 0xDEADBEEFDEADBEEF  ; .text sh_addr   (link-time vaddr, patched)
  mov r14, 0xCAFEBABECAFEBABE  ; compressed_len   (patched)
  mov r13, 0xBEEFCAFEBEEFCAFE  ; uncompressed_len (patched)

  ; ── derive load_base from AT_PHDR in the auxiliary vector ────────────────
  mov rax, [rbp]
  lea rdi, [rbp + rax*8 + 16]  ; &envp[0]
.skip_envp:
  cmp qword [rdi], 0
  je  .envp_done
  add rdi, 8
  jmp .skip_envp
.envp_done:
  add rdi, 8                    ; skip NULL → &auxv[0]
.find_at_phdr:
  mov rax, [rdi]
  test rax, rax
  jz  .no_at_phdr
  cmp rax, 3                    ; AT_PHDR = 3
  je  .got_at_phdr
  add rdi, 16
  jmp .find_at_phdr
.got_at_phdr:
  mov rdi, [rdi + 8]
  mov rax, 0xFEEDFACEFEEDFACE   ; link-time PT_PHDR p_vaddr (patched)
  sub rdi, rax                  ; load_base = AT_PHDR_runtime − PT_PHDR_vaddr
  jmp .have_load_base
.no_at_phdr:
  xor rdi, rdi
.have_load_base:
  mov rbp, rdi
  add r15, rbp                  ; r15 = runtime .text address

  ; ── RC4 KSA: build S-box on the stack ────────────────────────────────────
  sub rsp, 272
  mov [rsp + 256], r12          ; persist rtld_fini (saved before first syscall)

  xor r8, r8
.ksa_init:
  mov byte [rsp + r8], r8b
  inc r8
  cmp r8, 256
  jne .ksa_init

  xor r8, r8                    ; i = 0
  xor r9, r9                    ; j = 0
.ksa_mix:
  mov al,   byte [rsp + r8]
  mov r10,  r8
  and r10,  0x0F                ; i % 16
  add al,   byte [rcx + r10]
  add r9b,  al
  mov al,   byte [rsp + r8]
  mov r10b, byte [rsp + r9]
  mov byte  [rsp + r8], r10b
  mov byte  [rsp + r9], al
  inc r8d
  cmp r8d, 256
  jne .ksa_mix

  ; ── mmap buffer for RC4-decrypted compressed stream ──────────────────────
  ; We never mprotect .text to PROT_WRITE.  On kernels with Intel CET/IBT
  ; support, adding PROT_WRITE to an executable page clears the kernel's
  ; endbr64 endpoint tracking for that page.  Restoring PROT_EXEC does NOT
  ; rebuild the tracking, so the next indirect call into the page (e.g.
  ; _dl_fini → .fini at exit) faults with SIGSEGV/SEGV_ACCERR.
  ; Writing through /proc/self/mem bypasses page permissions entirely and
  ; leaves IBT metadata untouched.
  xor rdi, rdi
  mov rsi, r14          ; compressed_len
  mov rdx, 3            ; PROT_READ|PROT_WRITE
  mov r10, 0x22         ; MAP_PRIVATE|MAP_ANONYMOUS
  mov r8,  -1
  xor r9,  r9
  mov rax, 9            ; sys_mmap
  syscall               ; ← clobbers rcx, r11 (SYSRET mechanism)
  mov rbx, rax          ; rbx = decrypted_buf base (rbx survives syscalls)

  ; ── RC4 PRGA: decrypt .text[0..compressed_len) → decrypted_buf ───────────
  ; After each syscall rcx and r11 are undefined; we re-initialise them here.
  xor r8d, r8d          ; i = 0
  xor r9d, r9d          ; j = 0
  mov r11, r15          ; r11 = advancing source ptr (ciphertext in .text)
  mov rdi, rbx          ; rdi = advancing dest ptr (plaintext → decrypted_buf)
  mov rcx, r14          ; rcx = byte counter
.prga:
  test rcx, rcx
  jz   .prga_done
  inc  r8b
  add  r9b, byte [rsp + r8]
  mov  al,   byte [rsp + r8]   ; al   = S[i]
  mov  r10b, byte [rsp + r9]   ; r10b = S[j]
  mov  byte [rsp + r8], r10b   ; swap
  mov  byte [rsp + r9], al
  add  al, r10b                ; al = (S[i] + S[j]) % 256
  movzx rax, al
  mov  al, byte [rsp + rax]    ; keystream byte
  xor  al, byte [r11]          ; XOR with ciphertext
  mov  byte [rdi], al
  inc  r11
  inc  rdi
  dec  rcx
  jmp  .prga
.prga_done:
  ; rdi = decrypted_buf + compressed_len (spent)
  ; r11 = text_start   + compressed_len (spent; will be clobbered by next syscall anyway)
  ; rbx = decrypted_buf BASE             (preserved)

  ; ── mmap buffer for LZSS decompressed output ─────────────────────────────
  xor rdi, rdi
  mov rsi, r13          ; uncompressed_len
  mov rdx, 3
  mov r10, 0x22
  mov r8,  -1
  xor r9,  r9
  mov rax, 9            ; sys_mmap
  syscall               ; ← clobbers rcx, r11
  ; rax = decomp_buf address.
  ; Save to both a working register (r11, used only within LZSS before the
  ; next syscall) and the stack (survives all subsequent syscalls).
  mov r11, rax
  mov [rsp + 264], rax  ; [rsp+264] = decomp_buf base (syscall-safe storage)

  ; ── LZSS decompression: decrypted_buf → decomp_buf ──────────────────────
  ; r10 = advancing read ptr  (starts at decrypted_buf base = rbx)
  ; r12 = advancing write ptr (starts at decomp_buf base = r11)
  ; r8b = current flag byte, r9b = bits remaining in flag byte
  mov r10, rbx          ; read from start of decrypted_buf
  mov r12, r11          ; write to start of decomp_buf
  xor r8d, r8d
  xor r9d, r9d

.lzss_loop:
  mov rax, r12
  sub rax, r11          ; bytes written = write_ptr − decomp_buf_base
  cmp rax, r13
  jae .lzss_done

  test r9b, r9b
  jnz  .lzss_have_bit
  mov  r8b, [r10]
  inc  r10
  mov  r9b, 8
.lzss_have_bit:
  shl  r8b, 1           ; CF=1 → literal, CF=0 → match
  dec  r9b
  jnc  .lzss_match

.lzss_literal:
  mov al, [r10]
  inc r10
  mov [r12], al
  inc r12
  jmp .lzss_loop

.lzss_match:
  ; byte1 = back_dist[11:4], byte2 = back_dist[3:0] | (len − 3)
  movzx eax, byte [r10]
  inc   r10
  movzx edx, byte [r10]
  inc   r10
  mov   ecx, eax
  shl   ecx, 4
  mov   esi, edx
  shr   esi, 4
  or    ecx, esi          ; ecx = back_dist
  and   edx, 0xF
  add   edx, 3            ; edx = match length
  mov   rsi, r12
  sub   rsi, rcx          ; rsi = copy source (back-reference in output)
.lzss_copy:
  mov al, [rsi]
  mov [r12], al
  inc rsi
  inc r12
  dec edx
  jnz .lzss_copy
  jmp .lzss_loop

.lzss_done:
  ; r11 still = decomp_buf base (LZSS had no syscalls, r11 intact here).
  ; All syscalls below will clobber r11; use [rsp+264] for decomp_buf base.

  ; ── write decompressed .text back via /proc/self/mem ─────────────────────
  call .after_procmem_path
  db "/proc/self/mem", 0
.after_procmem_path:
  pop  rdi                ; rdi = path
  mov  rax, 2             ; sys_open
  mov  rsi, 2             ; O_RDWR
  xor  rdx, rdx
  syscall                 ; clobbers r11, rcx
  mov  r12, rax           ; r12 = fd (r12 is callee-saved; safe across syscalls)

  mov  rdi, r12           ; fd
  mov  rsi, r15           ; offset = runtime .text address
  xor  rdx, rdx           ; SEEK_SET = 0
  mov  rax, 8             ; sys_lseek
  syscall

  mov  rdi, r12           ; fd
  mov  rsi, [rsp + 264]   ; decomp_buf base (from stack; r11 was clobbered)
  mov  rdx, r13           ; uncompressed_len
  mov  rax, 1             ; sys_write
  syscall

  mov  rdi, r12           ; fd
  mov  rax, 3             ; sys_close
  syscall

  ; ── release temporary buffers ────────────────────────────────────────────
  mov rdi, rbx            ; decrypted_buf base
  mov rsi, r14            ; compressed_len
  mov rax, 11             ; sys_munmap
  syscall

  mov rdi, [rsp + 264]    ; decomp_buf base
  mov rsi, r13            ; uncompressed_len
  mov rax, 11
  syscall

  ; ── restore ABI state and jump to original entry point ───────────────────
  mov rdx, [rsp + 256]    ; restore rtld_fini for _start
  add rsp, 272            ; undo stack frame

  mov r11, 0xAAAAAAAAAAAAAAAA  ; e_entry placeholder (patched)
  add r11, rbp                 ; + load_base = runtime _start
  jmp r11
