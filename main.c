/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cbopp <cbopp@student.42lausanne.ch>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/25 13:23:26 by cbopp             #+#    #+#             */
/*   Updated: 2026/04/17 13:48:55 by cbopp            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <elf.h>
#include <fcntl.h>
#include <stdio.h>
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
  Elf64_Shdr *note_section = NULL;
  for (int i = 0; i < ehdr->e_shnum; i++) {
    if (strcmp(names + shdr[i].sh_name, ".note") == 0) {
      text_section = &shdr[i];
      break;
    }
  }
}
