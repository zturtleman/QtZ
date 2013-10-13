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
// tr_light.c

#include "tr_local.h"

#define	DLIGHT_AT_RADIUS		16
// at the edge of a dlight's influence, this amount of light will be added

#define	DLIGHT_MINIMUM_RADIUS	16		
// never calculate a range less than this to prevent huge light numbers


// Transforms the origins of an array of dlights.
//	Used by both the front end (for DlightBmodel) and the back end (before doing the lighting calculation)
void R_TransformDlights( int count, dlight_t *dl, orientationr_t *or) {
	int		i;
	vector3	temp;

	for ( i = 0 ; i < count ; i++, dl++ ) {
		VectorSubtract( &dl->origin, &or->origin, &temp );
		dl->transformed.x = DotProduct( &temp, &or->axis[0] );
		dl->transformed.y = DotProduct( &temp, &or->axis[1] );
		dl->transformed.z = DotProduct( &temp, &or->axis[2] );
	}
}

// Determine which dynamic lights may effect this bmodel
void R_DlightBmodel( bmodel_t *bmodel ) {
	int			i, j;
	dlight_t	*dl;
	int			mask;
	msurface_t	*surf;

	// transform all the lights
	R_TransformDlights( tr.refdef.num_dlights, tr.refdef.dlights, &tr.or );

	mask = 0;
	for ( i=0 ; i<tr.refdef.num_dlights ; i++ ) {
		dl = &tr.refdef.dlights[i];

		// see if the point is close enough to the bounds to matter
		for ( j=0; j<3; j++ ) {
			if ( dl->transformed.data[j] - bmodel->bounds[1].data[j] > dl->radius )
				break;
			if ( bmodel->bounds[0].data[j] - dl->transformed.data[j] > dl->radius )
				break;
		}
		if ( j < 3 )
			continue;

		// we need to check this light
		mask |= 1 << i;
	}

	tr.currentEntity->needDlights = (mask != 0);

	// set the dlight bits in all the surfaces
	for ( i = 0 ; i < bmodel->numSurfaces ; i++ ) {
		surf = bmodel->firstSurface + i;

			 if ( *surf->data == SF_FACE )		((srfSurfaceFace_t *)	surf->data)->dlightBits = mask;
		else if ( *surf->data == SF_GRID )		((srfGridMesh_t *)		surf->data)->dlightBits = mask;
		else if ( *surf->data == SF_TRIANGLES )	((srfTriangles_t *)		surf->data)->dlightBits = mask;
	}
}


/*

	LIGHT SAMPLING

*/

extern	cvar_t	*r_ambientScale;
extern	cvar_t	*r_directedScale;
extern	cvar_t	*r_debugLight;

static void R_SetupEntityLightingGrid( trRefEntity_t *ent ) {
	vector3	lightOrigin;
	int		pos[3];
	int		i, j;
	byte	*gridData;
	float	frac[3];
	int		gridStep[3];
	vector3	direction;
	float	totalFactor;

	if ( ent->e.renderfx & RF_LIGHTING_ORIGIN ) {
		// seperate lightOrigins are needed so an object that is
		// sinking into the ground can still be lit, and so
		// multi-part models can be lit identically
		VectorCopy( &ent->e.lightingOrigin, &lightOrigin );
	} else {
		VectorCopy( &ent->e.origin, &lightOrigin );
	}

	VectorSubtract( &lightOrigin, &tr.world->lightGridOrigin, &lightOrigin );
	for ( i = 0 ; i < 3 ; i++ ) {
		float	v;

		v = lightOrigin.data[i]*tr.world->lightGridInverseSize.data[i];
		pos[i] = (int)floorf( v );
		frac[i] = v - pos[i];
		if ( pos[i] < 0 ) {
			pos[i] = 0;
		} else if ( pos[i] >= tr.world->lightGridBounds.data[i] - 1 ) {
			pos[i] = tr.world->lightGridBounds.data[i] - 1;
		}
	}

	VectorClear( &ent->ambientLight );
	VectorClear( &ent->directedLight );
	VectorClear( &direction );

	assert( tr.world->lightGridData ); // NULL with -nolight maps

	// trilerp the light value
	gridStep[0] = 8;
	gridStep[1] = 8 * tr.world->lightGridBounds.x;
	gridStep[2] = 8 * tr.world->lightGridBounds.x * tr.world->lightGridBounds.y;
	gridData = tr.world->lightGridData + pos[0] * gridStep[0] + pos[1] * gridStep[1] + pos[2] * gridStep[2];

	totalFactor = 0;
	for ( i = 0 ; i < 8 ; i++ ) {
		float	factor;
		byte	*data;
		int		lat, lng;
		vector3	normal;
		#ifdef idppc
		float d0, d1, d2, d3, d4, d5;
		#endif
		factor = 1.0;
		data = gridData;
		for ( j = 0 ; j < 3 ; j++ ) {
			if ( i & (1<<j) ) {
				factor *= frac[j];
				data += gridStep[j];
			} else {
				factor *= (1.0f - frac[j]);
			}
		}

		if ( !(data[0]+data[1]+data[2]) ) {
			continue;	// ignore samples in walls
		}
		totalFactor += factor;
		#ifdef idppc
		d0 = data[0]; d1 = data[1]; d2 = data[2];
		d3 = data[3]; d4 = data[4]; d5 = data[5];

		ent->ambientLight[0] += factor * d0;
		ent->ambientLight[1] += factor * d1;
		ent->ambientLight[2] += factor * d2;

		ent->directedLight[0] += factor * d3;
		ent->directedLight[1] += factor * d4;
		ent->directedLight[2] += factor * d5;
		#else
		ent->ambientLight.r += factor * data[0];
		ent->ambientLight.g += factor * data[1];
		ent->ambientLight.b += factor * data[2];

		ent->directedLight.r += factor * data[3];
		ent->directedLight.g += factor * data[4];
		ent->directedLight.b += factor * data[5];
		#endif
		lat = data[7];
		lng = data[6];
		lat *= (FUNCTABLE_SIZE/256);
		lng *= (FUNCTABLE_SIZE/256);

		// decode X as cos( lat ) * sin( long )
		// decode Y as sin( lat ) * sin( long )
		// decode Z as cos( long )

		normal.x = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
		normal.y = tr.sinTable[lat] * tr.sinTable[lng];
		normal.z = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

		VectorMA( &direction, factor, &normal, &direction );
	}

	if ( totalFactor > 0 && totalFactor < 0.99 ) {
		totalFactor = 1.0f / totalFactor;
		VectorScale( &ent->ambientLight, totalFactor, &ent->ambientLight );
		VectorScale( &ent->directedLight, totalFactor, &ent->directedLight );
	}

	VectorScale( &ent->ambientLight, r_ambientScale->value, &ent->ambientLight );
	VectorScale( &ent->directedLight, r_directedScale->value, &ent->directedLight );

	VectorNormalize2( &direction, &ent->lightDir );
}

static void LogLight( trRefEntity_t *ent ) {
	float max1, max2;

	if ( !(ent->e.renderfx & RF_FIRST_PERSON ) )
		return;

	max1 = ent->ambientLight.r;
		 if ( ent->ambientLight.g > max1 )		max1 = ent->ambientLight.g;
	else if ( ent->ambientLight.b > max1 )		max1 = ent->ambientLight.b;

	max2 = ent->directedLight.r;
		 if ( ent->directedLight.g > max2 )	max2 = ent->directedLight.g;
	else if ( ent->directedLight.b > max2 )	max2 = ent->directedLight.b;

	ri->Printf( PRINT_ALL, "amb:%.2f  dir:%.2f\n", max1, max2 );
}

// Calculates all the lighting values that will be used by the Calc_* functions
void R_SetupEntityLighting( const trRefdef_t *refdef, trRefEntity_t *ent ) {
	int				i;
	dlight_t		*dl;
	float			power;
	vector3			dir;
	float			d;
	vector3			lightDir;
	vector3			lightOrigin;

	// lighting calculations 
	if ( ent->lightingCalculated ) {
		return;
	}
	ent->lightingCalculated = qtrue;

	//
	// trace a sample point down to find ambient light
	//
	if ( ent->e.renderfx & RF_LIGHTING_ORIGIN ) {
		// seperate lightOrigins are needed so an object that is
		// sinking into the ground can still be lit, and so
		// multi-part models can be lit identically
		VectorCopy( &ent->e.lightingOrigin, &lightOrigin );
	} else {
		VectorCopy( &ent->e.origin, &lightOrigin );
	}

	// if NOWORLDMODEL, only use dynamic lights (menu system, etc)
	if ( !(refdef->rdflags & RDF_NOWORLDMODEL ) 
		&& tr.world->lightGridData ) {
		R_SetupEntityLightingGrid( ent );
	} else {
		ent->ambientLight.r = ent->ambientLight.g = ent->ambientLight.b = tr.identityLight * 150;
		ent->directedLight.r = ent->directedLight.g = ent->directedLight.b = tr.identityLight * 150;
		VectorCopy( &tr.sunDirection, &ent->lightDir );
	}

	// bonus items and view weapons have a fixed minimum add
	if ( 1 /* ent->e.renderfx & RF_MINLIGHT */ ) {
		// give everything a minimum light add
		ent->ambientLight.r += tr.identityLight * 32;
		ent->ambientLight.g += tr.identityLight * 32;
		ent->ambientLight.b += tr.identityLight * 32;
	}

	//RAZTODO: RF_RGB_TINT
	//	Or just rgbgen entity shaders..?
#if 0
	if ( ent->e.renderfx & RF_RGB_TINT )
	{
		ent->ambientLight[0] = ent->e.shaderRGBA[0];
		ent->ambientLight[1] = ent->e.shaderRGBA[1];
		ent->ambientLight[2] = ent->e.shaderRGBA[2];
	}
#endif

	//
	// modify the light by dynamic lights
	//
	d = VectorLength( &ent->directedLight );
	VectorScale( &ent->lightDir, d, &lightDir );

	for ( i = 0 ; i < refdef->num_dlights ; i++ ) {
		dl = &refdef->dlights[i];
		VectorSubtract( &dl->origin, &lightOrigin, &dir );
		d = VectorNormalize( &dir );

		power = DLIGHT_AT_RADIUS * ( dl->radius * dl->radius );
		if ( d < DLIGHT_MINIMUM_RADIUS ) {
			d = DLIGHT_MINIMUM_RADIUS;
		}
		d = power / ( d * d );

		VectorMA( &ent->directedLight, d, &dl->color, &ent->directedLight );
		VectorMA( &lightDir, d, &dir, &lightDir );
	}

	// clamp ambient
	for ( i=0; i<3; i++ ) {
		if (ent->ambientLight.data[i] > (float)tr.identityLightByte)
			ent->ambientLight.data[i] = (float)tr.identityLightByte;
	}

	if ( r_debugLight->integer ) {
		LogLight( ent );
	}

	// save out the byte packet version
	((byte *)&ent->ambientLightInt)[0] = (byte)ri->ftol(ent->ambientLight.r);
	((byte *)&ent->ambientLightInt)[1] = (byte)ri->ftol(ent->ambientLight.g);
	((byte *)&ent->ambientLightInt)[2] = (byte)ri->ftol(ent->ambientLight.b);
	((byte *)&ent->ambientLightInt)[3] = 0xff;
	
	// transform the direction to local space
	VectorNormalize( &lightDir );
	ent->lightDir.x = DotProduct( &lightDir, &ent->e.axis[0] );
	ent->lightDir.y = DotProduct( &lightDir, &ent->e.axis[1] );
	ent->lightDir.z = DotProduct( &lightDir, &ent->e.axis[2] );
}

int R_LightForPoint( vector3 *point, vector3 *ambientLight, vector3 *directedLight, vector3 *lightDir ) {
	trRefEntity_t ent;
	
	if ( tr.world->lightGridData == NULL )
	  return qfalse;

	memset(&ent, 0, sizeof(ent));
	VectorCopy( point, &ent.e.origin );
	R_SetupEntityLightingGrid( &ent );
	VectorCopy(&ent.ambientLight, ambientLight);
	VectorCopy(&ent.directedLight, directedLight);
	VectorCopy(&ent.lightDir, lightDir);

	return qtrue;
}
