/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// common.c -- misc functions used in client and server

#include "q_shared.h"
#include "qcommon.h"
#include <setjmp.h>
#ifndef _WIN32
#include <netinet/in.h>
#include <sys/stat.h> // umask
#else
#include <winsock.h>
#endif

#define MAX_NUM_ARGVS	50

#define MIN_DEDICATED_COMHUNKMEGS 1
#define MIN_COMHUNKMEGS		64
#define DEF_COMHUNKMEGS		256
#define DEF_COMZONEMEGS		64
#define DEF_COMHUNKMEGS_S	XSTRING(DEF_COMHUNKMEGS)
#define DEF_COMZONEMEGS_S	XSTRING(DEF_COMZONEMEGS)

int		com_argc;
char	*com_argv[MAX_NUM_ARGVS+1];

jmp_buf abortframe;		// an ERR_DROP occured, exit the entire frame


FILE *debuglogfile;
static fileHandle_t pipefile;
static fileHandle_t logfile;
fileHandle_t	com_journalFile;			// events are written here
fileHandle_t	com_journalDataFile;		// config files are written here

cvar_t	*in_numpadbug;
cvar_t	*com_speeds;
cvar_t	*com_developer;
cvar_t	*com_dedicated;
cvar_t	*com_timescale;
cvar_t	*com_fixedtime;
cvar_t	*com_journal;
cvar_t	*com_frametime;
cvar_t	*com_frametimeUnfocused;
cvar_t	*com_frametimeMinimized;
cvar_t	*com_altivec;
cvar_t	*com_timedemo;
cvar_t	*com_sv_running;
cvar_t	*com_cl_running;
cvar_t	*com_logfile;		// 1 = buffer log, 2 = flush after each print
cvar_t	*com_pipefile;
cvar_t	*com_showtrace;
cvar_t	*com_version;
cvar_t	*com_buildScript;	// for automated data building scripts
cvar_t	*cl_paused;
cvar_t	*sv_paused;
cvar_t  *cl_packetdelay;
cvar_t  *sv_packetdelay;
cvar_t	*com_cameraMode;
cvar_t	*com_ansiColor;
cvar_t	*com_unfocused;
cvar_t	*com_minimized;
cvar_t	*com_abnormalExit;
cvar_t	*com_gamename;
cvar_t	*com_protocol;
cvar_t	*com_basegame;
cvar_t  *com_homepath;
cvar_t	*com_busyWait;

#ifdef idx64
	int (*Q_VMftol)( void );
#elif defined(id386)
	long (QDECL *Q_ftol)(float f);
	int (QDECL *Q_VMftol)( void );
#endif

// com_speeds times
int		time_game;
int		time_frontend;		// renderer frontend time
int		time_backend;		// renderer backend time

int			com_frameTime;
int			com_frameNumber;

qboolean	com_errorEntered = qfalse;
qboolean	com_fullyInitialized = qfalse;
qboolean	com_gameRestarting = qfalse;

char	com_errorMessage[MAXPRINTMSG];

void Com_WriteConfig_f( void );

static char		*rd_buffer;
static size_t	rd_buffersize;
static void		(*rd_flush)( char *buffer );

void Com_BeginRedirect (char *buffer, size_t buffersize, void (*flush)( char *) ) {
	if (!buffer || !buffersize || !flush)
		return;
	rd_buffer = buffer;
	rd_buffersize = buffersize;
	rd_flush = flush;

	*rd_buffer = 0;
}

void Com_EndRedirect( void ) {
	if ( rd_flush ) {
		rd_flush(rd_buffer);
	}

	rd_buffer = NULL;
	rd_buffersize = 0;
	rd_flush = NULL;
}

// Both client and server can use this, and it will output to the appropriate place.
//	A raw string should NEVER be passed as fmt, because of "%f" type crashers.
void QDECL Com_Printf( const char *fmt, ... ) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];
  static qboolean opening_qconsole = qfalse;


	va_start (argptr,fmt);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	if ( rd_buffer ) {
		if ((strlen (msg) + strlen(rd_buffer)) > (rd_buffersize - 1)) {
			rd_flush(rd_buffer);
			*rd_buffer = 0;
		}
		Q_strcat(rd_buffer, rd_buffersize, msg);
    // TTimo nooo .. that would defeat the purpose
		//rd_flush(rd_buffer);			
		//*rd_buffer = 0;
		return;
	}

#ifndef DEDICATED
	CL_ConsolePrint( msg );
#endif

#if defined(_DEBUG) && defined(_WIN32)
	OutputDebugString( msg );
#endif

	// echo to dedicated console and early console
	Sys_Print( msg );

	// logfile
	if ( com_logfile && com_logfile->integer ) {
    // TTimo: only open the qconsole.log if the filesystem is in an initialized state
    //   also, avoid recursing in the qconsole.log opening (i.e. if fs_debug is on)
		if ( !logfile && FS_Initialized() && !opening_qconsole) {
			struct tm *newtime;
			time_t aclock;

      opening_qconsole = qtrue;

			time( &aclock );
			newtime = localtime( &aclock );

			logfile = FS_FOpenFileWrite( "qconsole.log" );
			
			if(logfile)
			{
				Com_Printf( "logfile opened on %s\n", asctime( newtime ) );
			
				if ( com_logfile->integer > 1 )
				{
					// force it to not buffer so we get valid
					// data even if we are crashing
					FS_ForceFlush(logfile);
				}
			}
			else
			{
				Com_Printf("Opening qconsole.log failed!\n");
				Cvar_SetValue("logfile", 0);
			}

      opening_qconsole = qfalse;
		}
		if ( logfile && FS_Initialized()) {
			FS_Write(msg, (int)strlen(msg), logfile);
		}
	}
}

// A Com_Printf that only shows up if the "developer" cvar is set
void QDECL Com_DPrintf( const char *fmt, ...) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];
		
	if ( !com_developer || !com_developer->integer ) {
		return;			// don't confuse non-developers with techie stuff...
	}

	va_start (argptr,fmt);	
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);
	
	Com_Printf ("%s", msg);
}

// Both client and server can use this, and it will do the appropriate thing.
void QDECL Com_Error( int code, const char *fmt, ... ) {
	va_list		argptr;
	static int	lastErrorTime;
	static int	errorCount;
	int			currentTime;

	if(com_errorEntered)
		Sys_Error("recursive error after: %s", com_errorMessage);

	com_errorEntered = qtrue;

	Cvar_Set("com_errorCode", va("%i", code));

	// when we are running automated scripts, make sure we
	// know if anything failed
	if ( com_buildScript && com_buildScript->integer ) {
		code = ERR_FATAL;
	}

	// if we are getting a solid stream of ERR_DROP, do an ERR_FATAL
	currentTime = Sys_Milliseconds();
	if ( currentTime - lastErrorTime < 100 ) {
		if ( ++errorCount > 3 ) {
			code = ERR_FATAL;
		}
	} else {
		errorCount = 0;
	}
	lastErrorTime = currentTime;

	va_start (argptr,fmt);
	Q_vsnprintf (com_errorMessage, sizeof(com_errorMessage),fmt,argptr);
	va_end (argptr);

	Cvar_Set( "com_errorMessage", com_errorMessage );

	if ( code == ERR_SERVERDISCONNECT ) {
		SV_Shutdown( "Server disconnected" );
		CL_Disconnect( qtrue, NULL );
		CL_FlushMemory( );

		// make sure we can get at our local stuff
		FS_PureServerSetLoadedPaks("", "");
		com_errorEntered = qfalse;
		longjmp (abortframe, -1);
	} else if (code == ERR_DROP) {
		Com_Printf ("********************\nERROR: %s\n********************\n", com_errorMessage);
		SV_Shutdown (va("Server crashed: %s",  com_errorMessage));
		CL_Disconnect( qtrue, "Server crashed" );
		CL_FlushMemory( );

		FS_PureServerSetLoadedPaks("", "");
		com_errorEntered = qfalse;
		longjmp (abortframe, -1);
	} else {
		CL_Shutdown(va("Client fatal crashed: %s", com_errorMessage), qtrue, qtrue);
		SV_Shutdown( va( "Server fatal crashed: %s", com_errorMessage ) );
	}

	Com_Shutdown();

	Sys_Error ("%s", com_errorMessage);
}


// Both client and server can use this, and it will do the appropriate things.
void Com_Quit_f( void ) {
	// don't try to shutdown if we are in a recursive error
	const char *p = Cmd_Args( );
	if ( !com_errorEntered ) {
		// Some VMs might execute "quit" command directly,
		// which would trigger an unload of active VM error.
		// Sys_Quit will kill this process anyways, so
		// a corrupt call stack makes no difference
		SV_Shutdown(p[0] ? p : "Server quit");
		CL_Shutdown(p[0] ? p : "Client quit", qtrue, qtrue);
		Com_Shutdown();
		FS_Shutdown(qtrue);
	}
	Sys_Quit();
}



/*

	COMMAND LINE FUNCTIONS

	+ characters seperate the commandLine string into multiple console command lines.

	All of these are valid:

	qtz +set test blah +map test
	qtz set test blah+map test
	qtz set test blah + map test

*/

#define	MAX_CONSOLE_LINES	32
int		com_numConsoleLines;
char	*com_consoleLines[MAX_CONSOLE_LINES];

// Break it up into multiple console lines
void Com_ParseCommandLine( char *commandLine ) {
    int inq = 0;
    com_consoleLines[0] = commandLine;
    com_numConsoleLines = 1;

    while ( *commandLine ) {
        if (*commandLine == '"') {
            inq = !inq;
        }
        // look for a + seperating character
        // if commandLine came from a file, we might have real line seperators
        if ( (*commandLine == '+' && !inq) || *commandLine == '\n'  || *commandLine == '\r' ) {
            if ( com_numConsoleLines == MAX_CONSOLE_LINES ) {
                return;
            }
            com_consoleLines[com_numConsoleLines] = commandLine + 1;
            com_numConsoleLines++;
            *commandLine = 0;
        }
        commandLine++;
    }
}


// Check for "safe" on the command line, which will skip loading of qtzconfig.cfg
qboolean Com_SafeMode( void ) {
	int		i;

	for ( i = 0 ; i < com_numConsoleLines ; i++ ) {
		Cmd_TokenizeString( com_consoleLines[i] );
		if ( !Q_stricmp( Cmd_Argv(0), "safe" )
			|| !Q_stricmp( Cmd_Argv(0), "cvar_restart" ) ) {
			com_consoleLines[i][0] = 0;
			return qtrue;
		}
	}
	return qfalse;
}

// Searches for command line parameters that are set commands.
//	If match is not NULL, only that cvar will be looked for.
//	That is necessary because cddir and basedir need to be set before the filesystem is started, but all
//	other sets should be after execing the config and default.
void Com_StartupVariable( const char *match ) {
	int i=0;
	const char *s = NULL;

	for ( i=0; i<com_numConsoleLines; i++ ) {
		Cmd_TokenizeString( com_consoleLines[i] );
		if ( strcmp( Cmd_Argv(0), "set" ) )
			continue;

		s = Cmd_Argv(1);
		
		if ( !match || !strcmp( s, match ) ) {
			if ( Cvar_Flags( s ) == CVAR_NONEXISTENT )
				Cvar_Get( s, Cmd_Argv( 2 ), CVAR_USER_CREATED, NULL, NULL );
			else
				Cvar_Set2( s, Cmd_Argv( 2 ), qfalse );
		}
	}
}

// Adds command line parameters as script statements
//	Commands are seperated by + signs
//	Returns qtrue if any late commands were added, which will keep the demoloop from immediately starting
qboolean Com_AddStartupCommands( void ) {
	int		i;
	qboolean	added;

	added = qfalse;
	// quote every token, so args with semicolons can work
	for (i=0 ; i < com_numConsoleLines ; i++) {
		if ( !com_consoleLines[i] || !com_consoleLines[i][0] ) {
			continue;
		}

		// set commands already added with Com_StartupVariable
		if ( !Q_stricmpn( com_consoleLines[i], "set", 3 ) ) {
			continue;
		}

		added = qtrue;
		Cbuf_AddText( com_consoleLines[i] );
		Cbuf_AddText( "\n" );
	}

	return added;
}

void Info_Print( const char *s ) {
	char	key[BIG_INFO_KEY];
	char	value[BIG_INFO_VALUE];
	char	*o;
	ptrdiff_t l;

	if (*s == '\\')
		s++;
	while (*s)
	{
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < 20)
		{
			memset (o, ' ', 20-l);
			key[20] = 0;
		}
		else
			*o = 0;
		Com_Printf ("%s ", key);

		if (!*s)
		{
			Com_Printf ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Com_Printf ("%s\n", value);
	}
}

char *Com_StringContains(char *str1, char *str2, int casesensitive) {
	size_t len, i, j;

	len = strlen(str1) - strlen(str2);
	for (i = 0; i <= len; i++, str1++) {
		for (j = 0; str2[j]; j++) {
			if (casesensitive) {
				if (str1[j] != str2[j]) {
					break;
				}
			}
			else {
				if (toupper(str1[j]) != toupper(str2[j])) {
					break;
				}
			}
		}
		if (!str2[j]) {
			return (char *)str1;
		}
	}
	return NULL;
}

int Com_Filter(const char *filter, char *name, int casesensitive) {
	char buf[MAX_TOKEN_CHARS];
	char *ptr;
	int i, found;

	while(*filter) {
		if (*filter == '*') {
			filter++;
			for (i = 0; *filter; i++) {
				if (*filter == '*' || *filter == '?') break;
				buf[i] = *filter;
				filter++;
			}
			buf[i] = '\0';
			if (strlen(buf)) {
				ptr = Com_StringContains(name, buf, casesensitive);
				if (!ptr) return qfalse;
				name = ptr + strlen(buf);
			}
		}
		else if (*filter == '?') {
			filter++;
			name++;
		}
		else if (*filter == '[' && *(filter+1) == '[') {
			filter++;
		}
		else if (*filter == '[') {
			filter++;
			found = qfalse;
			while(*filter && !found) {
				if (*filter == ']' && *(filter+1) != ']') break;
				if (*(filter+1) == '-' && *(filter+2) && (*(filter+2) != ']' || *(filter+3) == ']')) {
					if (casesensitive) {
						if (*name >= *filter && *name <= *(filter+2)) found = qtrue;
					}
					else {
						if (toupper(*name) >= toupper(*filter) &&
							toupper(*name) <= toupper(*(filter+2))) found = qtrue;
					}
					filter += 3;
				}
				else {
					if (casesensitive) {
						if (*filter == *name) found = qtrue;
					}
					else {
						if (toupper(*filter) == toupper(*name)) found = qtrue;
					}
					filter++;
				}
			}
			if (!found) return qfalse;
			while(*filter) {
				if (*filter == ']' && *(filter+1) != ']') break;
				filter++;
			}
			filter++;
			name++;
		}
		else {
			if (casesensitive) {
				if (*filter != *name) return qfalse;
			}
			else {
				if (toupper(*filter) != toupper(*name)) return qfalse;
			}
			filter++;
			name++;
		}
	}
	return qtrue;
}

int Com_FilterPath(const char *filter, char *name, int casesensitive) {
	int i;
	char new_filter[MAX_QPATH];
	char new_name[MAX_QPATH];

	for (i = 0; i < MAX_QPATH-1 && filter[i]; i++) {
		if ( filter[i] == '\\' || filter[i] == ':' ) {
			new_filter[i] = '/';
		}
		else {
			new_filter[i] = filter[i];
		}
	}
	new_filter[i] = '\0';
	for (i = 0; i < MAX_QPATH-1 && name[i]; i++) {
		if ( name[i] == '\\' || name[i] == ':' ) {
			new_name[i] = '/';
		}
		else {
			new_name[i] = name[i];
		}
	}
	new_name[i] = '\0';
	return Com_Filter(new_filter, new_name, casesensitive);
}

int Com_RealTime(qtime_t *qtime) {
	time_t t;
	struct tm *tms;

	t = time(NULL);
	if (!qtime)
		return (int)t;
	tms = localtime(&t);
	if (tms) {
		qtime->tm_sec = tms->tm_sec;
		qtime->tm_min = tms->tm_min;
		qtime->tm_hour = tms->tm_hour;
		qtime->tm_mday = tms->tm_mday;
		qtime->tm_mon = tms->tm_mon;
		qtime->tm_year = tms->tm_year;
		qtime->tm_wday = tms->tm_wday;
		qtime->tm_yday = tms->tm_yday;
		qtime->tm_isdst = tms->tm_isdst;
	}
	return (int)t;
}


/*

	ZONE MEMORY ALLOCATION

	There is never any space between memblocks, and there will never be two contiguous free memblocks.

	The rover can be left pointing at a non-empty block

	The zone calls are pretty much only used for small strings and structures, all big things are allocated on the hunk.

*/

#define	ZONEID	0x1d4a11
#define MINFRAGMENT	64

typedef struct zonedebug_s {
	char *label;
	char *file;
	int line;
	size_t allocSize;
} zonedebug_t;

typedef struct memblock_s {
	size_t	size;           // including the header and possibly tiny fragments
	int     tag;            // a tag of 0 is a free block
	struct memblock_s       *next, *prev;
	int     id;        		// should be ZONEID
#ifdef ZONE_DEBUG
	zonedebug_t d;
#endif
} memblock_t;

typedef struct memzone_s {
	size_t		size;			// total bytes malloced, including header
	size_t		used;			// total bytes used
	memblock_t	blocklist;	// start / end cap for linked list
	memblock_t	*rover;
} memzone_t;

// main zone for all "dynamic" memory allocation
memzone_t	*mainzone;
// we also have a small zone for small allocations that would only
// fragment the main zone (think of cvar and cmd strings)
memzone_t	*smallzone;

void Z_CheckHeap( void );

void Z_ClearZone( memzone_t *zone, size_t size ) {
	memblock_t	*block;
	
	// set the entire zone to one free block

	zone->blocklist.next = zone->blocklist.prev = block =
		(memblock_t *)( (byte *)zone + sizeof(memzone_t) );
	zone->blocklist.tag = 1;	// in use block
	zone->blocklist.id = 0;
	zone->blocklist.size = 0;
	zone->rover = block;
	zone->size = size;
	zone->used = 0;
	
	block->prev = block->next = &zone->blocklist;
	block->tag = 0;			// free block
	block->id = ZONEID;
	block->size = size - sizeof(memzone_t);
}

size_t Z_AvailableZoneMemory( memzone_t *zone ) {
	return zone->size - zone->used;
}

size_t Z_AvailableMemory( void ) {
	return Z_AvailableZoneMemory( mainzone );
}

void Z_Free( void *ptr ) {
	memblock_t	*block, *other;
	memzone_t *zone;
	
	if (!ptr) {
		Com_Error( ERR_DROP, "Z_Free: NULL pointer" );
	}

	block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID) {
		Com_Error( ERR_FATAL, "Z_Free: freed a pointer without ZONEID" );
	}
	if (block->tag == 0) {
		Com_Error( ERR_FATAL, "Z_Free: freed a freed pointer" );
	}
	// if static memory
	if (block->tag == TAG_STATIC) {
		return;
	}

	// check the memory trash tester
	if ( *(int *)((byte *)block + block->size - 4 ) != ZONEID ) {
		Com_Error( ERR_FATAL, "Z_Free: memory block wrote past end" );
	}

	if (block->tag == TAG_SMALL) {
		zone = smallzone;
	}
	else {
		zone = mainzone;
	}

	zone->used -= block->size;
	// set the block to something that should cause problems
	// if it is referenced...
	memset( ptr, 0xaa, block->size - sizeof( *block ) );

	block->tag = 0;		// mark as free
	
	other = block->prev;
	if (!other->tag) {
		// merge with previous free block
		other->size += block->size;
		other->next = block->next;
		other->next->prev = other;
		if (block == zone->rover) {
			zone->rover = other;
		}
		block = other;
	}

	zone->rover = block;

	other = block->next;
	if ( !other->tag ) {
		// merge the next free block onto the end
		block->size += other->size;
		block->next = other->next;
		block->next->prev = block;
		if (other == zone->rover) {
			zone->rover = block;
		}
	}
}

void Z_FreeTags( int tag ) {
	int			count;
	memzone_t	*zone;

	if ( tag == TAG_SMALL ) {
		zone = smallzone;
	}
	else {
		zone = mainzone;
	}
	count = 0;
	// use the rover as our pointer, because
	// Z_Free automatically adjusts it
	zone->rover = zone->blocklist.next;
	do {
		if ( zone->rover->tag == tag ) {
			count++;
			Z_Free( (void *)(zone->rover + 1) );
			continue;
		}
		zone->rover = zone->rover->next;
	} while ( zone->rover != &zone->blocklist );
}

#ifdef ZONE_DEBUG
void *Z_TagMallocDebug( size_t size, int tag, char *label, char *file, int line ) {
	size_t	allocSize;
#else
void *Z_TagMalloc( size_t size, int tag ) {
#endif
	ptrdiff_t	extra;
	memblock_t	*start, *rover, *new, *base;
	memzone_t *zone;

	if (!tag) {
		Com_Error( ERR_FATAL, "Z_TagMalloc: tried to use a 0 tag" );
	}

	if ( tag == TAG_SMALL ) {
		zone = smallzone;
	}
	else {
		zone = mainzone;
	}

#ifdef ZONE_DEBUG
	allocSize = size;
#endif
	//
	// scan through the block list looking for the first free block
	// of sufficient size
	//
	size += sizeof(memblock_t);	// account for size of block header
	size += 4;					// space for memory trash tester
	size = PAD(size, sizeof(intptr_t));		// align to 32/64 bit boundary
	
	base = rover = zone->rover;
	start = base->prev;
	
	do {
		if (rover == start)	{
			// scaned all the way around the list
#ifdef ZONE_DEBUG
			Z_LogHeap();

			Com_Error(ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes from the %s zone: %s, line: %d (%s)",
								(int)size, zone == smallzone ? "small" : "main", file, line, label);
#else
			Com_Error(ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes from the %s zone",
								(int)size, zone == smallzone ? "small" : "main");
#endif
			return NULL;
		}
		if (rover->tag) {
			base = rover = rover->next;
		} else {
			rover = rover->next;
		}
	} while (base->tag || base->size < size);
	
	//
	// found a block big enough
	//
	extra = base->size - size;
	if (extra > MINFRAGMENT) {
		// there will be a free fragment after the allocated block
		new = (memblock_t *) ((byte *)base + size );
		new->size = extra;
		new->tag = 0;			// free block
		new->prev = base;
		new->id = ZONEID;
		new->next = base->next;
		new->next->prev = new;
		base->next = new;
		base->size = size;
	}
	
	base->tag = tag;			// no longer a free block
	
	zone->rover = base->next;	// next allocation will start looking here
	zone->used += base->size;	//
	
	base->id = ZONEID;

#ifdef ZONE_DEBUG
	base->d.label = label;
	base->d.file = file;
	base->d.line = line;
	base->d.allocSize = allocSize;
#endif

	// marker for memory trash testing
	*(int *)((byte *)base + base->size - 4) = ZONEID;

	return (void *) ((byte *)base + sizeof(memblock_t));
}

#ifdef ZONE_DEBUG
void *Z_MallocDebug( size_t size, char *label, char *file, int line ) {
#else
void *Z_Malloc( size_t size ) {
#endif
	void	*buf;
	
  //Z_CheckHeap();	// DEBUG

#ifdef ZONE_DEBUG
	buf = Z_TagMallocDebug( size, TAG_GENERAL, label, file, line );
#else
	buf = Z_TagMalloc( size, TAG_GENERAL );
#endif
	memset( buf, 0, size );

	return buf;
}

#ifdef ZONE_DEBUG
void *S_MallocDebug( size_t size, char *label, char *file, int line ) {
	return Z_TagMallocDebug( size, TAG_SMALL, label, file, line );
}
#else
void *S_Malloc( size_t size ) {
	return Z_TagMalloc( size, TAG_SMALL );
}
#endif

void Z_CheckHeap( void ) {
	memblock_t	*block;
	
	for (block = mainzone->blocklist.next ; ; block = block->next) {
		if (block->next == &mainzone->blocklist) {
			break;			// all blocks have been hit
		}
		if ( (byte *)block + block->size != (byte *)block->next)
			Com_Error( ERR_FATAL, "Z_CheckHeap: block size does not touch the next block" );
		if ( block->next->prev != block) {
			Com_Error( ERR_FATAL, "Z_CheckHeap: next block doesn't have proper back link" );
		}
		if ( !block->tag && !block->next->tag ) {
			Com_Error( ERR_FATAL, "Z_CheckHeap: two consecutive free blocks" );
		}
	}
}

void Z_LogZoneHeap( memzone_t *zone, char *name ) {
#ifdef ZONE_DEBUG
	char dump[32], *ptr;
	int  i, j;
#endif
	memblock_t	*block;
	char		buf[4096];
	size_t size, allocSize;
	int numBlocks;

	if (!logfile || !FS_Initialized())
		return;
	size = numBlocks = 0;
#ifdef ZONE_DEBUG
	allocSize = 0;
#endif
	Com_sprintf(buf, sizeof(buf), "\r\n================\r\n%s log\r\n================\r\n", name);
	FS_Write(buf, (int)strlen(buf), logfile);
	for (block = zone->blocklist.next ; block->next != &zone->blocklist; block = block->next) {
		if (block->tag) {
#ifdef ZONE_DEBUG
			ptr = ((char *) block) + sizeof(memblock_t);
			j = 0;
			for (i = 0; i < 20 && i < block->d.allocSize; i++) {
				if (ptr[i] >= 32 && ptr[i] < 127) {
					dump[j++] = ptr[i];
				}
				else {
					dump[j++] = '_';
				}
			}
			dump[j] = '\0';
			Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s) [%s]\r\n", (int)block->d.allocSize, block->d.file,
				block->d.line, block->d.label, dump);
			FS_Write(buf, (int)strlen(buf), logfile);
			allocSize += block->d.allocSize;
#endif
			size += block->size;
			numBlocks++;
		}
	}
#ifdef ZONE_DEBUG
	// subtract debug memory
	size -= numBlocks * sizeof(zonedebug_t);
#else
	allocSize = numBlocks * sizeof(memblock_t); // + 32 bit alignment
#endif
	Com_sprintf(buf, sizeof(buf), "%d %s memory in %d blocks\r\n", (int)size, name, numBlocks);
	FS_Write(buf, (int)strlen(buf), logfile);
	Com_sprintf(buf, sizeof(buf), "%d %s memory overhead\r\n", (int)(size - allocSize), name);
	FS_Write(buf, (int)strlen(buf), logfile);
}

void Z_LogHeap( void ) {
	Z_LogZoneHeap( mainzone, "MAIN" );
	Z_LogZoneHeap( smallzone, "SMALL" );
}

// static mem blocks to reduce a lot of small zone overhead
typedef struct memstatic_s {
	memblock_t b;
	byte mem[2];
} memstatic_t;

memstatic_t emptystring =
	{ {(sizeof(memblock_t)+2 + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'\0', '\0'} };
memstatic_t numberstring[] = {
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'0', '\0'} },
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'1', '\0'} },
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'2', '\0'} },
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'3', '\0'} },
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'4', '\0'} },
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'5', '\0'} },
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'6', '\0'} },
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'7', '\0'} },
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'8', '\0'} }, 
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'9', '\0'} }
};

// NOTE: never write over the memory CopyString returns because memory from a memstatic_t might be returned
char *CopyString( const char *in ) {
	char	*out;

	if (!in[0]) {
		return ((char *)&emptystring) + sizeof(memblock_t);
	}
	else if (!in[1]) {
		if (in[0] >= '0' && in[0] <= '9') {
			return ((char *)&numberstring[in[0]-'0']) + sizeof(memblock_t);
		}
	}
	out = S_Malloc (strlen(in)+1);
	strcpy (out, in);
	return out;
}

/*

Goals:
		reproducable without history effects -- no out of memory errors on weird map to map changes
		allow restarting of the client without fragmentation
		minimize total pages in use at run time
		minimize total pages needed during load time

	Single block of memory with stack allocators coming from both ends towards the middle.

	One side is designated the temporary memory allocator.

	Temporary memory can be allocated and freed in any order.

	A highwater mark is kept of the most in use at any time.

	When there is no temporary memory allocated, the permanent and temp sides can be switched, allowing the
	already touched temp memory to be used for permanent storage.

	Temp memory must never be allocated on two ends at once, or fragmentation could occur.

	If we have any in-use temp memory, additional temp allocations must come from that side.

	If not, we can choose to make either side the new temp side and push future permanent allocations to the other side.
	Permanent allocations should be kept on the side that has the current greatest wasted highwater mark.

*/


#define	HUNK_MAGIC	0x89537892
#define	HUNK_FREE_MAGIC	0x89537893

typedef struct hunkHeader_s {
	int		magic;
	size_t	size;
} hunkHeader_t;

typedef struct hunkUsed_s {
	size_t	mark;
	size_t	permanent;
	size_t	temp;
	size_t	tempHighwater;
} hunkUsed_t;

typedef struct hunkblock_s {
	size_t	size;
	byte	printed;
	struct hunkblock_s *next;
	char	*label;
	char	*file;
	int		line;
} hunkblock_t;

static	hunkblock_t *hunkblocks;

static	hunkUsed_t	hunk_low, hunk_high;
static	hunkUsed_t	*hunk_permanent, *hunk_temp;

static	byte	*s_hunkData = NULL;
static	size_t	s_hunkTotal;

static	size_t	s_zoneTotal;
static	size_t	s_smallZoneTotal;

void Com_Meminfo_f( void ) {
	memblock_t	*block;
	size_t		zoneBytes, zoneBlocks;
	size_t		smallZoneBytes, smallZoneBlocks;
	size_t		botlibBytes, rendererBytes;
	size_t		unused;

	zoneBytes = 0;
	botlibBytes = 0;
	rendererBytes = 0;
	zoneBlocks = 0;
	for (block = mainzone->blocklist.next ; ; block = block->next) {
		if ( Cmd_Argc() != 1 ) {
			Com_Printf ("block:%p    size:%7i    tag:%3i\n",
				(void *)block, (int)block->size, block->tag);
		}
		if ( block->tag ) {
			zoneBytes += block->size;
			zoneBlocks++;
			if ( block->tag == TAG_BOTLIB ) {
				botlibBytes += block->size;
			} else if ( block->tag == TAG_RENDERER ) {
				rendererBytes += block->size;
			}
		}

		if (block->next == &mainzone->blocklist) {
			break;			// all blocks have been hit	
		}
		if ( (byte *)block + block->size != (byte *)block->next) {
			Com_Printf ("ERROR: block size does not touch the next block\n");
		}
		if ( block->next->prev != block) {
			Com_Printf ("ERROR: next block doesn't have proper back link\n");
		}
		if ( !block->tag && !block->next->tag ) {
			Com_Printf ("ERROR: two consecutive free blocks\n");
		}
	}

	smallZoneBytes = 0;
	smallZoneBlocks = 0;
	for (block = smallzone->blocklist.next ; ; block = block->next) {
		if ( block->tag ) {
			smallZoneBytes += block->size;
			smallZoneBlocks++;
		}

		if (block->next == &smallzone->blocklist) {
			break;			// all blocks have been hit	
		}
	}

	Com_Printf( "%8i bytes total hunk\n", (int)s_hunkTotal );
	Com_Printf( "%8i bytes total zone\n", (int)s_zoneTotal );
	Com_Printf( "\n" );
	Com_Printf( "%8i low mark\n", (int)hunk_low.mark );
	Com_Printf( "%8i low permanent\n", (int)hunk_low.permanent );
	if ( hunk_low.temp != hunk_low.permanent ) {
		Com_Printf( "%8i low temp\n", (int)hunk_low.temp );
	}
	Com_Printf( "%8i low tempHighwater\n", (int)hunk_low.tempHighwater );
	Com_Printf( "\n" );
	Com_Printf( "%8i high mark\n", (int)hunk_high.mark );
	Com_Printf( "%8i high permanent\n", (int)hunk_high.permanent );
	if ( hunk_high.temp != hunk_high.permanent ) {
		Com_Printf( "%8i high temp\n", (int)hunk_high.temp );
	}
	Com_Printf( "%8i high tempHighwater\n", (int)hunk_high.tempHighwater );
	Com_Printf( "\n" );
	Com_Printf( "%8i total hunk in use\n", (int)(hunk_low.permanent + hunk_high.permanent) );
	unused = 0;
	if ( hunk_low.tempHighwater > hunk_low.permanent ) {
		unused += hunk_low.tempHighwater - hunk_low.permanent;
	}
	if ( hunk_high.tempHighwater > hunk_high.permanent ) {
		unused += hunk_high.tempHighwater - hunk_high.permanent;
	}
	Com_Printf( "%8i unused highwater\n", (int)unused );
	Com_Printf( "\n" );
	Com_Printf( "%8i bytes in %i zone blocks\n", (int)zoneBytes, (int)zoneBlocks	);
	Com_Printf( "        %8i bytes in dynamic botlib\n", (int)botlibBytes );
	Com_Printf( "        %8i bytes in dynamic renderer\n", (int)rendererBytes );
	Com_Printf( "        %8i bytes in dynamic other\n", (int)(zoneBytes - ( botlibBytes + rendererBytes )) );
	Com_Printf( "        %8i bytes in small Zone memory\n", (int)smallZoneBytes );
}

// Touch all known used data to make sure it is paged in
void Com_TouchMemory( void ) {
	int		start, end;
	size_t	i, j;
	size_t	sum;
	memblock_t	*block;

	Z_CheckHeap();

	start = Sys_Milliseconds();

	sum = 0;

	j = hunk_low.permanent >> 2;
	for ( i = 0 ; i < j ; i+=64 ) {			// only need to touch each page
		sum += ((int *)s_hunkData)[i];
	}

	i = ( s_hunkTotal - hunk_high.permanent ) >> 2;
	j = hunk_high.permanent >> 2;
	for (  ; i < j ; i+=64 ) {			// only need to touch each page
		sum += ((int *)s_hunkData)[i];
	}

	for (block = mainzone->blocklist.next ; ; block = block->next) {
		if ( block->tag ) {
			j = block->size >> 2;
			for ( i = 0 ; i < j ; i+=64 ) {				// only need to touch each page
				sum += ((int *)block)[i];
			}
		}
		if ( block->next == &mainzone->blocklist ) {
			break;			// all blocks have been hit	
		}
	}

	end = Sys_Milliseconds();

	Com_Printf( "Com_TouchMemory: %i msec\n", end - start );
}

void Com_InitSmallZoneMemory( void ) {
	s_smallZoneTotal = 512 * 1024;
	smallzone = calloc( s_smallZoneTotal, 1 );
	if ( !smallzone ) {
		Com_Error( ERR_FATAL, "Small zone data failed to allocate %1.1f megs", (float)s_smallZoneTotal / (1024*1024) );
	}
	Z_ClearZone( smallzone, s_smallZoneTotal );
	
	return;
}

void Com_InitZoneMemory( void ) {
	cvar_t	*cv;

	// Please note: com_zoneMegs can only be set on the command line, and
	// not in qtzconfig.cfg or Com_StartupVariable, as they haven't been
	// executed by this point. It's a chicken and egg problem. We need the
	// memory manager configured to handle those places where you would
	// configure the memory manager.

	// allocate the random block zone
	cv = Cvar_Get( "com_zoneMegs", DEF_COMZONEMEGS_S, CVAR_LATCH|CVAR_ARCHIVE, NULL, NULL );

	if ( cv->integer < DEF_COMZONEMEGS ) {
		s_zoneTotal = 1024 * 1024 * DEF_COMZONEMEGS;
	} else {
		s_zoneTotal = cv->integer * 1024 * 1024;
	}

	mainzone = calloc( s_zoneTotal, 1 );
	if ( !mainzone ) {
		Com_Error( ERR_FATAL, "Zone data failed to allocate %i megs", (int)(s_zoneTotal / (1024*1024)) );
	}
	Z_ClearZone( mainzone, s_zoneTotal );

}

void Hunk_Log( void) {
	hunkblock_t	*block;
	char		buf[4096];
	size_t size;
	int numBlocks;

	if (!logfile || !FS_Initialized())
		return;
	size = 0;
	numBlocks = 0;
	Com_sprintf(buf, sizeof(buf), "\r\n================\r\nHunk log\r\n================\r\n");
	FS_Write(buf, (int)strlen(buf), logfile);
	for (block = hunkblocks ; block; block = block->next) {
#ifdef HUNK_DEBUG
		Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s)\r\n", (int)block->size, block->file, block->line, block->label);
		FS_Write(buf, (int)strlen(buf), logfile);
#endif
		size += block->size;
		numBlocks++;
	}
	Com_sprintf(buf, sizeof(buf), "%d Hunk memory\r\n", (int)size);
	FS_Write(buf, (int)strlen(buf), logfile);
	Com_sprintf(buf, sizeof(buf), "%d hunk blocks\r\n", (int)numBlocks);
	FS_Write(buf, (int)strlen(buf), logfile);
}

void Hunk_SmallLog( void) {
	hunkblock_t	*block, *block2;
	char		buf[4096];
	size_t size, locsize;
	int numBlocks;

	if (!logfile || !FS_Initialized())
		return;
	for (block = hunkblocks ; block; block = block->next) {
		block->printed = qfalse;
	}
	size = 0;
	numBlocks = 0;
	Com_sprintf(buf, sizeof(buf), "\r\n================\r\nHunk Small log\r\n================\r\n");
	FS_Write(buf, (int)strlen(buf), logfile);
	for (block = hunkblocks; block; block = block->next) {
		if (block->printed) {
			continue;
		}
		locsize = block->size;
		for (block2 = block->next; block2; block2 = block2->next) {
			if (block->line != block2->line) {
				continue;
			}
			if (Q_stricmp(block->file, block2->file)) {
				continue;
			}
			size += block2->size;
			locsize += block2->size;
			block2->printed = qtrue;
		}
#ifdef HUNK_DEBUG
		Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s)\r\n", (int)locsize, block->file, block->line, block->label);
		FS_Write(buf, (int)strlen(buf), logfile);
#endif
		size += block->size;
		numBlocks++;
	}
	Com_sprintf(buf, sizeof(buf), "%d Hunk memory\r\n", (int)size);
	FS_Write(buf, (int)strlen(buf), logfile);
	Com_sprintf(buf, sizeof(buf), "%d hunk blocks\r\n", numBlocks);
	FS_Write(buf, (int)strlen(buf), logfile);
}

void Com_InitHunkMemory( void ) {
	cvar_t	*cv;
	int nMinAlloc;
	char *pMsg = NULL;

	// make sure the file system has allocated and "not" freed any temp blocks
	// this allows the config and product id files ( journal files too ) to be loaded
	// by the file system without redunant routines in the file system utilizing different 
	// memory systems
	if (FS_LoadStack() != 0) {
		Com_Error( ERR_FATAL, "Hunk initialization failed. File system load stack not zero");
	}

	// allocate the stack based hunk allocator
	cv = Cvar_Get( "com_hunkMegs", DEF_COMHUNKMEGS_S, CVAR_LATCH|CVAR_ARCHIVE, NULL, NULL );

	// if we are not dedicated min allocation is 56, otherwise min is 1
	if (com_dedicated && com_dedicated->integer) {
		nMinAlloc = MIN_DEDICATED_COMHUNKMEGS;
		pMsg = "Minimum com_hunkMegs for a dedicated server is %i, allocating %i megs.\n";
	}
	else {
		nMinAlloc = MIN_COMHUNKMEGS;
		pMsg = "Minimum com_hunkMegs is %i, allocating %i megs.\n";
	}

	if ( cv->integer < nMinAlloc ) {
		s_hunkTotal = 1024 * 1024 * nMinAlloc;
	    Com_Printf(pMsg, nMinAlloc, s_hunkTotal / (1024 * 1024));
	} else {
		s_hunkTotal = cv->integer * 1024 * 1024;
	}

	s_hunkData = calloc( s_hunkTotal + 31, 1 );
	if ( !s_hunkData ) {
		Com_Error( ERR_FATAL, "Hunk data failed to allocate %i megs", (int)(s_hunkTotal / (1024*1024)) );
	}
	// cacheline align
	s_hunkData = (byte *) ( ( (intptr_t)s_hunkData + 31 ) & ~31 );
	Hunk_Clear();

	Cmd_AddCommand( "meminfo", Com_Meminfo_f, NULL );
#ifdef ZONE_DEBUG
	Cmd_AddCommand( "zonelog", Z_LogHeap, NULL );
#endif
#ifdef HUNK_DEBUG
	Cmd_AddCommand( "hunklog", Hunk_Log, NULL );
	Cmd_AddCommand( "hunksmalllog", Hunk_SmallLog, NULL );
#endif
}

size_t Hunk_MemoryRemaining( void ) {
	size_t low, high;

	low = hunk_low.permanent > hunk_low.temp ? hunk_low.permanent : hunk_low.temp;
	high = hunk_high.permanent > hunk_high.temp ? hunk_high.permanent : hunk_high.temp;

	return s_hunkTotal - ( low + high );
}

// The server calls this after the level and game VM have been loaded
void Hunk_SetMark( void ) {
	hunk_low.mark = hunk_low.permanent;
	hunk_high.mark = hunk_high.permanent;
}

// The client calls this before starting a vid_restart or snd_restart
void Hunk_ClearToMark( void ) {
	hunk_low.permanent = hunk_low.temp = hunk_low.mark;
	hunk_high.permanent = hunk_high.temp = hunk_high.mark;
}

qboolean Hunk_CheckMark( void ) {
	if( hunk_low.mark || hunk_high.mark ) {
		return qtrue;
	}
	return qfalse;
}

void CL_ShutdownCGame( void );
void CL_ShutdownUI( void );

// The server calls this before shutting down or loading a new map
void Hunk_Clear( void ) {

#ifndef DEDICATED
	CL_ShutdownCGame();
	CL_ShutdownUI();
#endif
	SV_ShutdownGameProgs();
	hunk_low.mark = 0;
	hunk_low.permanent = 0;
	hunk_low.temp = 0;
	hunk_low.tempHighwater = 0;

	hunk_high.mark = 0;
	hunk_high.permanent = 0;
	hunk_high.temp = 0;
	hunk_high.tempHighwater = 0;

	hunk_permanent = &hunk_low;
	hunk_temp = &hunk_high;

	Com_Printf( "Hunk_Clear: reset the hunk ok\n" );
#ifdef HUNK_DEBUG
	hunkblocks = NULL;
#endif
}

static void Hunk_SwapBanks( void ) {
	hunkUsed_t	*swap;

	// can't swap banks if there is any temp already allocated
	if ( hunk_temp->temp != hunk_temp->permanent ) {
		return;
	}

	// if we have a larger highwater mark on this side, start making
	// our permanent allocations here and use the other side for temp
	if ( hunk_temp->tempHighwater - hunk_temp->permanent >
		hunk_permanent->tempHighwater - hunk_permanent->permanent ) {
		swap = hunk_temp;
		hunk_temp = hunk_permanent;
		hunk_permanent = swap;
	}
}

// Allocate permanent (until the hunk is cleared) memory
#ifdef HUNK_DEBUG
void *Hunk_AllocDebug( size_t size, hunkallocPref_t preference, char *label, char *file, int line ) {
#else
void *Hunk_Alloc( size_t size, hunkallocPref_t preference ) {
#endif
	void	*buf;

	if ( s_hunkData == NULL)
	{
		Com_Error( ERR_FATAL, "Hunk_Alloc: Hunk memory system not initialized" );
	}

	// can't do preference if there is any temp allocated
	if (preference == PREF_DONTCARE || hunk_temp->temp != hunk_temp->permanent) {
		Hunk_SwapBanks();
	} else {
		if (preference == PREF_LOW && hunk_permanent != &hunk_low) {
			Hunk_SwapBanks();
		} else if (preference == PREF_HIGH && hunk_permanent != &hunk_high) {
			Hunk_SwapBanks();
		}
	}

#ifdef HUNK_DEBUG
	size += sizeof(hunkblock_t);
#endif

	// round to cacheline
	size = (size+31)&~31;

	if ( hunk_low.temp + hunk_high.temp + size > s_hunkTotal ) {
#ifdef HUNK_DEBUG
		Hunk_Log();
		Hunk_SmallLog();

		Com_Error(ERR_DROP, "Hunk_Alloc failed on %i: %s, line: %d (%s)", (int)size, file, line, label);
#else
		Com_Error(ERR_DROP, "Hunk_Alloc failed on %i", (int)size);
#endif
	}

	if ( hunk_permanent == &hunk_low ) {
		buf = (void *)(s_hunkData + hunk_permanent->permanent);
		hunk_permanent->permanent += size;
	} else {
		hunk_permanent->permanent += size;
		buf = (void *)(s_hunkData + s_hunkTotal - hunk_permanent->permanent );
	}

	hunk_permanent->temp = hunk_permanent->permanent;

	memset( buf, 0, size );

#ifdef HUNK_DEBUG
	{
		hunkblock_t *block;

		block = (hunkblock_t *) buf;
		block->size = size - sizeof(hunkblock_t);
		block->file = file;
		block->label = label;
		block->line = line;
		block->next = hunkblocks;
		hunkblocks = block;
		buf = ((byte *) buf) + sizeof(hunkblock_t);
	}
#endif
	return buf;
}

// This is used by the file loading system.
//	Multiple files can be loaded in temporary memory.
//	When the files-in-use count reaches zero, all temp memory will be deleted
void *Hunk_AllocateTempMemory( size_t size ) {
	void		*buf;
	hunkHeader_t	*hdr;

	// return a Z_Malloc'd block if the hunk has not been initialized
	// this allows the config and product id files ( journal files too ) to be loaded
	// by the file system without redunant routines in the file system utilizing different 
	// memory systems
	if ( s_hunkData == NULL )
	{
		return Z_Malloc(size);
	}

	Hunk_SwapBanks();

	size = PAD(size, sizeof(intptr_t)) + sizeof( hunkHeader_t );

	if ( hunk_temp->temp + hunk_permanent->permanent + size > s_hunkTotal ) {
		Com_Error( ERR_DROP, "Hunk_AllocateTempMemory: failed on %i", (int)size );
	}

	if ( hunk_temp == &hunk_low ) {
		buf = (void *)(s_hunkData + hunk_temp->temp);
		hunk_temp->temp += size;
	} else {
		hunk_temp->temp += size;
		buf = (void *)(s_hunkData + s_hunkTotal - hunk_temp->temp );
	}

	if ( hunk_temp->temp > hunk_temp->tempHighwater ) {
		hunk_temp->tempHighwater = hunk_temp->temp;
	}

	hdr = (hunkHeader_t *)buf;
	buf = (void *)(hdr+1);

	hdr->magic = HUNK_MAGIC;
	hdr->size = size;

	// don't bother clearing, because we are going to load a file over it
	return buf;
}

void Hunk_FreeTempMemory( void *buf ) {
	hunkHeader_t	*hdr;

	  // free with Z_Free if the hunk has not been initialized
	  // this allows the config and product id files ( journal files too ) to be loaded
	  // by the file system without redunant routines in the file system utilizing different 
	  // memory systems
	if ( s_hunkData == NULL )
	{
		Z_Free(buf);
		return;
	}


	hdr = ( (hunkHeader_t *)buf ) - 1;
	if ( hdr->magic != HUNK_MAGIC ) {
		Com_Error( ERR_FATAL, "Hunk_FreeTempMemory: bad magic" );
	}

	hdr->magic = HUNK_FREE_MAGIC;

	// this only works if the files are freed in stack order,
	// otherwise the memory will stay around until Hunk_ClearTempMemory
	if ( hunk_temp == &hunk_low ) {
		if ( hdr == (void *)(s_hunkData + hunk_temp->temp - hdr->size ) ) {
			hunk_temp->temp -= hdr->size;
		} else {
			Com_Printf( "Hunk_FreeTempMemory: not the final block\n" );
		}
	} else {
		if ( hdr == (void *)(s_hunkData + s_hunkTotal - hunk_temp->temp ) ) {
			hunk_temp->temp -= hdr->size;
		} else {
			Com_Printf( "Hunk_FreeTempMemory: not the final block\n" );
		}
	}
}


// The temp space is no longer needed.
//	If we have left more touched but unused memory on this side, have future permanent allocs use this side.
void Hunk_ClearTempMemory( void ) {
	if ( s_hunkData != NULL ) {
		hunk_temp->temp = hunk_temp->permanent;
	}
}

/*

	EVENTS AND JOURNALING

	In addition to these events, .cfg files are also copied to the journaled file

*/

#define	MAX_PUSHED_EVENTS	            1024
static int com_pushedEventsHead = 0;
static int com_pushedEventsTail = 0;
static sysEvent_t	com_pushedEvents[MAX_PUSHED_EVENTS];

void Com_InitJournaling( void ) {
	Com_StartupVariable( "journal" );
	com_journal = Cvar_Get( "journal", "0", CVAR_INIT, NULL, NULL );
	if ( !com_journal->integer ) {
		return;
	}

	if ( com_journal->integer == 1 ) {
		Com_Printf( "Journaling events\n");
		com_journalFile = FS_FOpenFileWrite( "journal.dat" );
		com_journalDataFile = FS_FOpenFileWrite( "journaldata.dat" );
	} else if ( com_journal->integer == 2 ) {
		Com_Printf( "Replaying journaled events\n");
		FS_FOpenFileRead( "journal.dat", &com_journalFile, qtrue );
		FS_FOpenFileRead( "journaldata.dat", &com_journalDataFile, qtrue );
	}

	if ( !com_journalFile || !com_journalDataFile ) {
		Cvar_Set( "com_journal", "0" );
		com_journalFile = 0;
		com_journalDataFile = 0;
		Com_Printf( "Couldn't open journal files\n" );
	}
}

/*

	EVENT LOOP

*/

#define MAX_QUEUED_EVENTS  256
#define MASK_QUEUED_EVENTS ( MAX_QUEUED_EVENTS - 1 )

static sysEvent_t  eventQueue[ MAX_QUEUED_EVENTS ];
static int         eventHead = 0;
static int         eventTail = 0;

// A time of 0 will get the current time
//	Ptr should either be null, or point to a block of data that can be freed by the game later.
void Com_QueueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr ) {
	sysEvent_t  *ev;

	ev = &eventQueue[ eventHead & MASK_QUEUED_EVENTS ];

	if ( eventHead - eventTail >= MAX_QUEUED_EVENTS )
	{
		Com_Printf("Com_QueueEvent: overflow\n");
		// we are discarding an event, but don't leak memory
		if ( ev->evPtr )
		{
			Z_Free( ev->evPtr );
		}
		eventTail++;
	}

	eventHead++;

	if ( time == 0 )
	{
		time = Sys_Milliseconds();
	}

	ev->evTime = time;
	ev->evType = type;
	ev->evValue = value;
	ev->evValue2 = value2;
	ev->evPtrLength = ptrLength;
	ev->evPtr = ptr;
}

sysEvent_t Com_GetSystemEvent( void ) {
	sysEvent_t  ev;
	char        *s;

	// return if we have data
	if ( eventHead > eventTail )
	{
		eventTail++;
		return eventQueue[ ( eventTail - 1 ) & MASK_QUEUED_EVENTS ];
	}

	// check for console commands
	s = Sys_ConsoleInput();
	if ( s )
	{
		char  *b;
		size_t   len;

		len = strlen( s ) + 1;
		b = Z_Malloc( len );
		strcpy( b, s );
		Com_QueueEvent( 0, SE_CONSOLE, 0, 0, (int)len, b );
	}

	// return if we have data
	if ( eventHead > eventTail )
	{
		eventTail++;
		return eventQueue[ ( eventTail - 1 ) & MASK_QUEUED_EVENTS ];
	}

	// create an empty event to return
	memset( &ev, 0, sizeof( ev ) );
	ev.evTime = Sys_Milliseconds();

	return ev;
}

sysEvent_t	Com_GetRealEvent( void ) {
	int			r;
	sysEvent_t	ev;

	// either get an event from the system or the journal file
	if ( com_journal->integer == 2 ) {
		r = FS_Read( &ev, sizeof(ev), com_journalFile );
		if ( r != sizeof(ev) ) {
			Com_Error( ERR_FATAL, "Error reading from journal file" );
		}
		if ( ev.evPtrLength ) {
			ev.evPtr = Z_Malloc( ev.evPtrLength );
			r = FS_Read( ev.evPtr, ev.evPtrLength, com_journalFile );
			if ( r != ev.evPtrLength ) {
				Com_Error( ERR_FATAL, "Error reading from journal file" );
			}
		}
	} else {
		ev = Com_GetSystemEvent();

		// write the journal value out if needed
		if ( com_journal->integer == 1 ) {
			r = FS_Write( &ev, sizeof(ev), com_journalFile );
			if ( r != sizeof(ev) ) {
				Com_Error( ERR_FATAL, "Error writing to journal file" );
			}
			if ( ev.evPtrLength ) {
				r = FS_Write( ev.evPtr, ev.evPtrLength, com_journalFile );
				if ( r != ev.evPtrLength ) {
					Com_Error( ERR_FATAL, "Error writing to journal file" );
				}
			}
		}
	}

	return ev;
}

void Com_InitPushEvent( void ) {
  // clear the static buffer array
  // this requires SE_NONE to be accepted as a valid but NOP event
  memset( com_pushedEvents, 0, sizeof(com_pushedEvents) );
  // reset counters while we are at it
  // beware: GetEvent might still return an SE_NONE from the buffer
  com_pushedEventsHead = 0;
  com_pushedEventsTail = 0;
}

void Com_PushEvent( sysEvent_t *event ) {
	sysEvent_t		*ev;
	static int printedWarning = 0;

	ev = &com_pushedEvents[ com_pushedEventsHead & (MAX_PUSHED_EVENTS-1) ];

	if ( com_pushedEventsHead - com_pushedEventsTail >= MAX_PUSHED_EVENTS ) {

		// don't print the warning constantly, or it can give time for more...
		if ( !printedWarning ) {
			printedWarning = qtrue;
			Com_Printf( "WARNING: Com_PushEvent overflow\n" );
		}

		if ( ev->evPtr ) {
			Z_Free( ev->evPtr );
		}
		com_pushedEventsTail++;
	} else {
		printedWarning = qfalse;
	}

	*ev = *event;
	com_pushedEventsHead++;
}

sysEvent_t	Com_GetEvent( void ) {
	if ( com_pushedEventsHead > com_pushedEventsTail ) {
		com_pushedEventsTail++;
		return com_pushedEvents[ (com_pushedEventsTail-1) & (MAX_PUSHED_EVENTS-1) ];
	}
	return Com_GetRealEvent();
}

void Com_RunAndTimeServerPacket( netadr_t *evFrom, msg_t *buf ) {
	int		t1, t2, msec;

	t1 = 0;

	if ( com_speeds->integer ) {
		t1 = Sys_Milliseconds();
	}

	SV_PacketEvent( *evFrom, buf );

	if ( com_speeds->integer ) {
		t2 = Sys_Milliseconds();
		msec = t2 - t1;
		if ( com_speeds->integer == 3 ) {
			Com_Printf( "SV_PacketEvent time: %i\n", msec );
		}
	}
}

// Returns last event time
int Com_EventLoop( void ) {
	sysEvent_t	ev;
	netadr_t	evFrom;
	byte		bufData[MAX_MSGLEN];
	msg_t		buf;

	MSG_Init( &buf, bufData, sizeof( bufData ) );

	while ( 1 ) {
		ev = Com_GetEvent();

		// if no more events are available
		if ( ev.evType == SE_NONE ) {
			// manually send packet events for the loopback channel
			while ( NET_GetLoopPacket( NS_CLIENT, &evFrom, &buf ) ) {
				CL_PacketEvent( evFrom, &buf );
			}

			while ( NET_GetLoopPacket( NS_SERVER, &evFrom, &buf ) ) {
				// if the server just shut down, flush the events
				if ( com_sv_running->integer ) {
					Com_RunAndTimeServerPacket( &evFrom, &buf );
				}
			}

			return ev.evTime;
		}


		switch(ev.evType)
		{
			case SE_KEY:
				CL_KeyEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
			case SE_CHAR:
				CL_CharEvent( ev.evValue );
			break;
			case SE_MOUSE:
				CL_MouseEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
			case SE_JOYSTICK_AXIS:
				CL_JoystickEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
			case SE_CONSOLE:
				Cbuf_AddText( (char *)ev.evPtr );
				Cbuf_AddText( "\n" );
			break;
			default:
				Com_Error( ERR_FATAL, "Com_EventLoop: bad event type %i", ev.evType );
			break;
		}

		// free any block data
		if ( ev.evPtr ) {
			Z_Free( ev.evPtr );
		}
	}

	return 0;	// never reached
}

// Can be used for profiling, but will be journaled accurately
int Com_Milliseconds( void ) {
	sysEvent_t	ev;

	// get events and push them until we get a null event with the current time
	do {

		ev = Com_GetRealEvent();
		if ( ev.evType != SE_NONE ) {
			Com_PushEvent( &ev );
		}
	} while ( ev.evType != SE_NONE );
	
	return ev.evTime;
}

// Just throw a fatal error to test error shutdown procedures
static void Com_Error_f( void ) {
	if ( Cmd_Argc() > 1 ) {
		Com_Error( ERR_DROP, "Testing drop error" );
	} else {
		Com_Error( ERR_FATAL, "Testing fatal error" );
	}
}

// Just freeze in place for a given number of seconds to test error recovery
static void Com_Freeze_f( void ) {
	float	s;
	int		start, now;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "freeze <seconds>\n" );
		return;
	}
	s = (float)atof( Cmd_Argv(1) );

	start = Com_Milliseconds();

	while ( 1 ) {
		now = Com_Milliseconds();
		if ( ( now - start ) * 0.001f > s ) {
			break;
		}
	}
}

// A way to force a bus error for development reasons
static void Com_Crash_f( void ) {
	* ( volatile int * ) 0 = 0x12345678;
}

// For controlling environment variables
void Com_Setenv_f( void ) {
	int argc = Cmd_Argc();
	const char *arg1 = Cmd_Argv(1);

	if(argc > 2)
	{
		const char *arg2 = Cmd_ArgsFrom(2);
		
		Sys_SetEnv(arg1, arg2);
	}
	else if(argc == 2)
	{
		char *env = getenv(arg1);
		
		if(env)
			Com_Printf("%s=%s\n", arg1, env);
		else
			Com_Printf("%s undefined\n", arg1);
        }
}

// For controlling environment variables
void Com_ExecuteCfg( void ) {
	Cbuf_ExecuteText( EXEC_NOW, "exec default.cfg\n" );
	Cbuf_Execute(); // Always execute after exec to prevent text buffer overflowing

	if ( !Com_SafeMode() ) {
		// skip the qtzconfig.cfg and autoexec.cfg if "safe" is on the command line
		Cbuf_ExecuteText( EXEC_NOW, "exec "QTZCONFIG_CFG"\n" );
		Cbuf_Execute();
		Cbuf_ExecuteText( EXEC_NOW, "exec autoexec.cfg\n" );
		Cbuf_Execute();
	}
}

// Change to a new mod properly with cleaning up cvars before switching.
void Com_GameRestart(int checksumFeed, qboolean disconnect) {
	// make sure no recursion can be triggered
	if(!com_gameRestarting && com_fullyInitialized)
	{
		int clWasRunning;
		
		com_gameRestarting = qtrue;
		clWasRunning = com_cl_running->integer;
		
		// Kill server if we have one
		if(com_sv_running->integer)
			SV_Shutdown("Game directory changed");

		if ( clWasRunning ) {
			if ( disconnect )
				CL_Disconnect( qfalse, "Game restart" );
				
			CL_Shutdown("Game directory changed", disconnect, qfalse);
		}

		FS_Restart(checksumFeed);
	
		// Clean out any user and VM created cvars
		Cvar_Restart();
		Com_ExecuteCfg();

		if(disconnect)
		{
			// We don't want to change any network settings if gamedir
			// change was triggered by a connect to server because the
			// new network settings might make the connection fail.
			NET_Restart_f();
		}

		if(clWasRunning)
		{
			CL_Init();
			CL_StartHunkUsers(qfalse);
		}
		
		com_gameRestarting = qfalse;
	}
}

// Expose possibility to change current running mod to the user
void Com_GameRestart_f( void ) {
	if(!FS_FilenameCompare(Cmd_Argv(1), com_basegame->string))
	{
		// This is the standard base game. Servers and clients should
		// use "" and not the standard basegame name because this messes
		// up pak file negotiation and lots of other stuff
		
		Cvar_Set("fs_game", "");
	}
	else
		Cvar_Set("fs_game", Cmd_Argv(1));

	Com_GameRestart(0, qtrue);
}

static void Com_DetectAltivec( void ) {
	// Only detect if user hasn't forcibly disabled it.
	if (com_altivec->integer) {
		static qboolean altivec = qfalse;
		static qboolean detected = qfalse;
		if (!detected) {
			altivec = ( Sys_GetProcessorFeatures( ) & CF_ALTIVEC );
			detected = qtrue;
		}

		if (!altivec) {
			Cvar_Set( "com_altivec", "0" );  // we don't have it! Disable support!
		}
	}
}

#if defined(id386) || defined(idx64)

// Find out whether we have SSE support for Q_ftol function
static void Com_DetectSSE( void ) {
#if !defined(idx64)
	cpuFeatures_t feat;
	
	feat = Sys_GetProcessorFeatures();

	if(feat & CF_SSE)
	{
		Q_ftol = qftolsse;
#endif
		Q_VMftol = qvmftolsse;

		Com_Printf("Have SSE support\n");
#if !defined(idx64)
	}
	else
	{
		Q_ftol = qftolx87;
		Q_VMftol = qvmftolx87;

		Com_Printf("No SSE support on this machine\n");
	}
#endif
}

#else

#define Com_DetectSSE()

#endif

// Seed the random number generator, if possible with an OS supplied random seed.
static void Com_InitRand( void ) {
	unsigned int seed;

	if(Sys_RandomBytes((byte *) &seed, sizeof(seed)))
		srand(seed);
	else
		srand((unsigned int)time(NULL));
}

void Com_Init( char *commandLine ) {
	int	qport;

#ifdef DEDICATED
	Com_Printf( "%s (dedicated) running on %s compiled on %s\n", QTZ_VERSION, PLATFORM_STRING, __DATE__ );
#else
	Com_Printf( "%s running on %s compiled on %s\n", QTZ_VERSION, PLATFORM_STRING, __DATE__ );
#endif

	if ( setjmp (abortframe) ) {
		Sys_Error ("Error during initialization");
	}

	// Clear queues
	memset( &eventQueue[ 0 ], 0, MAX_QUEUED_EVENTS * sizeof( sysEvent_t ) );

	// initialize the weak pseudo-random number generator for use later.
	Com_InitRand();

	// do this before anything else decides to push events
	Com_InitPushEvent();

	Com_InitSmallZoneMemory();
	Cvar_Init();

	// prepare enough of the subsystems to handle
	// cvar and command buffer management
	Com_ParseCommandLine( commandLine );

//	Swap_Init();
	Cbuf_Init();

	Com_DetectSSE();

	// override anything from the config files with command line args
	Com_StartupVariable( NULL );

	Com_InitZoneMemory();
	Cmd_Init();

	// get the developer cvar set as early as possible
#ifdef _DEBUG
	com_developer = Cvar_Get( "developer", "1", CVAR_TEMP, "Enable developer features for debugging/testing", NULL );
#else
	com_developer = Cvar_Get( "developer", "0", CVAR_TEMP, "Enable developer features for debugging/testing", NULL );
#endif

	// done early so bind command exists
	CL_InitKeyCommands();

	com_basegame = Cvar_Get("com_basegame", BASEGAME, CVAR_INIT, NULL, NULL);
	com_homepath = Cvar_Get("com_homepath", "", CVAR_INIT, NULL, NULL);
	
	if(!com_basegame->string[0])
		Cvar_ForceReset("com_basegame");

	//QtZ: Allow up to 2048 file handles to be open (From iodfe)
	#ifdef _WIN32
		_setmaxstdio( 2048 );
	#endif
	//~QtZ

	FS_InitFilesystem();

	Com_InitJournaling();

	// Add some commands here already so users can use them from config files
	Cmd_AddCommand( "setenv", Com_Setenv_f, NULL );
	if (com_developer && com_developer->integer)
	{
		Cmd_AddCommand( "error", Com_Error_f, NULL );
		Cmd_AddCommand( "crash", Com_Crash_f, NULL );
		Cmd_AddCommand( "freeze", Com_Freeze_f, NULL );
	}
	Cmd_AddCommand( "quit", Com_Quit_f, NULL );
	Cmd_AddCommand( "changeVectors", MSG_ReportChangeVectors_f, NULL );
	Cmd_AddCommand( "writeconfig", Com_WriteConfig_f, Cmd_CompleteCfgName );
	Cmd_AddCommand( "game_restart", Com_GameRestart_f, NULL );

	Com_ExecuteCfg();

	// override anything from the config files with command line args
	Com_StartupVariable( NULL );

  // get dedicated here for proper hunk megs initialization
#ifdef DEDICATED
	com_dedicated = Cvar_Get ("dedicated", "1", CVAR_INIT, NULL, NULL);
	Cvar_CheckRange( com_dedicated, 1, 2, qtrue );
#else
	com_dedicated = Cvar_Get ("dedicated", "0", CVAR_LATCH, NULL, NULL);
	Cvar_CheckRange( com_dedicated, 0, 2, qtrue );
#endif
	// allocate the stack based hunk allocator
	Com_InitHunkMemory();

	// if any archived cvars are modified after this, we will trigger a writing
	// of the config file
	cvar_modifiedFlags &= ~CVAR_ARCHIVE;

	//
	// init commands and vars
	//
	com_altivec				= Cvar_Get( "com_altivec",				"1",								CVAR_ARCHIVE,				NULL, NULL );
	com_frametime			= Cvar_Get( "com_frametime",			"8",								CVAR_ARCHIVE,				NULL, NULL );
	com_frametimeUnfocused	= Cvar_Get( "com_frametimeUnfocused",	"40",								CVAR_ARCHIVE,				NULL, NULL );
	com_frametimeMinimized	= Cvar_Get( "com_frametimeMinimized",	"40",								CVAR_ARCHIVE,				NULL, NULL );
	com_logfile				= Cvar_Get( "logfile",					"0",								CVAR_TEMP,					NULL, NULL );
	com_timescale			= Cvar_Get( "timescale",				"1",								CVAR_CHEAT|CVAR_SYSTEMINFO,	NULL, NULL );
	com_fixedtime			= Cvar_Get( "fixedtime",				"0",								CVAR_CHEAT,					NULL, NULL );
	com_showtrace			= Cvar_Get( "com_showtrace",			"0",								CVAR_CHEAT,					NULL, NULL );
	com_speeds				= Cvar_Get( "com_speeds",				"0",								CVAR_NONE,					NULL, NULL );
	com_timedemo			= Cvar_Get( "timedemo",					"0",								CVAR_CHEAT,					NULL, NULL );
	com_cameraMode			= Cvar_Get( "com_cameraMode",			"0",								CVAR_CHEAT,					NULL, NULL );
	cl_paused				= Cvar_Get( "cl_paused",				"0",								CVAR_ROM,					NULL, NULL );
	sv_paused				= Cvar_Get( "sv_paused",				"0",								CVAR_ROM,					NULL, NULL );
	cl_packetdelay			= Cvar_Get( "cl_packetdelay",			"0",								CVAR_CHEAT,					NULL, NULL );
	sv_packetdelay			= Cvar_Get( "sv_packetdelay",			"0",								CVAR_CHEAT,					NULL, NULL );
	com_sv_running			= Cvar_Get( "sv_running",				"0",								CVAR_ROM,					NULL, NULL );
	com_cl_running			= Cvar_Get( "cl_running",				"0",								CVAR_ROM,					NULL, NULL );
	com_buildScript			= Cvar_Get( "com_buildScript",			"0",								CVAR_NONE,					NULL, NULL );
	com_ansiColor			= Cvar_Get( "com_ansiColor",			"0",								CVAR_ARCHIVE,				NULL, NULL );
	com_unfocused			= Cvar_Get( "com_unfocused",			"0",								CVAR_ROM,					NULL, NULL );
	com_minimized			= Cvar_Get( "com_minimized",			"0",								CVAR_ROM,					NULL, NULL );
	com_abnormalExit		= Cvar_Get( "com_abnormalExit",			"0",								CVAR_ROM,					NULL, NULL );
	com_busyWait			= Cvar_Get( "com_busyWait",				"0",								CVAR_ARCHIVE,				NULL, NULL );
							  Cvar_Get( "com_errorMessage",			"",									CVAR_ROM|CVAR_NORESTART,	NULL, NULL );
	in_numpadbug			= Cvar_Get( "in_numpadbug",				"0",								CVAR_ARCHIVE,				NULL, NULL );
	com_version				= Cvar_Get( "com_version",				QTZ_VERSION" "PLATFORM_STRING" "__DATE__,	CVAR_ROM|CVAR_SERVERINFO,	NULL, NULL );
	com_gamename			= Cvar_Get( "com_gamename",				GAMENAME_FOR_MASTER,				CVAR_SERVERINFO|CVAR_INIT,	NULL, NULL );
	com_protocol			= Cvar_Get( "com_protocol",				XSTRING( PROTOCOL_VERSION ),		CVAR_SERVERINFO|CVAR_INIT,	NULL, NULL );
							  Cvar_Get( "protocol",					com_protocol->string,				CVAR_ROM,					NULL, NULL );

	Sys_Init();

	if( Sys_WritePIDFile( ) ) {
#ifndef DEDICATED
		const char *message = "The last time " CLIENT_WINDOW_TITLE " ran, "
			"it didn't exit properly. This may be due to inappropriate video "
			"settings. Would you like to start with \"safe\" video settings?";

		if( Sys_Dialog( DT_YES_NO, message, "Abnormal Exit" ) == DR_YES ) {
			Cvar_Set( "com_abnormalExit", "1" );
		}
#endif
	}

	// Pick a random port value
	Com_RandomBytes( (byte*)&qport, sizeof(int) );
	Netchan_Init( qport & 0xffff );

	SV_Init();

	com_dedicated->modified = qfalse;
#ifndef DEDICATED
	CL_Init();
#endif

	// set com_frameTime so that if a map is started on the
	// command line it will still be able to count on com_frameTime
	// being random enough for a serverid
	com_frameTime = Com_Milliseconds();

	// add + commands from command line
	Com_AddStartupCommands();

	// start in full screen ui mode
	Cvar_Set("r_uiFullScreen", "1");

	CL_StartHunkUsers( qfalse );

	//QtZ: Raw mouse input
	#ifdef _WIN32
		Cbuf_AddText( "in_restart;" );
	#endif

	com_fullyInitialized = qtrue;

	// always set the cvar, but only print the info if it makes sense.
	Com_DetectAltivec();
#ifdef idppc
	Com_Printf ("Altivec support is %s\n", com_altivec->integer ? "enabled" : "disabled");
#endif

	com_pipefile = Cvar_Get( "com_pipefile", "", CVAR_ARCHIVE|CVAR_LATCH, NULL, NULL );
	if( com_pipefile->string[0] )
	{
		pipefile = FS_FCreateOpenPipeFile( com_pipefile->string );
	}

	Com_Printf ("--- Common Initialization Complete ---\n");
}

// Read whatever is in com_pipefile, if anything, and execute it
void Com_ReadFromPipe( void ) {
	static char buf[MAX_STRING_CHARS];
	static size_t accu = 0;
	int read;

	if( !pipefile )
		return;

	while( ( read = FS_Read( buf + accu, (int)(sizeof( buf ) - accu-1), pipefile ) ) > 0 )
	{
		char *brk = NULL;
		size_t i;

		for( i = accu; i < accu + read; ++i )
		{
			if( buf[ i ] == '\0' )
				buf[ i ] = '\n';
			if( buf[ i ] == '\n' || buf[ i ] == '\r' )
				brk = &buf[ i + 1 ];
		}
		buf[ accu + read ] = '\0';

		accu += read;

		if( brk )
		{
			char tmp = *brk;
			*brk = '\0';
			Cbuf_ExecuteText( EXEC_APPEND, buf );
			*brk = tmp;

			accu -= brk - buf;
			memmove( buf, brk, accu + 1 );
		}
		else if( accu >= sizeof( buf ) - 1 ) // full
		{
			Cbuf_ExecuteText( EXEC_APPEND, buf );
			accu = 0;
		}
	}
}

void Com_WriteConfigToFile( const char *filename ) {
	fileHandle_t f;
	char fn[MAX_QPATH] = {0};
	qtime_t	now;
	
	Com_sprintf( fn, sizeof( fn ), "cfg/%s", filename );

	f = FS_FOpenFileWrite( fn );
	if ( !f ) {
		Com_Printf( "Couldn't write %s.\n", filename );
		return;
	}

	Com_RealTime( &now );
	FS_Printf( f, "// generated by "PRODUCT_NAME" (%04d-%02d-%02d-%02d:%02d:%02d), do not modify\n\n", 1900 + now.tm_year, 1 + now.tm_mon, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec );

	FS_Printf( f, "// binds\n" );
	Key_WriteBindings( f );
	FS_Printf( f, "\n" );

	FS_Printf( f, "// cvars\n" );
	Cvar_WriteVariables( f );
	FS_Printf( f, "\n" );

	FS_FCloseFile( f );
}

// Writes key bindings and archived cvars to config file if modified
void Com_WriteConfiguration( void ) {
	// if we are quiting without fully initializing, make sure
	// we don't write out anything
	if ( !com_fullyInitialized ) {
		return;
	}

	if ( !(cvar_modifiedFlags & CVAR_ARCHIVE ) ) {
		return;
	}
	cvar_modifiedFlags &= ~CVAR_ARCHIVE;

	Com_WriteConfigToFile( QTZCONFIG_CFG );
}

// Write the config file to a specific name
void Com_WriteConfig_f( void ) {
	char	filename[MAX_QPATH];

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: writeconfig <filename>\n" );
		return;
	}

	Q_strncpyz( filename, Cmd_Argv(1), sizeof( filename ) );
	COM_DefaultExtension( filename, sizeof( filename ), ".cfg" );
	Com_Printf( "Writing %s.\n", filename );
	Com_WriteConfigToFile( filename );
}

int Com_ModifyMsec( int msec ) {
	int		clampTime;

	//
	// modify time for debugging values
	//
	if ( com_fixedtime->integer )
		msec = com_fixedtime->integer;
	else if ( com_timescale->value || com_cameraMode->integer )
		msec = (int)(msec * com_timescale->value);
	
	// don't let it scale below 1 msec
	if ( msec < 1 && com_timescale->value) {
		msec = 1;
	}

	if ( com_dedicated->integer ) {
		// dedicated servers don't want to clamp for a much longer
		// period, because it would mess up all the client's views
		// of time.
		if (com_sv_running->integer && msec > 500)
			Com_Printf( "Hitch warning: %i msec frame time\n", msec );

		clampTime = 5000;
	} else 
	if ( !com_sv_running->integer ) {
		// clients of remote servers do not want to clamp time, because
		// it would skew their view of the server's time temporarily
		clampTime = 5000;
	} else {
		// for local single player gaming
		// we may want to clamp the time to prevent players from
		// flying off edges when something hitches.
		clampTime = 200;
	}

	if ( msec > clampTime ) {
		msec = clampTime;
	}

	return msec;
}

int Com_TimeVal(int minMsec) {
	int timeVal;

	timeVal = Sys_Milliseconds() - com_frameTime;

	if(timeVal >= minMsec)
		timeVal = 0;
	else
		timeVal = minMsec - timeVal;

	return timeVal;
}

void Com_Frame( void ) {

	int		msec, minMsec;
	int		timeVal, timeValSV;
	static int	lastTime = 0, bias = 0;
 
	int		timeBeforeFirstEvents;
	int		timeBeforeServer;
	int		timeBeforeEvents;
	int		timeBeforeClient;
	int		timeAfter;
  

	if ( setjmp (abortframe) ) {
		return;			// an ERR_DROP was thrown
	}

	timeBeforeFirstEvents =0;
	timeBeforeServer =0;
	timeBeforeEvents =0;
	timeBeforeClient = 0;
	timeAfter = 0;

	// write config file if anything changed
	Com_WriteConfiguration(); 

	//
	// main event loop
	//
	if ( com_speeds->integer ) {
		timeBeforeFirstEvents = Sys_Milliseconds();
	}

	// Figure out how much time we have
	if(!com_timedemo->integer)
	{
		if(com_dedicated->integer)
			minMsec = SV_FrameMsec();
		else
		{
			if ( com_minimized->integer && com_frametimeMinimized->integer > 0 )
				minMsec = com_frametimeMinimized->integer;
			else if ( com_unfocused->integer && com_frametimeUnfocused->integer > 0 )
				minMsec = com_frametimeUnfocused->integer;
			else if ( com_frametime->integer > 0 )
				minMsec = com_frametime->integer;
			else
				minMsec = 1;
			
			timeVal = com_frameTime - lastTime;
			bias += timeVal - minMsec;
			
			if(bias > minMsec)
				bias = minMsec;
			
			// Adjust minMsec if previous frame took too long to render so
			// that framerate is stable at the requested value.
			minMsec -= bias;
		}
	}
	else
		minMsec = 1;

	do
	{
		if(com_sv_running->integer)
		{
			timeValSV = SV_SendQueuedPackets();
			
			timeVal = Com_TimeVal(minMsec);

			if(timeValSV < timeVal)
				timeVal = timeValSV;
		}
		else
			timeVal = Com_TimeVal(minMsec);
		
		if(com_busyWait->integer || timeVal < 1)
			NET_Sleep(0);
		else
			NET_Sleep(timeVal - 1);
	} while(Com_TimeVal(minMsec));
	
	lastTime = com_frameTime;
	com_frameTime = Com_EventLoop();
	
	msec = com_frameTime - lastTime;

	Cbuf_Execute();

	if (com_altivec->modified)
	{
		Com_DetectAltivec();
		com_altivec->modified = qfalse;
	}

	// mess with msec if needed
	msec = Com_ModifyMsec(msec);

	//
	// server side
	//
	if ( com_speeds->integer ) {
		timeBeforeServer = Sys_Milliseconds();
	}

	SV_Frame( msec );

	// if "dedicated" has been modified, start up
	// or shut down the client system.
	// Do this after the server may have started,
	// but before the client tries to auto-connect
	if ( com_dedicated->modified ) {
		// get the latched value
		Cvar_Get( "dedicated", "0", 0, NULL, NULL );
		com_dedicated->modified = qfalse;
		if ( !com_dedicated->integer ) {
			SV_Shutdown( "dedicated set to 0" );
			CL_FlushMemory();
		}
	}

#ifndef DEDICATED
	//
	// client system
	//
	//
	// run event loop a second time to get server to client packets
	// without a frame of latency
	//
	if ( com_speeds->integer ) {
		timeBeforeEvents = Sys_Milliseconds();
	}
	Com_EventLoop();
	Cbuf_Execute();


	//
	// client side
	//
	if ( com_speeds->integer ) {
		timeBeforeClient = Sys_Milliseconds();
	}

	CL_Frame( msec );

	if ( com_speeds->integer ) {
		timeAfter = Sys_Milliseconds();
	}
#else
	if ( com_speeds->integer ) {
		timeAfter = Sys_Milliseconds();
		timeBeforeEvents = timeAfter;
		timeBeforeClient = timeAfter;
	}
#endif


	NET_FlushPacketQueue();

	//
	// report timing information
	//
	if ( com_speeds->integer ) {
		int			all, sv, ev, cl;

		all = timeAfter - timeBeforeServer;
		sv = timeBeforeEvents - timeBeforeServer;
		ev = timeBeforeServer - timeBeforeFirstEvents + timeBeforeClient - timeBeforeEvents;
		cl = timeAfter - timeBeforeClient;
		sv -= time_game;
		cl -= time_frontend + time_backend;

		Com_Printf ("frame:%i all:%3i sv:%3i ev:%3i cl:%3i gm:%3i rf:%3i bk:%3i\n", 
					 com_frameNumber, all, sv, ev, cl, time_game, time_frontend, time_backend );
	}	

	//
	// trace optimization tracking
	//
	if ( com_showtrace->integer ) {
	
		extern	int c_traces, c_brush_traces, c_patch_traces;
		extern	int	c_pointcontents;

		Com_Printf ("%4i traces  (%ib %ip) %4i points\n", c_traces,
			c_brush_traces, c_patch_traces, c_pointcontents);
		c_traces = 0;
		c_brush_traces = 0;
		c_patch_traces = 0;
		c_pointcontents = 0;
	}

	Com_ReadFromPipe( );

	com_frameNumber++;
}

void Com_Shutdown( void ) {
	if (logfile) {
		FS_FCloseFile (logfile);
		logfile = 0;
	}

	if ( com_journalFile ) {
		FS_FCloseFile( com_journalFile );
		com_journalFile = 0;
	}

	if( pipefile ) {
		FS_FCloseFile( pipefile );
		FS_HomeRemove( com_pipefile->string );
	}

}

/*

	command line completion

*/

static const char *completionString;
static char shortestMatch[MAX_TOKEN_CHARS];
static int	matchCount;
// field we are working on, passed to Field_AutoComplete(&g_consoleCommand for instance)
static field_t *completionField;

static void FindMatches( const char *s ) {
	int		i;

	if ( Q_stricmpn( s, completionString, strlen( completionString ) ) ) {
		return;
	}
	matchCount++;
	if ( matchCount == 1 ) {
		Q_strncpyz( shortestMatch, s, sizeof( shortestMatch ) );
		return;
	}

	// cut shortestMatch to the amount common with s

	//QtZ: following code by Cgg
	//was wrong when s had fewer chars than shortestMatch
	i = 0;
	do {
		if ( tolower(shortestMatch[i]) != tolower(s[i]) ) {
			shortestMatch[i] = 0;
		}
	} while (s[i++]);
}

static void PrintMatches( const char *s ) {
	if ( !Q_stricmpn( s, shortestMatch, strlen( shortestMatch ) ) )
		Com_Printf( S_COLOR_GREY"Cmd  "S_COLOR_WHITE"%s\n", s );
}

#ifndef DEDICATED
static void PrintKeyMatches( const char *s ) {
	if ( !Q_stricmpn( s, shortestMatch, strlen( shortestMatch ) ) ) {
		Com_Printf( S_COLOR_GREY"Key  "S_COLOR_WHITE"%s\n", s );
	}
}
#endif

static void PrintCvarMatches( const char *s ) {
	char value[TRUNCATE_LENGTH] = {0};

	if ( !Q_stricmpn( s, shortestMatch, strlen( shortestMatch ) ) ) {
		Com_TruncateLongString( value, sizeof( value ), Cvar_VariableString( s ) );
		Com_Printf( S_COLOR_GREY"Cvar "S_COLOR_WHITE"%s = "S_COLOR_GREY"\""S_COLOR_WHITE"%s"S_COLOR_GREY"\""S_COLOR_WHITE"\n", s, value );
	}
}

static char *Field_FindFirstSeparator( char *s ) {
	int i;

	for( i = 0; i < strlen( s ); i++ )
	{
		if( s[ i ] == ';' )
			return &s[ i ];
	}

	return NULL;
}

static qboolean Field_Complete( void ) {
	ptrdiff_t completionOffset;

	if( matchCount == 0 )
		return qtrue;

	completionOffset = strlen( completionField->buffer ) - strlen( completionString );

	Q_strncpyz( &completionField->buffer[ completionOffset ], shortestMatch,
		sizeof( completionField->buffer ) - completionOffset );

	completionField->cursor = strlen( completionField->buffer );

	if( matchCount == 1 )
	{
		Q_strcat( completionField->buffer, sizeof( completionField->buffer ), " " );
		completionField->cursor++;
		return qtrue;
	}

	Com_Printf( "%c%s\n", CONSOLE_PROMPT_CHAR, completionField->buffer );

	return qfalse;
}

#ifndef DEDICATED
void Field_CompleteKeyname( void ) {
	matchCount = 0;
	shortestMatch[ 0 ] = 0;

	Key_KeynameCompletion( FindMatches );

	if( !Field_Complete( ) )
		Key_KeynameCompletion( PrintKeyMatches );
}
#endif

void Field_CompleteFilename( const char *dir, const char *ext, qboolean stripExt, qboolean allowNonPureFilesOnDisk ) {
	matchCount = 0;
	shortestMatch[ 0 ] = 0;

	FS_FilenameCompletion( dir, ext, stripExt, FindMatches, allowNonPureFilesOnDisk );

	if ( !Field_Complete() )
		FS_FilenameCompletion( dir, ext, stripExt, PrintMatches, allowNonPureFilesOnDisk );
}

void Field_CompleteCommand( char *cmd, qboolean doCommands, qboolean doCvars ) {
	int		completionArgument = 0;

	// Skip leading whitespace and quotes
	cmd = Com_SkipCharset( cmd, " \"" );

	Cmd_TokenizeStringIgnoreQuotes( cmd );
	completionArgument = Cmd_Argc( );

	// If there is trailing whitespace on the cmd
	if( *( cmd + strlen( cmd ) - 1 ) == ' ' )
	{
		completionString = "";
		completionArgument++;
	}
	else
		completionString = Cmd_Argv( completionArgument - 1 );

#ifndef DEDICATED
	// Unconditionally add a '\' to the start of the buffer
	if( completionField->buffer[ 0 ] &&
			completionField->buffer[ 0 ] != '\\' )
	{
		if( completionField->buffer[ 0 ] != '/' )
		{
			// Buffer is full, refuse to complete
			if( strlen( completionField->buffer ) + 1 >=
				sizeof( completionField->buffer ) )
				return;

			memmove( &completionField->buffer[ 1 ],
				&completionField->buffer[ 0 ],
				strlen( completionField->buffer ) + 1 );
			completionField->cursor++;
		}

		completionField->buffer[ 0 ] = '\\';
	}
#endif

	if( completionArgument > 1 )
	{
		const char *baseCmd = Cmd_Argv( 0 );
		char *p;

#ifndef DEDICATED
		// This should always be true
		if( baseCmd[ 0 ] == '\\' || baseCmd[ 0 ] == '/' )
			baseCmd++;
#endif

		if( ( p = Field_FindFirstSeparator( cmd ) ) )
			Field_CompleteCommand( p + 1, qtrue, qtrue ); // Compound command
		else
			Cmd_CompleteArgument( baseCmd, cmd, completionArgument ); 
	}
	else
	{
		if( completionString[0] == '\\' || completionString[0] == '/' )
			completionString++;

		matchCount = 0;
		shortestMatch[ 0 ] = 0;

		if( strlen( completionString ) == 0 )
			return;

		if( doCommands )
			Cmd_CommandCompletion( FindMatches );

		if( doCvars )
			Cvar_CommandCompletion( FindMatches );

		if( !Field_Complete( ) )
		{
			// run through again, printing matches
			if( doCommands )
				Cmd_CommandCompletion( PrintMatches );

			if( doCvars )
				Cvar_CommandCompletion( PrintCvarMatches );
		}
	}
}

// Perform Tab expansion
void Field_AutoComplete( field_t *field ) {
	//QtZ: Don't complete empty fields.
	if ( !field || !field->buffer[0] )
		return;

	completionField = field;

	Field_CompleteCommand( completionField->buffer, qtrue, qtrue );
}

// fills string array with len radom bytes, peferably from the OS randomizer
void Com_RandomBytes( byte *string, size_t len ) {
	unsigned int i;

	if( Sys_RandomBytes( string, len ) )
		return;

	Com_Printf( "Com_RandomBytes: using weak randomization\n" );
	for( i = 0; i < len; i++ )
		string[i] = (unsigned char)( rand() % 255 );
}


// Returns non-zero if given clientNum is enabled in voipTargets, zero otherwise.
//	If clientNum is negative return if any bit is set.
qboolean Com_IsVoipTarget(uint8_t *voipTargets, int voipTargetsSize, int clientNum) {
	int index;
	if(clientNum < 0)
	{
		for(index = 0; index < voipTargetsSize; index++)
		{
			if(voipTargets[index])
				return qtrue;
		}
		
		return qfalse;
	}

	index = clientNum >> 3;
	
	if(index < voipTargetsSize)
		return (voipTargets[index] & (1 << (clientNum & 0x07)));

	return qfalse;
}
