
#ifdef XCVAR_PROTO
	#define XCVAR_DEF( name, defVal, update, flags, desc ) extern cvar_t *name;
#endif

#ifdef XCVAR_DECL
	#define XCVAR_DEF( name, defVal, update, flags, desc ) cvar_t *name = NULL;
#endif

#ifdef XCVAR_LIST
	#define XCVAR_DEF( name, defVal, update, flags, desc ) { & name , #name , defVal , update , flags , desc },
#endif

//			name					default value	update callback			flags					description

XCVAR_DEF( capturelimit,			"8",			NULL,					CVAR_NONE,				NULL )
XCVAR_DEF( cg_brassTime,			"2500",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( cg_drawCrosshair,		"4",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( cg_drawCrosshairNames,	"1",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( cg_hudFiles,				"ui/hud.txt",	NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( cg_marks,				"1",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( cg_selectedPlayer,		"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( cg_selectedPlayerName,	"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( g_arenasFile,			"",				NULL,					CVAR_INIT|CVAR_ROM,		NULL )
XCVAR_DEF( g_botsFile,				"",				NULL,					CVAR_INIT|CVAR_ROM,		NULL )
XCVAR_DEF( g_difficulty,			"3",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( g_spAwards,				"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( g_spScores1,				"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( g_spScores2,				"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( g_spScores3,				"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( g_spScores4,				"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( g_spScores5,				"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( g_spVideos,				"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( g_warmup,				"20",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( server1,					"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( server10,				"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( server11,				"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( server12,				"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( server13,				"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( server14,				"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( server15,				"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( server16,				"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( server2,					"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( server3,					"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( server4,					"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( server5,					"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( server6,					"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( server7,					"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( server8,					"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( server9,					"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_bigFont,				"0.4",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_blueteam,				"Stroggs",		NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_blueteam1,			"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_blueteam2,			"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_blueteam3,			"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_blueteam4,			"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_blueteam5,			"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_browserMaster,		"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_browserShowEmpty,		"1",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_browserShowFull,		"1",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_captureLimit,			"5",			NULL,					CVAR_NONE,				NULL )
XCVAR_DEF( ui_ctf_capturelimit,		"8",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_ctf_friendly,			"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_ctf_timelimit,		"30",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_currentMap,			"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_currentNetMap,		"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_currentOpponent,		"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_debug,				"0",			NULL,					CVAR_TEMP,				NULL )
XCVAR_DEF( ui_dedicated,			"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_ffa_fraglimit,		"20",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_ffa_timelimit,		"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_findPlayer,			DEFAULT_NAME,	NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_fragLimit,			"10",			NULL,					CVAR_NONE,				NULL )
XCVAR_DEF( ui_gametype,				"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_initialized,			"0",			NULL,					CVAR_TEMP,				NULL )
XCVAR_DEF( ui_lastServerRefresh_0,	"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_lastServerRefresh_1,	"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_lastServerRefresh_2,	"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_lastServerRefresh_3,	"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_lastServerRefresh_4,	"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_lastServerRefresh_5,	"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_lastServerRefresh_6,	"",				NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_mapIndex,				"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_menuFiles,			"ui/menus.txt",	NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_netSource,			"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_opponentName,			"Stroggs",		NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_recordSPDemo,			"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_redteam,				"Pagans",		NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_redteam1,				"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_redteam2,				"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_redteam3,				"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_redteam4,				"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_redteam5,				"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_scoreAccuracy,		"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_scoreAssists,			"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_scoreBase,			"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_scoreCaptures,		"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_scoreDefends,			"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_scoreExcellents,		"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_scoreGauntlets,		"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_scoreImpressives,		"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_scorePerfect,			"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_scoreScore,			"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_scoreShutoutBonus,	"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_scoreSkillBonus,		"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_scoreTeam,			"0 to 0",		NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_scoreTime,			"00:00",		NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_scoreTimeBonus,		"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_selectedPlayer,		"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_serverFilterType,		"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_serverStatusTimeOut,	"7000",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_smallFont,			"0.25",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_spSelection,			"",				NULL,					CVAR_ROM,				NULL )
XCVAR_DEF( ui_teamName,				"Pagans",		NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_team_fraglimit,		"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_team_friendly,		"1",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_team_timelimit,		"20",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_tourney_fraglimit,	"0",			NULL,					CVAR_ARCHIVE,			NULL )
XCVAR_DEF( ui_tourney_timelimit,	"15",			NULL,					CVAR_ARCHIVE,			NULL )