/*
 * This file has been modified for the cdrkit suite.
 *
 * The behaviour and appearence of the program code below can differ to a major
 * extent from the version distributed by the original author(s).
 *
 * For details, see Changelog file distributed with the cdrkit package. If you
 * received this file from another source then ask the distributing person for
 * a log of modifications.
 *
 */

/*
 * Program boot-mipsel.c - Handle Mipsel boot extensions to iso9660.
 *
 *  Written by Steve McIntyre <steve@einval.com> (2004).
 *
 * Heavily inspired by / borrowed from delo:
 *
 * Copyright: (C) 2002 by Florian Lohoff <flo@rfc822.org>
 *            (C) 2004 by Thiemo Seufer <seufer@csv.ica.uni-stuttgart.de>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, Version 2, as published by the
 * Free Software Foundation.
 *
 * Format for volume header information
 *
 * The volume header is a block located at the beginning of all disk
 * media (sector 0).  It contains information pertaining to physical
 * device parameters and logical partition information.
 *
 * The volume header is manipulated by disk formatters/verifiers,
 * partition builders (e.g. fx, dvhtool, and mkfs), and disk drivers.
 *
 * Previous versions of IRIX wrote a copy of the volume header is
 * located at sector 0 of each track of cylinder 0.  These copies were
 * never used, and reduced the capacity of the volume header to hold large
 * files, so this practice was discontinued.
 * The volume header is constrained to be less than or equal to 512
 * bytes long.  A particular copy is assumed valid if no drive errors
 * are detected, the magic number is correct, and the 32 bit 2's complement
 * of the volume header is correct.  The checksum is calculated by initially
 * zeroing vh_csum, summing the entire structure and then storing the
 * 2's complement of the sum.  Thus a checksum to verify the volume header
 * should be 0.
 *
 * The error summary table, bad sector replacement table, and boot blocks are
 * located by searching the volume directory within the volume header.
 *
 * Tables are sized simply by the integral number of table records that
 * will fit in the space indicated by the directory entry.
 *
 * The amount of space allocated to the volume header, replacement blocks,
 * and other tables is user defined when the device is formatted.
 */

#include <mconfig.h>
#include "genisoimage.h"
#include <fctldefs.h>
#include <utypes.h>
#include <intcvt.h>
#include "match.h"
#include "diskmbr.h"
#include "bootinfo.h"
#include <schily.h>
#include "endianconv.h"
#include <errno.h>
#include <glibc_elf.h>

int             add_boot_mipsel_filename(char *filename);
static  int     boot_mipsel_write(FILE *outfile);

static  char   *boot_file_name = NULL;

#define MAX_MAPS        51
#define DEC_BOOT_MAGIC  0x02757a
#define HD_SECTOR_SIZE  512

/* Those were stolen from linux kernel headers. */

struct extent {
    uint32_t count;
    uint32_t start;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
   ;

struct dec_bootblock {
    int8_t pad[8];
    int32_t magic;          /* We are a DEC BootBlock */
    int32_t mode;           /* 0: Single extent, 1: Multi extent boot */
    int32_t loadAddr;       /* Load below kernel */
    int32_t execAddr;       /* And exec there */
    struct extent bootmap[MAX_MAPS];
}
#ifdef __GNUC__
__attribute__((packed))
#endif
   ;

static void swap_in_elf32_ehdr(Elf32_Ehdr *ehdr)
{
    ehdr->e_type = read_le16((unsigned char *)&ehdr->e_type);
    ehdr->e_machine = read_le16((unsigned char *)&ehdr->e_machine);
    ehdr->e_version = read_le32((unsigned char *)&ehdr->e_version);
    ehdr->e_entry = read_le32((unsigned char *)&ehdr->e_entry);
    ehdr->e_phoff = read_le32((unsigned char *)&ehdr->e_phoff);
    ehdr->e_shoff = read_le32((unsigned char *)&ehdr->e_shoff);
    ehdr->e_flags = read_le32((unsigned char *)&ehdr->e_flags);
    ehdr->e_ehsize = read_le16((unsigned char *)&ehdr->e_ehsize);
    ehdr->e_phentsize = read_le16((unsigned char *)&ehdr->e_phentsize);
    ehdr->e_phnum = read_le16((unsigned char *)&ehdr->e_phnum);
    ehdr->e_shentsize = read_le16((unsigned char *)&ehdr->e_shentsize);
    ehdr->e_shnum = read_le16((unsigned char *)&ehdr->e_shnum);
    ehdr->e_shstrndx = read_le16((unsigned char *)&ehdr->e_shstrndx);
}

static void swap_in_elf32_phdr(Elf32_Phdr *phdr)
{
    phdr->p_type = read_le32((unsigned char *)&phdr->p_type);
    phdr->p_offset = read_le32((unsigned char *)&phdr->p_offset);
    phdr->p_vaddr = read_le32((unsigned char *)&phdr->p_vaddr);
    phdr->p_paddr = read_le32((unsigned char *)&phdr->p_paddr);
    phdr->p_filesz = read_le32((unsigned char *)&phdr->p_filesz);
    phdr->p_memsz = read_le32((unsigned char *)&phdr->p_memsz);
    phdr->p_flags = read_le32((unsigned char *)&phdr->p_flags);
    phdr->p_align = read_le32((unsigned char *)&phdr->p_align);
}

/* Simple function: store the filename to be used later when we need
   to find the boot file */
extern int add_boot_mipsel_filename(char *filename)
{
    boot_file_name = filename;
    return 0;
}

/* Parse the ELF header of the boot loaded to work out the load
   address and exec address */
static int parse_boot_file(char *filename, int32_t *loadaddr, int32_t *execaddr, int32_t *offset, int32_t *count)
{
    int error = 0;
    FILE *loader = NULL;
    Elf32_Ehdr ehdr;
    Elf32_Phdr phdr;
    
    loader = fopen(filename, "rb");
    if (!loader)
        return errno;
    
    error = fread(&ehdr, sizeof(ehdr), 1, loader);
    if (1 != error)
        return EIO;

    swap_in_elf32_ehdr(&ehdr);
    if (!(ehdr.e_ident[EI_MAG0] == ELFMAG0
          && ehdr.e_ident[EI_MAG1] == ELFMAG1
          && ehdr.e_ident[EI_MAG2] == ELFMAG2
          && ehdr.e_ident[EI_MAG3] == ELFMAG3
          && ehdr.e_ident[EI_CLASS] == ELFCLASS32
          && ehdr.e_ident[EI_DATA] == ELFDATA2LSB
          && ehdr.e_ident[EI_VERSION] == EV_CURRENT
          && ehdr.e_type == ET_EXEC
          && ehdr.e_machine == EM_MIPS
          && ehdr.e_version == EV_CURRENT))
    {
        fprintf(stderr, "Sorry, %s is not a MIPS ELF32 little endian file", filename);        
        return EINVAL;
    }
    if (ehdr.e_phnum != 1)
    {
        fprintf(stderr, "Sorry, %s has more than one ELF segment", filename);
        return EINVAL;
    }
    fseek(loader, ehdr.e_phoff, SEEK_SET);
    error = fread(&phdr, sizeof(phdr), 1, loader);
    if (1 != error)
        return EIO;

    *loadaddr = phdr.p_vaddr;
    *execaddr = ehdr.e_entry;
	*offset = (phdr.p_offset + HD_SECTOR_SIZE - 1) / HD_SECTOR_SIZE;
	*count = (phdr.p_filesz + HD_SECTOR_SIZE - 1) / HD_SECTOR_SIZE;

    fprintf(stderr, "Parsed mipsel boot image %s: using loadaddr 0x%X, execaddr 0x%X, offset 0x%X, count 0x%X\n",
            filename, *loadaddr, *execaddr, *offset, *count);

    fclose(loader);
    return 0;
}

static int boot_mipsel_write(FILE *outfile)
{
    char sector[2048];
    struct dec_bootblock *bb = (struct dec_bootblock *)sector;
    int error = 0;
    int offset = 0;
    int count = 0;
    struct directory_entry	*boot_file;	/* Boot file we need to search for in the image */
    unsigned long length = 0;
    unsigned long extent = 0;
    int loadaddr = 0;
    int execaddr = 0;

    memset(sector, 0, sizeof(sector));

    /* Fill in our values we care on */
    write_le32(DEC_BOOT_MAGIC, (unsigned char *)&bb->magic);
    write_le32(1, (unsigned char *)&bb->mode);

    /* Find the file entry in the CD image */
    boot_file = search_tree_file(root, boot_file_name);
    if (!boot_file)
    {
#ifdef	USE_LIBSCHILY
		comerrno(EX_BAD, "Uh oh, unable to find the mipsel boot file '%s'!\n",
                 boot_file_name);
#else
		fprintf(stderr, "Uh oh, unable to find the mipsel boot file '%s'!\n",
                boot_file_name);
		exit(1);
#endif
    }

    extent = get_733(boot_file->isorec.extent);
    length = get_733(boot_file->isorec.size);
    fprintf(stderr, "Found mipsel boot loader %s: using extent %lu, #blocks %lu\n",
            boot_file_name, extent, length);

    /* Parse the ELF headers on the boot file */
    error = parse_boot_file(boot_file->whole_name, &loadaddr, &execaddr, &offset, &count);
    if (error)
    {
#ifdef	USE_LIBSCHILY
		comerrno(EX_BAD, "Uh oh, unable to parse the mipsel boot file '%s'!\n",
                 boot_file->whole_name);
#else
		fprintf(stderr, "Uh oh, unable to parse the mipsel boot file '%s'!\n",
                boot_file->whole_name);
		exit(1);
#endif
    }

    write_le32(loadaddr, (unsigned char *)&bb->loadAddr);
    write_le32(execaddr, (unsigned char *)&bb->execAddr);
    write_le32((extent * 4) + offset, (unsigned char *)&bb->bootmap[0].start);
    write_le32(count, (unsigned char *)&bb->bootmap[0].count);
    
    jtwrite(sector, sizeof(sector), 1, 0, FALSE);
    xfwrite(sector, sizeof(sector), 1, outfile, 0, FALSE);
    last_extent_written++;

    return 0;
}

struct output_fragment mipselboot_desc = {NULL, oneblock_size, NULL, boot_mipsel_write, "mipsel boot block"};
