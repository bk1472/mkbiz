#ifndef __PROCESS_H__
#define __PROCESS_H__

#ifdef __cplusplus
extern "C" {
#endif
/*	Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996 Santa Cruz Operation, Inc. All Rights Reserved.	*/
/*	Copyright (c) 1988, 1990 AT&T, Inc. All Rights Reserved.	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF Santa Cruz Operation, Inc.	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)size:common/process.h	1.1"
    /*  process.h contains format strings for printing size information
     *
     *  Different format strings are used for hex, octal and decimal
     *  output.  The appropriate string is chosen by the value of numbase:
     *  pr???[0] for hex, pr???[1] for octal and pr???[2] for decimal.
     */


/* FORMAT STRINGS */

static char *prusect[3] = {
        "%lx",
        "%lo",
        "%ld"
        };

static char *prusum[3] = {
        " = 0x%lx\n",
        " = 0%lo\n",
        " = %ld\n"
        };
extern void		swap_half(void *cp);
extern void		swap_long(void *cp);
extern void		swap_ehdr(Elf32_Ehdr *ehdr);
extern void		swap_phdr(Elf32_Phdr *phdr);
extern void		swap_shdr(Elf32_Shdr *shdr);
extern void		swap_stab(Elf32_Sym *pSym);
extern int		Elf32_gettype(FILE* fp);
#ifdef __cplusplus
}
#endif
#endif/*__PROCESS_H__*/
