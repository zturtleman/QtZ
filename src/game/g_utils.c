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
//
// g_utils.c -- misc utility functions for game module

#include "g_local.h"

/*

	model / sound configstring indexes

*/

int G_FindConfigstringIndex( char *name, int start, int max, qboolean create ) {
	int		i;
	char	s[MAX_STRING_CHARS];

	if ( !name || !name[0] ) {
		return 0;
	}

	for ( i=1 ; i<max ; i++ ) {
		trap->SV_GetConfigstring( start + i, s, sizeof( s ) );
		if ( !s[0] ) {
			break;
		}
		if ( !strcmp( s, name ) ) {
			return i;
		}
	}

	if ( !create ) {
		return 0;
	}

	if ( i == max ) {
		trap->Error( ERR_DROP, "G_FindConfigstringIndex: overflow" );
	}

	trap->SV_SetConfigstring( start + i, name );

	return i;
}


int G_ModelIndex( char *name ) {
	return G_FindConfigstringIndex (name, CS_MODELS, MAX_MODELS, qtrue);
}

int G_SoundIndex( char *name ) {
	return G_FindConfigstringIndex (name, CS_SOUNDS, MAX_SOUNDS, qtrue);
}

// Broadcasts a command to only a specific team
void G_TeamCommand( team_t team, const char *cmd ) {
	int		i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected == CON_CONNECTED ) {
			if ( level.clients[i].sess.sessionTeam == team ) {
				trap->SV_GameSendServerCommand( i, va("%s", cmd ));
			}
		}
	}
}

// Searches all active entities for the next one that holds the matching string at fieldofs (use the FOFS() macro) in the structure.
//	Searches beginning at the entity after from, or the beginning if NULL
//	NULL will be returned if the end of the list is reached.
gentity_t *G_Find (gentity_t *from, int fieldofs, const char *match) {
	char	*s;

	if (!from)
		from = g_entities;
	else
		from++;

	for ( ; from < &g_entities[level.num_entities] ; from++)
	{
		if (!from->inuse)
			continue;
		s = *(char **) ((byte *)from + fieldofs);
		if (!s)
			continue;
		if (!Q_stricmp (s, match))
			return from;
	}

	return NULL;
}

#define MAXCHOICES	32

// Selects a random entity from among the targets
gentity_t *G_PickTarget (char *targetname) {
	gentity_t	*ent = NULL;
	int		num_choices = 0;
	gentity_t	*choice[MAXCHOICES];

	if (!targetname)
	{
		trap->Print("G_PickTarget called with NULL targetname\n");
		return NULL;
	}

	while(1)
	{
		ent = G_Find (ent, FOFS(targetname), targetname);
		if (!ent)
			break;
		choice[num_choices++] = ent;
		if (num_choices == MAXCHOICES)
			break;
	}

	if (!num_choices)
	{
		trap->Print("G_PickTarget: target %s not found\n", targetname);
		return NULL;
	}

	return choice[rand() % num_choices];
}

// "activator" should be set to the entity that initiated the firing.
//	Search for (string)targetname in all entities that match (string)self.target and call their .use function
void G_UseTargets( gentity_t *ent, gentity_t *activator ) {
	gentity_t		*t;
	
	if ( !ent ) {
		return;
	}

	if ( !ent->target ) {
		return;
	}

	t = NULL;
	while ( (t = G_Find (t, FOFS(targetname), ent->target)) != NULL ) {
		if ( t == ent ) {
			trap->Print ("WARNING: Entity used itself.\n");
		} else {
			if ( t->use ) {
				t->use (t, ent, activator);
			}
		}
		if ( !ent->inuse ) {
			trap->Print("entity was removed while using targets\n");
			return;
		}
	}
}

// The editor only specifies a single value for angles (yaw), but we have special constants to generate an up or down direction.
//	Angles will be cleared, because it is being used to represent a direction instead of an orientation.
void G_SetMovedir( vector3 *angles, vector3 *movedir ) {
	static vector3 VEC_UP		= { 0, -1,  0};
	static vector3 MOVEDIR_UP	= { 0,  0,  1};
	static vector3 VEC_DOWN		= { 0, -2,  0};
	static vector3 MOVEDIR_DOWN	= { 0,  0, -1};

	if ( VectorCompare( angles, &VEC_UP ) )
		VectorCopy( &MOVEDIR_UP, movedir );
	else if ( VectorCompare( angles, &VEC_DOWN ) )
		VectorCopy( &MOVEDIR_DOWN, movedir );
	else
		AngleVectors( angles, movedir, NULL, NULL );

	VectorClear( angles );
}

float vectoyaw( const vector3 *vec ) {
	float	yaw;
	
	if (vec->yaw == 0 && vec->pitch == 0) {
		yaw = 0;
	} else {
		if (vec->pitch) {
			yaw = ( atan2f( vec->yaw, vec->pitch) * 180 / M_PI );
		} else if (vec->yaw > 0) {
			yaw = 90;
		} else {
			yaw = 270;
		}
		if (yaw < 0) {
			yaw += 360;
		}
	}

	return yaw;
}


void G_InitGentity( gentity_t *e ) {
	e->inuse = qtrue;
	e->classname = "noclass";
	e->s.number = ARRAY_INDEX( g_entities, e );
	e->r.ownerNum = ENTITYNUM_NONE;
}

// Either finds a free entity, or allocates a new one.
//	The slots from 0 to MAX_CLIENTS-1 are always reserved for clients, and will never be used by anything else.
//	Try to avoid reusing an entity that was recently freed, because it can cause the client to think the entity
//	morphed into something else instead of being removed and recreated, which can cause interpolated angles and bad trails.
gentity_t *G_Spawn( void ) {
	int			i, force;
	gentity_t	*e;

	e = NULL;	// shut up warning
	i = 0;		// shut up warning
	for ( force = 0 ; force < 2 ; force++ ) {
		// if we go through all entities and can't find one to free,
		// override the normal minimum times before use
		e = &g_entities[MAX_CLIENTS];
		for ( i = MAX_CLIENTS ; i<level.num_entities ; i++, e++) {
			if ( e->inuse ) {
				continue;
			}

			// the first couple seconds of server time can involve a lot of
			// freeing and allocating, so relax the replacement policy
			if ( !force && e->freetime > level.startTime + 2000 && level.time - e->freetime < 1000 ) {
				continue;
			}

			// reuse this slot
			G_InitGentity( e );
			return e;
		}
		if ( i != MAX_GENTITIES ) {
			break;
		}
	}
	if ( i == ENTITYNUM_MAX_NORMAL ) {
		for (i = 0; i < MAX_GENTITIES; i++) {
			trap->Print("%4i: %s\n", i, g_entities[i].classname);
		}
		trap->Error( ERR_DROP, "G_Spawn: no free entities" );
	}
	
	// open up a new slot
	level.num_entities++;

	// let the server system know that there are more entities
	trap->SV_LocateGameData( (sharedEntity_t *)level.gentities, level.num_entities, sizeof( gentity_t ), 
		&level.clients[0].ps, sizeof( level.clients[0] ) );

	G_InitGentity( e );
	return e;
}

qboolean G_EntitiesFree( void ) {
	int			i;
	gentity_t	*e;

	e = &g_entities[MAX_CLIENTS];
	for ( i = MAX_CLIENTS; i < level.num_entities; i++, e++) {
		if ( e->inuse ) {
			continue;
		}
		// slot available
		return qtrue;
	}
	return qfalse;
}

// Marks the entity as free
void G_FreeEntity( gentity_t *ed ) {
	trap->SV_UnlinkEntity ((sharedEntity_t *)ed);		// unlink from world

	if ( ed->neverFree ) {
		return;
	}

	memset (ed, 0, sizeof(*ed));
	ed->classname = "freed";
	ed->freetime = level.time;
	ed->inuse = qfalse;
}

// Spawns an event entity that will be auto-removed
//	The origin will be snapped to save net bandwidth, so care must be taken if the
//	origin is right on a surface (snap towards start vector first)
gentity_t *G_TempEntity( vector3 *origin, int event ) {
	gentity_t		*e;
	vector3		snapped;

	e = G_Spawn();
	e->s.eType = ET_EVENTS + event;

	e->classname = "tempEntity";
	e->eventTime = level.time;
	e->freeAfterEvent = qtrue;

	VectorCopy( origin, &snapped );
	VectorSnap( &snapped );		// save network bandwidth
	G_SetOrigin( e, &snapped );

	// find cluster for PVS
	trap->SV_LinkEntity( (sharedEntity_t *)e );

	return e;
}



/*

	Kill box

*/

// Kills all entities that would touch the proposed new positioning of ent.
//	Ent should be unlinked before calling this!
void G_KillBox (gentity_t *ent) {
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	vector3		mins, maxs;

	VectorAdd( &ent->client->ps.origin, &ent->r.mins, &mins );
	VectorAdd( &ent->client->ps.origin, &ent->r.maxs, &maxs );
	num = trap->SV_AreaEntities( &mins, &maxs, touch, MAX_GENTITIES );

	for (i=0 ; i<num ; i++) {
		hit = &g_entities[touch[i]];
		if ( !hit->client ) {
			continue;
		}

		// nail it
		G_Damage ( hit, ent, ent, NULL, NULL, NULL, 100000, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);
	}

}

// Use for non-pmove events that would also be predicted on the client side: jumppads and item pickups
//	Adds an event+parm and twiddles the event counter
void G_AddPredictableEvent( gentity_t *ent, int event, int eventParm ) {
	if ( !ent->client )
		return;

	BG_AddPredictableEventToPlayerstate( event, eventParm, &ent->client->ps );
}

// Adds an event+parm and twiddles the event counter
void G_AddEvent( gentity_t *ent, int event, int eventParm ) {
	int		bits;

	if ( !event ) {
		trap->Print( "G_AddEvent: zero event added for entity %i\n", ent->s.number );
		return;
	}

	// clients need to add the event in playerState_t instead of entityState_t
	if ( ent->client ) {
		bits = ent->client->ps.externalEvent & EV_EVENT_BITS;
		bits = ( bits + EV_EVENT_BIT1 ) & EV_EVENT_BITS;
		ent->client->ps.externalEvent = event | bits;
		ent->client->ps.externalEventParm = eventParm;
		ent->client->ps.externalEventTime = level.time;
	} else {
		bits = ent->s.event & EV_EVENT_BITS;
		bits = ( bits + EV_EVENT_BIT1 ) & EV_EVENT_BITS;
		ent->s.event = event | bits;
		ent->s.eventParm = eventParm;
	}
	ent->eventTime = level.time;
}

void G_Sound( gentity_t *ent, int channel, int soundIndex ) {
	gentity_t	*te;

	te = G_TempEntity( &ent->r.currentOrigin, EV_GENERAL_SOUND );
	te->s.eventParm = soundIndex;
}

// Sets the pos trajectory for a fixed position
void G_SetOrigin( gentity_t *ent, vector3 *origin ) {
	VectorCopy( origin, &ent->s.pos.trBase );
	ent->s.pos.trType = TR_STATIONARY;
	ent->s.pos.trTime = 0;
	ent->s.pos.trDuration = 0;
	VectorClear( &ent->s.pos.trDelta );

	VectorCopy( origin, &ent->r.currentOrigin );
}

// debug polygons only work when running a local game with r_debugSurface set to 2
int DebugLine(vector3 *start, vector3 *end, int color) {
	vector3 points[4], dir, cross, up = {0, 0, 1};
	float dot;

	VectorCopy(start, &points[0]);
	VectorCopy(start, &points[1]);
	//points[1][2] -= 2;
	VectorCopy(end, &points[2]);
	//points[2][2] -= 2;
	VectorCopy(end, &points[3]);


	VectorSubtract(end, start, &dir);
	VectorNormalize(&dir);
	dot = DotProduct(&dir, &up);
	if (dot > 0.99f || dot < -0.99f) VectorSet(&cross, 1, 0, 0);
	else CrossProduct(&dir, &up, &cross);

	VectorNormalize(&cross);

	VectorMA(&points[0],  2, &cross, &points[0]);
	VectorMA(&points[1], -2, &cross, &points[1]);
	VectorMA(&points[2], -2, &cross, &points[2]);
	VectorMA(&points[3],  2, &cross, &points[3]);

	return trap->DebugPolygonCreate(color, 4, points);
}
