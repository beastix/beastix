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

/* test_BITFIELD_HTOL.c derived from cdrtools aclocal.m4 by Joerg Schilling */
/* Return 1 if bitfields are high-to-low, 0 if bitfields are low-to-high */
int main()
{
	union {
		unsigned char ch;
		struct { unsigned char bf1:4, bf2:4; } bf;
	} u;
	u.ch = 0x12;
	return (u.bf.bf1 == 1);
}
