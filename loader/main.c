static char RCSId[] = "$Id: wine.c,v 1.2 1993/07/04 04:04:21 root Exp root $";
static char Copyright[] = "Copyright  Robert J. Amstadt, 1993";

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "neexe.h"
#include "segmem.h"
#include "prototypes.h"
#include "dlls.h"
#include "wine.h"
#include "windows.h"
#include "wineopts.h"
#include "arch.h"
#include "task.h"
#include "options.h"
#include "stddebug.h"
/* #define DEBUG_DLL /* */
/* #undef  DEBUG_DLL /* */
#include "debug.h"


char *GetDosFileName(char *unixfilename);
extern char WindowsPath[256];

char **Argv;
int Argc;
HINSTANCE hSysRes;
unsigned short WIN_StackSize;
unsigned short WIN_HeapSize;

/**********************************************************************
 *					myerror
 */
void
myerror(const char *s)
{
    if (s == NULL)
	perror("wine");
    else
	fprintf(stderr, "wine: %s\n", s);

    exit(1);
}

#ifndef WINELIB
/**********************************************************************
 *					main
 */
int _WinMain(int argc, char **argv)
{
	char *p, filename[256];
	HANDLE		hTaskMain;
	HINSTANCE	hInstMain;
	struct w_files *wpnt;
#ifdef WINESTAT
	char * cp;
#endif

	Argc = argc - 1;
	Argv = argv + 1;

	if (strchr(Argv[0], '\\') || strchr(Argv[0],'/')) {
            for (p = Argv[0] + strlen(Argv[0]); *p != '\\' && *p !='/'; p--)
		/* NOTHING */;
		
	    strncpy(filename, Argv[0], p - Argv[0]);
	    filename[p - Argv[0]] = '\0';
	    strcat(WindowsPath, ";");
	    if (strchr(filename, '/'))
		    strcat(WindowsPath, GetDosFileName(filename));
	    else
	    	    strcat(WindowsPath, filename);
	}
	
	if ((hInstMain = LoadImage(Argv[0], EXE, 1)) < 32) {
		fprintf(stderr, "wine: can't load %s!.\n", Argv[0]);
		exit(1);
	}
	hTaskMain = CreateNewTask(hInstMain, 0);
	printf("_WinMain // hTaskMain=%04X hInstMain=%04X !\n", hTaskMain, hInstMain);

	GetPrivateProfileString("wine", "SystemResources", "sysres.dll", 
				filename, sizeof(filename), WINE_INI);

	hSysRes = LoadImage(filename, DLL, 0);
	if (hSysRes < 32) {
		fprintf(stderr, "wine: can't load %s!.\n", filename);
		exit(1);
	} else
 	    dprintf_dll(stddeb,"System Resources Loaded // hSysRes='%04X'\n", hSysRes);
	
#ifdef WINESTAT
    cp = strrchr(argv[0], '/');
    if(!cp) cp = argv[0];
	else cp++;
    if(strcmp(cp,"winestat") == 0) {
	    winestat();
	    exit(0);
    };
#endif

    /*
     * Initialize signal handling.
     */
    init_wine_signals();

    wpnt = GetFileInfo(hInstMain);
    if (Options.debug)
	wine_debug(0, NULL);

    if (wpnt->ne)
	StartNEprogram(wpnt);
    else
	StartPEprogram(wpnt);
}

#endif /* #ifndef WINELIB */
