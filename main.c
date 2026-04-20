/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cbopp <cbopp@student.42lausanne.ch>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/25 13:23:26 by cbopp             #+#    #+#             */
/*   Updated: 2026/04/17 15:43:19 by cbopp            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <elf.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

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

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Not enough arguments.\n");
    printf("Expected usage: ./woody_woodpacker <ELF binary>");
    return (1);
  }
  // TODO: CHECK IF FAIL
  int fd = open(argv[1], O_RDONLY);
  struct stat st;
  // TODO: CHECK IF FAIL
  fstat(fd, &st);

  // TODO: CHECK IF FAIL
  void *map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  Elf64_Ehdr *ehdr = (Elf64_Ehdr *)map;

  if (check_elf(ehdr)) {
    munmap(map, st.st_size);
    close(fd);
    return (1);
  }

  // Program headers with ehdr->e_phnum entries
  Elf64_Phdr *phdr = (Elf64_Phdr *)(map + ehdr->e_phoff);

  // Section headers with ehdr->e_shnum entries
  Elf64_Shdr *shdr = (Elf64_Shdr *)(map + ehdr->e_shoff);

  // get string tbale section
  Elf64_Shdr *strtab = &shdr[ehdr->e_shstrndx];
  char *names = (char *)(map + strtab->sh_offset);

  // finding .text
  Elf64_Shdr *text_section = NULL;
  for (int i = 0; i < ehdr->e_shnum; i++) {
    if (strcmp(names + shdr[i].sh_name, ".text") == 0) {
      text_section = &shdr[i];
      break;
    }
  }

  void *text_data = map + text_section->sh_offset;
  // text_section->sh_size bytes starting here

  // finding .note
  Elf64_Phdr *note_segment = NULL;
  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (phdr[i].p_type == PT_NOTE) {
      note_segment = &phdr[i];
      break;
    }
  }

  // RW version of binary
  size_t output_size = st.st_size;
  void *woody = malloc(output_size);
  if (!woody)
    return (printf("Woody malloc failed."), 1);
  memcpy(woody, map, output_size);

  // cleanup memory
  munmap(map, st.st_size);
  close(fd);
}
