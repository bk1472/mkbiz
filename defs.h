/*	Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996 Santa Cruz Operation, Inc. All Rights Reserved.	*/
/*	Copyright (c) 1988, 1990 AT&T, Inc. All Rights Reserved.	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF Santa Cruz Operation, Inc.	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)size:common/defs.h	1.1"
#define FATAL   1
#define HEX     0
#define OCTAL   1
#define DECIMAL 2

#define TROUBLE -1L
#define MAXLEN  512

#define	MAX_BIN_SIZE	0x03000000	/* 48M byte	for binary image			*/
#define MAX_SYM_BUFF	0x00800000	/*  8M byte for symbol strings & dwarf	*/
#define MAX_SYM_COUNT	   1048576	/* Maximum number of symbols			*/
#define MAX_DWARF_PKT		 32768	/* No dwarf packets for debug			*/

/* define for adding aux data 												*/
#define	AUX_OPT_NONE		0x0000	/* Save no auxiliary data				*/
#define AUX_OPT_FUNC_SYM	0x0001	/* Attach function symbol table			*/
#define AUX_OPT_DATA_SYM	0x0002	/* Attach object symbol table			*/
#define AUX_OPT_LINE_NUM	0x0004	/* Attach object symbols and line no	*/
#define AUX_OPT_ONLY_AUX	0x0040	/* Export only aux data					*/
#define AUX_OPT_VERBOSE		0x0080	/* Verbose display decoding dwarf		*/
#define	AUX_OPT_BINARY		0x1000	/* Attach original binary image			*/
#define AUX_OPT_DEBUG		0x00c7	/* FUNC | DATA | LINE_NO | VERBOS		*/

#define	MAX_VER_LEN            128	/* Maximum length of version field		*/
#define	PRINT(x...)		if (verbose) { printf(x); fflush(stdout); usleep(2000);}

extern unsigned int		dwarfLst[2*MAX_DWARF_PKT];	/* Pointer to dwarf pkts*/
extern unsigned int		nDwarfLst;					/* Number of dwarf pkts	*/

extern unsigned long	elf_start;
extern unsigned long	verbose;
extern unsigned long	aux_opts;
extern unsigned char	bin_buff[MAX_BIN_SIZE];
extern unsigned long	sym_hash[MAX_SYM_COUNT];
extern long		sym_tabs[MAX_SYM_COUNT][3];
extern char     sym_buff[MAX_SYM_BUFF];
extern int		nTxtSyms;
extern int		nSbSize;

extern char		versionBuf[MAX_VER_LEN];/* Buffer space for version			*/
extern char		tmStampBuf[MAX_VER_LEN];/* Buffer space for build time		*/

