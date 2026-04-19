BITS 64
global stub

stub:
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

  ; KSA
  sub rsp, 256
  mov r8, 0

  ; s[i] = i (until 256)
.loop:
  mov byte [rsp + r8], r8b
  inc r8
  cmp r8, 256
  jne .loop

  ; r8 = i, r9b = j, rcx = key_pointer, rsp = S-box
  mov r8, 0
  mov r9, 0
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
  mov r13, 0xBADDCAFEBADDCAFE ; placeholder for inflate() address
  mov r14, 0xDEADBEEFDEADBEEF ; placeholder for text_data address
  mov r15, 0xCAFEBABECAFEBABE ; placeholder for text size

  ; save original .text and size
  mov r12, r14 ; r12 = original text address
  mov rbx, r15 ; rbx = original text size

  ; mprotext(text_addr, text_size, PROT_READ|PROT_WRITE|PROT_EXEC)
  mov rdi, r14
  and rdi, ~0xFFF ; round down to page boundary
  mov rsi, r15
  mov rdx, 7 ; RWX
  mov rax, 10 ; mprotect
  syscall

  mov r8, 0
  mov r9, 0

.loop3:
  inc r8d
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


  ; restore .text to R-X
  mov rdi, r12
  and rdi, ~0xFFF
  mov rsi, rbx ; original text size
  mov rdx, 5 ; R-X
  mov rax, 10
  syscall
  
  add rsp, 256 ; restore stack - undo the sub rsp, 256
  and rsp, ~0xF ; align to 16 bytes

  ; TODO: call r13 once inflate_plt is patched in
  ; call r13

  mov r11, 0xAAAAAAAAAAAAAAAA ; placeholder for e_entry
  jmp r11
