#ifndef _MYTYPES_H
#define	_MYTYPES_H

#pragma ident	"@(#)mytypes.h	1.00"
#ifdef	__cplusplus
extern "C" {
#endif

#ifndef TRUE
#define TRUE	(1)
#endif
#ifndef FALSE
#define	FALSE	(0)
#endif

/* POSIX Extensions */

typedef	unsigned char	uchar_t;
typedef	unsigned short	ushort_t;
typedef	unsigned int	uint_t;
typedef	unsigned long	ulong_t;

typedef unsigned char	UINT08;
typedef unsigned short	UINT16;
typedef unsigned long	UINT32;

typedef char			SINT08;
typedef short			SINT16;
typedef long			SINT32;

typedef unsigned char	UCHAR;
typedef unsigned short	USHORT;
typedef unsigned long	ULONG;

#ifdef	__cplusplus
}
#endif

#endif	/* _MYTYPES_H */
