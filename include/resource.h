#ifndef __WINE_RESOURCE_H
#define __WINE_RESOURCE_H

#include "dlls.h"

typedef struct resource_s {
	struct resource_s *next;
	HANDLE info_mem;	/* this struct */
	int size_shift;
	struct resource_nameinfo_s nameinfo;
	HANDLE rsc_mem;		/* resource data */
	/* */
	HANDLE instance;	/* resource instance */
	LPSTR name;		/* resource name */
	LPSTR type;		/* resource type */
	int count;		/* lock count */
	int size;		/* resource size */
	int fd;			/* fd */
	int offset;		/* offset */
	struct w_files *wpnt;
} RESOURCE;

#endif /* __WINE_RESOURCE_H */
