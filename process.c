/*	Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996 Santa Cruz Operation, Inc. All Rights Reserved.	*/
/*	Copyright (c) 1988, 1990 AT&T, Inc. All Rights Reserved.	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF Santa Cruz Operation, Inc.	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)size:common/process.c	1.18"

/* UNIX HEADER */
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ar.h>

/* ELF HEADERS */
#include	"libelf.h"

/* SIZE HEADERS */
#include	"defs.h"
#include	"mytypes.h"
#include	"process.h"
#include	"dwarf.h"


#define	DEBUG	0

extern int	searchLineInfo(char **ppDebugLine, size_t *pSize, unsigned int srchAddr, char **ppFileName);

int			need_swap = 0;

int
Elf32_gettype(fp)
FILE	*fp;
{
	char	id[16];

	fseek(fp, 0, SEEK_SET);
	if (fread(id, 1, 16, fp) != 16)
		return(-1);
	if (!strncmp(id, ARMAG, SARMAG)) {
		return(ELF_K_AR);

	} else if ((*id == 0x7F) && !strncmp(id+1, "ELF", 3)) {
		return(ELF_K_ELF);
	} else
		return(ELF_K_NONE);

}

Elf32_Ehdr*
Elf32_getehdr(fp)
FILE	*fp;
{
	static Elf32_Ehdr	ehdr;
	int	i;

	fseek(fp, elf_start, SEEK_SET);
	if ((fread(&ehdr, sizeof(ehdr), 1, fp)) != 1)
		return((Elf32_Ehdr *) NULL);

	if (ehdr.e_ident[EI_DATA] == ELFDATA2MSB)
		need_swap = 1;
	else
		need_swap = 0;

	swap_ehdr(&ehdr);
	return(&ehdr);
}

Elf32_Phdr*
Elf32_getphdr( fp, ehdr )
FILE	*fp;
Elf32_Ehdr	*ehdr;
{
	int		i, j;
	Elf32_Phdr	*phdr = NULL;

	if (ehdr->e_phnum == 0)
		return(NULL);

	fseek(fp, elf_start + ehdr->e_phoff, SEEK_SET);
	phdr = (Elf32_Phdr *) malloc(ehdr->e_phentsize * ehdr->e_phnum);
	memset(phdr, 0, ehdr->e_phentsize * ehdr->e_phnum);
	if (fread(phdr, ehdr->e_phentsize, ehdr->e_phnum, fp) != ehdr->e_phnum)
		return((Elf32_Phdr*) NULL);
	for (i=0; i < ehdr->e_phnum; i++)
		swap_phdr(phdr+i);
	return(phdr);
}

Elf32_Shdr*
Elf32_getshdr( fp, ehdr )
FILE	*fp;
Elf32_Ehdr	*ehdr;
{
	int		i;
	Elf32_Shdr	*shdr = NULL;

	if (ehdr->e_shnum == 0)
		return(NULL);

	fseek(fp, elf_start + ehdr->e_shoff, SEEK_SET);
	shdr = (Elf32_Shdr *) malloc(ehdr->e_shentsize * ehdr->e_shnum);
	if (fread(shdr, ehdr->e_shentsize, ehdr->e_shnum, fp) != ehdr->e_shnum)
		return((Elf32_Shdr*) NULL);
	for (i=0; i < ehdr->e_shnum; i++)
		swap_shdr(shdr+i);
	return(shdr);
}

char *
Elf32_getstrtab(fp, ehdr, off, size)
FILE	*fp;
int		off, size;
Elf32_Ehdr	*ehdr;
{
	char	*strp = NULL;

	strp = (char *) malloc(size);
	fseek(fp, elf_start + off, SEEK_SET);
	fread(strp, 1, size, fp);
	return(strp);
}

int
compare_sym(const void *a, const void *b)
{
	int *ia = (int *)a, *ib = (int *)b;

	return( ( (ia[0] > ib[0]) ? 1 : ((ia[0] == ib[0]) ? 0 : -1) ) );
}

int
compare_str(const void *a, const void *b)
{
	int res = strcmp(&sym_buff[sym_tabs[*(ulong_t *)a][2]], &sym_buff[sym_tabs[*(ulong_t *)b][2]]);

	return( ( (res > 0) ? 1 : ((res ==  0) ? 0 : -1) ) );
}

void *
fseek_and_read(FILE *fp, unsigned int offset, unsigned int size)
{
	char *pData = NULL;

	if (size)
	{
		pData = (char *) malloc(size);
		fseek(fp, offset, SEEK_SET);
		fread(pData, 1, size, fp);
	}
	return(pData);
}

/* SIZE FUNCTIONS CALLED */
extern void	error();
static void	process_phdr();

ulong_t
process( fp, bOffset, pVa_base, pLoadAddr, pMachType, pBss_Size)
FILE	*fp;
ulong_t	bOffset;
ulong_t	*pVa_base;
ulong_t	*pLoadAddr;
ulong_t	*pMachType;
ulong_t	*pBss_Size;
{
	/* EXTERNAL VARIABLES USED */
	extern char	*src_name;
	extern char	*bin_name;
	extern int	gap_size;

	/* LOCAL VARIABLES */
	Elf32_Ehdr	*ehdr;
	Elf32_Phdr	*phdr;
	Elf32_Shdr	*shdr;
	Elf32_Shdr	*pSec, *pSymtabHdr = NULL, *pStrtabHdr = NULL, *pRodataHdr = NULL;
	Elf32_Shdr	*pDebugLineHdr = NULL, *pDebugAbbrHdr = NULL;
	Elf32_Shdr	*pDebugInfoHdr = NULL, *pDebugStrHdr = NULL;
	char		*symp, *buf;
	int			i, notfirst=0;
	ulong_t		va_addr = 0xffffffff;
	ulong_t		va_base = 0xffffffff;
	ulong_t		mv_src  = 0;
	ulong_t		mv_dst  = 0;
	ulong_t		sh_flags;
	ulong_t		start_rodata = 0, end_rodata = 0;

/***************************************************************************/
/* If there is a program header and the -f flag requesting section infor-  */
/* mation is not set, then process segments with the process_phdr function.*/
/* Otherwise, process sections.  For the default case, the first number    */
/* shall be the size of all sections that are allocatable, nonwritable and */
/* not of type NOBITS; the second number shall be the size of all sections */
/* that are allocatable, writable, and not of type NOBITS; the third number*/
/* is the size of all sections that are writable and not of type NOBITS.   */
/* If -f is set, print the size of each allocatable section, followed by   */
/* the section name in parentheses.                                        */
/* If -n is set, print the size of all sections, followed by the section   */
/* name in parentheses.                                                    */
/***************************************************************************/

	versionBuf[0] = 0;
	tmStampBuf[0] = 0;

	if ((ehdr = Elf32_getehdr(fp)) == NULL) {
		fprintf(stderr, "cannot read ehdr\n");
		return 0;
	}

#if (DEBUG > 0)
	fprintf(stderr, "Start address = %08x\n", ehdr->e_entry);
	fprintf(stderr, "Machine Type  = %08x\n", ehdr->e_machine);
	fflush(stderr);
#endif	/* DEBUG */

	if ((phdr = Elf32_getphdr(fp, ehdr)) == NULL) {
		fprintf(stderr, "cannot read phdr\n");
		return 0;
	}

	if (pLoadAddr != NULL)
		*pLoadAddr = ehdr->e_entry;

	if (pMachType != NULL)
		*pMachType = ehdr->e_machine;

	process_phdr(phdr, ehdr->e_phnum);

	if (ehdr->e_shnum == 0)
		error(src_name, "no section data");

	buf   = (char *)&bin_buff[bOffset];
	i     = ehdr->e_shstrndx;
	shdr  = Elf32_getshdr(fp, ehdr);
	symp  = Elf32_getstrtab(fp, ehdr, shdr[i].sh_offset, shdr[i].sh_size);

#if (DEBUG > 0)
	fprintf(stderr, "shstrndx = %d\n", i);
#endif

#if (DEBUG > 0)
	fprintf(stderr, "\nSection Header\n");
	fprintf(stderr, "No Name            Type sh_flags     Addr   Offset  Size Aln\n");
	fflush(stderr);
#endif	/* DEBUG */
	for (i = 0, pSec = shdr; i < ehdr->e_shnum; i++, pSec++)
	{
		sh_flags = (pSec->sh_flags & ~SHF_MASKPROC);

		#if (DEBUG > 0)
		fprintf(stderr, "%2d ",  i);
		fprintf(stderr, "%-16s", symp+pSec->sh_name);
		fprintf(stderr, "%4x ",  pSec->sh_type);
		fprintf(stderr, "%8x ",  pSec->sh_flags);
		fprintf(stderr, "%8x ",  pSec->sh_addr);
		fprintf(stderr, "%8x ",  pSec->sh_offset);
		fprintf(stderr, "%5x ",  pSec->sh_size);
		fprintf(stderr, "%3x ",  pSec->sh_addralign);
		fprintf(stderr, "%8x(%06x)\n", va_addr, va_addr-va_base);
		fflush(stderr);
		#endif	/* DEBUG */

		if      (pSec->sh_type == SHT_SYMTAB)
		{
			pSymtabHdr = pSec;
		}
		else if ( (pSec->sh_type == SHT_STRTAB) && (strcmp(symp+pSec->sh_name, ".strtab") == 0) && (pStrtabHdr == NULL) )
		{
			pStrtabHdr = pSec;
		}
		else if (strcmp(symp+pSec->sh_name, ".rodata") == 0)
		{
			pRodataHdr   = pSec;
			start_rodata = pSec->sh_addr,
			end_rodata   = start_rodata + pSec->sh_size;
		}
		else if (strcmp(symp+pSec->sh_name, ".debug_line") == 0)
		{
			pDebugLineHdr = pSec;
		}
		else if (strcmp(symp+pSec->sh_name, ".debug_info") == 0)
		{
			pDebugInfoHdr = pSec;
		}
		else if (strcmp(symp+pSec->sh_name, ".debug_abbrev") == 0)
		{
			pDebugAbbrHdr = pSec;
		}
		else if (strcmp(symp+pSec->sh_name, ".debug_str") == 0)
		{
			pDebugStrHdr = pSec;
		}

		if ((pSec->sh_size == 0) || !(sh_flags & SHF_ALLOC))
			continue;

		if (pSec->sh_type == SHT_NOBITS)	/* This is bss	*/
		{
			if ((aux_opts & AUX_OPT_DEBUG) && (pSec->sh_addr >= va_addr))
			{
				*pBss_Size = pSec->sh_size;
				PRINT("s_bss - e_data is %d, bss_size = %d(0x%x)\n", pSec->sh_addr - va_addr, *pBss_Size, *pBss_Size);
				PRINT("Symbol table will be loaded 0x%x\n", va_addr + *pBss_Size);
			}
			continue;
		}

		if (pSec->sh_type != SHT_PROGBITS)
			continue;

		if (va_base == 0xffffffff)
		{
			va_base = va_addr = pSec->sh_addr;
		}
		else if (va_addr == pSec->sh_addr)
		{
			; 	// Nothing to do
		}
		else if (va_addr > pSec->sh_addr)
		{
			if (mv_src == 0)
			{
				mv_src = pSec->sh_addr;
				mv_dst = va_addr;
			}
		}
		else if ((pSec->sh_size != 0) && ((pSec->sh_addr - va_addr) < gap_size))
		{
			va_addr = pSec->sh_addr;
		}
		else
		{
	//		break;
		}

		if (pSec->sh_type == SHT_PROGBITS)
		{
			PRINT("Adding section %s, type 0x%x, flags=0x%x, size=0x%x\n",
							symp+pSec->sh_name, pSec->sh_type, sh_flags, pSec->sh_size);
			fseek(fp, pSec->sh_offset, SEEK_SET);
			if (fread(buf+va_addr-va_base, 1, pSec->sh_size, fp) != pSec->sh_size)
			{
				fprintf(stderr, "cannot read section\n");
			}

			va_addr += pSec->sh_size;
		}
	}

	/*
	 *	Header for debugging
	 */
	if (pRodataHdr    != NULL) { PRINT("pRodataHdr    = %x\n", pRodataHdr);    }
	if (pDebugLineHdr != NULL) { PRINT("pDebugLineHdr = %x\n", pDebugLineHdr); }
	if (pDebugInfoHdr != NULL) { PRINT("pDebugInfoHdr = %x\n", pDebugInfoHdr); }
	if (pDebugAbbrHdr != NULL) { PRINT("pDebugAbbrHdr = %x\n", pDebugAbbrHdr); }
	if (pDebugStrHdr  != NULL) { PRINT("pDebugStrHdr  = %x\n", pDebugStrHdr);  }

	if ( (buf != NULL) && (mv_src != 0) )
	{
		fprintf(stderr, "Data in [0x%x..0x%x] are moved to 0x%x\n",
						 mv_src, mv_src + (va_addr - mv_dst), mv_dst);
	}

	if (pVa_base != NULL)
		*pVa_base = va_base;

	if ((pSymtabHdr != NULL) && (pStrtabHdr != NULL))
	{
		int				n, nEntries;
		Elf32_Sym		*pSymtab, *pSym;
		char			*pStrtab;
		char			*pDebugLine = NULL;
		unsigned int	tableSize;
		unsigned int	sortTableSize;
		unsigned int	debugLineSize;
		unsigned int	dwarfTableSize;

		nEntries = pSymtabHdr->sh_size / sizeof(Elf32_Sym);
		pSymtab	 = fseek_and_read(fp, pSymtabHdr->sh_offset, pSymtabHdr->sh_size);
		pStrtab	 = fseek_and_read(fp, pStrtabHdr->sh_offset, pStrtabHdr->sh_size);

		for (n = 0, pSym = pSymtab; n < nEntries; n++, pSym++)
		{
			unsigned int	myAddr, mySize, myOffset;
			unsigned int	begAddr, endAddr;
			char			*pContent = NULL;
			char			*pSymName;
			int				sType, symLen;

			swap_stab(pSym);
			pSymName = pStrtab+pSym->st_name;

/*
char VerNumber[] = ToStr(DI28FZ11-2.1.5.0.1.0.0.0);
char BuiltDate[] = __DATE__;
char BuiltTime[] = __TIME__;
*/
			if ( ( ((ELF32_ST_TYPE(pSym->st_info) == STT_FUNC  ) && (aux_opts & AUX_OPT_FUNC_SYM)) ||
				   ((ELF32_ST_TYPE(pSym->st_info) == STT_OBJECT) && (aux_opts & AUX_OPT_DATA_SYM)) ) &&
				 (pSymName[0] != '$') && (pSymName[1] != '$') )
			{
				symLen = strlen(pSymName);
				if (nTxtSyms >= MAX_SYM_COUNT)
				{
					fprintf(stderr, "Number of functions(%d) are exceed %d\n", nTxtSyms, MAX_SYM_COUNT);
				}
				else if ((nSbSize + symLen) >= MAX_SYM_BUFF)
				{
					fprintf(stderr, "Symbol list buffer overflow(case 1), %d >= %d\n", nSbSize, MAX_SYM_BUFF);
				}
				else
				{
					#if (DEBUG > 2)
					fprintf(stderr, "0x%08x 0x%04x %02x %s\n", pSym->st_value, pSym->st_size, pSym->st_info, pSymName);
					#endif
					sym_tabs[nTxtSyms][0] = pSym->st_value;
					sym_tabs[nTxtSyms][1] = pSym->st_size;
					sym_tabs[nTxtSyms][2] = nSbSize;

					nSbSize += snprintf(&sym_buff[nSbSize], MAX_SYM_BUFF, "%s", pSymName) + 1;
					nTxtSyms++;
//printf("%s %s\n", syIm_buff, pSymName);
				}
			}

			if      (strcmp(pSymName, "versionString" ) == 0) sType = 0;
			else if (strcmp(pSymName, "VerNumber"     ) == 0) sType = 0;
			else if (strcmp(pSymName, "buildVers"     ) == 0) sType = 0;
			else if (strcmp(pSymName, "creationDate"  ) == 0) sType = 1;
			else if (strcmp(pSymName, "BuiltDate"     ) == 0) sType = 1;
			else if (strcmp(pSymName, "BuiltTime"     ) == 0) sType = 2;
			else                                              continue;

			pSec = shdr+pSym->st_shndx;
			begAddr = pSec->sh_addr;
			endAddr = pSec->sh_addr + pSec->sh_size;

			myAddr  = pSym->st_value;
			mySize  = pSym->st_size;
			myOffset= pSec->sh_offset+(myAddr-begAddr);

			/* Workarround for SDT Arm Compiler	*/
			if (pSym->st_size == 0) mySize = 512;

			pContent = fseek_and_read(fp, myOffset, mySize + 1);

			myAddr = *(int *)pContent;
			swap_long(&myAddr);
			if ((mySize == 4) && ((myAddr >= start_rodata) && (myAddr <= end_rodata)))
			{
				/*
				 *		Case for CIPDP with vxworks and D2A with uC/OS-II
				 *	Since the variable has been declared like
				 *	  char * buildVers = "blahblah";
				 *	We need to go through the pointer
				 */
				free(pContent);

				mySize  = 512;
				myOffset= pRodataHdr->sh_offset + (myAddr - pRodataHdr->sh_addr);

				pContent = fseek_and_read(fp, myOffset, mySize);
			}

			#if (DEBUG > 0)
			fprintf(stderr, "++++ Symbol %s is %s, with 0x%x(%d)\n", pSymName, pContent, myOffset, mySize);
			fflush(stderr);
			#endif

			switch (sType)
			{
				case 0: snprintf(versionBuf, 128, pContent); break;
				case 1: snprintf(tmStampBuf, 128, pContent); break;
				case 2: snprintf(tmStampBuf, 128, "%s %s", tmStampBuf, pContent); break;
			}

			free(pContent);
		}

		free(pSymtab);
		free(pStrtab);

		#if (DEBUG > 0)
		fprintf(stderr, "++++ %d symbol has been imported(%d bytes)\n", nTxtSyms, nSbSize);
		fflush(stderr);
		#endif

		/*
		 *	Now build tables to store debug informations
		 */
		qsort(&sym_tabs[0][0], nTxtSyms, 3 * sizeof(int), compare_sym);

		#if	(DEBUG > 1)
		fprintf(stderr, "++++ Dumping text symbols\n");
		for (n = 0; n < nTxtSyms; n++)
		{
			long	*pTab = &sym_tabs[n][0];

			fprintf(stderr, "%08x %08x T %s\n", pTab[0], pTab[1], &sym_buff[pTab[2]]);
		}
		fflush(stderr);
		#endif

		#if	(DEBUG > 1)
		fprintf(stderr, "++++ Sorting symbol table by name\n");
		fflush(stderr);
		#endif

		for (n = 0; n < nTxtSyms; n++)
			sym_hash[n] = n;

		qsort(&sym_hash[0], nTxtSyms, sizeof(long), compare_str);

		#if	(DEBUG > 1)
		fprintf(stderr, "++++ Dumping other symbols\n");
		for (n = 0; n < nTxtSyms; n++)
		{
			ulong_t v = sym_hash[n];
			long  *pTab = &sym_tabs[v][0];

			fprintf(stderr, "%d:: symbol %04x %04x %04x %04x %s\n", n, v,
							pTab[0], pTab[1], pTab[2], &sym_buff[pTab[2]]);
		}
		fprintf(stderr, "done\n");
		fflush(stderr);
		#endif

		if ((aux_opts & AUX_OPT_LINE_NUM) && pDebugLineHdr)
		{
			extern char *pDebugStr, *pDebugInfo, *pDebugAbbr;
			extern size_t debugStrSize, debugInfoSize, debugAbbrSize;

			/* Read Dwarf2 debug section data */
			if (pDebugLineHdr)
			{
				debugLineSize = pDebugLineHdr->sh_size;
				pDebugLine = fseek_and_read(fp, pDebugLineHdr->sh_offset, debugLineSize);
			}
			if (pDebugInfoHdr)
			{
				debugInfoSize = pDebugInfoHdr->sh_size;
				pDebugInfo = fseek_and_read(fp, pDebugInfoHdr->sh_offset, debugInfoSize);
			}
			if (pDebugAbbrHdr)
			{
				debugAbbrSize = pDebugAbbrHdr->sh_size;
				pDebugAbbr = fseek_and_read(fp, pDebugAbbrHdr->sh_offset, debugAbbrSize);
			}
			if (pDebugStrHdr)
			{
				debugStrSize  = pDebugStrHdr->sh_size;
				pDebugStr  = fseek_and_read(fp, pDebugStrHdr->sh_offset,  debugStrSize);
			}

			#if	(DEBUG > 1)
			fprintf(stderr, "++++ Parsing all comp units\n");
			fflush(stderr);
			#endif

			/* Build debug_info table */
			parse_all_comp_units();

			/* Build debug_line table */
			searchLineInfo(&pDebugLine, &debugLineSize, (unsigned int)-1, NULL);
			//testAddr2Line(ehdr->e_entry);

			/* Release debug_info table */
			release_all_comp_units();

			free(pDebugInfo);
			free(pDebugAbbr);
			free(pDebugStr);

			PRINT("%5d dwarf  packets use %d+%d(=%d) bytes\n",
					nDwarfLst, nDwarfLst*8, debugLineSize, nDwarfLst*8 + debugLineSize);
		}

		/* hash table 이 32bit 가 되면서 size가 2에서 4로 바뀜 */
		sortTableSize	= 4 + sizeof(long) * ((nTxtSyms + 1) & ~1);
		dwarfTableSize	= (nDwarfLst ? (12 + 8 * nDwarfLst + debugLineSize) : 0);
		tableSize = sortTableSize + dwarfTableSize;

		for (n = 0; n < nTxtSyms; n++)
			swap_long(&sym_hash[n]);

		/* Check symbol buffer overflow */
		if ((nSbSize + tableSize) >= MAX_SYM_BUFF)
		{
			fprintf(stderr, "Symbol list buffer overflow(case 2), %d+%d >= %d\n", tableSize, nSbSize, MAX_SYM_BUFF);
			fflush(stderr);
			return(0);
		}

		/* Move original symbol strings */
		PRINT("Moving symbols from %p..%p to %p..%p\n", 0, nSbSize, tableSize, tableSize + nSbSize);
		memmove(&sym_buff[tableSize], &sym_buff[0], nSbSize);

		/* Insert alphabetic sort table */
		/* 만약 16 bit 이면 n 값을 0으로 하지만 32bit 이면 n 값을 2로 함 */
		n = 2;	swap_long(&n);	/* Mark this is sorted hash table */
		memcpy (&sym_buff[0], &n,           4);
		memcpy (&sym_buff[4], &sym_hash[0], sortTableSize - 4);

		/* Insert dwarf index table & encoded string */
		if (nDwarfLst)
		{
			char *pSymBuff = &sym_buff[sortTableSize];

			for (n = 0; n < 2 * nDwarfLst; n++)
				swap_long(&dwarfLst[n]);

			n = 1;				swap_long(&n);	/* Mark this is dwarf table */
			memcpy (pSymBuff, &n, 4);						pSymBuff += 4;
			n = nDwarfLst;		swap_long(&n);
			memcpy (pSymBuff, &n, 4);						pSymBuff += 4;
			n = debugLineSize;	swap_long(&n);
			memcpy (pSymBuff, &n, 4);						pSymBuff += 4;
			memcpy (pSymBuff, dwarfLst,   8 * nDwarfLst);	pSymBuff += 8 * nDwarfLst;
			memcpy (pSymBuff, pDebugLine, debugLineSize);	pSymBuff += debugLineSize;
		}


		/* Advance size of symbol buffer size */
		nSbSize += tableSize;

		if (pDebugLine)
			free(pDebugLine);

		#if	(DEBUG > 1)
		for (n = 0; n < nTxtSyms; n++)
		{
			ulong_t v = sym_hash[n];
			long *pTab;

			swap_long(&v);
			pTab = &sym_tabs[v][0];
			fprintf(stderr, "symbol %04x %04x %04x %04x %s\n", v,
							pTab[0], pTab[1], pTab[2], &sym_buff[pTab[2] + tableSize]);
		}
		#endif

		PRINT("%5d object symbols use %d+%d(=%d) bytes\n", nTxtSyms, nTxtSyms*12, nSbSize, nTxtSyms*12 + nSbSize);
	}

	free(phdr);
	free(shdr);
	free(symp);

	return (va_addr - va_base);
}



/***************************************************************************/
/* If there is a program exection header, process segments. In the default */
/* case, the first number is the file size of all nonwritable segments     */
/* of type PT_LOAD; the second number is the file size of all writable     */
/* segments whose type is PT_LOAD; the third number is the memory size     */
/* minus the file size of all writable segments of type PT_LOAD.           */
/* If the -F flag is set, size will print the memory size of each loadable */
/* segment, followed by its permission flags.                              */
/* If -n is set, size will print the memory size of all loadable segments  */
/* and the file size of all non-loadable segments, followed by their       */
/* permission flags.                                                       */
/***************************************************************************/

static void
process_phdr(phdr,num)
Elf32_Phdr	*phdr;
Elf32_Half	num;

{
	Elf32_Phdr	*p = phdr;
	int			i;

#if (DEBUG > 0)
	fprintf(stderr, "Program Header\n");
	fprintf(stderr, "  Type Offset  vAddr  pAddr FileSz  MemSz flag    Align\n");
	fflush(stderr);
	for (i = 0; i < (int)num; i++, ++p)
	{
		if (p->p_type == PT_LOAD) fprintf(stderr, "  LOAD ");
		else					  fprintf(stderr, "       ");
		fprintf(stderr, "%6x ", p->p_offset);
		fprintf(stderr, "%6x ", p->p_vaddr);
		fprintf(stderr, "%6x ", p->p_paddr);
		fprintf(stderr, "%6x ", p->p_filesz);
		fprintf(stderr, "%6x ", p->p_memsz);
		fprintf(stderr, "%4x ", p->p_flags);
		fprintf(stderr, "%8x\n", p->p_align);
	} /* end for statement */
	fflush(stderr);
#endif	/* DEBUG */
}

void swap_half(void *vp)
{
	char	t, *cp = (char *) vp;
	if (!need_swap)
		return;
	t = cp[0]; cp[0] = cp[1]; cp[1] = t;
}

void swap_long(void *vp)
{
	char	t, *cp = (char *) vp;
	if (!need_swap)
		return;
	t = cp[0]; cp[0] = cp[3]; cp[3] = t;
	t = cp[1]; cp[1] = cp[2]; cp[2] = t;
}

void swap_ehdr(Elf32_Ehdr *ehdr)
{
	swap_half((char *) &ehdr->e_type);
	swap_half((char *) &ehdr->e_machine);
	swap_long((char *) &ehdr->e_version);
	swap_long((char *) &ehdr->e_entry);
	swap_long((char *) &ehdr->e_phoff);
	swap_long((char *) &ehdr->e_shoff);
	swap_long((char *) &ehdr->e_flags);
	swap_half((char *) &ehdr->e_ehsize);
	swap_half((char *) &ehdr->e_phentsize);
	swap_half((char *) &ehdr->e_phnum);
	swap_half((char *) &ehdr->e_shentsize);
	swap_half((char *) &ehdr->e_shnum);
	swap_half((char *) &ehdr->e_shstrndx);
}

void swap_phdr(Elf32_Phdr *phdr)
{
	swap_long((char *) &phdr->p_type);
	swap_long((char *) &phdr->p_offset);
	swap_long((char *) &phdr->p_vaddr);
	swap_long((char *) &phdr->p_paddr);
	swap_long((char *) &phdr->p_filesz);
	swap_long((char *) &phdr->p_memsz);
	swap_long((char *) &phdr->p_flags);
}

void swap_shdr(Elf32_Shdr *shdr)
{
	swap_long((char *) &shdr->sh_name);
	swap_long((char *) &shdr->sh_type);
	swap_long((char *) &shdr->sh_flags);
	swap_long((char *) &shdr->sh_addr);
	swap_long((char *) &shdr->sh_offset);
	swap_long((char *) &shdr->sh_size);
	swap_long((char *) &shdr->sh_link);
	swap_long((char *) &shdr->sh_info);
	swap_long((char *) &shdr->sh_addralign);
	swap_long((char *) &shdr->sh_entsize);
}

void swap_stab(Elf32_Sym *pSym)
{
	swap_long((char *)&pSym->st_name);
	swap_long((char *)&pSym->st_value);
	swap_long((char *)&pSym->st_size);
	swap_half((char *)&pSym->st_shndx);
}
