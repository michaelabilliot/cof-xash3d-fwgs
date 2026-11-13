// pe_win32_guess.c -- reads the export names of a game dll from its PE image
// (the stdcall helpers are those of the PEFILE.C sample, "The Portable Executable
// File Format from Top to Bottom")

#include "quakedef.h"
#include "mem.h"
#include "zone.h"

void	FS_GetLocalCopy( char *pszFileName );

typedef struct functiontable_s
{
	uint32		pFunction;
	char		*pFunctionName;
} functiontable_t;

typedef struct extensiondll_s
{
	void			*lDLLHandle;
	functiontable_t	*functionTable;
	int				functionCount;
} extensiondll_t;

#define SIZE_OF_NT_SIGNATURE	sizeof (DWORD)

/* global macros to define header offsets into file */
/* offset to PE file signature */
#define NTSIGNATURE(a) ((LPVOID)((BYTE *)a		     +	\
			((PIMAGE_DOS_HEADER)a)->e_lfanew))

/* DOS header identifies the NT PEFile signature dword
   the PEFILE header exists just after that dword */
#define PEFHDROFFSET(a) ((LPVOID)((BYTE *)a		     +	\
			 ((PIMAGE_DOS_HEADER)a)->e_lfanew    +	\
			 SIZE_OF_NT_SIGNATURE))

/* PE optional header is immediately after PEFile header */
#define OPTHDROFFSET(a) ((LPVOID)((BYTE *)a		     +	\
			 ((PIMAGE_DOS_HEADER)a)->e_lfanew    +	\
			 SIZE_OF_NT_SIGNATURE		     +	\
			 sizeof (IMAGE_FILE_HEADER)))

/* section headers are immediately after PE optional header */
#define SECHDROFFSET(a) ((LPVOID)((BYTE *)a		     +	\
			 ((PIMAGE_DOS_HEADER)a)->e_lfanew    +	\
			 SIZE_OF_NT_SIGNATURE		     +	\
			 sizeof (IMAGE_FILE_HEADER)	     +	\
			 sizeof (IMAGE_OPTIONAL_HEADER)))


/* return file signature */
DWORD WINAPI ImageFileType (
    LPVOID    lpFile)
{
    /* dos file signature comes first */
    if (*(USHORT *)lpFile == IMAGE_DOS_SIGNATURE)
	{
	/* determine location of PE File header from dos header */
	if (LOWORD (*(DWORD *)NTSIGNATURE (lpFile)) == IMAGE_OS2_SIGNATURE ||
	    LOWORD (*(DWORD *)NTSIGNATURE (lpFile)) == IMAGE_OS2_SIGNATURE_LE)
	    return (DWORD)LOWORD(*(DWORD *)NTSIGNATURE (lpFile));

	else if (*(DWORD *)NTSIGNATURE (lpFile) == IMAGE_NT_SIGNATURE)
	    return IMAGE_NT_SIGNATURE;

	else
	    return IMAGE_DOS_SIGNATURE;
	}

    else
	/* unknown file type */
	return 0;
}


/* return the total number of sections in the module */
int WINAPI NumOfSections (
    LPVOID    lpFile)
{
    /* number os sections is indicated in file header */
    return (int)((PIMAGE_FILE_HEADER)PEFHDROFFSET (lpFile))->NumberOfSections;
}


/* retrieve the offset of the image directory */
LPVOID WINAPI ImageDirectoryOffset (
	LPVOID	  lpFile,
	DWORD	  dwIMAGE_DIRECTORY)
{
    PIMAGE_OPTIONAL_HEADER   poh = (PIMAGE_OPTIONAL_HEADER)OPTHDROFFSET (lpFile);
    PIMAGE_SECTION_HEADER    psh = (PIMAGE_SECTION_HEADER)SECHDROFFSET (lpFile);
    int 		     nSections = NumOfSections (lpFile);
    int 		     i = 0;
    LPVOID		     VAImageDir;

    /* must be 0 thru (NumberOfRvaAndSizes-1) */
    if (dwIMAGE_DIRECTORY >= poh->NumberOfRvaAndSizes)
	return NULL;

    /* locate specific image directory's relative virtual address */
    VAImageDir = (LPVOID)poh->DataDirectory[dwIMAGE_DIRECTORY].VirtualAddress;

    /* locate section containing image directory */
    while (i++<nSections)
	{
	if (psh->VirtualAddress <= (DWORD)VAImageDir &&
	    psh->VirtualAddress + psh->SizeOfRawData > (DWORD)VAImageDir)
	    break;
	psh++;
	}

    if (i > nSections)
	return NULL;

    /* return image import directory offset */
    return (LPVOID)(((int)lpFile + (int)VAImageDir - psh->VirtualAddress) +
				   (int)psh->PointerToRawData);
}


/* function retrieve names of all the sections in the file */
int WINAPI GetSectionNames (
    LPVOID    lpFile,
    HANDLE    hHeap,
    char      **pszSections)
{
    int 		     nSections = NumOfSections (lpFile);
    int 		     i, nCnt = 0;
    PIMAGE_SECTION_HEADER    psh;
    char		     *ps;


    if (ImageFileType (lpFile) != IMAGE_NT_SIGNATURE ||
	(psh = (PIMAGE_SECTION_HEADER)SECHDROFFSET (lpFile)) == NULL)
	return 0;

    /* count the number of chars used in the section names */
    for (i=0; i<nSections; i++)
	nCnt += strlen (psh[i].Name) + 1;

    /* allocate space for all section names from heap */
    ps = *pszSections = (char *)HeapAlloc (hHeap, HEAP_ZERO_MEMORY, nCnt);


    for (i=0; i<nSections; i++)
	{
	strcpy (ps, psh[i].Name);
	ps += strlen (psh[i].Name) + 1;
	}

    return nCnt;
}


/* function gets the function header for a section identified by name */
BOOL	WINAPI GetSectionHdrByName (
    LPVOID		     lpFile,
    IMAGE_SECTION_HEADER     *sh,
    char		     *szSection)
{
    PIMAGE_SECTION_HEADER    psh;
    int 		     nSections = NumOfSections (lpFile);
    int 		     i;


    if ((psh = (PIMAGE_SECTION_HEADER)SECHDROFFSET (lpFile)) != NULL)
	{
	/* find the section by name */
	for (i=0; i<nSections; i++)
	    {
	    if (!strcmp (psh->Name, szSection))
		{
		/* copy data to header */
		CopyMemory ((LPVOID)sh,
			    (LPVOID)psh,
			    sizeof (IMAGE_SECTION_HEADER));
		return TRUE;
		}
	    else
		psh++;
	    }
	}

    return FALSE;
}


BOOL	WINAPI IsDebugInfoStripped (
    LPVOID    lpFile)
{
    PIMAGE_FILE_HEADER	 pfh;

    pfh = (PIMAGE_FILE_HEADER)PEFHDROFFSET (lpFile);

    return (pfh->Characteristics & IMAGE_FILE_DEBUG_STRIPPED);
}


/* the names of the exported functions, located through .rdata */
char *GetExportNamesPtr_guess( LPVOID lpFile )
{
	IMAGE_SECTION_HEADER		sh;
	PIMAGE_EXPORT_DIRECTORY		ped;
	char						*pNames;

	/* get section header and pointer to data directory for the export section */
	if ( ( ped = (PIMAGE_EXPORT_DIRECTORY)ImageDirectoryOffset( lpFile, IMAGE_DIRECTORY_ENTRY_EXPORT ) ) == NULL )
		return NULL;
	if ( !GetSectionHdrByName( lpFile, &sh, ".rdata" ) )
		return NULL;

	/* determine the offset of the export function names */
	pNames = (char *)( *(int *)( (int)ped->AddressOfNames -
								 (int)sh.VirtualAddress +
								 (int)sh.PointerToRawData +
								 (int)lpFile ) -
					   (int)sh.VirtualAddress +
					   (int)sh.PointerToRawData +
					   (int)lpFile );
	return pNames;
}

/* how many bytes the export function names take */
int GetExportNamesSize_guess( LPVOID lpFile )
{
	PIMAGE_EXPORT_DIRECTORY		ped;
	char						*pNames, *pCnt;
	int							i, nCnt;

	if ( ( ped = (PIMAGE_EXPORT_DIRECTORY)ImageDirectoryOffset( lpFile, IMAGE_DIRECTORY_ENTRY_EXPORT ) ) == NULL )
		return 0;
	if ( ( pNames = GetExportNamesPtr_guess( lpFile ) ) == NULL )
		return 0;

	/* figure out how much memory to allocate for all strings */
	pCnt = pNames;
	for ( i = 0; i < (int)ped->NumberOfNames; i++ )
		while ( *pCnt++ );
	nCnt = (int)( pCnt - pNames );

	return nCnt;
}

/* copies the export function names to pszFunctions */
int GetExportNames_guess( LPVOID lpFile, char *pszFunctions )
{
	PIMAGE_EXPORT_DIRECTORY		ped;
	char						*pNames, *pCnt;
	int							i, nCnt;

	if ( ( ped = (PIMAGE_EXPORT_DIRECTORY)ImageDirectoryOffset( lpFile, IMAGE_DIRECTORY_ENTRY_EXPORT ) ) == NULL )
		return 0;
	if ( ( pNames = GetExportNamesPtr_guess( lpFile ) ) == NULL )
		return 0;

	/* figure out how much memory to allocate for all strings */
	pCnt = pNames;
	for ( i = 0; i < (int)ped->NumberOfNames; i++ )
		while ( *pCnt++ );
	nCnt = (int)( pCnt - pNames );

	/* copy all string to buffer */
	memcpy( pszFunctions, pNames, nCnt );

	return nCnt;
}

/* maps the dll file and returns its export names in temporary hunk memory */
int GetExportNamesFromFile_guess( char *pszFilename, char **ppNames )
{
	HANDLE		fh_guess;
	OFSTRUCT	of_guess;
	int			cnt_guess;
	LPVOID		pView_guess;
	HANDLE		hMap_guess;
	char		*pData_guess;

	FS_GetLocalCopy( pszFilename );

	fh_guess = (HANDLE)OpenFile( pszFilename, &of_guess, OF_READ );
	if ( !fh_guess )
		return 0;

	hMap_guess = CreateFileMapping( fh_guess, NULL, PAGE_READONLY, 0, 0, NULL );
	if ( !hMap_guess )
	{
		CloseHandle( fh_guess );
		return 0;
	}

	pView_guess = MapViewOfFile( hMap_guess, FILE_MAP_READ, 0, 0, 0 );
	if ( !pView_guess )
	{
		CloseHandle( hMap_guess );
		CloseHandle( fh_guess );
		return 0;
	}

	cnt_guess = GetExportNamesSize_guess( pView_guess );
	if ( cnt_guess )
	{
		pData_guess = (char *)Hunk_TempAlloc( cnt_guess );
		GetExportNames_guess( pView_guess, pData_guess );
	}
	else
	{
		pData_guess = NULL;
	}

	UnmapViewOfFile( pView_guess );
	CloseHandle( hMap_guess );
	CloseHandle( fh_guess );

	*ppNames = pData_guess;
	return cnt_guess;
}

/* "?Name@Class@@..." becomes "Name@Class" */
char *StripMangledName_guess( char *pszMangled, char *pszName, int nNameSize )
{
	int		nAt_guess;
	int		i_guess;

	i_guess = 1;
	nAt_guess = 0;
	while ( nAt_guess < 2 && i_guess < nNameSize )
	{
		pszName[i_guess - 1] = pszMangled[i_guess];
		if ( pszMangled[i_guess] == '@' )
			nAt_guess++;
		i_guess++;
	}

	pszName[i_guess - 2] = 0;

	if ( i_guess == nNameSize )
		Con_DPrintf( "Export name is too large\n" );

	return pszName;
}

void BuildExportTable_guess( extensiondll_t *pextdll, char *pszDllFilename )
{
	int		cbNames_guess;
	char	szName_guess[256];
	int		n_guess;
	char	*pList_guess;
	int		cur_guess;
	int		slen_guess;
	char	*p_guess;
	int		nTotal_guess;
	char	*base_guess;

	if ( !pextdll || !pszDllFilename )
		return;

	cbNames_guess = GetExportNamesFromFile_guess( pszDllFilename, &pList_guess );
	pextdll->functionTable = NULL;
	pextdll->functionCount = 0;

	if ( !pList_guess )
		return;

	n_guess = 0;
	nTotal_guess = 0;
	cur_guess = 0;
	while ( cur_guess < cbNames_guess )
	{
		if ( pList_guess[cur_guess] == '?' )
		{
			n_guess++;
			StripMangledName_guess( &pList_guess[cur_guess], szName_guess, sizeof( szName_guess ) );
			nTotal_guess += strlen( szName_guess ) + 1;
		}
		slen_guess = strlen( &pList_guess[cur_guess] ) + 1;
		cur_guess += slen_guess;
	}

	pextdll->functionCount = n_guess;
	pextdll->functionTable = (functiontable_t *)Mem_Malloc( nTotal_guess + n_guess * sizeof( functiontable_t ) );

	base_guess = (char *)&pextdll->functionTable[n_guess];
	cur_guess = 0;
	n_guess = 0;
	p_guess = base_guess;
	while ( cur_guess < cbNames_guess )
	{
		if ( pList_guess[cur_guess] == '?' )
		{
			StripMangledName_guess( &pList_guess[cur_guess], p_guess, sizeof( szName_guess ) );
			pextdll->functionTable[n_guess].pFunctionName = p_guess;
			pextdll->functionTable[n_guess].pFunction = (uint32)GetProcAddress( (HMODULE)pextdll->lDLLHandle, &pList_guess[cur_guess] );
			p_guess += strlen( p_guess ) + 1;
			n_guess++;
		}
		slen_guess = strlen( &pList_guess[cur_guess] ) + 1;
		cur_guess += slen_guess;
	}
}
