/*	Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996 Santa Cruz Operation, Inc. All Rights Reserved.	*/
/*	Copyright (c) 1988, 1990 AT&T, Inc. All Rights Reserved.	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF Santa Cruz Operation, Inc.	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)size:common/main.c	1.15"
/* UNIX HEADERS */
#include	<stdio.h>
#include	<stdlib.h>
#include	<fcntl.h>
#include	<sys/stat.h>
#include	<sys/types.h>
#include	<string.h>
#include	<errno.h>
#include	<ar.h>
#include	<time.h>
#include	<unistd.h>

/* ELF HEADERS */
#include	"libelf.h"
#include	"sys/elf.h"

/* SIZE HEADER */
#include	"defs.h"
#include	"mytypes.h"

/* SIZE HEADER */
#include	"zlib.h"

#ifdef	__GNUC__
#define		PLU_PKG		"GNU C/C++ Compilation System Version "
#endif

#define		MKBIZ_VER	"v3.24"
#define		TEST_MKBIZ	0

#ifndef		O_BINARY
#define		O_BINARY	0
#endif

/* EXTERNAL VARIABLES DEFINED */
static int	errflag   =  0;	/* Global error flag							*/
int			pkg_flag  =  0;	/* Global bootrom+app package flag				*/
int			pkg_vers  =  2; /* Version of flash map package					*/
int			exitcode  =  0;	/* Global exit code								*/
char		*src_name =  0;	/* Source      file name pointer				*/
char		*bin_name =  0;	/* Destination file name: in binary format		*/
char		*sym_name =  0;	/* Destination file name: symbol table file		*/
char		*bi0_name =  0;	/* Destination file name: Even Words(bin)		*/
char		*bi1_name =  0;	/* Destination file name: Odd  Words(bin)		*/
char		*biz_name =  0;	/* Destination file name: in biz    			*/
char		*pkg_name =  0;	/* Source/Dest file name: Boot+App Package		*/
char		inf_str[256]="";/* Information string buffer					*/
int			is_biz	  =  1;	/* Produce output image as zipped format		*/
int			gap_size  =256;	/* Stop processing after gap is bigger than it	*/

ulong_t		elf_start =  0;	/* Start offset of current ELF file				*/
ulong_t		pgm_entry =  0;	/* Entry point of program						*/
ulong_t		mach_type =  0;	/* Machine type									*/
ulong_t		app_entry =  0;	/* Entry point of application program (V1 Only)	*/
ulong_t		va_base   = -1;	/* Virtual address base of program				*/
ulong_t		load_addr = -1;	/* Overrided load address of program			*/
ulong_t		store_adr =  0;	/* Store address for bootrom          (V2 Only)	*/
ulong_t		bss_size  =  0;	/* Size of bss section 				  (V2 Only) */
ulong_t		aux_opts  =  0; /* Additional data for debugging      (V2 Only)	*/
ulong_t		aux_data  =  0; /* Start position of aux data         (V2 Only) */

ulong_t		flash_siz =  0; /* Size of flash memory							*/
ulong_t		flash_swap=  0; /* Swap 16bit word or not 						*/
ulong_t		num_flash =  1;	/* Number of flash memory						*/
ulong_t		n_section =  1; /* Number of sections in flash map				*/
char		ic_str[2][64] = { "_Even", "_Odd"} ;	/* Tag name of IC		*/

ulong_t		verbose   =  0; /* Verbose message */

static char	*tool_name;		/* Command name of this tool					*/

static void	usagerr();
void		error();

extern int	close();
extern char	*strp;
extern Elf32_Phdr	*phdr;
extern Elf32_Shdr	*shdr;

#define OPTSTR  "a:bP:zg:l:o:s:?Vv"	/* option string for getopt			*/
#define OPTMSG  "abPzglos?Vv"			/* option string for help message	*/

/* Version Independant definitions											*/
#define BIZ_MAGIC		0xB12791EE

/* Version 1: Internal definition of size of bootrom for DInnQ82/DInnDZ11	*/
#define V1_C_RST_VEC	0x9fc00000	/*       Cached   Reset Vector for MIPS	*/
#define V1_U_RST_VEC	0xbfc00000	/*       Uncached Reset Vector for MIPS	*/

/* Internal map of flash memory will be defined in xxxx.pkg					*/
/* Version 1: DIxxQ82, DIxxFZ11												*/
/* Version 2: LST3100A, LST4200A, LCSET										*/

/* Internal variable to make header message of biz file						*/
#define	MAX_HDR_SIZE		4096
#define	NUM_HDR_ITEM		  10
#define	HDR_ALIGN			  16


uchar_t    	bin_buff[MAX_BIN_SIZE];		/* Buffer for binary file			*/
ulong_t		sym_hash[MAX_SYM_COUNT];	/* String hash table				*/
long		sym_tabs[MAX_SYM_COUNT][3];	/* Buffer for symbol table			*/
char		sym_buff[MAX_SYM_BUFF];		/* Buffer for symbol list			*/
int			nTxtSyms = 0;				/* Number of function symbols		*/
int			nSbSize  = 0;				/* Size of symbol list				*/


char		versionBuf[MAX_VER_LEN];	/* Buffer space for version			*/
char		tmStampBuf[MAX_VER_LEN];	/* Buffer space for build time		*/

typedef struct
{
	char		*fm_name;		/* Name of section in flash map				*/
	char		fm_mode;		/* Exact size match mode of this section	*/
	char		fm_fill;		/* Fill pattern for remain area				*/
	ulong_t		fm_base;		/* Base address of this section				*/
	ulong_t		fm_bsiz;		/* Size of this binary section				*/
	ulong_t		fm_zbas;		/* Base address of 							*/
	ulong_t		fm_zsiz;		/* Size of this compressed section			*/
	char		*fm_data;		/* Data to be filled						*/
} FlashMap_t;


/*
 *	Default map for single image
 *	Map_V1 for DIxxQ82, DIxx28FZ11
 *	Map_V2 for LST3100A, LST4200A, LCSET
 */
FlashMap_t	defFlashMap[] = { { "Executable", 0, 0xff, 0x000000, MAX_BIN_SIZE, 0, 0, NULL },
							  { "",           0, 0xff, 0x000000, 0x000000,     0, 0, NULL },
							  { "",           0, 0xff, 0x000000, 0x000000,     0, 0, NULL },
							  { "",           0, 0xff, 0x000000, 0x000000,     0, 0, NULL },
							  { "",           0, 0xff, 0x000000, 0x000000,     0, 0, NULL } };

#define	USE_OLD_DATE_INFO	0
/*
 *	Do not change Header Format, it will be used in bootrom.
 *	If changed, it should be applied to bootrom source.
 *	First three lines are for checking download start and end condition.
 */
char	Header[MAX_HDR_SIZE] = { 0, };

char	v1_HeaderFormat[]	=			// For DInnQ82
			"%c%c"						// Tag 0,1(GZIP Magic)
			"HeadSize=0x%08X\r\n"		// Tag   2
			"GzipSize=0x%08X\r\n"		// Tag	 3
			"FileSize=0x%08X\r\n"		// Tag	 4
			"LoadAddr=0x%08x\r\n"		// Tag	 5
			"BuildDate=%24.24s\r\n"		// Tag	 6
			"Builder=%s\r\n"			// Tag	 7
			"Version=%s\r\n"			// Tag	 8
			"Comment=%s\r\n";			// Tag	 9

char	v2_HeaderFormat[]	=			// For Pallas
			"%c%c"						// Tag 0,1(GZIP Magic)
			"HeadSize=0x%08x\n"			// Tag   2
			"GzipSize=0x%08x\n"			// Tag   3
			"FileSize=0x%08x\n"			// Tag   4
			"Gzip_CRC=0x%08x\n"			// Tag   5
			"File_CRC=0x%08x\n"			// Tag   6
			"StartAdr=0x%08x\n"			// Tag   7
			"LoadAddr=0x%08x\n"			// Tag   8
			"StoreLoc=0x%08x\n"			// Tag   9
			"Aux_Data=0x%08x\n"			// Tag  10
			#if	USE_OLD_DATE_INFO
			"BuildDate=%-24.24s\n"		// Tag  11
			#else
			"BuildDate=%s\n"			// Tag  11
			#endif	/* OLD_DATE_INFO */
			"Builder=%s\n"				// Tag  12
			"Version=%s\n"				// Tag  13
			"Comment=%s, by mkbiz %s\n"	// Tag  14
			"\n\n\n\n";

char	*marker1 = (char *)"ffffffff";
char	*marker2 = (char *)"FFFFFFFF";


/*
 * Definition for bizinfo in bootrom
 */

#define	NAME_SZ			64				// Size of named strings
#define	NOTE_SZ		(NAME_SZ*3)			// Size of comment strings
#define	LW_256			64				// Long words in 256 bytes
#define	LW_768			192				// Long words in 768 bytes
#define	LW_1K			256				// Long words in 1K  bytes

/*--------------------------------------------------------------------------*/
/*	DInnQ82:	Information field for BIZ File [ 2K bytes ]					*/
/*--------------------------------------------------------------------------*/
typedef struct {
  long	biz_fsize;				/* 0x000: Size of Zipped bin file			*/
  long	biz_magic;				/* 0x004: Magic number(BIZ_MAGIC)			*/
  long	biz_dstamp;				/* 0x008: Incremental Download Stamp 		*/
  long	biz_protect;			/* 0x00C: Write protection for longer save	*/
  long  biz_gzcrc;				/* 0x010: CRC32 of Zipped bin file			*/
  long  biz_osize;				/* 0x014: Size of unzipped image file		*/
  long  biz_hsize;              /* 0x018: Size of transmission header		*/
  long  biz_laddr;              /* 0x01C: Load address of Unzipped image	*/
  long	biz_pad1[LW_256-8];		/* 0x020: Extenstion for primary header		*/

  /*		Next field must start  0x100									*/
  char	biz_build_date[NAME_SZ];/* 0x100: Date Time Stamp for build date	*/
  char	biz_load_date[NAME_SZ];	/* 0x140: Date Time Stamp for download date	*/
								/*        Not usable: Need RTC support		*/
  char	biz_version[NAME_SZ];	/* 0x180: Version of Executable code		*/
  char	biz_company[NAME_SZ];	/* 0x1C0: Company name of provider			*/
  char	biz_builder[NAME_SZ];	/* 0x200: Name of program builder			*/
  char	biz_comment[NOTE_SZ];	/* 0x240: Description of this program 		*/
  long	biz_pad2[LW_768-8*16];	/* 0x300: Extension for secondary header	*/

  /*		Next field must start  0x400									*/
  long	biz_pad3[LW_1K-3];		/* 0x400: Reserved for Future Extensions	*/
  ulong_t	bReverted;			/* 0x7F4: Revert information flag           */
  ulong_t	vReverted;			/* 0x7F8: Reverted Version Information		*/
  ulong_t	biz_crc32;			/* 0x7FC: Crc32 of Header for Sanity Check	*/
} V1BizInfo_t;

/*--------------------------------------------------------------------------*/
/*	Pallas:		Information field for BIZ File [ 2K bytes ]					*/
/*--------------------------------------------------------------------------*/
typedef struct BizInfo {
  long	biz_magic;				/* 0x000: Magic number(BIZ_MAGIC)			*/
  long  biz_hsize;              /* 0x004: Size of transmission header		*/
  long	biz_gzsiz;				/* 0x008: Size of Zipped bin file			*/
  long  biz_gzcrc;				/* 0x00c: CRC32 of Zipped bin file			*/
  long  biz_bisiz;				/* 0x010: Size of unzipped image file		*/
  long  biz_bicrc;				/* 0x014: CRC32 of Zipped bin file			*/
  long  biz_saddr;              /* 0x018: Start vector of Unzipped image	*/
  long  biz_laddr;              /* 0x01C: Load address of Unzipped image	*/
  long  biz_adata;              /* 0x020: Auxillary data for debugging		*/
  long	biz_dstamp;				/* 0x024: Incremental Download Stamp 		*/
  long	biz_protect;			/* 0x028: Write protection for longer save	*/
  long	biz_mvmem;				/* 0x02c: Memory movement factor 			*/
  long	biz_pad1[LW_256-12];	/* 0x030: Extenstion for primary header		*/

  /*		Next field must start  0x100									*/
  char	biz_build_date[NAME_SZ];/* 0x100: Date Time Stamp for build date	*/
  char	biz_load_date[NAME_SZ];	/* 0x140: Date Time Stamp for download date	*/
								/*        Not usable: Need RTC support		*/
  char	biz_version[NAME_SZ];	/* 0x180: Version of Executable code		*/
  char	biz_company[NAME_SZ];	/* 0x1C0: Company name of provider			*/
  char	biz_builder[NAME_SZ];	/* 0x200: Name of program builder			*/
  char	biz_comment[NOTE_SZ];	/* 0x240: Description of this program 		*/
  long	biz_pad2[LW_768-8*16];	/* 0x300: Extension for secondary header	*/

  /*		Next field must start  0x400									*/
  long	biz_pad3[LW_1K-3];		/* 0x400: Reserved for Future Extensions	*/
  ulong_t	bReverted;			/* 0x7F4: Revert information flag           */
  ulong_t	vReverted;			/* 0x7F8: Reverted Version Information		*/
  ulong_t	biz_crc32;			/* 0x7FC: Crc32 of Header for Sanity Check	*/
} V2BizInfo_t;


/*--------------------------------------------------------------------------*/
/*	calccrc32()																*/
/*		calculate crc32 value for validity check							*/
/*--------------------------------------------------------------------------*/
unsigned long calccrc32(unsigned char *buf, int len)
{
	#define CRC32_POLY 0x04c11db7     /* AUTODIN II, Ethernet, & FDDI */
	static unsigned long	crc32_table[256];
	unsigned char			*p;
	unsigned long			crc, c;
	int						i, j;

	if (!crc32_table[1])						/* if not already done, */
	{
		for (i = 0; i < 256; ++i) {
			for (c = i << 24, j = 8; j > 0; --j)
				c = c & 0x80000000 ? (c << 1) ^ CRC32_POLY : (c << 1);
				crc32_table[i] = c;
		}
	}

	crc = 0xffffffff;       /* preload shift register, per CRC-32 spec */
	for (p = buf; len > 0; ++p, --len)
		crc = (crc << 8) ^ crc32_table[(crc >> 24) ^ *p];
	return(crc);
}

/*--------------------------------------------------------------------------*/
/*	makebiz()																*/
/*  	convert bin file to biz format.										*/
/*--------------------------------------------------------------------------*/
void
makebiz(ulong_t bin_size)
{
	struct stat	sb_biz, sb_src;
	time_t		clock;
	char		*builder, *version, *comment, *cp, *bldDate;
	char		d_buf[64];
	char		t_buf[256];
	int			c, fd;
	int			HeaderSize;
	int			GzipSize;
	int			Gzip_CRC;
	int			File_CRC;
	gzFile		gfp;

	if (mach_type == EM_PPC)
	{
		if (store_adr && (((store_adr + bin_size) & 0x7fffffff) != 0))
		{
			fprintf(stderr, "mkbiz: Invalid Bootrom Image, missing reset vector\n");
			fprintf(stderr, "mkbiz: Store Address = %08x, Size = %08x\n", store_adr, bin_size);
			fprintf(stderr, "mkbiz: End   Address = %08x\n", store_adr + bin_size);
			return;
		}
	}

	memset(Header, 0, MAX_HDR_SIZE);
	#if	USE_OLD_DATE_INFO
	(void) time(&clock);
	bldDate = ctime(&clock);
	if ((cp = strchr(bldDate, '\n')) != NULL) *cp = '\0';
	#else
	stat(src_name, &sb_src);
	strftime(d_buf, 63, "%Y/%m/%d %H:%M:%S", localtime(&sb_src.st_mtime));
	bldDate = d_buf;
	#endif	/* OLD_DATE_INFO */

	sprintf(t_buf, "//%s%s", getenv("HOSTNAME"), getenv("PWD"));
	if (!(builder = (char *) getenv("USER"    )) && !(builder = (char *) getenv("USERNAME")))
												  builder = "[No User   ]";
	if (!(version = (char *) getenv("VERSION" ))) version = (versionBuf[0] ? versionBuf : "[No Version]");
	if (!(comment = (char *) getenv("COMMENT" ))) comment = &t_buf[0];

	if (store_adr && ((pgm_entry - va_base) == 16))
	{
		ulong_t		val1, val2;
		uchar_t		*cp1, *cp2 = &bin_buff[0];

		/* Add CRC32 value to verify code	*/
		val2 = bin_size - 8;
		val1 = calccrc32(cp2+8, val2);

		cp1 = (uchar_t *)&val1; cp2[0]=cp1[3]; cp2[1]=cp1[2]; cp2[2]=cp1[1]; cp2[3]=cp1[0];
		cp1 = (uchar_t *)&val2; cp2[4]=cp1[3]; cp2[5]=cp1[2]; cp2[6]=cp1[1]; cp2[7]=cp1[0];

		fprintf(stderr, "mkbiz: BootRom CRC: 0x%08x\n", val1);
	}

	/*
	 *	Change va_base to overrided load address
	 */
	if (load_addr != -1)
	{
		pgm_entry -= (va_base - load_addr);
		va_base    = load_addr;
	}

	/*
	 *	Calcuate CRC of original file and write header
	 */
	if      (aux_opts & AUX_OPT_DEBUG ) aux_data = (sizeof(int)*(5+3*nTxtSyms) + nSbSize);
	else if (aux_opts & AUX_OPT_BINARY) aux_data = bin_size;
	else                                aux_data = 0;
	File_CRC = calccrc32(&bin_buff[0], bin_size);

	if (pkg_vers == 1)
		sprintf(Header, v1_HeaderFormat,
				0x1f, 0x8b,			// Tag 0,1
				0xffffffff,			// Tag   2 : [HeadSize] will be set later
				0xffffffff,			// Tag   3 : [GzipSize] will be set later
				bin_size,			// Tag   4
				pgm_entry,			// Tag   5
				bldDate,			// Tag   6
				builder,			// Tag   7
				version,			// Tag   8
				comment				// Tag   9
		);
	else
		sprintf(Header, v2_HeaderFormat,
				0x1f, 0x8b,			// Tag 0,1
				0xffffffff,			// Tag   2 : [HeadSize] will be set later
				0xffffffff,			// Tag   3 : [GZipSize] will be set later
				bin_size,			// Tag   4
				0xffffffff,			// Tag   5 : [Gzip_CRC] will be set later
				File_CRC,			// Tag   6
				pgm_entry,			// Tag   7
				va_base,			// Tag   8
				store_adr,			// Tag   9
				aux_data,			// Tag  10
				bldDate,			// Tag  11
				builder,			// Tag  12
				version,			// Tag  13
				comment, MKBIZ_VER	// Tag  14
		);

	/*
	 *	HDR_ALIGN-1 for round up, NUM_HDR_ITEM for CR/LF Conversion.
	 *	To avoid null termination, sprintf & strncpy should be used.
	 */
	HeaderSize = (strlen(Header)+HDR_ALIGN-1)/HDR_ALIGN*HDR_ALIGN;


	/* First, write LG private header				 */
	fd = open(biz_name, O_RDWR|O_BINARY|O_TRUNC|O_CREAT, 0777);
	if (fd < 0)
	{
		fprintf(stderr, "mkbiz: cannot create file(%s)", biz_name);
		return;
	}

	write(fd, Header, HeaderSize);
	close(fd);

	/* Second, write compressed data through gzip	*/
	/*		Must open in append mode				*/
	gfp = gzopen(biz_name, "ab");
	gzwrite(gfp, &bin_buff[0], bin_size);

	if (aux_opts & AUX_OPT_DEBUG)
	{
		int		tmp;

		#if	0
		/*
		 *		2M인 경우, 5K 정도의 크기가 낭비되므로 사용하지 않는다.
		 *		대신 Header에 bss 크기를 같이 보낸다.
		 */
		PRINT("Padding BSS section(0x%x bytes)\n", bss_size);
		memset(&bin_buff, 0, bss_size);
		gzwrite(gfp, &bin_buff[0], bss_size);
		#endif

		PRINT("Appending %d(0x%06x) byte symbol table\n", aux_data, aux_data);

		tmp = BIZ_MAGIC;	swap_long(&tmp); gzwrite(gfp, &tmp, 4);	/* 0 */
		tmp = bss_size;		swap_long(&tmp); gzwrite(gfp, &tmp, 4); /* 1 */
		tmp = aux_data-20;	swap_long(&tmp); gzwrite(gfp, &tmp, 4); /* 2 */
		tmp = nTxtSyms;		swap_long(&tmp); gzwrite(gfp, &tmp, 4); /* 3 */
		tmp = nSbSize;		swap_long(&tmp); gzwrite(gfp, &tmp, 4); /* 4 */

		/* Write index table with addresses and ptr to name */
		for (tmp = 0; tmp < nTxtSyms; tmp++)
		{
			if(sym_tabs[tmp][1] == 0)
			{
				if(tmp <(nTxtSyms-1))
					sym_tabs[tmp][1] = sym_tabs[tmp+1][0];
			}
			else
			{
				sym_tabs[tmp][1] += sym_tabs[tmp][0];
			}
			swap_long(&sym_tabs[tmp][0]);
			swap_long(&sym_tabs[tmp][1]);
			swap_long(&sym_tabs[tmp][2]);
		}
		gzwrite(gfp, &sym_tabs[0][0], 3 * sizeof(int) * nTxtSyms);

		/* Write symbol strings */
		gzwrite(gfp, &sym_buff[0], nSbSize);

		gzclose(gfp);
	}
	else
	{
		gzclose(gfp);
	}

	PRINT("Original size of image : %d(0x%06x)\n", bin_size, bin_size);

	/* Third, append auxillary data to file			*/
	if (aux_opts & AUX_OPT_BINARY)
	{
		PRINT("Appending Original Image at the end of biz file \n");
		fd = open(biz_name, O_RDWR|O_BINARY|O_APPEND, 0777);
		write(fd, &bin_buff[0], aux_data);
		close(fd);
	}

	/* Fourth, modify imcomplete header informations	*/
	fd = open(biz_name, O_RDWR|O_BINARY, 0777);
	fstat(fd, &sb_biz);
	if (aux_opts & AUX_OPT_BINARY)
		GzipSize = sb_biz.st_size - HeaderSize - aux_data;
	else
		GzipSize = sb_biz.st_size - HeaderSize;
	lseek(fd, HeaderSize, SEEK_SET);
	read(fd, &bin_buff[0], GzipSize);
	Gzip_CRC = calccrc32(&bin_buff[0], GzipSize);

	PRINT("Compressed size of biz : %d(0x%06x)\n", GzipSize, GzipSize);

	if (pkg_vers == 1)
	{
		/*	In DInnQ82, Use capital hex-digits		*/
		cp = Header;
		sprintf(t_buf, "%08X", HeaderSize);
		cp = strstr(cp, marker1);
		if (cp == NULL) cp = strstr(Header, marker2);
		strncpy(cp, t_buf, 8);

		sprintf(t_buf, "%08X", GzipSize);
		cp = strstr(cp, marker1);
		if (cp == NULL) cp = strstr(Header, marker2);
		strncpy(cp, t_buf, 8);
	}
	else
	{
		PRINT("Update Header information 0x%08X\n",HeaderSize);
		/*	In Pallas, Use non-capital hex-digits	*/
		cp = Header;
		sprintf(t_buf, "%08x", HeaderSize);
		cp = strstr(cp, marker1);
		if (cp == NULL) cp = strstr(Header, marker2);
		strncpy(cp, t_buf, 8);

		sprintf(t_buf, "%08x", GzipSize);
		cp = strstr(cp, marker1);
		if (cp == NULL) cp = strstr(Header, marker2);
		strncpy(cp, t_buf, 8);

		sprintf(t_buf, "%08x", Gzip_CRC);
		cp = strstr(cp, marker1);
		if (cp == NULL) cp = strstr(Header, marker2);
		strncpy(cp, t_buf, 8);
	}

	PRINT("File Name : %s\n", biz_name);
	PRINT("==== Header Informations ================================\n%s\n", Header+2);
	lseek(fd, 0, SEEK_SET);
	if (write(fd, Header, HeaderSize) <= 0)
	{
		PRINT("Writing Error\n");
		close(fd);
		return;
	}

	close(fd);

}

inline
ulong_t swapUL(ulong_t uv)
{
	ulong_t	val = uv;
	uchar_t	t, *cp = (uchar_t *)&val;

	t = cp[0]; cp[0] = cp[3]; cp[3] = t;
	t = cp[1]; cp[1] = cp[2]; cp[2] = t;

	return(val);
}

/*--------------------------------------------------------------------------*/
/*	v1_mkbizinfo()															*/
/*  	Make bizinfo header for DInnQ82 bootrom								*/
/*--------------------------------------------------------------------------*/
int
v1_mkbizinfo(ulong_t img_size, ulong_t max_size, uchar_t *pSrc, uchar_t *pDst)
{
	V1BizInfo_t		*bp = (V1BizInfo_t *)pSrc;
	struct stat		sb_biz;
	time_t	clock;
	char	*builder, *version, *comment, t_buf[256];
	char	*bldDate, *company, *cp;
	ulong_t	GzipSize, Gzip_CRC;
	int		fd;
	gzFile	gfp;

	PRINT("=== v1_mkbizinfo() called ===============================\n");

	/*	First,	Prepare some build related informations 	*/
	(void) time(&clock);
	company = (char *) "LG electronics Inc,.";
#if	TEST_MKBIZ
	bldDate = (char *) "Sat Apr 13 17:21:58 2002";
	version = (char *) "UK-iDTV  2.1.0.0.1.0.0.0";
	builder = (char *) "freestar";
	comment = (char *) "UKFT-RELEASE";
#else
	bldDate = ctime(&clock);
	sprintf(t_buf, "//%s%s", getenv("HOSTNAME"), getenv("PWD"));
	if (!(builder = (char *) getenv("USER"   ))) builder = "[No User   ]";
	if (!(version = (char *) getenv("VERSION" ))) version = (versionBuf[0] ? versionBuf : "[No Version]");
	if (!(comment = (char *) getenv("COMMENT"))) comment = &t_buf[0];
#endif	/* TEST_BIZ */

	/* Second,	write compressed data through zlib	*/
#if	TEST_MKBIZ
	fd = open("rom32.bin", O_RDWR|O_BINARY|O_CREAT|O_TRUNC, 0777);
	write(fd, pSrc+sizeof(V1BizInfo_t), img_size);
	close(fd);
	system("gzip.exe rom32.bin");
	fd = open("rom32.bin.gz", O_RDWR|O_BINARY, 0777);
#else
	gfp = gzopen(biz_name, "wb");
	gzwrite(gfp, pSrc+sizeof(V1BizInfo_t), img_size);
	gzclose(gfp);
	fd = open(biz_name, O_RDWR|O_BINARY, 0777);
#endif	/* TEST_MKBIZ */

	fstat(fd, &sb_biz);
	GzipSize = sb_biz.st_size;

	PRINT("Original size of image : %d(0x%06x)\n", img_size, img_size);
	PRINT("Compressed size of biz : %d(0x%06x)\n", GzipSize, GzipSize);

	if (GzipSize > max_size)	/* BizSize - BizInfoSize	*/
	{
		(void)fprintf(stderr, "mkbiz: Application image is too big(0x%06x>0x%06x)\n",
							   GzipSize, max_size);
		fflush(stderr);
		close(fd);
		unlink(biz_name);
		return(-1);
	}

	read(fd, pDst+sizeof(V1BizInfo_t), GzipSize);
#if	TEST_MKBIZ
	pDst[sizeof(V1BizInfo_t)+4] = 0xA6;
	pDst[sizeof(V1BizInfo_t)+5] = 0xEA;
	pDst[sizeof(V1BizInfo_t)+6] = 0xB7;
	pDst[sizeof(V1BizInfo_t)+7] = 0x3C;
	pDst[sizeof(V1BizInfo_t)+9] = 0x0B;
#endif	/* TEST_MKBIZ */
	Gzip_CRC = calccrc32((uchar_t *)pDst+sizeof(V1BizInfo_t), GzipSize);
	close(fd);
#if	TEST_MKBIZ
	unlink("rom32.bin.gz");
#else
	unlink(biz_name);
#endif	/* TEST_MKBIZ */

	memset((void *)bp, 0, sizeof(V1BizInfo_t));
	if ((cp = strchr(bldDate, '\n')) != NULL) *cp = '\0';

	bp->biz_fsize	= swapUL(GzipSize);		/* 0x000: size of gzip part		*/
	bp->biz_magic	= swapUL(BIZ_MAGIC);	/* 0x004: biz magic number		*/
	bp->biz_dstamp	= swapUL(1);			/* 0x008: download stamp as 1	*/
	bp->biz_protect	= swapUL(0x00000000);	/* 0x00C:						*/
	bp->biz_gzcrc	= swapUL(Gzip_CRC);		/* 0x010: crc  of gzip part		*/
	bp->biz_osize	= swapUL(img_size);		/* 0x014: size of bin file		*/
	bp->biz_hsize	= swapUL(0x000000d0);	/* 0x018: void on direct write	*/
	bp->biz_laddr	= swapUL(app_entry);	/* 0x01C: start address of app	*/
	sprintf(bp->biz_build_date,	 bldDate);
	sprintf(bp->biz_load_date,	 "Unknown");
	sprintf(bp->biz_version,	 version);
	sprintf(bp->biz_company,	 company);
	sprintf(bp->biz_builder,	 builder);
	sprintf(bp->biz_comment,	 comment);
	bp->bReverted	= swapUL(0);	/* 0x7F4: Revert information flag		*/
	bp->vReverted	= swapUL(0);	/* 0x7F8: Reverted Version Information	*/
	bp->biz_crc32	= swapUL(calccrc32((void *)bp, sizeof(V1BizInfo_t)-4));

	if (pSrc != pDst) memcpy(pDst, pSrc, sizeof(V1BizInfo_t));

	return(0);
}

/*--------------------------------------------------------------------------*/
/*	v2_mkbizinfo()															*/
/*  	Make bizinfo header for LST3100A/LST4200A bootrom					*/
/*--------------------------------------------------------------------------*/
int
v2_mkbizinfo(ulong_t img_size, ulong_t max_size, uchar_t *pSrc, uchar_t *pDst)
{
	V2BizInfo_t		*bp = (V2BizInfo_t *)pSrc;
	struct stat		sb_biz;
	time_t	clock;
	char	*builder, *version, *comment, t_buf[256];
	char	*bldDate, *company, *cp;
	ulong_t	GzipSize, Gzip_CRC, Unzip_CRC;
	int		fd;
	gzFile	gfp;

	PRINT("=== v2_mkbizinfo() called ===============================\n");

	/*	First,	Prepare some build related informations 	*/
	(void) time(&clock);
	company = (char *) "LG electronics Inc,.";
#if	TEST_MKBIZ
	bldDate = (char *) "Wed Jan 07 00:12:58 2004";
	version = (char *) "LST-3100A_MP-1.07";
	builder = (char *) "jackee";
	comment = (char *) "//JackeesNB/cygdrive/d/lgstb/lst3100A/apps/atsc2";
#else
	bldDate = ctime(&clock);
	sprintf(t_buf, "//%s%s", getenv("HOSTNAME"), getenv("PWD"));
	if (!(builder = (char *) getenv("USER"   ))) builder = "[No User   ]";
	if (!(version = (char *) getenv("VERSION" ))) version = (versionBuf[0] ? versionBuf : "[No Version]");
	if (!(comment = (char *) getenv("COMMENT"))) comment = &t_buf[0];
#endif	/* TEST_BIZ */
	Unzip_CRC = calccrc32((uchar_t *)pSrc+sizeof(V2BizInfo_t), img_size);

	/* Second,	write compressed data through zlib	*/
	unlink(biz_name);
	gfp = gzopen(biz_name, "ab");
	gzwrite(gfp, pSrc+sizeof(V2BizInfo_t), img_size);
	gzclose(gfp);

	fd = open(biz_name, O_RDWR|O_BINARY, 0777);
	fstat(fd, &sb_biz);
	GzipSize = sb_biz.st_size;

	PRINT("Original size of image : %d(0x%06x)\n", img_size, img_size);
	PRINT("Compressed size of biz : %d(0x%06x)\n", GzipSize, GzipSize);

	if (GzipSize > max_size)	/* BizSize - BizInfoSize	*/
	{
		(void)fprintf(stderr, "mkbiz: Application image is too big(%x>%x)\n", GzipSize, max_size);
		close(fd);
		unlink(biz_name);
		return(-1);
	}

	read(fd, pDst+sizeof(V2BizInfo_t), GzipSize);
	memset(pDst+sizeof(V2BizInfo_t)+GzipSize, 0xff, max_size - GzipSize);
	Gzip_CRC = calccrc32((uchar_t *)pDst+sizeof(V2BizInfo_t), GzipSize);
	close(fd);
	unlink(biz_name);

	memset((void *)bp, 0, sizeof(V2BizInfo_t));
	if ((cp = strchr(bldDate, '\n')) != NULL) *cp = '\0';

	bp->biz_magic	= swapUL(BIZ_MAGIC);
	bp->biz_hsize	= swapUL(sizeof(V2BizInfo_t));
	bp->biz_gzsiz	= swapUL(GzipSize);
	bp->biz_gzcrc	= swapUL(Gzip_CRC);
	bp->biz_bisiz	= swapUL(img_size);
	bp->biz_bicrc	= swapUL(Unzip_CRC);
	bp->biz_saddr	= swapUL(app_entry);
	bp->biz_laddr	= swapUL(va_base);
	bp->biz_adata	= swapUL(aux_data);
	bp->biz_dstamp	= swapUL(0x0);
	bp->biz_protect	= swapUL(0x0);
	bp->biz_mvmem	= swapUL(0x0);
	sprintf(bp->biz_build_date,	 bldDate);
	sprintf(bp->biz_load_date,	 "Unknown");
	sprintf(bp->biz_version,	 version);
	sprintf(bp->biz_company,	 company);
	sprintf(bp->biz_builder,	 builder);
	sprintf(bp->biz_comment,	 comment);
	bp->bReverted	= swapUL(0);	/* 0x7F4: Revert information flag		*/
	bp->vReverted	= swapUL(0);	/* 0x7F8: Reverted Version Information	*/
	bp->biz_crc32	= swapUL(calccrc32((void *)bp, sizeof(V2BizInfo_t)-4));

	if (pSrc != pDst) memcpy(pDst, pSrc, sizeof(V2BizInfo_t));

	return(0);
}

/*--------------------------------------------------------------------------*/
/*	main(argc, argv)														*/
/* 		parses the command line												*/
/*		opens, processes and closes each object file command line argument	*/
/*																			*/
/*		exits 1 - errors found, 0 - no errors								*/
/*--------------------------------------------------------------------------*/
main(argc, argv)
int		argc;
char	**argv;
{
	/* UNIX FUNCTIONS CALLED */
	extern void		exit( );
	extern int		getopt();

	/* SIZE FUNCTIONS CALLED */
	extern ulong_t	process(FILE *, ulong_t, ulong_t *, ulong_t *, ulong_t *, ulong_t *);

	/* EXTERNAL VARIABLES USED */
	extern int		errflag;
	extern int		oneflag;
//	extern int		optind;
//	extern char		*optarg;

	struct ar_hdr	arhdr;
	FILE			*fp = NULL;
	int				Vflag = 0, i, c, ftype, len;
	int				n_min_files = 1;
	int				n_arg_files = 0;
	int				app_present = 0;
	char			*cp, *dst_name = NULL;
	ulong_t			img_size = 0;

	FlashMap_t		*pFlash = &defFlashMap[0];


	if      (cp = strrchr(argv[0], '/'))
		tool_name = cp + 1;
	else if (cp = strrchr(argv[0], '\\'))
		tool_name = cp + 1;
	else
		tool_name = argv[0];

	while ((c = getopt(argc, argv, OPTSTR)) != EOF) {
		switch (c) {
		case 'g':
			gap_size = atoi(optarg);
			if (gap_size <= 0)
				gap_size = 0x7fffffff;
			break;

		case 'l':
			sscanf(optarg, "%x", &load_addr);
			break;

		case 's':
			sscanf(optarg, "%x", &store_adr);
			break;

		case 'a':
			sscanf(optarg, "%x", &aux_opts);
			if      (aux_opts & AUX_OPT_DEBUG ) aux_opts &= AUX_OPT_DEBUG;
			else if (aux_opts & AUX_OPT_BINARY) aux_opts &= AUX_OPT_BINARY;
			else                                aux_opts  = 0;
			if (aux_opts & AUX_OPT_DATA_SYM) aux_opts |= AUX_OPT_FUNC_SYM;
			break;

		case 'b':
			is_biz = 0;
			aux_opts = 0;
			break;

		case 'z':
			is_biz = 1;
			break;

		case 'P':
			pkg_flag = 1;
			/* Fall Through */
		case 'o':
			dst_name = strdup(optarg);
			break;

		case 'V':
			Vflag++;
#ifdef	__GNUC__
			(void) fprintf(stderr,"%s: %s %s\n", tool_name, PLU_PKG, __VERSION__);
#else
			(void) fprintf(stderr,"%s: Unknown System/Version\n", tool_name);
#endif
			fflush(stderr);
			errflag++;
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
			errflag++;
			break;
		default:
			break;
		}
	}

	/* Set default number of files need				*/
	n_arg_files = argc - optind;

	if (pkg_flag)
	{
		char	line[256];
		char	*pEnvName, *pEnvStr;
		int		cur_sect = -1;
		int		count = 0;
		char	*dp = NULL;

		pkg_name = (char *)malloc(strlen(dst_name)+5);
		sprintf(pkg_name, "%s.pkg",  dst_name);

		if ((fp = fopen(pkg_name, "rb")) == (FILE *) NULL)
		{
			free(pkg_name);
			error(pkg_name, "cannot find pkg script file");
			usagerr();
			exit(1);
		}

		while (fgets(line, 256, fp) != NULL)
		{
			char	*cp1 = strchr(line, '\n');
			char	*cp2;

			if (cp1 != NULL)
				*cp1 = 0;

reCheckHeader:
			if (strncmp(line, "env,", 4) == 0)
			{
				pEnvName = &line[4];
				pEnvStr  = strchr(pEnvName, ',');
				if (pEnvStr) { *pEnvStr = 0; pEnvStr++; }

				if      (pEnvStr == NULL)                      {                                     }
				else if (strcmp(pEnvName, "MAP_VERSION") == 0) { sscanf(pEnvStr, "%d", &pkg_vers);   }
				else if (strcmp(pEnvName, "N_MIN_FILES") == 0) { sscanf(pEnvStr, "%d", &n_min_files);}
				else if (strcmp(pEnvName, "FLASH_SIZE" ) == 0) { sscanf(pEnvStr, "%x", &flash_siz);  }
				else if (strcmp(pEnvName, "FLASH_SWAP" ) == 0) { sscanf(pEnvStr, "%x", &flash_swap); }
				else if (strcmp(pEnvName, "FLASH_COUNT") == 0) { sscanf(pEnvStr, "%d", &num_flash);  }
				else if (strcmp(pEnvName, "IC_EVEN"    ) == 0) { sprintf(&ic_str[0][0], pEnvStr);    }
				else if (strcmp(pEnvName, "IC_ODD"     ) == 0) { sprintf(&ic_str[1][0], pEnvStr);    }
			}
			else if (strncmp(line, "section,", 8) == 0)
			{
				cp1 = &line[8];
				cp2 = strchr(cp1, ',');
				if (cp2)
				{
					cur_sect = n_section - 1;

					*cp2 = 0; cp2++;
					n_section++;
					pFlash[cur_sect].fm_name = strdup(cp1);
					sscanf(cp2, "%d,%x,%x,%x,%x,%x",
										&pFlash[cur_sect].fm_mode, &pFlash[cur_sect].fm_fill,
										&pFlash[cur_sect].fm_base, &pFlash[cur_sect].fm_bsiz,
										&pFlash[cur_sect].fm_zbas, &pFlash[cur_sect].fm_zsiz);

					PRINT("=====%s, %d,%02x,%x,%x,%x,%x\n",
										pFlash[cur_sect].fm_name,
										pFlash[cur_sect].fm_mode, pFlash[cur_sect].fm_fill,
										pFlash[cur_sect].fm_base, pFlash[cur_sect].fm_bsiz,
										pFlash[cur_sect].fm_zbas, pFlash[cur_sect].fm_zsiz);

					dp = pFlash[cur_sect].fm_data = (char *)malloc(pFlash[cur_sect].fm_bsiz);
					memset(dp, pFlash[cur_sect].fm_fill, pFlash[cur_sect].fm_bsiz);
					count = 0;
				}
			}
			else if ( (cur_sect >= 0) && (pFlash[cur_sect].fm_mode >= 3) && (strncmp(line, "data,", 5) == 0))
			{
				int				is_eof = 0;
				int				ch, ch1, ch2;

				while (1)
				{
					if ((cp1 = strchr(line, '\n')) != NULL)
						*cp1 = 0;

					cp1 = &line[5];
					while ( ((ch = *cp1++) != 0) && (count < pFlash[cur_sect].fm_bsiz))
					{
						if (ch == '\\')
						{
							ch = *cp1++;
							if ((ch == 'x') || (ch =='X'))
							{
								ch1 = *cp1;
								if (isxdigit(ch1))
								{
									cp1++;
									ch = ((ch1 <= '9') ? (ch1 - '0') : ((ch1 | 0x20) - 0x61 + 10));
									ch2 = *cp1;
									if (isxdigit(ch2))
									{
										cp1++;
										ch = ch * 16 + ((ch2 <= '9') ? (ch2 - '0') : ((ch2 | 0x20) - 0x61 + 10));
									}
								}
							}
						}
						dp[count++] = ch;
					}

					/* Do Someting */
					if (fgets(line, 256, fp) == NULL)
					{
						is_eof = 1;
						break;
					}

					if (strncmp(line , "data", 5) != 0)
						break;
				}

				if (!is_eof)
					goto reCheckHeader;
			}
		}

		{
			int section;

			for(section = 0; section <= cur_sect; section++)
			{
				if(pFlash[section].fm_mode == 4)
				{
					uint_t crc_value;

					crc_value = (uint_t)crc32(0, (uchar_t *)&pFlash[section].fm_data[4], pFlash[section].fm_bsiz-4);

					PRINT("EEPROM Environment CRC value : 0x%08x\n", crc_value);

					pFlash[section].fm_data[0] = ((crc_value >> 24) & 0xFF);
					pFlash[section].fm_data[1] = ((crc_value >> 16) & 0xFF);
					pFlash[section].fm_data[2] = ((crc_value >>  8) & 0xFF);
					pFlash[section].fm_data[3] = ((crc_value >>  0) & 0xFF);

				}
			}
		}

		if (pkg_vers == 1) gap_size = 8192;

		PRINT("MAP_VERSION = %d\n", pkg_vers);
		PRINT("N_MIN_FILES = %d\n", n_min_files);
		PRINT("FLASH_SIZE  = 0x%06x\n", flash_siz);
		PRINT("FLASH_SWAP  = %d\n", flash_swap);
		PRINT("FLASH_COUNT = %d\n", num_flash);
		PRINT("IC_STRING   = %s, %s\n", ic_str[0], ic_str[1]);

		fclose(fp);
		free(pkg_name);
	}

	if (n_arg_files > n_min_files)
	{
		if (pkg_flag)	app_present = 1;
		else			n_arg_files = n_min_files;
	}

	if (errflag || (n_arg_files < n_min_files))
	{
		usagerr();
		exit(1);
	}

	if (dst_name == NULL)
		dst_name = strdup(argv[optind]);

	/* Rebuild out bin/biz name 					*/
	len = strlen(dst_name);

	if ( (cp = strrchr(dst_name, '.')) != NULL)
	{
		if ( !strcasecmp(cp, ".bin") ||
			 !strcasecmp(cp, ".biz") ||
			 !strcasecmp(cp, ".elf") ||
			 !strcasecmp(cp, ".axf") )
		{
			*cp++ = 0;	// Remove extension.
			len = strlen(dst_name);
		}
	}

	/* Allocate space for filenames and fill it		*/
	bin_name = (char *)malloc(len+64);
	sym_name = (char *)malloc(len+64);
	biz_name = (char *)malloc(len+64);
	bi0_name = (char *)malloc(len+64);
	bi1_name = (char *)malloc(len+64);
	sprintf(sym_name, "%s.sym",  dst_name);
	sprintf(bin_name, "%s.bin",  dst_name);
	sprintf(biz_name, "%s.biz",  dst_name);

	/* Remove previous symbol table file			*/
	if (aux_opts & AUX_OPT_ONLY_AUX)
	{
		unlink(sym_name);
	}

	/* clear binary data buffer						*/
	memset(&bin_buff[0], 0, MAX_BIN_SIZE);

//	for (i = 0; i < n_arg_files; i++)
	for (i = 0; i < n_section; i++)
	{
		ulong_t		base_tag, entry_tag, hdr_siz;
		ulong_t		fm_base, fm_bsiz;
		ulong_t		fm_zbas, fm_zsiz;
		char		fm_mode, fm_fill;
		char		*fm_name;

		fm_name = pFlash[i].fm_name;
		fm_mode = pFlash[i].fm_mode;
		fm_fill = pFlash[i].fm_fill;
		fm_base = pFlash[i].fm_base;
		fm_bsiz = pFlash[i].fm_bsiz;
		fm_zbas = pFlash[i].fm_zbas;
		fm_zsiz = pFlash[i].fm_zsiz;

		if (i >= n_arg_files)
		{
			if (fm_mode == 2)
			{
				memset(&bin_buff[fm_base], fm_fill, fm_bsiz);
				if ((fp = fopen(fm_name, "rb")) != (FILE *) NULL)
				{
					fread(&bin_buff[fm_base], 1, fm_bsiz, fp);
					fclose(fp);
				}
				else
				{
					error(fm_name, "cannot open binary input");
					goto flush_and_exit;
				}
			}
			else if (fm_mode >= 3)
			{
				memcpy(&bin_buff[fm_base], pFlash[i].fm_data, fm_bsiz);
				free(pFlash[i].fm_data);
				pFlash[i].fm_data = NULL;
			}
			continue;
		}

		src_name = argv[i+optind];

		if ((fp = fopen(src_name, "rb")) == (FILE *) NULL)
		{
			error(src_name,"cannot open");
			goto flush_and_exit;
		}

		ftype = Elf32_gettype(fp);

		if (ftype != ELF_K_ELF)
		{
			(void)fprintf(stderr, "%s: %s: File is not ELF format\n",
							tool_name, src_name);
			goto flush_and_exit;
		}

		if (fm_zsiz != 0)
		{
			if      (pkg_vers == 1) hdr_siz = sizeof(V1BizInfo_t);
			else if (pkg_vers == 2) hdr_siz = sizeof(V2BizInfo_t);

			fm_base += hdr_siz;
		}

		PRINT("=========================================================\n");

		img_size  = process(fp, fm_base, &base_tag, &entry_tag, &mach_type, &bss_size);

		if(img_size == 0)
		{
			(void)fprintf(stderr, "%s: %s(%s): Parsing image has been failed\n",
							tool_name, fm_name, src_name);
			goto flush_and_exit;
		}

		if ( (n_arg_files > 1) && (tmStampBuf[0] || versionBuf[0]) )
		{
			char	*typeTag;

			if (fm_zsiz) typeTag = (char *)"S/W ";
			else         typeTag = (char *)"Boot";

			printf("====  %s : Date %s, Version %s\n", typeTag, tmStampBuf, versionBuf);
			sprintf(inf_str, "%s====  %s : Date %s, Version %s\n", inf_str, typeTag, tmStampBuf, versionBuf);
		}

		PRINT("%s File(%s), fm_mode=%d, pkg_vers=%d\n", fm_name, src_name, fm_mode, pkg_vers);
		PRINT("fm_base  = 0x%08x, img_size = 0x%08x, max_size = 0x%08x\n", fm_base, img_size, fm_bsiz);
		PRINT("fm_zbas  = 0x%08x, max_zsiz = 0x%08x\n", fm_zbas,  fm_zsiz);
		PRINT("va_base  = 0x%08x, pgmEntry = 0x%08x\n", base_tag, entry_tag);

		if ((fm_mode == 0) && (img_size > fm_bsiz))
		{
			(void)fprintf(stderr, "%s: %s(%s): Binary image is too big %d > %d\n",
							tool_name, fm_name, src_name, img_size, fm_bsiz);
			goto flush_and_exit;
		}

		if ((fm_mode == 1) && (img_size != fm_bsiz))
		{
			(void)fprintf(stderr, "%s: %s(%s): File size is not match %d != %d\n",
							tool_name, fm_name, src_name, img_size, fm_bsiz);
			(void)fprintf(stderr, "%s: Invalid loader(%s) image, try with option '%s'\n",
							tool_name, src_name, "-g 8192");
			goto flush_and_exit;
		}

		if (img_size < fm_bsiz)
		{
			/* Set remain words of brom flash section as unused	*/
			memset(&bin_buff[fm_base+img_size], fm_fill, fm_bsiz-img_size);
		}

		if (i == 0)
		{
			va_base   = base_tag;
			pgm_entry = entry_tag;
		}

		/*	Version 1 Header의 Bootrom */
		if ((pkg_vers == 1) && (i == 1))
		{
			/*	Set copy dst address and size for bootrom loader	*/
			*(ulong_t *)(&bin_buff[pFlash[i-1].fm_bsiz-8]) = swapUL(entry_tag);
			*(ulong_t *)(&bin_buff[pFlash[i-1].fm_bsiz-4]) = swapUL(pFlash[i].fm_bsiz);

			PRINT("Update copy area for loader(0x%x, 0x%x)\n", entry_tag, pFlash[i].fm_bsiz);
		}

		if (fm_zsiz != 0)
		{
			if (pkg_vers == 1)
			{
				/* Set remain words of appl flash section as unused	*/
				memset(&bin_buff[fm_zbas], fm_fill, flash_siz - fm_zbas);

				/* Adjust offset */
				fm_base  -= hdr_siz;
				fm_zsiz  -= hdr_siz;

				/* Copy applicaton base address	*/
				app_entry = base_tag;

				/* Make biz header */
				PRINT("fm_zbas  = %08x, -------- making V1 header\n", fm_zbas);
				if (v1_mkbizinfo(img_size, fm_zsiz, &bin_buff[fm_base], &bin_buff[fm_zbas]) < 0)
				{
					(void)fprintf(stderr, "%s: %s(%s): Cannot make v1_bizinfo\n",
									tool_name, fm_name, src_name);
					goto flush_and_exit;
				}
			}

			if (pkg_vers == 2)
			{
				/* Adjust offset */
				fm_base  -= hdr_siz;
				fm_zsiz  -= hdr_siz;

				/* Copy applicaton base address	*/
				va_base   = base_tag;
				app_entry = entry_tag;

				/* Fill out BizInfo header for the application image */
				PRINT("fm_zbas  = 0x%08x, -------- making V2 header\n", fm_zbas);
				if (v2_mkbizinfo(img_size, fm_zsiz, &bin_buff[fm_base], &bin_buff[fm_zbas]) < 0)
				{
					(void)fprintf(stderr, "%s: %s(%s): Cannot make v2_bizinfo\n",
									tool_name, fm_name, src_name);
					goto flush_and_exit;
				}
			}
		}

		(void)fclose(fp);
	}

	PRINT("=========================================================\n");

	if (pkg_vers == 1)
	{
		aux_opts = 0;
		store_adr= 0;
		if (pgm_entry == V1_C_RST_VEC) pgm_entry = V1_U_RST_VEC;
	}

	if		(aux_opts & AUX_OPT_ONLY_AUX)
	{
		FILE	*ofp;
		int		tmp;

		/* Produce bin image without add on application	*/
		if ((ofp = fopen(sym_name, "wb")) == NULL)
			ofp = stdout;

		aux_data = (sizeof(int)*(5+3*nTxtSyms) + nSbSize);

		PRINT("Exporting %d(0x%06x) byte symbol table\n", aux_data, aux_data);

		tmp = BIZ_MAGIC;	swap_long(&tmp); fwrite(&tmp, 1, 4, ofp); /* 0 */
		tmp = bss_size;		swap_long(&tmp); fwrite(&tmp, 1, 4, ofp); /* 1 */
		tmp = aux_data-20;	swap_long(&tmp); fwrite(&tmp, 1, 4, ofp); /* 2 */
		tmp = nTxtSyms;		swap_long(&tmp); fwrite(&tmp, 1, 4, ofp); /* 3 */
		tmp = nSbSize;		swap_long(&tmp); fwrite(&tmp, 1, 4, ofp); /* 4 */

		/* Write index table with addresses and ptr to name */
		for (tmp = 0; tmp < nTxtSyms; tmp++)
		{
			if(sym_tabs[tmp][1] == 0)
			{
				if(tmp <(nTxtSyms-1))
					sym_tabs[tmp][1] = sym_tabs[tmp+1][0];
			}
			else
			{
				sym_tabs[tmp][1] += sym_tabs[tmp][0];
			}
			swap_long(&sym_tabs[tmp][0]);
			swap_long(&sym_tabs[tmp][1]);
			swap_long(&sym_tabs[tmp][2]);
		}

		fwrite(&sym_tabs[0][0], sizeof(int), 3 * nTxtSyms, ofp);

		/* Write symbol strings */
		fwrite(&sym_buff[0], 1, nSbSize, ofp);

		if (ofp != stdout)
			fclose(ofp);
	}
	else if	(is_biz && !app_present)
	{
		/* Produce biz image without add on application	*/
		(void)makebiz(img_size);
	}
	else if	(!app_present)
	{
		FILE	*ofp;

		/* Produce bin image without add on application	*/
		if ((ofp = fopen(bin_name, "wb")) == NULL)
			ofp = stdout;

		if (fwrite(&bin_buff[0], 1, img_size, ofp) != img_size)
			fprintf(stderr, "mkbiz: cannot write binary image\n");

		if (ofp != stdout)
			fclose(ofp);
	}
	else
	{
		/* Make BR package with application for Q82/FZ11*/
		FILE	*bofp = NULL, *ofp0 = NULL, *ofp1 = NULL;
		int		i, j;
		char	*e_buf, *o_buf;
		size_t	nbyte = flash_siz;
		char	*version = (char *) getenv("VERSION");

		sprintf(bi0_name, "%s.info", version);
		if ((bofp = fopen(bi0_name, "wb")) == NULL)
		{
			fprintf(stderr, "mkbiz: cannot create file %s\n", bi0_name);
			goto write_failed;
		}
		fprintf(bofp, inf_str);
		fclose(bofp); bofp = NULL;

		if (version != NULL)
		{
			sprintf(bi0_name, "%s%s.bin", version, ic_str[0]);
			sprintf(bi1_name, "%s%s.bin", version, ic_str[1]);
			sprintf(bin_name, "%s.bin",   version);
		}
		else
		{
			sprintf(bi0_name, "%s%s.bin", dst_name, ic_str[0]);
			sprintf(bi1_name, "%s%s.bin", dst_name, ic_str[1]);
		}

		PRINT("Image name is : %s, %s\n", bi0_name, bi1_name);

		if ((bofp = fopen(bin_name, "wb")) == NULL)
		{
			fprintf(stderr, "mkbiz: cannot create file %s\n", bin_name);
			goto write_failed;
		}

		/* Write full binary image for JTAG				*/
		if (fwrite(&bin_buff[0], 1, nbyte, bofp) != nbyte)
		{
			fprintf(stderr, "mkbiz: cannot write bootrom package: full binary\n");
			goto write_failed;
		}

		if (num_flash > 1)
		{
			if ((ofp0 = fopen(bi0_name, "wb")) == NULL)
			{
				fprintf(stderr, "mkbiz: cannot create file %s\n", bi0_name);
				goto write_failed;
			}
			if ((ofp1 = fopen(bi1_name, "wb")) == NULL)
			{
				fprintf(stderr, "mkbiz: cannot create file %s\n", bi1_name);
				goto write_failed;
			}

			e_buf = (char *)malloc((nbyte+1)/2);	/* 2.0M or 1.0M	*/
			o_buf = (char *)malloc((nbyte+1)/2);	/* 2.0M or 1.0M	*/
			for (i = 0, j = 0; i < nbyte; i += 4, j += 2)
			{
				if (flash_swap == 0)
				{
					e_buf[j+0] = bin_buff[i+0];
					e_buf[j+1] = bin_buff[i+1];
					o_buf[j+0] = bin_buff[i+2];
					o_buf[j+1] = bin_buff[i+3];
				}
				else
				{
					e_buf[j+1] = bin_buff[i+0];
					e_buf[j+0] = bin_buff[i+1];
					o_buf[j+1] = bin_buff[i+2];
					o_buf[j+0] = bin_buff[i+3];
				}
			}

			/* Write swaped first  16bit words for IC_EVEN	*/
			if (fwrite(e_buf, 1, nbyte/2, ofp0) != nbyte/2)
			{
				fprintf(stderr, "mkbiz: cannot write bootrom package: even words\n");
				goto write_failed_with_mem;
			}

			/* Write swaped second 16bit words for IC_ODD	*/
			if (fwrite(o_buf, 1, nbyte/2, ofp1) != nbyte/2)
			{
				fprintf(stderr, "mkbiz: cannot write bootrom package: odd  words\n");
				goto write_failed_with_mem;
			}

write_failed_with_mem:
			free(e_buf);
			free(o_buf);
		}
		else
		{
			if ((ofp0 = fopen(bi0_name, "wb")) == NULL)
			{
				fprintf(stderr, "mkbiz: cannot create file %s\n", bi0_name);
				goto write_failed;
			}

			e_buf = (char *)malloc(nbyte);
			for (i = 0, j = 0; i < nbyte; i += 2, j += 2)
			{
				if (flash_swap == 0)
				{
					e_buf[j+0] = bin_buff[i+0];
					e_buf[j+1] = bin_buff[i+1];
				}
				else
				{
					e_buf[j+1] = bin_buff[i+0];
					e_buf[j+0] = bin_buff[i+1];
				}
			}

			/* Write swaped first  16bit words for flash IC	*/
			if (fwrite(e_buf, 1, nbyte, ofp0) != nbyte)
			{
				fprintf(stderr, "mkbiz: cannot write bootrom package\n");
				goto write_failed_with_mem2;
			}

write_failed_with_mem2:
			free(e_buf);
		}

write_failed:
		if (bofp != NULL) fclose(bofp);
		if (ofp0 != NULL) fclose(ofp0);
		if (ofp1 != NULL) fclose(ofp1);
	}

	PRINT("=========================================================\n");

flush_and_exit:
	if (biz_name) free(biz_name);
	if (bin_name) free(bin_name);
	if (sym_name) free(sym_name);
	if (bi0_name) free(bi0_name);
	if (bi1_name) free(bi1_name);

	if (exitcode)	exit(FATAL);
	else			exit(0);

	return 0;
}

/*  error(file, string)
 *
 *  simply prints the error message string
 *  simply returns
 */

void
error(file, string)
char	*file;
char	*string;
{
    (void) fflush(stdout);
   	(void) fprintf(stderr, "mkbiz: %s: %s\n", file, string);
    exitcode++;
    return;
}

static void
usagerr()
{
	(void)fprintf(stderr,"mkbiz: [MaKe BInary Zipped] MIPS/PPC/ARM/AM33," MKBIZ_VER ", %s\n",
					__DATE__ " "__TIME__);
	(void)fprintf(stderr,"Usage: %s [-%s] [[loader] bootrom] application ...\n", tool_name, OPTMSG);

	if (errflag)
	{
		(void) fprintf(stderr,
"   [-?]       Print this help message\n"
"   [-o name]  Set output file name\n"
"   [-g size]  Set gap size between two sections(Default:256)\n"
"              If you set this value as 0, gap limitation is disabled\n"
"   [-l addr]  Override load address for bootrom\n"
"   [-s addr]  Set store location for bootrom\n"
"   [-a opt]   0x0001 : Append symbol table(functions)\n"
"              0x0002 : Append symbol table(functions & variables)\n"
"              0x0004 : Append debug line number table\n"
"              0x0040 : Emit just debug symbols\n"
"              0x1000 : Append original binary image\n"
"   [-b]       Produce output as binary(Default: Compressed Binary)\n\n"

"   [-P name]  Make packaged image of bootrom and application(name.pkg file should be present)\n"
"   [-v]       Verbose display\n"
"Example:\n"
"   1. mkbiz -b ram.elf  <= Make ram.bin from ram.elf\n"
"   2. mkbiz    ram.elf  <= Make ram.biz from ram.elf\n"
"   3. mkbiz -P Q82   loader.elf br32A.elf           <= Q82/FZ11 Make brom.biz(Boot update)\n"
"   4. mkbiz -P Q82   loader.elf br32A.elf rom32.elf <= Q82/FZ11 Make Boot+App package\n"
"   5. mkbiz -P LGSTB loader.elf ram.elf             <= Generic  Make Boot+App package\n"
"Note:\n"
"   Revision History\n"
#if	0
"     V2.00: Add creating MP Image for DI32Q82 products\n"
"     V2.03: Support ARM core cpu for LST3100A\n"
"     V2.04: Add creating MP Image for LST3100A, LST4200A\n"
"            Read compiled time from original image instead of system time\n"
"     V2.05: Modify some minor bugs, Refine code structure\n"
"     V2.06: Fix crash when reading version(bug in 2.04, 2.05)\n"
"     V2.07: Increase maximum binary size to 32M byte\n"
#endif
"     V3.00: Append symbol table at the end of biz as aux_opts\n"
"     V3.01: Increase # of symbols stored in biz file to 16384\n"
"     V3.02: Add string sort hash table to symbol table\n"
"     V3.03: Add object symbol table and debug line info\n"
"     V3.04: Minor bug fix in handling ARM SDT objects\n"
"     V3.05: Extract .debug_line with shrunk, and address search bug fixes\n"
"     V3.06: Fix faulty emission of size of gzip in headers\n"
"     V3.07: Encode line info with full path name\n"
"     V3.08: Script based flash package support, remove -B,-C and add -P\n"
"     V3.09: Insert data file into script file\n"
"     V3.10: Fix CRC generation bug in package image generation\n"
"     V3.11: Add EEPROM environment CRC value calculation logic\n"
"     V3.12: Create information file with package image\n"
"     V3.13: Emit just symbol table for linux application\n"
"     V3.14: Increase maximum number of symbols(for ACAP)\n"
"     V3.15: Add work-around code for building symbol tabel of Big Endian Processor\n"
"     V3.16: Increase maximum number of symbols(for DVR) hash entry bit size expand\n"
"     V3.17: Hash sort table index value change from 0 to 2 (for old version)\n"
"     V3.18: Increase maximum number of symbols(for DVR + ACAP) hash entry bit size expand\n"
"     V3.19: Increase dwarf table buffer and index buffer for huge debug info.\n"
"     V3.20: Add function to support machine type AM33, 34, 35 (Panasonic)..\n"
"     V3.21: Fix stack drash problem of addr2line\n"
"     V3.22: Change fine, dir count array stack variable to heap area\n"
"     V3.23: Support DWARF version 3 and 4\n"
"     V3.24: Source Match for Latest update!\n"
		);

	}
	exitcode++;
}
