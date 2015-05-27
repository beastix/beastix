/*
 * jte.c
 *
 * Copyright (c) 2004-2006 Steve McIntyre <steve@einval.com>
 *
 * Implementation of the Jigdo Template Engine - make jigdo files
 * directly when making ISO images
 *
 * GNU GPL v2
 */

#undef BZ2_SUPPORT

#include <mconfig.h>
#include "genisoimage.h"
#include <timedefs.h>
#include <fctldefs.h>
#include <zlib.h>
#ifdef BZ2_SUPPORT
#   include <bzlib.h>
#endif
#include <regex.h>
#ifdef SORTING
#include "match.h"
#endif /* SORTING */
#include <errno.h>
#include <schily.h>
#ifdef DVD_VIDEO
#include "dvd_reader.h"
#include "dvd_file.h"
#include "ifo_read.h"
#include "md5.h"
#include "endianconv.h"
#endif
#ifdef APPLE_HYB
#include <ctype.h>
#endif

#ifdef	VMS
#include "vms.h"
#endif

/* Different types used in building our state list below */
#define JTET_FILE_MATCH 1
#define JTET_NOMATCH    2

#define JTE_VER_MAJOR     0x0001
#define JTE_VER_MINOR     0x000F
#define JTE_NAME          "JTE"
#define JTE_COMMENT       "JTE at http://www.einval.com/~steve/software/JTE/ ; jigdo at http://atterer.net/jigdo/"

#define JIGDO_TEMPLATE_VERSION "1.1"

#ifdef BZ2_SUPPORT
int use_bz2 = 0;
#endif

/*	
	Simple list to hold the results of -jigdo-exclude and
	-jigdo-force-match command line options. Seems easiest to do this
	using regexps.
*/
struct path_match
{
    regex_t  match_pattern;
    char    *match_rule;
    struct path_match *next;
};

/* List of mappings e.g. Debian=/mirror/debian */
struct path_mapping
{
    char                *from;
    char                *to;
    struct path_mapping *next;
};

FILE	*jtjigdo = NULL;       /* File handle used throughout for the jigdo file */
FILE	*jttemplate = NULL;    /* File handle used throughout for the template file */
char    *jjigdo_out = NULL;    /* Output name for jigdo .jigdo file; NULL means don't do it */
char    *jtemplate_out = NULL; /* Output name for jigdo template file; NULL means don't do it */
char    *jmd5_list = NULL;     /* Name of file to use for MD5 checking */
int      jte_min_size = MIN_JIGDO_FILE_SIZE;
struct  path_match *exclude_list = NULL;
struct  path_match *include_list = NULL;
struct  path_mapping  *map_list = NULL;
unsigned long long template_size = 0;
unsigned long long image_size = 0;

static struct mk_MD5Context iso_context;
static struct mk_MD5Context template_context;

/* List of files that we've seen, ready to write into the template and
   jigdo files */
typedef struct _file_entry
{
    unsigned char       md5[16];
    off_t               file_length;
    unsigned long long  rsyncsum;
    char               *filename;
} file_entry_t;

typedef struct _unmatched_entry
{
    off_t uncompressed_length;
} unmatched_entry_t;    

typedef struct _entry
{
    int entry_type; /* JTET_TYPE as above */
    struct _entry *next;
    union
    {
        file_entry_t      file;
        unmatched_entry_t chunk;
    } data;
} entry_t;

typedef struct _jigdo_file_entry
{
    unsigned char type;
    unsigned char fileLen[6];
    unsigned char fileRsync[8];
    unsigned char fileMD5[16];
} jigdo_file_entry_t;

typedef struct _jigdo_chunk_entry
{
    unsigned char type;
    unsigned char skipLen[6];
} jigdo_chunk_entry_t;

typedef struct _jigdo_image_entry
{
    unsigned char type;
    unsigned char imageLen[6];
    unsigned char imageMD5[16];
    unsigned char blockLen[4];
} jigdo_image_entry_t;

typedef struct _md5_list_entry
{
    struct _md5_list_entry *next;
    unsigned char       MD5[16];
    unsigned long long  size;
    char               *filename;
} md5_list_entry_t;
    
entry_t *entry_list = NULL;
entry_t *entry_last = NULL;
FILE    *t_file = NULL;
FILE    *j_file = NULL;
int      num_matches = 0;
int      num_chunks = 0;
md5_list_entry_t *md5_list = NULL;
md5_list_entry_t *md5_last = NULL;

/* Grab the file component from a full path */
static char *file_base_name(char *path)
{
    char *endptr = path;
    char *ptr = path;
    
    while (*ptr != '\0')
    {
        if ('/' == *ptr)
            endptr = ++ptr;
        else
            ++ptr;
    }
    return endptr;
}

/* Dump a buffer in hex */
static char *hex_dump(unsigned char *buf, size_t buf_size)
{
    unsigned int i;
    static char output_buffer[2048];
    char *p = output_buffer;

    memset(output_buffer, 0, sizeof(output_buffer));
    if (buf_size >= (sizeof(output_buffer) / 2))
    {
        fprintf(stderr, "hex_dump: Buffer too small!\n");
        exit(1);
    }

    for (i = 0; i < buf_size ; i++)
        p += sprintf(p, "%2.2x", buf[i]);

    return output_buffer;
}

/* Build the list of exclusion regexps */
extern int jte_add_exclude(char *pattern)
{
    struct path_match *new = NULL;
    
    new = malloc(sizeof *new);
    if (!new)
        return ENOMEM;    
    
    regcomp(&new->match_pattern, pattern, REG_NEWLINE);
	new->match_rule = pattern;

    /* Order on the exclude list doesn't matter! */
    if (NULL != exclude_list)
        new->next = exclude_list;

    exclude_list = new;
    return 0;
}

/* Check if the file should be excluded because of a filename match. 1
   means exclude, 0 means not */
static int check_exclude_by_name(char *filename, char **matched)
{
    struct path_match *ptr = exclude_list;
    regmatch_t pmatch[1];
    int i = 0;

    while (ptr)
    {
        if (!regexec(&ptr->match_pattern, filename, 1, pmatch, 0))
        {
            *matched = ptr->match_rule;
            return 1;
        }
        ptr = ptr->next;
    }
    
    /* Not matched, so return 0 */
    return 0;
}

/* Build the list of required inclusion regexps */
extern int jte_add_include(char *pattern)
{
    struct path_match *new = NULL;
    
    new = malloc(sizeof *new);
    if (!new)
        return ENOMEM;    
    
    regcomp(&new->match_pattern, pattern, REG_NEWLINE);
	new->match_rule = pattern;

    /* Order on the include list doesn't matter! */
    if (NULL != include_list)
        new->next = include_list;

    include_list = new;
    return 0;
}

/* Check if a file has to be MD5-matched to be valid. If we get called
   here, we've failed to match any of the MD5 entries we were
   given. If the path to the filename matches one of the paths in our
   list, clearly it must have been corrupted. Abort with an error. */
static void check_md5_file_match(char *filename)
{
    struct path_match *ptr = include_list;
    regmatch_t pmatch[1];
    int i = 0;

    while (ptr)
    {
        if (!regexec(&ptr->match_pattern, filename, 1, pmatch, 0))
        {
#ifdef	USE_LIBSCHILY
			comerr("File %s should have matched an MD5 entry, but didn't! (Rule '%s')\n", filename, ptr->match_rule);
#else
			fprintf(stderr, "File %s should have matched an MD5 entry, but didn't! (Rule '%s')\n", filename, ptr->match_rule);
			exit(1);
#endif
		}
        ptr = ptr->next;
    }
}    

/* Should we list a file separately in the jigdo output, or should we
   just dump it into the template file as binary data? Three things
   cases to look for here:

   1. Small files are better simply folded in, as they take less space that way.

   2. Files in /doc (for example) may change in the archive all the
      time and it's better to not have to fetch snapshot copies if we
      can avoid it.      

   3. Files living in specified paths *must* match an entry in the
      md5-list, or they must have been corrupted. If we find a corrupt
      file, bail out with an error.

*/
extern int list_file_in_jigdo(char *filename, off_t size, char **realname, unsigned char md5[16])
{
    char *matched_rule;
    md5_list_entry_t *entry = md5_list;
    int md5sum_done = 0;
    
    if (!jtemplate_out)
        return 0;

    memset(md5, 0, sizeof(md5));

    /* Cheaper to check file size first */
    if (size < jte_min_size)
    {
        if (verbose > 0)
            fprintf(stderr, "Jigdo-ignoring file %s; it's too small\n", filename);
        return 0;
    }
    
    /* Now check the excluded list by name */
    if (check_exclude_by_name(filename, &matched_rule))
    {
        if (verbose > 0)
            fprintf(stderr, "Jigdo-ignoring file %s; it's covered in the exclude list by \"%s\"\n", filename, matched_rule);
        return 0;
    }

    /* Check to see if the file is in our md5 list. Check three things:
       
       1. the size
       2. the filename
       3. (only if the first 2 match) the md5sum

       If we get a match for all three, include the file and return
       the full path to the file that we have gleaned from the mirror.
    */

    while (entry)
    {
        if (size == entry->size)
        {
            if (!strcmp(file_base_name(filename), file_base_name(entry->filename)))
            {
                if (!md5sum_done)
                {
                    calculate_md5sum(filename, size, md5);
                    md5sum_done = 1;
                }
                if (!memcmp(md5, entry->MD5, sizeof(entry->MD5)))
                {
                    *realname = entry->filename;
                    return 1;
                }
            }
        }
        entry = entry->next;
    }

    /* We haven't found an entry in our MD5 list to match this
     * file. If we should have done, complain and bail out. */
    check_md5_file_match(filename);
    return 0;
}

/* Add a mapping of pathnames (e.g. Debian=/mirror/debian). We should
   be passed TO=FROM here */
extern int jte_add_mapping(char *arg)
{
    int error = 0;
    struct path_mapping *new = NULL;
    struct path_mapping *entry = NULL;
    char *p = arg;
    char *from = NULL;
    char *to = NULL;

    /* Find the "=" in the string passed. Set it to NULL and we can
       use the string in-place */
    while (*p)
    {
        if ('=' == *p)
        {
            *p = 0;
            p++;
            to = arg;
            from = p;
        }
        p++;
    }
    if (!from || !strlen(from) || !to || !strlen(to))
        return EINVAL;
    
    new = malloc(sizeof(*new));
    if (!new)
        return ENOMEM;
    
    new->from = from;
    new->to = to;
    new->next = NULL;

    if (verbose > 0)
        fprintf(stderr, "Adding mapping from %s to %s for the jigdo file\n", from, to);
    if (!map_list)
        map_list = new;
    else
    {
        /* Order is important; add to the end of the list */
        entry = map_list;
        while (NULL != entry->next)
            entry = entry->next;
        entry->next = new;
    }
    return 0;
}

/* Check if the filename should be remapped; if so map it, otherwise
   return the original name. */
static char *remap_filename(char *filename)
{
    char *new_name = filename;
    struct path_mapping *entry = map_list;
    
    while (entry)
    {
        if (!strncmp(filename, entry->from, strlen(entry->from)))
        {
            new_name = calloc(1, 2 + strlen(filename) + strlen(entry->to) - strlen(entry->from));
            if (!new_name)
            {
                fprintf(stderr, "Failed to malloc new filename; abort!\n");
                exit(1);
            }
            sprintf(new_name, "%s:%s", entry->to, &filename[strlen(entry->from)]);
            return new_name;
        }
        entry = entry->next;
    }

    /* No mapping in effect */
    return strdup(filename);
}    

/* Write data to the template file and update the MD5 sum */
static size_t template_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    mk_MD5Update(&template_context, ptr, size * nmemb);
    template_size += (unsigned long long)size * nmemb;
    return fwrite(ptr, size, nmemb, stream);
}

/* Create a new template file and initialise it */
static void write_template_header()
{
    char buf[2048];
    int i = 0;
    char *p = buf;

    memset(buf, 0, sizeof(buf));

    mk_MD5Init(&template_context);
    i += sprintf(p, "JigsawDownload template %s %s/%d.%d \r\n",
                 JIGDO_TEMPLATE_VERSION, JTE_NAME, JTE_VER_MAJOR, JTE_VER_MINOR);
    p = &buf[i];

    i += sprintf(p, "%s \r\n", JTE_COMMENT);
    p = &buf[i];

    i += sprintf(p, "\r\n");
    template_fwrite(buf, i, 1, t_file);
}

/* Read the MD5 list and build a list in memory for us to use later */
static void add_md5_entry(unsigned char *md5, unsigned long long size, char *filename)
{
    int error = 0;
    md5_list_entry_t *new = NULL;
    
    new = calloc(1, sizeof(md5_list_entry_t));
    memcpy(new->MD5, md5, sizeof(new->MD5));
    new->size = size;
    new->filename = strdup(filename);
    
    /* Add to the end of the list */
    if (NULL == md5_last)
    {
        md5_last = new;
        md5_list = new;
    }
    else
    {
        md5_last->next = new;
        md5_last = new;
    }
}

/* Parse a 12-digit decimal number */
static unsigned long long parse_number(unsigned char in[12])
{
    unsigned long long size = 0;
    int i = 0;
    
    for (i = 0; i < 12; i++)
    {
        size *= 10;
        if (isdigit(in[i]))
            size += (in[i] - '0');
    }

    return size;
}
    
/* Read the MD5 list and build a list in memory for us to use later
   MD5 list format:

   <---MD5--->  <--Size-->  <--Filename-->
       32          12          remaining
*/
static void parse_md5_list(void)
{
    FILE *md5_file = NULL;
    unsigned char buf[1024];
    unsigned char md5[16];
    char *filename = NULL;
    unsigned char *numbuf = NULL;
    int num_files = 0;
    unsigned long long size = 0;

    md5_file = fopen(jmd5_list, "rb");
    if (!md5_file)
    {
#ifdef	USE_LIBSCHILY
        comerr("cannot read from MD5 list file '%s'\n", jmd5_list);
#else
        fprintf(stderr, "cannot read from MD5 list file '%s'\n", jmd5_list);
        exit(1);
#endif
    }

    memset(buf, 0, sizeof(buf));
    while (fgets(buf, sizeof(buf), md5_file))
    {
        numbuf = &buf[34];
        filename = &buf[48];
        /* Lose the trailing \n from the fgets() call */
        if (buf[strlen(buf)-1] == '\n')
            buf[strlen(buf)-1] = 0;

        if (mk_MD5Parse(buf, md5))
        {
#ifdef	USE_LIBSCHILY
            comerr("cannot parse MD5 file '%s'\n", jmd5_list);
#else
            fprintf(stderr, "cannot parse MD5 file '%s'\n", jmd5_list);
            exit(1);
#endif
        }
        size = parse_number(numbuf);
        add_md5_entry(md5, size, filename);
        memset(buf, 0, sizeof(buf));
        num_files++;
    }
    if (verbose > 0)
        fprintf(stderr, "parse_md5_list: added MD5 checksums for %d files\n", num_files);
    fclose(md5_file);
}

/* Initialise state and start the jigdo template file */
void write_jt_header(FILE *template_file, FILE *jigdo_file)
{
    t_file = template_file;
    j_file = jigdo_file;

    /* Start MD5 work for the image */
    mk_MD5Init(&iso_context);

    /* Start the template file */
    write_template_header();

    /* Load up the MD5 list if we've been given one */
    if (jmd5_list)
        parse_md5_list();
}

/* Compress and flush out a buffer full of template data */
static void flush_gzip_chunk(void *buffer, off_t size)
{
    z_stream c_stream; /* compression stream */
    unsigned char comp_size_out[6];
    unsigned char uncomp_size_out[6];
    off_t compressed_size_out = 0;
    int err = 0;
    unsigned char *comp_buf = NULL;

    c_stream.zalloc = NULL;
    c_stream.zfree = NULL;
    c_stream.opaque = NULL;

    err = deflateInit(&c_stream, Z_BEST_COMPRESSION);
    comp_buf = malloc(2 * size); /* Worst case */
    c_stream.next_out = comp_buf;
    c_stream.avail_out = 2 * size;
    c_stream.next_in = buffer;
    c_stream.avail_in = size;
    
    err = deflate(&c_stream, Z_NO_FLUSH);
    err = deflate(&c_stream, Z_FINISH);
    
    compressed_size_out = c_stream.total_out + 16;
    err = deflateEnd(&c_stream);

    template_fwrite("DATA", 4, 1, t_file);

    write_le48(compressed_size_out, &comp_size_out[0]);
    template_fwrite(comp_size_out, sizeof(comp_size_out), 1, t_file);

    write_le48(size, &uncomp_size_out[0]);
    template_fwrite(uncomp_size_out, sizeof(uncomp_size_out), 1, t_file);
    
    template_fwrite(comp_buf, c_stream.total_out, 1, t_file);
    free(comp_buf);
}

#ifdef BZ2_SUPPORT
/* Compress and flush out a buffer full of template data */
static void flush_bz2_chunk(void *buffer, off_t size)
{
    bz_stream c_stream; /* compression stream */
    unsigned char comp_size_out[6];
    unsigned char uncomp_size_out[6];
    off_t compressed_size_out = 0;
    int err = 0;
    unsigned char *comp_buf = NULL;

    c_stream.bzalloc = NULL;
    c_stream.bzfree = NULL;
    c_stream.opaque = NULL;

    err = BZ2_bzCompressInit(&c_stream, 9, 0, 0);
    comp_buf = malloc(2 * size); /* Worst case */
    c_stream.next_out = comp_buf;
    c_stream.avail_out = 2 * size;
    c_stream.next_in = buffer;
    c_stream.avail_in = size;
    
    err = BZ2_bzCompress(&c_stream, BZ_FINISH);
    
    compressed_size_out = c_stream.total_out_lo32 + 16;
    err = BZ2_bzCompressEnd(&c_stream);

    template_fwrite("DATA", 4, 1, t_file);

    write_le48(compressed_size_out, &comp_size_out[0]);
    template_fwrite(comp_size_out, sizeof(comp_size_out), 1, t_file);

    write_le48(size, &uncomp_size_out[0]);
    template_fwrite(uncomp_size_out, sizeof(uncomp_size_out), 1, t_file);
    
    template_fwrite(comp_buf, c_stream.total_out_lo32, 1, t_file);
    free(comp_buf);
}
#endif

static void flush_compressed_chunk(void *buffer, off_t size)
{
#ifdef BZ2_SUPPORT
    if (use_bz2)
        flush_bz2_chunk(buffer, size);
    else
#endif
        flush_gzip_chunk(buffer, size);
}

/* Append to an existing data buffer, and compress/flush it if
   necessary */
static void write_compressed_chunk(unsigned char *buffer, size_t size)
{
    static unsigned char uncomp_buf[1024 * 1024];
    static size_t uncomp_buf_used = 0;

    if ((uncomp_buf_used + size) > sizeof(uncomp_buf))
    {
        flush_compressed_chunk(uncomp_buf, uncomp_buf_used);
        uncomp_buf_used = 0;
    }

    if (!size) /* Signal a flush before we start writing the DESC entry */
    {
        flush_compressed_chunk(uncomp_buf, uncomp_buf_used);
        return;
    }
    
    if (!uncomp_buf_used)
        memset(uncomp_buf, 0, sizeof(uncomp_buf));

    while (size > sizeof(uncomp_buf))
    {
        flush_compressed_chunk(buffer, sizeof(uncomp_buf));
        buffer += sizeof(uncomp_buf);
        size -= sizeof(uncomp_buf);
    }
    memcpy(&uncomp_buf[uncomp_buf_used], buffer, size);
    uncomp_buf_used += size;
}

/* Loop through the list of DESC entries that we've built up and
   append them to the template file */
static void write_template_desc_entries(off_t image_len, char *image_md5)
{
    entry_t *entry = entry_list;
    off_t desc_len = 0;
    unsigned char out_len[6];
    jigdo_image_entry_t jimage;

    desc_len = 16 /* DESC + length twice */
        + (sizeof(jigdo_file_entry_t) * num_matches)
        + (sizeof(jigdo_chunk_entry_t) * num_chunks)
        + sizeof(jigdo_image_entry_t);

    write_le48(desc_len, &out_len[0]);
    write_compressed_chunk(NULL, 0);
    template_fwrite("DESC", 4, 1, t_file);
    template_fwrite(out_len, sizeof(out_len), 1, t_file);
    
    while (entry)
    {
        switch (entry->entry_type)
        {
            case JTET_FILE_MATCH:
            {
                jigdo_file_entry_t jfile;
                jfile.type = 6; /* Matched file */
                write_le48(entry->data.file.file_length, &jfile.fileLen[0]);
                write_le64(entry->data.file.rsyncsum, &jfile.fileRsync[0]);
                memcpy(jfile.fileMD5, entry->data.file.md5, sizeof(jfile.fileMD5));
                template_fwrite(&jfile, sizeof(jfile), 1, t_file);
                break;
            }
            case JTET_NOMATCH:
            {
                jigdo_chunk_entry_t jchunk;
#ifdef BZ2_SUPPORT
                if (use_bz2)
                    jchunk.type = 8; /* Raw data, bzipped */
                else
#endif
                    jchunk.type = 2; /* Raw data, gzipped */
                write_le48(entry->data.chunk.uncompressed_length, &jchunk.skipLen[0]);
                template_fwrite(&jchunk, sizeof(jchunk), 1, t_file);
                break;
            }
        }
        entry = entry->next;
    }

    jimage.type = 5;
    write_le48(image_len, &jimage.imageLen[0]);
    memcpy(jimage.imageMD5, image_md5, sizeof(jimage.imageMD5));
    write_le32(MIN_JIGDO_FILE_SIZE, &jimage.blockLen[0]);
    template_fwrite(&jimage, sizeof(jimage), 1, t_file);    
    template_fwrite(out_len, sizeof(out_len), 1, t_file);
}

/* Dump a buffer in jigdo-style "base64" */
static char *base64_dump(unsigned char *buf, size_t buf_size)
{
    const char *b64_enc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    int value = 0;
    unsigned int i;
    int bits = 0;
    static char output_buffer[2048];
    char *p = output_buffer;

    memset(output_buffer, 0, sizeof(output_buffer));
    if (buf_size >= (sizeof(output_buffer) * 6/8))
    {
        fprintf(stderr, "base64_dump: Buffer too small!\n");
        exit(1);
    }

    for (i = 0; i < buf_size ; i++)
    {
        value = (value << 8) | buf[i];
        bits += 2;
        p += sprintf(p, "%c", b64_enc[(value >> bits) & 63U]);
        if (bits >= 6) {
            bits -= 6;
            p += sprintf(p, "%c", b64_enc[(value >> bits) & 63U]);
        }
    }
    if (bits > 0)
    {
        value <<= 6 - bits;
        p += sprintf(p, "%c", b64_enc[value & 63U]);
    }
    return output_buffer;
}

/* Write the .jigdo file to match the .template we've just finished. */
static void write_jigdo_file(void)
{
    unsigned char template_md5sum[16];
    entry_t *entry = entry_list;
    struct path_mapping *map = map_list;

    mk_MD5Final(&template_md5sum[0], &template_context);

    fprintf(j_file, "# JigsawDownload\n");
    fprintf(j_file, "# See <http://atterer.net/jigdo/> for details about jigdo\n");
    fprintf(j_file, "# See <http://www.einval.com/~steve/software/CD/JTE/> for details about JTE\n\n");
    
    fprintf(j_file, "[Jigdo]\n");
    fprintf(j_file, "Version=%s\n", JIGDO_TEMPLATE_VERSION);
    fprintf(j_file, "Generator=%s/%d.%d\n\n", JTE_NAME, JTE_VER_MAJOR, JTE_VER_MINOR);

    fprintf(j_file, "[Image]\n");
    fprintf(j_file, "Filename=%s\n", file_base_name(outfile));
    fprintf(j_file, "Template=http://localhost/%s\n", jtemplate_out);
    fprintf(j_file, "Template-MD5Sum=%s \n",
            base64_dump(&template_md5sum[0], sizeof(template_md5sum)));
    fprintf(j_file, "# Template Hex MD5sum %s\n",
            hex_dump(&template_md5sum[0], sizeof(template_md5sum)));
    fprintf(j_file, "# Template size %lld bytes\n", template_size);
    fprintf(j_file, "# Image size %lld bytes\n\n", image_size);

    fprintf(j_file, "[Parts]\n");
    while (entry)
    {
        if (JTET_FILE_MATCH == entry->entry_type)
        {
            char *new_name = remap_filename(entry->data.file.filename);
            fprintf(j_file, "%s=%s\n",
                    base64_dump(&entry->data.file.md5[0], sizeof(entry->data.file.md5)),
                    new_name);
            free(new_name);
        }
        entry = entry->next;
    }

    fprintf(j_file, "\n[Servers]\n");
    fflush(j_file);
}

/* Finish and flush state; for now:
   
   1. Dump the DESC blocks and the footer information in the jigdo template file
   2. Write the jigdo .jigdo file containing file pointers
*/
void write_jt_footer(void)
{
    unsigned char md5[16]; /* MD5SUM of the entire image */

    /* Finish calculating the image's checksum */
    mk_MD5Final(&md5[0], &iso_context);

    /* And calculate the image size */
    image_size = (unsigned long long)SECTOR_SIZE * last_extent_written;

    write_template_desc_entries(image_size, md5);

    write_jigdo_file();
}

/* Add a raw data entry to the list of extents; no file to match */
static void add_unmatched_entry(int uncompressed_length)
{
    entry_t *new_entry = NULL;

    /* Can we extend a previous non-match entry? */
    if (entry_last && (JTET_NOMATCH == entry_last->entry_type))
    {
        entry_last->data.chunk.uncompressed_length += uncompressed_length;
        return;
    }

    new_entry = calloc(1, sizeof(entry_t));
    new_entry->entry_type = JTET_NOMATCH;
    new_entry->next = NULL;
    new_entry->data.chunk.uncompressed_length = uncompressed_length;

    /* Add to the end of the list */
    if (NULL == entry_last)
    {
        entry_last = new_entry;
        entry_list = new_entry;
    }
    else
    {
        entry_last->next = new_entry;
        entry_last = new_entry;
    }
    num_chunks++;
}

/* Add a file match entry to the list of extents */
static void add_file_entry(char *filename, off_t size, unsigned char *md5,
                           unsigned long long rsyncsum)
{
    entry_t *new_entry = NULL;

    new_entry = calloc(1, sizeof(entry_t));
    new_entry->entry_type = JTET_FILE_MATCH;
    new_entry->next = NULL;
    memcpy(new_entry->data.file.md5, md5, sizeof(new_entry->data.file.md5));
    new_entry->data.file.file_length = size;
    new_entry->data.file.rsyncsum = rsyncsum;
    new_entry->data.file.filename = strdup(filename);

    /* Add to the end of the list */
    if (NULL == entry_last)
    {
        entry_last = new_entry;
        entry_list = new_entry;
    }
    else
    {
        entry_last->next = new_entry;
        entry_last = new_entry;
    }
    num_matches++;
}    

/* Cope with an unmatched block in the .iso file:

   1. Write a compressed data chunk in the jigdo template file
   2. Add an entry in our list of unmatched chunks for later */
void jtwrite(buffer, size, count, submode, islast)
	void	*buffer;
	int	size;
	int	count;
	int	submode;
	BOOL	islast;
{
#ifdef	JTWRITE_DEBUG
	if (count != 1 || (size % 2048) != 0)
		fprintf(stderr, "Count: %d, size: %d\n", count, size);
#endif

    if (!jtemplate_out)
        return;

    /* Update the global image checksum */
    mk_MD5Update(&iso_context, buffer, size*count);

    /* Write a compressed version of the data to the template file,
       and add a reference on the state list so we can write that
       later. */
    write_compressed_chunk(buffer, size*count);
    add_unmatched_entry(size*count);
}

/* Cope with a file entry in the .iso file:

   1. Read the file for the image's md5 checksum
   2. Add an entry in our list of files to be written into the .jigdo later
*/
void write_jt_match_record(char *filename, char *mirror_name, int sector_size, off_t size, unsigned char md5[16])
{
    unsigned long long  tmp_size = 0;
    char                buf[32768];
    off_t               remain = size;
	FILE               *infile = NULL;
	int	                use = 0;
    unsigned long long  rsync64_sum = 0;
    int first_block = 1;

    memset(buf, 0, sizeof(buf));

    if ((infile = fopen(filename, "rb")) == NULL) {
#ifdef	USE_LIBSCHILY
		comerr("cannot open '%s'\n", filename);
#else
#ifndef	HAVE_STRERROR
		fprintf(stderr, "cannot open '%s': (%d)\n",
				filename, errno);
#else
		fprintf(stderr, "cannot open '%s': %s\n",
				filename, strerror(errno));
#endif
		exit(1);
#endif
	}

    while (remain > 0)
    {
        use = remain;
        if (remain > sizeof(buf))
            use = sizeof(buf);
		if (fread(buf, 1, use, infile) == 0)
        {
#ifdef	USE_LIBSCHILY
			comerr("cannot read from '%s'\n", filename);
#else
			fprintf(stderr, "cannot read from '%s'\n", filename);
			exit(1);
#endif
		}
        if (first_block)
            rsync64_sum = rsync64(buf, MIN_JIGDO_FILE_SIZE);
        mk_MD5Update(&iso_context, buf, use);
        remain -= use;
        first_block = 0;
    }

    fclose(infile);
    
    /* Update the image checksum with any necessary padding data */
    if (size % sector_size)
    {
        int pad_size = sector_size - (size % sector_size);
        memset(buf, 0, pad_size);
        mk_MD5Update(&iso_context, buf, pad_size);
    }

    add_file_entry(mirror_name, size, &md5[0], rsync64_sum);
    if (size % sector_size)
    {
        int pad_size = sector_size - (size % sector_size);
        write_compressed_chunk(buf, pad_size);
        add_unmatched_entry(pad_size);
    }        
}
