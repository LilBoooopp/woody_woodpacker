/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cbopp <cbopp@student.42lausanne.ch>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/25 13:23:26 by cbopp             #+#    #+#             */
/*   Updated: 2026/04/20 00:00:40 by cbopp            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "stub.h"
#include <elf.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#define KEY_SIZE 16

// typedef struct {
//   Elf64_Word sh_name;       // Index into string table
//   Elf64_Word sh_type;       // Section type (SH_PROGBITS for .text)
//   Elf64_Xword sh_flags;     // Flags (SHF_ALLOC | SHF_EXECINSTR for .text)
//   Elf64_Addr sh_addr;       // virtual address in memory at runtime
//   Elf64_Off sh_offset;      // Offset in the file on disk
//   Elf64_Xword sh_size;      // Size in bytes
//   Elf64_Word sh_link;       // Link to another section (context-dependent)
//   Elf64_Word sh_info;       // Extra info (context-dependent)
//   Elf64_Xword sh_addralign; // Alighment requirement
//   Elf64_Xword sh_entsize;   // Entry size if section holds a table
// } Elf64_Shdr;

// typedef struct {
//   unsigned char e_ident[16]; // Magic bytes + class (32/64), endianness, OS
//   ABI Elf64_Half e_type;         // ET_EXEC (executable), ET_DYN (shared lib
//   / PIE) Elf64_Half e_machine;      // Target ISA: EM_X86_64, EM_386, etc
//   Elf64_Word e_version;      // ELF version (1)
//   Elf64_Addr e_entry;        // Virtual address of entry point (_start)
//   Elf64_Off e_phoff;         // Byte offset to program header table in file
//   Elf64_Off e_shoff;         // Byte offset to section header table in file
//   Elf64_Word e_flags;        // Architecture-specific flags (0 on x86_64)
//   Elf64_Half e_ehsize;       // Size of this ELF header
//   Elf64_Half e_phentsize;    // Size of one program header entry
//   Elf64_Half e_phnum;        // Number of program header
//   Elf64_Half e_shentsize;    // Size of one section header entry
//   Elf64_Half e_shnum;        // Number of section header entries
//   Elf64_Half e_shstrndx;     // Index of section name string table in shdr
//   array
// } Elf64_Ehdr;

// typedef struct {
//   Elf64_Word p_type;  // Secment type: PT_LOAD, PT_NOTE, PT_PHDR, etc.
//   Elf64_Word p_flags; // Permissions: PF_R (read), PF_W (write), PF_X
//   (exectue) Elf64_Off p_offset; // Byte offset of segment data within the ELF
//   file Elf64_Addr p_vaddr; // Virtual address where segment is loaded at
//   runtime Elf64_Addr p_paddr; // Physical address (irrelevant on modern
//   systems) Elf64_Xword p_filesz; // Size of segment data in the file
//   Elf64_Xword p_memsz;  // Size of segment in memory (memsz > filesz for
//   .bss) Elf64_Xword p_align;  // Alighment requirement for loading
// } Elf64_Phdr;

/**
 * @brief validates the ELF magic bytes at the start of file (0x7f)
 */
int check_mem(Elf64_Ehdr *elf) {
  if (memcmp(elf->e_ident, ELFMAG, SELFMAG) != 0) {
    printf("Error: Not an ELF file\n");
    return (1);
  }
  return (0);
}

/**
 * @brief Check for 64-bit
 * e_ident[EI_CLASS] is ELFCLASS32 (1) or ELFCLASS64 (2).
 */
int check_64bit(Elf64_Ehdr *elf) {
  if (elf->e_ident[EI_CLASS] != ELFCLASS64) {
      printf("Error: Not a 64-bit ELF\n");
    return (1);
  }
  return (0);
}

/**
 * @brief validates architecture.
 * EM_X86_64 = 62
 */
int check_8664(Elf64_Ehdr *elf) {
  if (elf->e_machine != EM_X86_64) {
    printf("File architecture not supported. x86_64 only\n");
    return (1);
  }
  return (0);
}

/**
 * @brief Checks that file is executable or shared library.
 */
int check_exec(Elf64_Ehdr *elf) {
  if (elf->e_type != ET_EXEC && elf->e_type != ET_DYN) {
    printf("Error: Nt an executable\n");
    return (1);
  }
  return (0);
}

int check_elf(Elf64_Ehdr *elf) {
  int ret;

  ret = 0;
  ret += check_mem(elf);
  ret += check_64bit(elf);
  ret += check_8664(elf);
  ret += check_exec(elf);
  return (ret);
}

/**
 * @brief Key Scheduling Algorithm
 * @params *s 256 byte array
 * @params *key scrambles order of *s
 * @params key_len
 */
void rc4_ksa(uint8_t *s, uint8_t *key, size_t key_len) {
  for (int i = 0; i < 256; i++) {
    s[i] = i;
  }

  int j = 0;
  for (int i = 0; i < 256; i++) {
    j = (j + s[i] + key[i % key_len]) % 256;

    uint8_t tmp = s[i];
    s[i] = s[j];
    s[j] = tmp;
  }
}

/**
 * @brief Pseudo-random key gen
 * Generates keystream bytes by strring *s and then XOR keystream byte with data
 * byte
 */
void rc4_prga(uint8_t *s, uint8_t *data, size_t data_len) {
  int i = 0;
  int j = 0;
  for (int k = 0; (size_t)k < data_len; k++) {
    i = (i + 1) % 256;
    j = (j + s[i]) % 256;
    uint8_t tmp = s[i];
    s[i] = s[j];
    s[j] = tmp;
    data[k] ^= s[(s[i] + s[j]) % 256];
  }
}

void generate_key(uint8_t *key, size_t key_len) {
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd < 0) {
    printf("Error: could not open /dev/urandom\n");
    return;
  }
  read(fd, key, key_len);
  close(fd);
  for (size_t i = 0; i < key_len; i++)
    printf("%02X", key[i]);
  printf("\n");
}

int hex_char_to_int(char c) {
  if (c >= '0' && c <= '9') return (c - '0');
  if (c >= 'a' && c <= 'f') return (c - 'a' + 10);
  if (c >= 'A' && c <= 'F') return (c - 'A' + 10);
  return (-1);
}

int parse_custom_key(const char *hex_str, uint8_t *key_out) {
  if (strlen(hex_str) != 32) {
    printf("Error: Custom key must be exactly 32 hex characters (16 bytes).\n");
    return (1);
  }

  for (int i = 0; i < 16; i++) {
    int high = hex_char_to_int(hex_str[i * 2]);
    int low = hex_char_to_int(hex_str[i * 2 + 1]);
    
    if (high == -1 || low == -1) {
      printf("Error: Invalid hex character in custom key.\n");
      return (1);
    }
    key_out[i] = (high << 4) | low;
  }
  return (0);
}

void rc4_encrypt(uint8_t *data, size_t data_len, uint8_t *key, size_t key_len) {
  uint8_t s[256];

  rc4_ksa(s, key, key_len);
  rc4_prga(s, data, data_len);
}

void inject_stub(void *woody, Elf64_Phdr *note_segment, Elf64_Ehdr *ehdr,
                 Elf64_Shdr *text_section, uint8_t *key, uint64_t inflate_plt,
                 size_t file_size, uint64_t phdr_link_vaddr) {
  note_segment->p_offset = file_size;
  memcpy(woody + file_size, stub_bin, stub_bin_len);

  uint64_t placeholder = 0xDEADBEEFDEADBEEF;
  uint64_t real_value = text_section->sh_addr;
  void *patch_site = memmem(woody + file_size, stub_bin_len, &placeholder, 8);
  if (patch_site)
    memcpy(patch_site, &real_value, 8);

  placeholder = 0xBADDCAFEBADDCAFE;
  real_value = inflate_plt;
  patch_site = memmem(woody + file_size, stub_bin_len, &placeholder, 8);
  if (patch_site)
    memcpy(patch_site, &real_value, 8);

  placeholder = 0xCAFEBABECAFEBABE;
  real_value = text_section->sh_size;
  patch_site = memmem(woody + file_size, stub_bin_len, &placeholder, 8);
  if (patch_site)
    memcpy(patch_site, &real_value, 8);

  placeholder = 0xAAAAAAAAAAAAAAAA;
  real_value = ehdr->e_entry;
  patch_site = memmem(woody + file_size, stub_bin_len, &placeholder, 8);
  if (patch_site)
    memcpy(patch_site, &real_value, 8);

  placeholder = 0xFEEDFACEFEEDFACE;
  real_value = phdr_link_vaddr;
  patch_site = memmem(woody + file_size, stub_bin_len, &placeholder, 8);
  if (patch_site)
    memcpy(patch_site, &real_value, 8);

  uint8_t key_placeholder[16] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD,
                                 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
                                 0xDE, 0xAD, 0xBE, 0xEF};
  patch_site = memmem(woody + file_size, stub_bin_len, key_placeholder, 16);
  if (patch_site)
    memcpy(patch_site, key, 16);

  note_segment->p_type = PT_LOAD;
  note_segment->p_flags = PF_R | PF_X;
  note_segment->p_vaddr = 0xc000000;
  note_segment->p_paddr = 0xc000000;
  note_segment->p_filesz = stub_bin_len;
  note_segment->p_memsz = stub_bin_len;
  note_segment->p_align = 0x1000;
  ehdr->e_entry = 0xc000000;
}

int main(int argc, char **argv) {
  if (argc < 2 && argc > 3) {
    printf("Expected usage: ./woody_woodpacker <ELF binary> [custom hex key]\n");
    return (1);
  }
  int fd = open(argv[1], O_RDONLY);
  if (fd < 0)
    return (printf("Error: could not open file\n"), 1);
  struct stat st;
  if (fstat(fd, &st) < 0)
    return (printf("Error: fstat failed\n"), close(fd), 1);

  if (st.st_size < (off_t)sizeof(Elf64_Ehdr))
    return (printf("Error: file too small\n"), close(fd), 1);

  void *map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (map == MAP_FAILED)
    return (printf("Error: mmap failed\n"), close(fd), 1);

  Elf64_Ehdr *ehdr = (Elf64_Ehdr *)map;
  if (check_elf(ehdr)) {
    munmap(map, st.st_size);
    close(fd);
    return (1);
  }

  size_t page_size = 0x1000;
  size_t aligned_offset = (st.st_size + page_size - 1) & ~(page_size - 1);
  // Writable version of binary
  size_t output_size = aligned_offset + stub_bin_len;
  void *woody = malloc(output_size);
  if (!woody)
    return (printf("Woody malloc failed."), munmap(map, st.st_size), close(fd),
            1);
  memset(woody, 0, output_size);
  memcpy(woody, map, st.st_size);
  msync(woody, st.st_size + stub_bin_len, MS_SYNC);
  // cleanup memory
  munmap(map, st.st_size);
  close(fd);

  ehdr = (Elf64_Ehdr *)woody;
  Elf64_Phdr *phdr = (Elf64_Phdr *)(woody + ehdr->e_phoff);
  Elf64_Shdr *shdr = (Elf64_Shdr *)(woody + ehdr->e_shoff);
  Elf64_Shdr *strtab = &shdr[ehdr->e_shstrndx];
  char *names = (char *)(woody + strtab->sh_offset);

  // finding .text
  Elf64_Shdr *text_section = NULL;
  for (int i = 0; i < ehdr->e_shnum; i++) {
    if (strcmp(names + shdr[i].sh_name, ".text") == 0) {
      text_section = &shdr[i];
      break;
    }
  }
  if (!text_section)
    return (printf("Error: .text section not found\n"), free(woody), 1);

  // text_section->sh_size bytes starting here
  void *text_data = (uint8_t *)(woody + text_section->sh_offset);

  // finding .note and PT_PHDR (for load_base computation in stub)
  Elf64_Phdr *note_segment = NULL;
  uint64_t phdr_link_vaddr = ehdr->e_phoff;  // fallback: works for PIE
  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (phdr[i].p_type == PT_NOTE && !note_segment)
      note_segment = &phdr[i];
    if (phdr[i].p_type == PT_PHDR)
      phdr_link_vaddr = phdr[i].p_vaddr;
  }
  if (!note_segment)
    return (printf("Error: PT_NOTE segment not found\n"), free(woody), 1);

  /*
   * TODO: call zlib_compress before encryption
   */

  uint8_t key[16];

  if (argc == 3) {
    if (parse_custom_key(argv[2], key) != 0) {
      free(woody);
      return (1);
    }
    printf("Using custom key: ");
  } else {
    generate_key(key, 16);
    printf("Generated random key: ");
  }

  for (int i = 0; i < 16; i++) {
    printf("%02X", key[i]);
  }
  printf("\n");
  
  rc4_encrypt(text_data, text_section->sh_size, key, 16);

  inject_stub(woody, note_segment, ehdr, text_section, key, 0, aligned_offset,
              phdr_link_vaddr);

  // write to disk
  int fd_output = open("woody", O_WRONLY | O_CREAT | O_TRUNC, 0755);
  if (fd_output < 0)
    return (printf("Error: Woody open() failed"), free(woody), 1);
  if (write(fd_output, woody, output_size) < 0)
    return (printf("Error: write() failed"), free(woody), close(fd_output), 1);
  close(fd_output);

  free(woody);
  return (0);
}
