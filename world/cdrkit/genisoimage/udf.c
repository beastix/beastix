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

/* @(#)udf.c	1.14 04/04/15 Copyright 2001 J. Schilling */
/*
 * udf.c - UDF support for genisoimage
 *
 * Written by Ben Rudiak-Gould (2001).
 *
 * Copyright 2001 J. Schilling.
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Some remaining issues:
 *
 * - Do not forget to edit joliet.c and remove the VIDEO_TS lines after
 *   we did implement a decent own file name handling for UDF.
 *
 * - UDF supports UNIX-style file permissions and uid/gid, but currently
 *   this code just sets them to default values and ignores their Rock
 *   Ridge counterparts. This would be easy to fix.
 *
 * - There's no support for symlinks, Mac type/creator, or Mac resource
 *   forks. Hard links and followed symlinks may work, but if so, it's
 *   only by accident.
 *
 * - The file system mirrors the Joliet file system, so files excluded
 *   from Joliet will also be excluded from UDF, the -jcharset option
 *   also applies to UDF, file names too long for Joliet will also be
 *   truncated on UDF, and characters not allowed by Joliet will also
 *   be translated on UDF. (Fortunately, Joliet is pretty lenient.)
 *
 * - convert_to_unicode is always called with in_nls, not hfs_nls. This
 *   may lead to incorrect name conversion sometimes when using a Mac
 *   filesystem. See joliet.c for an example of what's supposed to be
 *   done.
 *
 * - DVD-Video discs are supposed to have Copyright Management Information
 *   in both the ISO and UDF filesystems. This is not implemented in ISO,
 *   and the code to do it in UDF is currently #ifdef'd out. I'm not sure
 *   whether discs without this information are actually DVD-Video
 *   compliant. The Copyright Management Information is described in ECMA
 *   Technical Report TR/71.
 *
 * - Most of the space before sector 256 on the disc (~480K) is wasted,
 *   because UDF Bridge requires a pointer block at sector 256. ISO 9660
 *   structures could be moved below sector 256 if they're small enough, but
 *   this would be ugly to implement since it breaks the output_fragment
 *   abstraction.
 *
 * - Each file must have a File Entry, and each File Entry seems to
 *   require its own 2K sector. As a result, there is an overhead of more
 *   than 2K *per file* when using UDF. I couldn't see any way to avoid
 *   this.
 *
 * - Read performance would probably be improved by placing the File Entry
 *   for each file just before the file itself, instead of at the beginning
 *   of the disc. But this would not work for DVD-Video files, which have
 *   to be stored contiguously. So there would have to be an override
 *   mechanism to handle this case. I don't know if it's worth the trouble.
 */

#ifdef UDF

#include "config.h"
#include "genisoimage.h"
#include <timedefs.h>
#include <schily.h>

#include "udf.h"
#include "udf_fs.h"

extern	int	use_sparcboot;

extern struct directory *root;
extern time_t		begun;

static unsigned lba_main_seq;
static unsigned lba_main_seq_copy;
static unsigned lba_integ_seq;
static unsigned lba_udf_partition_start;
static unsigned lba_last_file_entry;
static unsigned lba_end_anchor_vol_desc;

static unsigned num_udf_files;
static unsigned num_udf_directories;

static unsigned volume_set_id[2];

#define	UDF_MAIN_SEQ_LENGTH (16)
#define	UDF_INTEG_SEQ_LENGTH (2)

/* only works for granularity a power of 2! */
#define	PAD(val, granularity)	(((val)+(granularity)-1)&~((granularity)-1))

#define	read_733(field) ((0[field]&255)+(1[field]&255)*256+(2[field]&255)*65536+(3[field]&255)*16777216)


/**************** SIZE ****************/

static int set_file_ident_desc(unsigned char *, unsigned, char *, int, 
										 unsigned, unsigned);

static unsigned
directory_size(struct directory *dpnt)
{
	unsigned size_in_bytes;
	struct directory_entry *de;
	Uchar dummy_buf[SECTOR_SIZE];

	/* parent directory */
	size_in_bytes = set_file_ident_desc(dummy_buf, 0, 0, 0, 0, 0);

	/* directory contents */
	for (de = dpnt->jcontents; de; de = de->jnext) {
		if (!(de->de_flags & INHIBIT_JOLIET_ENTRY)) {
			char *name = USE_MAC_NAME(de) ? de->hfs_ent->name : de->name;
			/* skip . and .. */
			if (name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0)))
				continue;
			size_in_bytes += set_file_ident_desc(dummy_buf, 0, name, 0, 0, 0);
		}
	}
	return (size_in_bytes);
}

static void
assign_udf_directory_addresses(struct directory *dpnt)
{
	if (!(dpnt->dir_flags & INHIBIT_JOLIET_ENTRY)) {
		dpnt->self->udf_file_entry_sector = last_extent;
		last_extent += 1 + ISO_BLOCKS(directory_size(dpnt));
		++num_udf_directories;
	}
	if (!(dpnt->dir_flags & INHIBIT_JOLIET_ENTRY) || dpnt == reloc_dir) {
		for (dpnt = dpnt->subdir; dpnt; dpnt = dpnt->next) {
			assign_udf_directory_addresses(dpnt);
		}
	}
}

static void
assign_udf_file_entry_addresses(struct directory *dpnt)
{
	if (!(dpnt->dir_flags & INHIBIT_JOLIET_ENTRY)) {
		struct directory_entry *de;
		for (de = dpnt->jcontents; de; de = de->jnext) {
			if (!(de->de_flags & RELOCATED_DIRECTORY) &&
			    !(de->isorec.flags[0] & ISO_DIRECTORY)) {
				de->udf_file_entry_sector = last_extent++;
				++num_udf_files;
			}
		}
	}
	if (!(dpnt->dir_flags & INHIBIT_JOLIET_ENTRY) || dpnt == reloc_dir) {
		for (dpnt = dpnt->subdir; dpnt; dpnt = dpnt->next) {
			assign_udf_file_entry_addresses(dpnt);
		}
	}
}

/****************************/

static int
udf_vol_recognition_area_size(int starting_extent)
{
	last_extent = starting_extent+3;
	return (0);
}

static int
udf_main_seq_size(int starting_extent)
{
	lba_main_seq = starting_extent;
	last_extent = starting_extent + UDF_MAIN_SEQ_LENGTH;
	return (0);
}

static int
udf_main_seq_copy_size(int starting_extent)
{
	lba_main_seq_copy = starting_extent;
	last_extent = starting_extent + UDF_MAIN_SEQ_LENGTH;
	return (0);
}

static int
udf_integ_seq_size(int starting_extent)
{
	lba_integ_seq = starting_extent;
	last_extent = starting_extent + UDF_INTEG_SEQ_LENGTH;
	return (0);
}

static int
udf_end_anchor_vol_desc_size(int starting_extent)
{
	lba_end_anchor_vol_desc = starting_extent;
	last_extent = starting_extent+1;
	return (0);
}

static int
udf_file_set_desc_size(int starting_extent)
{
	lba_udf_partition_start = starting_extent;
	last_extent = starting_extent+2;
	return (0);
}

static int
udf_dirtree_size(int starting_extent)
{
	num_udf_directories = 0;
	assign_udf_directory_addresses(root);
	return (0);
}

static int
udf_file_entries_size(int starting_extent)
{
	num_udf_files = 0;
	assign_udf_file_entry_addresses(root);
	lba_last_file_entry = last_extent-1;
	return (0);
}

static int
udf_pad_to_sector_32_size(int starting_extent)
{
	if (last_extent < session_start+32)
		last_extent = session_start+32;
	return (0);
}

static int
udf_pad_to_sector_256_size(int starting_extent)
{
	if (last_extent < session_start+256)
		last_extent = session_start+256;
	return (0);
}

static int
udf_padend_avdp_size(int starting_extent)
{
	lba_end_anchor_vol_desc = starting_extent;

	/* add at least 16 and at most 31 sectors, ending at a mult. of 16 */
	last_extent = (starting_extent+31) & ~15;
	if (!use_sparcboot)
		last_extent = starting_extent + 150;
	return (0);
}

extern int oneblock_size(int);

/**************** WRITE ****************/

static unsigned
crc_ccitt(unsigned char *buf, unsigned len)
{
	const unsigned poly = 0x11021;
	static unsigned short lookup[256];
	unsigned int r;
	unsigned int i;

	if (lookup[1] == 0) {
		unsigned int j, k;
		for (j = 0; j < 256; ++j) {
			unsigned int temp = j << 8;
			for (k = 0; k < 8; ++k) {
				unsigned int hibit = temp & 32768;
				temp <<= 1;
				if (hibit)
					temp ^= poly;
			}
			lookup[j] = temp;
		}
	}

	r = 0;
	for (i = 0; i < len; ++i) {
		r = (r << 8) ^ lookup[((r >> 8) ^ buf[i]) & 255];
	}

	return (r & 65535);
}

#define	set8(dst, src)	do { *(dst) = (src); } while (0)

static void
set16(udf_Uint16 *dst, unsigned int src)
{
	dst->l = (char)(src);
	dst->h = (char)(src>>8);
}

static void
set32(udf_Uint32 *dst, unsigned src)
{
	dst->l  = (char)(src);
	dst->ml = (char)(src>>8);
	dst->mh = (char)(src>>16);
	dst->h  = (char)(src>>24);
}

static void
set64(udf_Uint64 *dst, uint64_t src)
{
	set32(&dst->l, src);
	set32(&dst->h, src>>32);
}

static int
set_ostaunicode(unsigned char *dst, int dst_size, char *src)
{
	unsigned char buf[1024];
	int i;
	int expanded_length;

	expanded_length = joliet_strlen(src, in_nls);
	if (expanded_length > 1024)
		expanded_length = 1024;
	if (expanded_length > (dst_size-1)*2)
		expanded_length = (dst_size-1)*2;

	convert_to_unicode(buf, expanded_length, src, in_nls);
	dst[0] = 8;	/* use 8-bit representation by default */
	for (i = 0; i < (expanded_length>>1); ++i) {
		dst[i + 1] = buf[i*2+1];
		if (buf[i*2] != 0) {
			/*
			 * There's a Unicode character with value >=256.
			 * Use 16-bit representation instead.
			 */
			int length_to_copy = (dst_size-1) & ~1;
			if (length_to_copy > expanded_length)
				length_to_copy = expanded_length;
			dst[0] = 16;
			memcpy(dst+1, buf, length_to_copy);
			return (length_to_copy + 1);
		}
	}
	return ((expanded_length>>1) + 1);
}

static void
set_extent(udf_extent_ad *ext, unsigned lba, unsigned length_bytes)
{
	set32(&ext->extent_length, length_bytes);
	set32(&ext->extent_location, lba);
}

static void
set_dstring(udf_dstring *dst, char *src, int n)
{
	dst[n-1] = set_ostaunicode((Uchar *)dst, n-1, src);
}

static void
set_charspec(udf_charspec *dst)
{
	/*set8(&dst->character_set_type, 0);*/
	memcpy(dst->character_set_info, "OSTA Compressed Unicode", 23);
}

static void
set_impl_ident(udf_EntityID *ent)
{
	strcpy((char *)ent->ident, "*genisoimage");
}

static void
set_tag(udf_tag *t, unsigned tid, unsigned lba, int crc_length)
{
	unsigned char checksum;
	int i;

	set16(&t->tag_ident, tid);
	set16(&t->desc_version, 2);
	set16(&t->desc_crc, crc_ccitt((unsigned char *)t+16, crc_length-16));
	set16(&t->desc_crc_length, crc_length-16);
	set32(&t->tag_location, lba);
	set8(&t->tag_checksum, 0);
	checksum = 0;
	for (i = 0; i < 16; ++i)
		checksum += ((unsigned char *)t)[i];
	set8(&t->tag_checksum, checksum);
}

static void
set_timestamp_from_iso_date(udf_timestamp *ts, const char *iso_date_raw)
{
	struct {
		unsigned char years_since_1900;
		unsigned char month, day;
		unsigned char hour, minute, second;
		signed char offset_from_gmt;
	} *iso_date = (void *)iso_date_raw;

	set16(&ts->type_and_time_zone,
		4096 + ((iso_date->offset_from_gmt * 15) & 4095));
	set16(&ts->year, 1900 + iso_date->years_since_1900);
	set8(&ts->month, iso_date->month);
	set8(&ts->day, iso_date->day);
	set8(&ts->hour, iso_date->hour);
	set8(&ts->minute, iso_date->minute);
	set8(&ts->second, iso_date->second);
	/*set8(&ts->centiseconds, 0);*/
	/*set8(&ts->hundreds_of_microseconds, 0);*/
	/*set8(&ts->microseconds, 0);*/
}

static void
set_timestamp_from_time_t(udf_timestamp *ts, time_t t)
{
	char iso_date[7];
	iso9660_date(iso_date, t);
	set_timestamp_from_iso_date(ts, iso_date);
}


static void
set_anchor_volume_desc_pointer(unsigned char *buf, unsigned lba)
{
	udf_anchor_volume_desc_ptr *avdp = (udf_anchor_volume_desc_ptr *)buf;
	set_extent(&avdp->main_volume_desc_seq_extent,
		lba_main_seq, SECTOR_SIZE*UDF_MAIN_SEQ_LENGTH);
	set_extent(&avdp->reserve_volume_desc_seq_extent,
		lba_main_seq_copy, SECTOR_SIZE*UDF_MAIN_SEQ_LENGTH);
	set_tag(&avdp->desc_tag, UDF_TAGID_ANCHOR_VOLUME_DESC_PTR, lba, 512);
}

static void
set_primary_vol_desc(unsigned char *buf, unsigned lba)
{
	char temp[17];

	udf_primary_volume_desc *pvd = (udf_primary_volume_desc *)buf;
	/*set32(&pvd->volume_desc_seq_number, 0);*/
	/*set32(&pvd->primary_volume_desc_number, 0);*/
	set_dstring(pvd->volume_ident, volume_id, sizeof (pvd->volume_ident));
	set16(&pvd->volume_seq_number, 1);
	set16(&pvd->maximum_volume_seq_number, 1);
	set16(&pvd->interchange_level, 2);
	set16(&pvd->maximum_interchange_level, 2);
	set32(&pvd->character_set_list, 1);
	set32(&pvd->maximum_character_set_list, 1);
	sprintf(temp, "%08X%08X", volume_set_id[0], volume_set_id[1]);
	set_dstring(pvd->volume_set_ident, temp,
					sizeof (pvd->volume_set_ident));
	set_charspec(&pvd->desc_character_set);
	set_charspec(&pvd->explanatory_character_set);
	/*pvd->volume_abstract;*/
	/*pvd->volume_copyright_notice;*/
	/*pvd->application_ident;*/
	set_timestamp_from_time_t(&pvd->recording_date_and_time, begun);
	set_impl_ident(&pvd->impl_ident);
	set_tag(&pvd->desc_tag, UDF_TAGID_PRIMARY_VOLUME_DESC, lba, 512);
}

static void
set_impl_use_vol_desc(unsigned char *buf, unsigned lba)
{
	udf_impl_use_volume_desc *iuvd = (udf_impl_use_volume_desc *)buf;
	set32(&iuvd->volume_desc_seq_number, 1);
	strcpy((char *)iuvd->impl_ident.ident, "*UDF LV Info");
	iuvd->impl_ident.ident_suffix[0] = 2;
	iuvd->impl_ident.ident_suffix[1] = 1;
	set_charspec(&iuvd->impl_use.lvi_charset);
	set_dstring(iuvd->impl_use.logical_volume_ident, volume_id,
		sizeof (iuvd->impl_use.logical_volume_ident));
	/*set_dstring(iuvd->impl_use.lv_info1, "", sizeof (iuvd->impl_use.lv_info1));*/
	/*set_dstring(iuvd->impl_use.lv_info2, "", sizeof (iuvd->impl_use.lv_info2));*/
	/*set_dstring(iuvd->impl_use.lv_info3, "", sizeof (iuvd->impl_use.lv_info3));*/
	set_impl_ident(&iuvd->impl_use.impl_id);
	set_tag(&iuvd->desc_tag, UDF_TAGID_IMPL_USE_VOLUME_DESC, lba, 512);
}

static void
set_partition_desc(unsigned char *buf, unsigned lba)
{
	udf_partition_desc *pd = (udf_partition_desc *)buf;
	set32(&pd->volume_desc_seq_number, 2);
	set16(&pd->partition_flags, UDF_PARTITION_FLAG_ALLOCATED);
	/*set16(&pd->partition_number, 0);*/
	set8(&pd->partition_contents.flags, UDF_ENTITYID_FLAG_PROTECTED);	/*???*/
	strcpy((char *)pd->partition_contents.ident, "+NSR02");
	set32(&pd->access_type, UDF_ACCESSTYPE_READONLY);
	set32(&pd->partition_starting_location, lba_udf_partition_start);
	set32(&pd->partition_length,
			lba_end_anchor_vol_desc - lba_udf_partition_start);
	set_impl_ident(&pd->impl_ident);
	set_tag(&pd->desc_tag, UDF_TAGID_PARTITION_DESC, lba, 512);
}

static void
set_domain_ident(udf_EntityID *ent)
{
	strcpy((char *)ent->ident, "*OSTA UDF Compliant");
	memcpy(ent->ident_suffix, "\002\001\003", 3);
}

static void
set_logical_vol_desc(unsigned char *buf, unsigned lba)
{
	udf_logical_volume_desc *lvd = (udf_logical_volume_desc *)buf;
	set32(&lvd->volume_desc_seq_number, 3);
	set_charspec(&lvd->desc_character_set);
	set_dstring(lvd->logical_volume_ident, volume_id,
					sizeof (lvd->logical_volume_ident));
	set32(&lvd->logical_block_size, SECTOR_SIZE);
	set_domain_ident(&lvd->domain_ident);
	set32(&lvd->logical_volume_contents_use.extent_length, 2*SECTOR_SIZE);
	/*set32(&lvd->logical_volume_contents_use.extent_location.logical_block_number, 0);*/
	/*set16(&lvd->logical_volume_contents_use.extent_location.partition_reference_number, 0);*/
	set32(&lvd->map_table_length, 6);
	set32(&lvd->number_of_partition_maps, 1);
	set_impl_ident(&lvd->impl_ident);
	set_extent(&lvd->integrity_seq_extent, lba_integ_seq,
					SECTOR_SIZE*UDF_INTEG_SEQ_LENGTH);
	set8(&lvd->partition_map[0].partition_map_type,
					UDF_PARTITION_MAP_TYPE_1);
	set8(&lvd->partition_map[0].partition_map_length, 6);
	set16(&lvd->partition_map[0].volume_seq_number, 1);
	/*set16(&lvd->partition_map[0].partition_number, 0);*/
	set_tag(&lvd->desc_tag, UDF_TAGID_LOGICAL_VOLUME_DESC, lba, 446);
}

static void
set_unallocated_space_desc(unsigned char *buf, unsigned lba)
{
	udf_unallocated_space_desc *usd = (udf_unallocated_space_desc *)buf;
	set32(&usd->volume_desc_seq_number, 4);
	/*set32(&usd->number_of_allocation_descs, 0);*/
	set_tag(&usd->desc_tag, UDF_TAGID_UNALLOCATED_SPACE_DESC, lba, 24);
}

static void
set_terminating_desc(unsigned char *buf, unsigned lba)
{
	udf_terminating_desc *td = (udf_terminating_desc *)buf;
	set_tag(&td->desc_tag, UDF_TAGID_TERMINATING_DESC, lba, 512);
}

static void
set_logical_vol_integrity_desc(unsigned char *buf, unsigned lba)
{
	udf_logical_volume_integrity_desc *lvid =
				(udf_logical_volume_integrity_desc *)buf;

	set_timestamp_from_time_t(&lvid->recording_date, begun);
	set32(&lvid->integrity_type, UDF_INTEGRITY_TYPE_CLOSE);
	/*lvid->next_integrity_extent;*/
	set64(&lvid->logical_volume_contents_use.unique_id,
						lba_last_file_entry+1);
	set32(&lvid->number_of_partitions, 1);
	set32(&lvid->length_of_impl_use, 46);
	/*set32(&lvid->free_space_table, 0);*/
	set32(&lvid->size_table,
			lba_end_anchor_vol_desc - lba_udf_partition_start);
	set_impl_ident(&lvid->impl_use.impl_id);
	set32(&lvid->impl_use.number_of_files, num_udf_files);
	set32(&lvid->impl_use.number_of_directories, num_udf_directories);
	set16(&lvid->impl_use.minimum_udf_read_revision, 0x102);
	set16(&lvid->impl_use.minimum_udf_write_revision, 0x102);
	set16(&lvid->impl_use.maximum_udf_write_revision, 0x102);
	set_tag(&lvid->desc_tag, UDF_TAGID_LOGICAL_VOLUME_INTEGRITY_DESC,
								lba, 88+46);
}

static void
set_file_set_desc(unsigned char *buf, unsigned rba)
{
	udf_file_set_desc *fsd = (udf_file_set_desc *)buf;

	set_timestamp_from_time_t(&fsd->recording_date_and_time, begun);
	set16(&fsd->interchange_level, 3);
	set16(&fsd->maximum_interchange_level, 3);
	set32(&fsd->character_set_list, 1);
	set32(&fsd->maximum_character_set_list, 1);
	/*set32(&fsd->file_set_number, 0);*/
	/*set32(&fsd->file_set_desc_number, 0);*/
	set_charspec(&fsd->logical_volume_ident_character_set);
	set_dstring(fsd->logical_volume_ident, volume_id,
					sizeof (fsd->logical_volume_ident));
	set_charspec(&fsd->file_set_character_set);
	set_dstring(fsd->file_set_ident, volume_id,
					sizeof (fsd->file_set_ident));
	/*fsd->copyright_file_ident;*/
	/*fsd->abstract_file_ident;*/
	set32(&fsd->root_directory_icb.extent_length, SECTOR_SIZE);
	set32(&fsd->root_directory_icb.extent_location.logical_block_number,
		root->self->udf_file_entry_sector - lba_udf_partition_start);
	set_domain_ident(&fsd->domain_ident);
	/*fsd->next_extent;*/
	set_tag(&fsd->desc_tag, UDF_TAGID_FILE_SET_DESC, rba, 512);
}

static int
set_file_ident_desc(unsigned char *buf, unsigned rba, char *name, 
						  int is_directory, unsigned file_entry_rba, 
						  unsigned unique_id)
{
	udf_file_ident_desc *fid = (udf_file_ident_desc *)buf;
	int length_of_file_ident, length, padded_length;
	set16(&fid->file_version_number, 1);
	set8(&fid->file_characteristics,
		(is_directory ? UDF_FILE_CHARACTERISTIC_DIRECTORY : 0)
		+ (name == 0) * UDF_FILE_CHARACTERISTIC_PARENT);
	set32(&fid->icb.extent_length, SECTOR_SIZE);
	set32(&fid->icb.extent_location.logical_block_number, file_entry_rba);
	set16(&fid->icb.extent_location.partition_reference_number, 0);
	set32(&fid->icb.impl_use.unique_id, unique_id);
	set16(&fid->length_of_impl_use, 0);
	if (name) {
		length_of_file_ident =
			set_ostaunicode((Uchar *)fid->file_ident, 512, name);
	} else {
		length_of_file_ident = 0;
	}
	set8(&fid->length_of_file_ident, length_of_file_ident);
	length = 38 + length_of_file_ident;
	padded_length = PAD(length, 4);
	while (length < padded_length) {
		buf[length++] = 0;
	}
	set_tag(&fid->desc_tag, UDF_TAGID_FILE_IDENT_DESC, rba, length);
	return (length);
}

static void
set_file_entry(unsigned char *buf, unsigned rba, unsigned file_rba,
					uint64_t length, const char *iso_date, int is_directory,
					unsigned link_count, unsigned unique_id)
{
	udf_short_ad	*allocation_desc;
	unsigned	chunk;

	udf_file_entry *fe = (udf_file_entry *)buf;

	/*set32(&fe->icb_tag.prior_recorded_number_of_direct_entries, 0);*/
	set16(&fe->icb_tag.strategy_type, 4);
	/*set16(&fe->icb_tag.strategy_parameter, 0);*/
	set16(&fe->icb_tag.maximum_number_of_entries, 1);
	set8(&fe->icb_tag.file_type, is_directory
		? UDF_ICBTAG_FILETYPE_DIRECTORY : UDF_ICBTAG_FILETYPE_BYTESEQ);
	/*fe->icb_tag.parent_icb_location;*/
	set16(&fe->icb_tag.flags, UDF_ICBTAG_FLAG_NONRELOCATABLE
		| UDF_ICBTAG_FLAG_ARCHIVE | UDF_ICBTAG_FLAG_CONTIGUOUS);
	if (rationalize_uid)
		set32(&fe->uid, uid_to_use);
	else
		set32(&fe->uid, -1);
	if (rationalize_gid)
		set32(&fe->gid, gid_to_use);
	else
		set32(&fe->gid, -1);
	if (is_directory) {
		set32(&fe->permissions,
		    UDF_FILEENTRY_PERMISSION_OR | UDF_FILEENTRY_PERMISSION_OX |
		    UDF_FILEENTRY_PERMISSION_GR | UDF_FILEENTRY_PERMISSION_GX |
		    UDF_FILEENTRY_PERMISSION_UR | UDF_FILEENTRY_PERMISSION_UX);
	} else {
		set32(&fe->permissions, UDF_FILEENTRY_PERMISSION_OR
		| UDF_FILEENTRY_PERMISSION_GR | UDF_FILEENTRY_PERMISSION_UR);
	}
	set16(&fe->file_link_count, link_count);
	/*fe->record_format;*/
	/*fe->record_display_attributes;*/
	/*fe->record_length;*/
	set64(&fe->info_length, length);
	set64(&fe->logical_blocks_recorded, ISO_BLOCKS(length));
	if (iso_date) {
		set_timestamp_from_iso_date(&fe->access_time, iso_date);
		fe->modification_time = fe->access_time;
		fe->attribute_time = fe->access_time;
	}
	set32(&fe->checkpoint, 1);
	/*fe->ext_attribute_icb;*/
	set_impl_ident(&fe->impl_ident);
	set64(&fe->unique_id, unique_id);
	/*
	 * Extended attributes that may (?) be required for DVD-Video
	 * compliance
	 */
#if 0
	set32(&fe->length_of_ext_attributes, 24+52+56);
	set32(&fe->ext_attribute_header.impl_attributes_location, 24);
	set32(&fe->ext_attribute_header.application_attributes_location,
								24+52+56);
	set_tag(&fe->ext_attribute_header.desc_tag,
			UDF_TAGID_EXT_ATTRIBUTE_HEADER_DESC, rba, 24 /*???*/);
	set32(&fe->ext_attribute_free_ea_space.attribute_type, SECTOR_SIZE);
	set8(&fe->ext_attribute_free_ea_space.attribute_subtype, 1);
	set32(&fe->ext_attribute_free_ea_space.attribute_length, 52);
	set32(&fe->ext_attribute_free_ea_space.impl_use_length, 4);
	strcpy((char *)fe->ext_attribute_free_ea_space.impl_ident.ident,
							"*UDF FreeAppEASpace");
	set32(&fe->ext_attribute_dvd_cgms_info.attribute_type, SECTOR_SIZE);
	set8(&fe->ext_attribute_dvd_cgms_info.attribute_subtype, 1);
	set32(&fe->ext_attribute_dvd_cgms_info.attribute_length, 56);
	set32(&fe->ext_attribute_dvd_cgms_info.impl_use_length, 8);
	strcpy((char *)fe->ext_attribute_free_ea_space.impl_ident.ident,
							"*UDF DVD CGMS Info");
	fe->ext_attribute_free_ea_space.impl_ident.ident_suffix[0] = 2;
	fe->ext_attribute_free_ea_space.impl_ident.ident_suffix[1] = 1;
#else
	/*set32(&fe->length_of_ext_attributes, 0);*/
#endif

	allocation_desc = &fe->allocation_desc;
	/*
	 * Only a file size less than 1GB can be expressed by a single
	 * AllocationDescriptor. When the size of a file is larger than 1GB,
	 * 2 or more AllocationDescriptors should be used. We don't know
	 * whether a singl 8-byte AllocationDescriptor should be written or no
	 * one should be written if the size of a file is 0 byte. - FIXME.
	 *
	 * XXX We get called with buf[2048]. This allows a max. file size of
	 * XXX 234 GB. With more we would cause a buffer overflow.
	 * XXX We need to check whether UDF would allow files > 234 GB.
	 */
	for (; length > 0; length -= chunk) {
		chunk = (length > 0x3ffff800) ? 0x3ffff800 : length;
		set32(&allocation_desc->extent_length, chunk);
		set32(&allocation_desc->extent_position, file_rba);
		file_rba += chunk >> 11;
		allocation_desc++;
	}
	set32(&fe->length_of_allocation_descs,
				(unsigned char *) allocation_desc -
				(unsigned char *) &fe->allocation_desc);
	set_tag(&fe->desc_tag, UDF_TAGID_FILE_ENTRY, rba,
		(unsigned char *) allocation_desc - buf);
}

static unsigned
directory_link_count(struct directory *dpnt)
{
	/*
	 * The link count is equal to 1 (for the parent) plus the
	 * number of subdirectories.
	 */
	unsigned link_count = 1;
	struct directory_entry *de;

	/* count relocated subdirectories */
	for (de = dpnt->jcontents; de; de = de->jnext) {
		if ((de->de_flags &
		    (INHIBIT_JOLIET_ENTRY | RELOCATED_DIRECTORY)) ==
							RELOCATED_DIRECTORY) {
			link_count++;
		}
	}
	/* count ordinary subdirectories */
	for (dpnt = dpnt->subdir; dpnt; dpnt = dpnt->next) {
		if (!(dpnt->dir_flags & INHIBIT_JOLIET_ENTRY)) {
			link_count++;
		}
	}
	return (link_count);
}

static void
write_one_udf_directory(struct directory *dpnt, FILE *outfile)
{
	unsigned size_in_bytes, padded_size_in_bytes;
	struct directory_entry *de;
	unsigned ident_size;
	unsigned base_sector;
	struct directory *parent;
	Uchar buf[SECTOR_SIZE];

	memset(buf, 0, SECTOR_SIZE);
	set_file_entry(
		buf,
		last_extent_written - lba_udf_partition_start,
		last_extent_written+1 - lba_udf_partition_start,
		directory_size(dpnt),
		dpnt->self->isorec.date,
		1,	/* is_directory */
		directory_link_count(dpnt),
		(dpnt == root) ? 0 : dpnt->self->udf_file_entry_sector);
	jtwrite(buf, SECTOR_SIZE, 1, 0, FALSE);
	xfwrite(buf, SECTOR_SIZE, 1, outfile, 0, FALSE);
	last_extent_written++;

	base_sector = last_extent_written - lba_udf_partition_start;

	/* parent directory */
	parent = dpnt->parent;
	if (parent == reloc_dir) {
		parent = dpnt->self->parent_rec->filedir;
	}
	ident_size = set_file_ident_desc(
		buf,
		base_sector,
		0,
		1,
		parent->self->udf_file_entry_sector - lba_udf_partition_start,
		(parent == root) ? 0 : parent->self->udf_file_entry_sector);
	jtwrite(buf, ident_size, 1, 0, FALSE);
	xfwrite(buf, ident_size, 1, outfile, 0, FALSE);
	size_in_bytes = ident_size;

	/* directory contents */
	for (de = dpnt->jcontents; de; de = de->jnext) {
		char *name;
		struct directory_entry *de1;

		if (de->de_flags & INHIBIT_JOLIET_ENTRY)
			continue;

		name = USE_MAC_NAME(de) ? de->hfs_ent->name : de->name;

		/* skip . and .. */
		if (name[0] == '.' && (name[1] == 0 ||
		    (name[1] == '.' && name[2] == 0)))
			continue;

		/* look in RR_MOVED for relocated directories */
		de1 = de;
		if (de->de_flags & RELOCATED_DIRECTORY) {
			for (de1 = reloc_dir->contents; de1; de1 = de1->next) {
				if (de1->parent_rec == de) {
					break;
				}
			}
			if (!de1) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				"Unable to locate relocated directory\n");
#else
				fprintf(stderr,
				"Unable to locate relocated directory\n");
				exit(1);
#endif
			}
		}

		ident_size = set_file_ident_desc(
			buf,
			base_sector + (size_in_bytes / SECTOR_SIZE),
			name,
			!!(de1->isorec.flags[0] & ISO_DIRECTORY),
			de1->udf_file_entry_sector - lba_udf_partition_start,
			de1->udf_file_entry_sector);
		jtwrite(buf, ident_size, 1, 0, FALSE);
		xfwrite(buf, ident_size, 1, outfile, 0, FALSE);
		size_in_bytes += ident_size;
	}

	padded_size_in_bytes = PAD(size_in_bytes, SECTOR_SIZE);
	if (size_in_bytes < padded_size_in_bytes) {
		memset(buf, 0, padded_size_in_bytes - size_in_bytes);
		jtwrite(buf, padded_size_in_bytes - size_in_bytes, 1, 0, FALSE);
		xfwrite(buf, padded_size_in_bytes - size_in_bytes, 1, outfile, 0, FALSE);
	}

	last_extent_written += padded_size_in_bytes / SECTOR_SIZE;
}

static void
write_udf_directories(struct directory *dpnt, FILE *outfile)
{
	if (!(dpnt->dir_flags & INHIBIT_JOLIET_ENTRY)) {
		write_one_udf_directory(dpnt, outfile);
	}
	if (!(dpnt->dir_flags & INHIBIT_JOLIET_ENTRY) || dpnt == reloc_dir) {
		for (dpnt = dpnt->subdir; dpnt; dpnt = dpnt->next) {
			write_udf_directories(dpnt, outfile);
		}
	}
}

static void
write_udf_file_entries(struct directory *dpnt, FILE *outfile)
{
	Uchar buf[SECTOR_SIZE];

	memset(buf, 0, SECTOR_SIZE);

	if (!(dpnt->dir_flags & INHIBIT_JOLIET_ENTRY)) {
		struct directory_entry *de;
		for (de = dpnt->jcontents; de; de = de->jnext) {
			if (!(de->de_flags & RELOCATED_DIRECTORY) &&
			    !(de->isorec.flags[0] & ISO_DIRECTORY)) {

				memset(buf, 0, 512);
				set_file_entry(
					buf,
					(last_extent_written++) - lba_udf_partition_start,
					read_733(de->isorec.extent) - lba_udf_partition_start,
					de->realsize,
					de->isorec.date,
					0,	/* is_directory */
					1,	/* link_count */
					de->udf_file_entry_sector);
				jtwrite(buf, SECTOR_SIZE, 1, 0, FALSE);
				xfwrite(buf, SECTOR_SIZE, 1, outfile, 0, FALSE);
			}
		}
	}
	if (!(dpnt->dir_flags & INHIBIT_JOLIET_ENTRY) || dpnt == reloc_dir) {
		for (dpnt = dpnt->subdir; dpnt; dpnt = dpnt->next) {
			write_udf_file_entries(dpnt, outfile);
		}
	}
}

/****************************/

static int
udf_vol_recognition_area_write(FILE *out)
{
	static const char *identifiers[3] = { "BEA01", "NSR02", "TEA01" };
	int i;
	char buf[SECTOR_SIZE];
	udf_volume_recognition_desc *vsd = (udf_volume_recognition_desc *)buf;

	memset(buf, 0, sizeof (buf));
	/*set8(&vsd->structure_type, 0);*/
	set8(&vsd->structure_version, 1);
	for (i = 0; i < 3; ++i) {
		memcpy(vsd->standard_identifier, identifiers[i], 5);
		jtwrite(buf, SECTOR_SIZE, 1, 0, FALSE);
		xfwrite(buf, SECTOR_SIZE, 1, out, 0, FALSE);
	}
	last_extent_written += 3;
	return (0);
}

static int
udf_main_seq_write(FILE *out)
{
	Uchar buf[SECTOR_SIZE];
	int i;

	/*
	 * volume_set_id needs to be set to a (64-bit) "unique" number.
	 * This will have to do for now.
	 */
	volume_set_id[0] = begun;
	volume_set_id[1] = (unsigned)clock();	/* XXX Maybe non-portable */

	memset(buf, 0, sizeof (buf));
	set_primary_vol_desc(buf, last_extent_written++);
	jtwrite(buf, SECTOR_SIZE, 1, 0, FALSE);
	xfwrite(buf, SECTOR_SIZE, 1, out, 0, FALSE);

	memset(buf, 0, sizeof (buf));
	set_impl_use_vol_desc(buf, last_extent_written++);
	jtwrite(buf, SECTOR_SIZE, 1, 0, FALSE);
	xfwrite(buf, SECTOR_SIZE, 1, out, 0, FALSE);

	memset(buf, 0, sizeof (buf));
	set_partition_desc(buf, last_extent_written++);
	jtwrite(buf, SECTOR_SIZE, 1, 0, FALSE);
	xfwrite(buf, SECTOR_SIZE, 1, out, 0, FALSE);

	memset(buf, 0, sizeof (buf));
	set_logical_vol_desc(buf, last_extent_written++);
	jtwrite(buf, SECTOR_SIZE, 1, 0, FALSE);
	xfwrite(buf, SECTOR_SIZE, 1, out, 0, FALSE);

	memset(buf, 0, sizeof (buf));
	set_unallocated_space_desc(buf, last_extent_written++);
	jtwrite(buf, SECTOR_SIZE, 1, 0, FALSE);
	xfwrite(buf, SECTOR_SIZE, 1, out, 0, FALSE);

	memset(buf, 0, sizeof (buf));
	set_terminating_desc(buf, last_extent_written++);
	jtwrite(buf, SECTOR_SIZE, 1, 0, FALSE);
	xfwrite(buf, SECTOR_SIZE, 1, out, 0, FALSE);

	memset(buf, 0, sizeof (buf));
	for (i = 6; i < UDF_MAIN_SEQ_LENGTH; ++i) {
        jtwrite(buf, SECTOR_SIZE, 1, 0, FALSE);
		xfwrite(buf, SECTOR_SIZE, 1, out, 0, FALSE);
		last_extent_written++;
	}

	return (0);
}

static int
udf_integ_seq_write(FILE *out)
{
	Uchar buf[SECTOR_SIZE*UDF_INTEG_SEQ_LENGTH];

	memset(buf, 0, sizeof (buf));

	set_logical_vol_integrity_desc(buf+0*SECTOR_SIZE,
						last_extent_written++);
	set_terminating_desc(buf+1*SECTOR_SIZE, last_extent_written++);

	jtwrite(buf, SECTOR_SIZE, UDF_INTEG_SEQ_LENGTH, 0, FALSE);
	xfwrite(buf, SECTOR_SIZE, UDF_INTEG_SEQ_LENGTH, out, 0, FALSE);
	return (0);
}

static int
udf_anchor_vol_desc_write(FILE *out)
{
	Uchar buf[SECTOR_SIZE];

	memset(buf, 0, sizeof (buf));
	set_anchor_volume_desc_pointer(buf, last_extent_written++);
	jtwrite(buf, SECTOR_SIZE, 1, 0, FALSE);
	xfwrite(buf, SECTOR_SIZE, 1, out, 0, FALSE);
	return (0);
}

static int
udf_file_set_desc_write(FILE *out)
{
	Uchar buf[SECTOR_SIZE*2];

	memset(buf, 0, sizeof (buf));

	set_file_set_desc(buf+0*SECTOR_SIZE,
			(last_extent_written++) - lba_udf_partition_start);
	set_terminating_desc(buf+1*SECTOR_SIZE,
			(last_extent_written++) - lba_udf_partition_start);

	jtwrite(buf, SECTOR_SIZE, 2, 0, FALSE);
	xfwrite(buf, SECTOR_SIZE, 2, out, 0, FALSE);

	return (0);
}

static int
udf_dirtree_write(FILE *out)
{
	write_udf_directories(root, out);
	return (0);
}

static int
udf_file_entries_write(FILE *out)
{
	write_udf_file_entries(root, out);
	return (0);
}

static int
pad_to(unsigned last_extent_to_write, FILE *out)
{
	char buf[SECTOR_SIZE];
	memset(buf, 0, sizeof (buf));
	while (last_extent_written < last_extent_to_write) {
		jtwrite(buf, SECTOR_SIZE, 1, 0, FALSE);
		xfwrite(buf, SECTOR_SIZE, 1, out, 0, FALSE);
		++last_extent_written;
	}
	return (0);
}

static int
udf_pad_to_sector_32_write(FILE *out)
{
	return (pad_to(session_start+32, out));
}

static int
udf_pad_to_sector_256_write(FILE *out)
{
	return (pad_to(session_start+256, out));
}

static int
udf_padend_avdp_write(FILE *out)
{
	Uchar buf[SECTOR_SIZE];
	unsigned last_extent_to_write = (last_extent_written+31) & ~15;

	if (!use_sparcboot)
		last_extent_to_write = last_extent_written + 150;

	memset(buf, 0, sizeof (buf));
	while (last_extent_written < last_extent_to_write) {
		set_anchor_volume_desc_pointer(buf, last_extent_written++);
		jtwrite(buf, SECTOR_SIZE, 1, 0, FALSE);
		xfwrite(buf, SECTOR_SIZE, 1, out, 0, FALSE);
	}
	return (0);
}


struct output_fragment udf_vol_recognition_area_frag = { NULL, udf_vol_recognition_area_size, NULL, udf_vol_recognition_area_write, "UDF volume recognition area" };
struct output_fragment udf_main_seq_frag = { NULL, udf_main_seq_size, NULL, udf_main_seq_write, "UDF main seq" };
struct output_fragment udf_main_seq_copy_frag = { NULL, udf_main_seq_copy_size, NULL, udf_main_seq_write, "UDF second seq" };
struct output_fragment udf_integ_seq_frag = { NULL, udf_integ_seq_size, NULL, udf_integ_seq_write, "UDF integ seq" };
struct output_fragment udf_anchor_vol_desc_frag = { NULL, oneblock_size, NULL, udf_anchor_vol_desc_write, "UDF Anchor volume" };
struct output_fragment udf_file_set_desc_frag = { NULL, udf_file_set_desc_size, NULL, udf_file_set_desc_write, "UDF file set" };
struct output_fragment udf_dirtree_frag = { NULL, udf_dirtree_size, NULL, udf_dirtree_write, "UDF directory tree" };
struct output_fragment udf_file_entries_frag = { NULL, udf_file_entries_size, NULL, udf_file_entries_write, "UDF file entries" };
struct output_fragment udf_end_anchor_vol_desc_frag = { NULL, udf_end_anchor_vol_desc_size, NULL, udf_anchor_vol_desc_write, "UDF Anchor end volume" };

struct output_fragment udf_pad_to_sector_32_frag = { NULL, udf_pad_to_sector_32_size, NULL, udf_pad_to_sector_32_write, "UDF pad to sector 32" };
struct output_fragment udf_pad_to_sector_256_frag = { NULL, udf_pad_to_sector_256_size, NULL, udf_pad_to_sector_256_write, "UDF pad to sector 256" };
struct output_fragment udf_padend_avdp_frag = { NULL, udf_padend_avdp_size, NULL, udf_padend_avdp_write, "UDF Pad end" };

/*
 * This function assigns weights as follows:
 *
 * /VIDEO_TS/VIDEO_TS.IFO   11199
 * /VIDEO_TS/VIDEO_TS.VOB   11198
 * /VIDEO_TS/VIDEO_TS.BUP   11188
 * /VIDEO_TS/VTS_01_0.IFO   11187
 * /VIDEO_TS/VTS_01_0.VOB   11186
 *            :               :
 * /VIDEO_TS/VTS_01_9.VOB   11177
 * /VIDEO_TS/VTS_01_0.BUP   11176
 *            :               :
 * /VIDEO_TS/VTS_99_0.BUP   10000
 *
 * This ensures that DVD-Video files are laid out properly on the disc.
 * The same thing is done for AUDIO_TS files, except in the 20000 range
 * instead of the 10000 range.
 *
 * Question: what about JACKET_P files?
 *
 * Answer: At least as far as I know :)
 * JACKET_P files are still images (single frame mpeg video .i.e mp2
 * format). The DVD Jacket pictures will be displayed on the TV screen
 * when the player is in a stop/resume mode.
 * The location is not dependent on IFO information and the only must
 * as far as I know is that they are in upper case (both dir and files).
 * This sparce information makes me conclude that they don't need any
 * weight. This obviously needs to be tested.
 */
int
assign_dvd_weights(char *name, struct directory *this_dir, int val)
{
	int ts_number;
	int segment;
	int audio;

	if (name[0] != 'A' && name[0] != 'V')
		return (val);

	if (memcmp(name, "VIDEO_TS", 8) == 0) {
		ts_number = 0;
		audio = 0;
	} else if (memcmp(name, "VTS_", 4) == 0) {
		ts_number = 1;
		audio = 0;
	} else if (memcmp(name, "AUDIO_TS", 8) == 0) {
		ts_number = 0;
		audio = 1;
	} else if (memcmp(name, "ATS_", 4) == 0) {
		ts_number = 1;
		audio = 1;
	} else {
		return (val);
	}

	if (this_dir->parent != root ||
	    strcmp(this_dir->de_name, "VIDEO_TS") != 0)
		return (val);

	if (ts_number == 0) {
		segment = 0;
	} else {
		if (name[4] >= '0' && name[4] <= '9' &&
		    name[5] >= '0' && name[5] <= '9' &&
		    name[6] == '_' &&
		    name[7] >= '0' && name[7] <= '9') {
			ts_number = name[4] * 10 + name[5] - ('0' * 11);
			segment = name[7] - '0';
		} else {
			return (val);
		}
	}

	if (strcmp(name+8, audio ? ".AOB" : ".VOB") == 0) {
		return (audio * 10000 - ts_number * 12 - segment + 11198);
	} else if (strcmp(name+8, ".IFO") == 0) {
		return (audio * 10000 - ts_number * 12 + 11199);
	} else if (strcmp(name+8, ".BUP") == 0) {
		return (audio * 10000 - ts_number * 12 + 11188);
	} else {
		return (val);
	}
}

#endif  /* UDF */
