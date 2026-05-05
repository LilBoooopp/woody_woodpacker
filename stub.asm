BITS 64
global stub

stub:
  mov r15, rdx ; save rdx (rtld_fini) before write syscall clobbers it
  mov rbp, rsp ; save initial RSP for auxv walk
  call after_string
  db "....WOODY....", 10

after_string:
  pop rsi

  ; write "....WOODY...."
  mov rax, 1
  mov rdi, 1
  mov rdx, 14
  syscall

  call after_key
  db 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF ; 16-byte placeholder for S
after_key:
  pop rcx

  ; KSA init S-box
  sub rsp, 264         ; 256 for S-box + 8 to save rdx (rtld_fini)
  mov [rsp + 256], r15 ; save rtld_fini (captured in r15 before write syscall)
  xor r8, r8

  ; s[i] = i (until 256)
.loop:
  mov byte [rsp + r8], r8b
  inc r8
  cmp r8, 256
  jne .loop

  ; r8 = i, r9b = j, rcx = key_pointer, rsp = S-box
  xor r8, r8
  xor r9, r9
.loop2:
  mov al, byte [rsp + r8] ; al = S[i]
  mov r10, r8
  and r10, 0x0F ; r10 = i % 16
  add al, byte [rcx + r10] ; al = S[i] + key[i % s]
  add r9b, al ; j = (j + S[i] + key[i % 16]) % 256 (wraps naturally)

  ; swap bytes
  mov al, byte [rsp + r8]
  mov bl, byte [rsp + r9]
  mov byte [rsp + r8], bl
  mov byte [rsp + r9], al
  
  inc r8d
  cmp r8d, 256
  jne .loop2

  ; PRGA
  mov r14, 0xDEADBEEFDEADBEEF ; placeholder for text_data address
  mov r15, 0xCAFEBABECAFEBABE ; placeholder for text size

  ; get load_base via auxv AT_PHDR
  ; initial stack at rbp: [argc][argv][NULL][envp][NULL][auxv]
  mov rax, [rbp] ; argc
  lea rdi, [rbp + rax*8 + 16] ; &envp[0]: skip 8(argc) + argc*8(argv) + 8(NULL)

.skip_envp:
  cmp qword [rdi], 0
  je .envp_done
  add rdi, 8
  jmp .skip_envp
.envp_done:
  add rdi, 8 ; skip NULL -> &auxv[0]

.find_at_phdr:
  mov rax, [rdi]
  test rax, rax ; AT_NULL = 0?
  jz .no_at_phdr
  cmp rax, 3 ; AT_PHDR = 3
  je .got_at_phdr
  add rdi, 16
  jmp .find_at_phdr

.got_at_phdr:
  mov rdi, [rdi + 8] ; rdi = AT_PHDR runtime address
  mov rax, 0xFEEDFACEFEEDFACE ; placeholder: PT_PHDR link-timep p_vaddr
  sub rdi, rax ; load_base = AT_PHDR_runtime - PT_PHDR_link_vaddr
  jmp .have_load_base

.no_at_phdr:
  xor rdi, rdi ; load_base = 0 fallback

.have_load_base:
  mov rbp, rdi ; rbp = load_base (survives all subsequent syscalls)
  add r14, rbp ; r14 = actual runtime text address

  mov r12, r14 ; r12 = runtime text start (saved, survives syscall)

  
  ; mprotect(page_aligned(text_start), size + page_offset, RWX)
  mov rdi, r14
  mov rsi, r15
  mov rax, rdi
  and rax, 0xFFF  ; page offset of text start
  add rsi, rax    ; extend size to cover from page boundary to text end
  and rdi, ~0xFFF ; round down to page boundary
  mov rdx, 7 ; RWX
  mov rax, 10 ; mprotect
  syscall

  xor r8, r8
  xor r9, r9

.loop3:
  inc r8b                   ; i = (i+1) % 256 — must wrap at 8 bits, not 32
  movzx r9, r9b
  add r9b, byte [rsp + r8]

  ; swap s[i] s[j]
  mov al, byte [rsp + r8]
  mov bl, byte [rsp + r9]
  mov byte [rsp + r8], bl
  mov byte [rsp + r9], al

  ; data[k] ^= s[(s[i] + s[j]) % 256]
  mov al, byte [rsp + r8] ; al = new s[i]
  add al, byte [rsp + r9] ; al = new s[i] + new s[j]
  movzx rax, al ; zero extend al for use as index
  mov bl, byte [rsp + rax] ; bl = s[(s[i] + s[j]) % 256]
  xor byte [r14], bl ; data[k] ^= bl
  inc r14
  dec r15
  jnz .loop3

  ; mmap(NULL, uncompressed_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
  push r12 ; save original text_start
  xor rdi, rdi ; addr = NULL
  mov rsi, r13 ; length = uncompressed_size
  mov rdx, 3 ; PROT_READ|PROT_WRITE
  mov r10, 0x22 ; MAP_PRIVATE|MAP_ANONYMOUS
  mov r8, -1 ; fd = -1
  xor r9, r9 ; offset = 0
  mov rax, 9 ; mmap
  syscall
  pop r10 ; original text_start
  mov rbx, rax ; dst
  mov r11, rax ; r11 = mmap base

  ; TODO: aplib decompress
  ; r12 = src 
  ; rbx = dst (output buffer)
  ; r8b = tag, r9b = bitcount, r14d = R0, r15d = lwm

  mov r14d, 0xFFFFFFFF
  xor r15d, r15d ; lwm = 0
  xor r9d, r9d ; bitcount = 0

.decomp_loop:
  call .getbit
  jnc .lit ; bit=0 -> literal

  call .getbit
  jnc .block ; bit=0 -> 10 = block copy

  call .getbit
  jnc .short ; bit=0 -> 110 = short match

  ; 111 -> write literal zero byte
  mov byte [rbx], 0
  inc rbx
  xor r15d, r15d
  jmp .decomp_loop

.lit:
  mov al, [r12]
  inc r12
  mov [rbx], al
  inc rbx
  xor r15d, r15d
  jmp .decomp_loop

.short:
  movzx eax, byte [r12]
  inc r12
  mov ecx, eax
  shr ecx, 1 ; offset = byte >> 1 (7 bits)
  jz .decomp_done ; offset == 0 -> end of stream
  and eax, 1
  add eax, 2 ; len = (byte & 1) + 2 -> 2 or 3
  mov r14d, ecx ; R0 = offset
  mov r15d, 1 ; lwm = 1
  mov rsi, rbx
  sub rsi, rcx ; rsi = match src
.short_copy:
  mov dl, [rsi]
  mov [rbx], dl
  inc rsi
  inc rbx
  dec eax
  jnz .short_copy
  jmp .decomp_loop

.block:
  call .getgamma ; eax = gamma value

  ; memcpy: copy rbx back to r12, length = r13
  mov rdi, r12
  mov rsi, rbx
  mov rcx, r13
  rep movsb

  ; munmap(rbx, uncompressed_size)
  mov rdi, rbx
  mov rsi, r13
  mov rax, 11 ; munmap
  syscall

  ; restore .text to R-X
  ; size = r14 (text_end after loop) - r12 (text_start) - avoids relying on rbx
  mov rdi, r12
  mov rsi, r14
  sub rsi, r12
  mov rax, rdi
  and rax, 0xFFF  ; page offset of text start
  add rsi, rax    ; extend size to cover from page boundary to text end
  and rdi, ~0xFFF
  mov rdx, 5 ; R-X
  mov rax, 10 ; mprotect
  syscall
  
  mov rdx, [rsp + 256]  ; restore rtld_fini before jumping to _start
  add rsp, 264          ; restore rsp exactly to ISP — glibc _start aligns stack itself

  mov r11, 0xAAAAAAAAAAAAAAAA ; placeholder for e_entry
  add r11, rbp ; + load_base = actual runtime entry
  jmp r11
