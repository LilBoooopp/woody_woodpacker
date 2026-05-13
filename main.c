/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cbopp <cbopp@student.42lausanne.ch>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/25 13:23:26 by cbopp             #+#    #+#             */
/*   Updated: 2026/05/11 19:25:55 by ilyanar          ###   LAUSANNE.ch       */
/*                                                                            */
/* ************************************************************************** */

#include "stub.h"
#include <elf.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>
#include <curses.h>
#include "libft.h"

#define KEY_SIZE 16

//--------------------------------------------
/// LZSS compression
//-----------------------------------------
#define WINDOW_SIZE 4096
#define MATCH_LEN 18
#define THRESHOLD 2
#define INDEX WINDOW_SIZE

typedef struct LZSS_s{
	unsigned long int textsize;
	unsigned long int codesize;
	unsigned long int printcount;
	unsigned char text_buf[WINDOW_SIZE + MATCH_LEN - 1];
	int	match_position;
	int match_length;
	int lson[WINDOW_SIZE + 1];
	int rson[WINDOW_SIZE + 257];
	int dad[WINDOW_SIZE + 1];

	uint8_t *last_buffer;
	uint8_t *text_data;
} LZSS_t;

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

void InsertNode(int r, LZSS_t *lz)
{
	int i = 0;
	int cmp = 1;
	unsigned char  *key = &lz->text_buf[r];
	int p = WINDOW_SIZE + 1 + key[0];

	lz->rson[r] = lz->lson[r] = INDEX;
	lz->match_length = 0;

	while (true){
		if (cmp >= 0) {
			if (lz->rson[p] != INDEX)
				p = lz->rson[p];
			else {
				lz->rson[p] = r;
				lz->dad[r] = p;
				return;
			}
		} else {
			if (lz->lson[p] != INDEX)
				p = lz->lson[p];
			else {
				lz->lson[p] = r;
				lz->dad[r] = p;
				return;
			}
		}
		for (i = 1; i < MATCH_LEN; i++)
			if ((cmp = key[i] - lz->text_buf[p + i]) != 0)
				break;
		if (i > lz->match_length){
			lz->match_position = p;
			if ((lz->match_length = i) >= MATCH_LEN)
				break;
		}
	}
	lz->dad[r] = lz->dad[p];
	lz->lson[r] = lz->lson[p];
	lz->rson[r] = lz->rson[p];
	lz->dad[lz->lson[p]] = r;
	lz->dad[lz->rson[p]] = r;

	if (lz->rson[lz->dad[p]] == p)
		lz->rson[lz->dad[p]] = r;
	else
		lz->lson[lz->dad[p]] = r;
	lz->dad[p] = INDEX;
}

void DeleteNode(int p, LZSS_t *lz)
{
	int  q;
	
	if (lz->dad[p] == INDEX)
		return;
	if (lz->rson[p] == INDEX)
		q = lz->lson[p];
	else if (lz->lson[p] == INDEX)
		q = lz->rson[p];
	else {
		q = lz->lson[p];
		if (lz->rson[q] != INDEX) {
			do {
				q = lz->rson[q];
			}
			while (lz->rson[q] != INDEX);
			
			lz->rson[lz->dad[q]] = lz->lson[q];
			lz->dad[lz->lson[q]] = lz->dad[q];
			lz->lson[q] = lz->lson[p];
			lz->dad[lz->lson[p]] = q;
		}
		lz->rson[q] = lz->rson[p];
		lz->dad[lz->rson[p]] = q;
	}
	lz->dad[q] = lz->dad[p];
	if (lz->rson[lz->dad[p]] == p)
		lz->rson[lz->dad[p]] = q;
	else
		lz->lson[lz->dad[p]] = q;
	lz->dad[p] = INDEX;
}

static size_t LZSS_compression(uint8_t *text_data, size_t text_len){

	LZSS_t lz;
	bzero(&lz, sizeof(LZSS_t));
	lz.last_buffer = malloc(text_len);
	bzero(lz.last_buffer, text_len);
	lz.text_data = text_data;

	lz.textsize = 0;
	lz.codesize = 0;
	lz.printcount = 0;
	lz.match_position = 0;
	lz.match_length = 0;

	int  i = 0;
	int len = 0;
	int r = 0;
	int s = 0;
	int last_match_length = 0;
	int code_buf_ptr = 0;
	unsigned char code_buf[17] = {0};
	unsigned char mask = {0};

	size_t in_index = 0;
	size_t out_index = 0;
	for (size_t j = WINDOW_SIZE + 1; j <= WINDOW_SIZE + 256; j++)
		lz.rson[j] = INDEX;
	for (size_t j = 0; j < WINDOW_SIZE; j++)
		lz.dad[j] = INDEX;

	code_buf[0] = 0;
	code_buf_ptr = mask = 1;
	s = 0;
	r = WINDOW_SIZE - MATCH_LEN;
	for (i = s; i < r; i++)
		lz.text_buf[i] = ' ';
	for (len = 0; len < MATCH_LEN && in_index < text_len ; len++)
		lz.text_buf[r + len] = text_data[in_index++];
	if ((lz.textsize = len) == 0)
		return 0;
	for (i = 1; i <= MATCH_LEN; i++)
		InsertNode(r - i, &lz);
	InsertNode(r, &lz);
	do {
		if (lz.match_length > len)
			lz.match_length = len;
		if (lz.match_length <= THRESHOLD) {
			lz.match_length = 1;
			code_buf[0] |= mask;
			code_buf[code_buf_ptr++] = lz.text_buf[r];
		} else {
			code_buf[code_buf_ptr++] = (unsigned char)lz.match_position;
			code_buf[code_buf_ptr++] = (unsigned char)
				(((lz.match_position >> 4) & 0xf0)
			  | (lz.match_length - (THRESHOLD + 1)));
		}
		if ((mask <<= 1) == 0){
			for (i = 0; i < code_buf_ptr && out_index < text_len; i++)
				// putc(code_buf[i], outfile);
				lz.last_buffer[out_index++] = code_buf[i];
			lz.codesize += code_buf_ptr;
			code_buf_ptr = mask = 1;
		}
		last_match_length = lz.match_length;
		for (i = 0; i < last_match_length &&
				in_index < text_len; i++) {

			DeleteNode(s, &lz);
			lz.text_buf[s] = lz.last_buffer[out_index];
			if (s < MATCH_LEN - 1)
				lz.text_buf[s + WINDOW_SIZE] = lz.last_buffer[out_index++];
			s = (s + 1) & (WINDOW_SIZE - 1);
			r = (r + 1) & (WINDOW_SIZE - 1);
			InsertNode(r, &lz);
		}
		if ((lz.textsize += i) > lz.printcount) {
			lz.printcount += 1024;
		}
		while (i++ < last_match_length) {
			DeleteNode(s, &lz);
			s = (s + 1) & (WINDOW_SIZE - 1);
			r = (r + 1) & (WINDOW_SIZE - 1);
			if (--len)
				InsertNode(r, &lz);
		}
	} while (len > 0);
	printf("test2\n");
	if (code_buf_ptr > 1){
		for (i = 0; i < code_buf_ptr; i++)
			// putc(code_buf[i], outfile);
			lz.last_buffer[out_index++] = code_buf[i];
		lz.codesize += code_buf_ptr;
	}

	printf("In : %ld bytes\n", lz.textsize);
	printf("Out: %ld bytes\n", lz.codesize);
	printf("Out/In: %.3f\n", (double)lz.codesize / lz.textsize);

	
	text_data = lz.last_buffer;
	return lz.codesize;
}

/**
 * @brief Pseudo-random key gen
 * Generates keystream bytes by strring *s and then XOR keystream byte with data
 * byte
 */
void rc4_prga(uint8_t *s, uint8_t *data, size_t data_len) {
  for (size_t k = 0, i = 0, j = 0; k < data_len; k++) {
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
  if (argc < 2 || argc > 3) {
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
  uint8_t *text_data = (uint8_t *)(woody + text_section->sh_offset);

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

  size_t new_len = LZSS_compression(text_data, text_section->sh_size);

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
  
  rc4_encrypt(text_data, new_len, key, 16);

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
