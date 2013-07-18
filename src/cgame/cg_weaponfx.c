#include "cg_local.h"
#include "cg_weaponfx.h"

//================================
//	QUANTIZER
//================================

void FX_Quantizer_Missile( centity_t *cent, const struct weaponInfo_s *weapon )
{
	vector3 forward = { 0.0f };

	if ( VectorNormalize2( &cent->currentState.pos.trDelta, &forward ) == 0.0f )
		forward.z = 1.0f;

//	cgi.FX_PlayEffectID( cgs.effects.weapons.quantizerShotEffect, cent->lerpOrigin, forward, -1, -1 );
	cgi.R_AddLightToScene( &cent->lerpOrigin, 200, 0.13f, 0.13f, 0.87f );
}

void FX_Quantizer_HitWall( centity_t *cent, const struct weaponInfo_s *weapon, vector3 *origin, vector3 *dir )
{
//	cgi.FX_PlayEffectID( cgs.effects.weapons.quantizerHitWallEffect, origin, dir, -1, -1 );
}

void FX_Quantizer_HitPlayer( centity_t *cent, const struct weaponInfo_s *weapon, vector3 *origin, vector3 *dir )
{
//	cgi.FX_PlayEffectID( cgs.effects.weapons.quantizerHitPlayerEffect, origin, dir, -1, -1 );
}


//================================
//	REPEATER
//================================

void FX_Repeater_Missile( centity_t *cent, const struct weaponInfo_s *weapon )
{
	vector3 forward = { 0.0f };

	if ( VectorNormalize2( &cent->currentState.pos.trDelta, &forward ) == 0.0f )
		forward.z = 1.0f;

//	cgi.FX_PlayEffectID( cgs.effects.weapons.repeaterShotEffect, cent->lerpOrigin, forward, -1, -1 );
	cgi.R_AddLightToScene( &cent->lerpOrigin, 200, 0.87f, 0.13f, 0.13f );
}

void FX_Repeater_HitWall(  centity_t *cent, const struct weaponInfo_s *weapon, vector3 *origin, vector3 *dir )
{
//	cgi.FX_PlayEffectID( cgs.effects.weapons.repeaterHitWallEffect, origin, dir, -1, -1 );
}

void FX_Repeater_HitPlayer( centity_t *cent, const struct weaponInfo_s *weapon, vector3 *origin, vector3 *dir )
{
//	cgi.FX_PlayEffectID( cgs.effects.weapons.repeaterHitPlayerEffect, origin, dir, -1, -1 );
}


void FX_Splicer_Beam( centity_t *cent, vector3 *start, vector3 *end ) {
	trace_t  trace;
	refEntity_t  beam;
	vector3   forward;
	vector3   muzzlePoint, endPoint;
	int      anim;
	int		range = weaponData[WP_SPLICER].range;

	if ( cent->currentState.weapon != WP_SPLICER )
		return;

	memset( &beam, 0, sizeof( beam ) );

	if ( cent->currentState.number == cg.predictedPlayerState.clientNum && cg_delagHitscan.integer )
	{// unlagged
		// always shoot straight forward from our current position
		AngleVectors( &cg.predictedPlayerState.viewangles, &forward, NULL, NULL );
		VectorCopy( &cg.predictedPlayerState.origin, &muzzlePoint );
	}
	else if ((cent->currentState.number == cg.predictedPlayerState.clientNum) && (cg_trueLightning.value != 0))
	{// CPMA  "true" lightning
		vector3 angle;
		int i;
		// unlagged
		vector3 viewangles;
		VectorCopy( &cg.predictedPlayerState.viewangles, &viewangles );
		// ~unlagged

		for (i = 0; i < 3; i++) {
			float a = cent->lerpAngles.data[i] - viewangles.data[i];
			if (a > 180) {
				a -= 360;
			}
			if (a < -180) {
				a += 360;
			}

			angle.data[i] = viewangles.data[i] + a * (1.0f - cg_trueLightning.value);
			if (angle.data[i] < 0) {
				angle.data[i] += 360;
			}
			if (angle.data[i] > 360) {
				angle.data[i] -= 360;
			}
		}

		AngleVectors(&angle, &forward, NULL, NULL );
		// unlagged
		VectorCopy(&cg.predictedPlayerState.origin, &muzzlePoint );
//		VectorCopy(cent->lerpOrigin, muzzlePoint );
//		VectorCopy(cg.refdef.vieworg, muzzlePoint );
	} else {
		// !CPMA
		AngleVectors( &cent->lerpAngles, &forward, NULL, NULL );
		VectorCopy(&cent->lerpOrigin, &muzzlePoint );
	}

	anim = cent->currentState.legsAnim & ~ANIM_TOGGLEBIT;
	if ( anim == LEGS_WALKCR || anim == LEGS_IDLECR )
		muzzlePoint.z += CROUCH_VIEWHEIGHT;
	else
		muzzlePoint.z += DEFAULT_VIEWHEIGHT;

	VectorMA( &muzzlePoint, 14, &forward, &muzzlePoint );

	// project forward by the lightning range
	VectorMA( &muzzlePoint, (float)range, &forward, &endPoint );

	// see if it hit a wall
	CG_Trace( &trace, &muzzlePoint, &vec3_origin, &vec3_origin, &endPoint, cent->currentState.number, MASK_SHOT );

	// this is the endpoint
	VectorCopy( &trace.endpos, &beam.oldorigin );

	// use the provided origin, even though it may be slightly
	// different than the muzzle origin
	VectorCopy( start, &beam.origin );

	beam.reType = RT_ELECTRICITY;
	beam.customShader = cgs.media.weapons.splicerCore;
	cgi.R_AddRefEntityToScene( &beam );

	// add the impact flare if it hit something
	if ( trace.fraction < 1.0 ) {
		refEntity_t flare;

		flare.reType = RT_SPRITE;
		flare.radius = 6.0f;
		flare.customShader = cgs.media.weapons.splicerFlare;
		VectorCopy( &trace.endpos, &flare.origin );

		cgi.R_AddRefEntityToScene( &flare );
	}
}

void FX_Splicer_HitWall( centity_t *cent, const struct weaponInfo_s *weapon, vector3 *origin, vector3 *dir )
{
//	cgi.FX_PlayEffectID( cgs.effects.weapons.splicerHitWallEffect, origin, dir, -1, -1 );
}

void FX_Splicer_HitPlayer( centity_t *cent, const struct weaponInfo_s *weapon, vector3 *origin, vector3 *dir )
{
//	cgi.FX_PlayEffectID( cgs.effects.weapons.splicerHitPlayerEffect, origin, dir, -1, -1 );
}


//================================
//	MORTAR
//================================

void FX_Mortar_Missile( centity_t *cent, const struct weaponInfo_s *weapon )
{
	refEntity_t ent;
	vector3 ang;
	float vLen;
	float scale = 1.0f;
	refdef_t *refdef = &cg.refdef;

	memset( &ent, 0, sizeof( ent ) );

	VectorCopy( &cent->lerpOrigin, &ent.origin );

	VectorSubtract( &ent.origin, &refdef->vieworg, &ent.axis[0] );
	vLen = VectorLength( &ent.axis[0] );
	if ( VectorNormalize( &ent.axis[0] ) <= 0.1f )
		return;

//	VectorCopy(refdef->viewaxis[2], ent.axis[2]);
//	CrossProduct(ent.axis[0], ent.axis[2], ent.axis[1]);
	vectoangles( &ent.axis[0], &ang );
	ang.roll = (float)cent->miscTime;
	cent->miscTime += 12; //spin the half-sphere to give a "screwdriver" effect
	AnglesToAxis( &ang, ent.axis );

	//radius must be a power of 2, and is the actual captured texture size
//	if (vLen < 128)			ent.radius = 256;
//	else if (vLen < 256)	ent.radius = 128;
//	else if (vLen < 512)	ent.radius = 64;
//	else					ent.radius = 32;
	scale = 0.37f;
	VectorScale( &ent.axis[0],  scale, &ent.axis[0] );
	VectorScale( &ent.axis[1],  scale, &ent.axis[1] );
	VectorScale( &ent.axis[2], -scale, &ent.axis[2] );

	ent.hModel = cgs.media.weapons.mortarProjectile; //RAZTODO: mortar projectile model

	MAKERGB( ent.shaderRGBA, 128, 43, 255 );
	ent.shaderRGBA[3] = 48;
	ent.customShader = cgi.R_RegisterShaderNoMip( "gfx/effects/shock_ripple" );
//	ent.renderfx = (RF_RGB_TINT|RF_FORCE_ENT_ALPHA);
	cgi.R_AddRefEntityToScene( &ent );

	MAKERGB( ent.shaderRGBA, 255, 255, 255 );
	ent.shaderRGBA[3] = 255;
	ent.customShader	= cgi.R_RegisterShaderNoMip( "gfx/effects/caustic1" );
//	ent.renderfx		= (RF_RGB_TINT|RF_MINLIGHT);
	cgi.R_AddRefEntityToScene( &ent );

	scale = (1.15f + Q_fabs( sinf( cg.time / 80.0f ) )*0.65f);
	VectorScale( &ent.axis[0], scale, &ent.axis[0] );
	VectorScale( &ent.axis[1], scale, &ent.axis[1] );
	VectorScale( &ent.axis[2], scale, &ent.axis[2] );
	MAKERGB( ent.shaderRGBA, 51, 119, 255 );
	ent.customShader = cgi.R_RegisterShaderNoMip( "gfx/effects/eplosion_wave" );
//	ent.renderfx = (RF_RGB_TINT|RF_ALPHA_DEPTH);
	cgi.R_AddRefEntityToScene( &ent );

	cgi.R_AddLightToScene( &cent->lerpOrigin, 400, 0.13f, 0.43f, 0.87f );
}

void FX_Mortar_HitWall( centity_t *cent, const struct weaponInfo_s *weapon, vector3 *origin, vector3 *dir )
{
//	cgi.FX_PlayEffectID( cgs.effects.weapons.mortarHitWallEffect, origin, dir, -1, -1 );
}

void FX_Mortar_HitPlayer( centity_t *cent, const struct weaponInfo_s *weapon, vector3 *origin, vector3 *dir )
{
//	cgi.FX_PlayEffectID( cgs.effects.weapons.mortarHitPlayerEffect, origin, dir, -1, -1 );
}


//================================
//	DIVERGENCE
//================================

void FX_Divergence_Fire( centity_t *cent, vector3 *start, vector3 *end )
{
	localEntity_t	*leCore = CG_AllocLocalEntity(),	*leGlow = CG_AllocLocalEntity(),	*leMuzzle = CG_AllocLocalEntity();
	refEntity_t		*reCore = &leCore->refEntity,		*reGlow = &leGlow->refEntity,		*reMuzzle = &leMuzzle->refEntity;
	clientInfo_t	*ci = &cgs.clientinfo[cent->currentState.number];
	int				trailShader = cgi.R_RegisterShader( "divergenceCore" ),
				//	particleShader = cgi.R_RegisterShader( "gfx/effects/caustic1" ),
					muzzleShader = cgi.R_RegisterShader( "gfx/efx/wp/divergence/muzzle" );
	vector4 color = { 1.0f };
	float			trailTime = 250.0f;

	if ( cgs.gametype >= GT_TEAM )
	{
		if ( ci->team == TEAM_RED )
			VectorSet4( &color, 1.0f, 0.0f, 0.0f, 1.0f );
		else if ( ci->team == TEAM_BLUE )
			VectorSet4( &color, 0.0f, 0.0f, 1.0f, 1.0f );
		else
			VectorSet4( &color, 0.0f, 0.878431f, 1.0f, 1.0f );
	}
	else {
		VectorCopy( &ci->color2, (vector3 *)&color );
		color.a = 1.0f;
	}

	//MUZZLE
	leMuzzle->leFlags = LEF_PUFF_DONT_SCALE;
	leMuzzle->leType = LE_FADE_RGB;//LE_SCALE_FADE;
	leMuzzle->startTime = cg.time;
	leMuzzle->endTime = cg.time + (int)trailTime;
	leMuzzle->lifeRate = 1.0f / (leMuzzle->endTime - leMuzzle->startTime);

	reMuzzle->shaderTime = cg.time / trailTime;
	reMuzzle->reType = RT_SPRITE;
	reMuzzle->radius = 5.8f;
	reMuzzle->customShader = muzzleShader;

	reMuzzle->shaderRGBA[0] = (byte)(color.r*255);
	reMuzzle->shaderRGBA[1] = (byte)(color.g*255);
	reMuzzle->shaderRGBA[2] = (byte)(color.b*255);
	reMuzzle->shaderRGBA[3] = (byte)(color.a*255);
	VectorSet4( &leMuzzle->color, color.r*0.75f, color.g*0.75f, color.b*0.75f, 1.0f );

	VectorCopy( start, &reMuzzle->origin );
	VectorCopy( start, &reMuzzle->oldorigin );
	leMuzzle->pos.trType = TR_STATIONARY;
	leMuzzle->pos.trTime = cg.time;

	//Glow
	leGlow->leType = LE_FADE_RGB;
	leGlow->startTime = cg.time;
	leGlow->endTime = cg.time + (int)(trailTime*3.0f);
	leGlow->lifeRate = 1.0f / (leGlow->endTime - leGlow->startTime);
	reGlow->shaderTime = cg.time / (trailTime);
	reGlow->reType = RT_LINE;
	reGlow->radius = 1.5f;
	reGlow->customShader = trailShader;
	VectorCopy(start, &reGlow->origin);
	VectorCopy(end, &reGlow->oldorigin);
	reGlow->shaderRGBA[0] = (byte)(color.r * 255);
	reGlow->shaderRGBA[1] = (byte)(color.g * 255);
	reGlow->shaderRGBA[2] = (byte)(color.b * 255);
	reGlow->shaderRGBA[3] = 255;
	leGlow->color.r = (byte)(color.r * 255);
	leGlow->color.g = (byte)(color.g * 255);
	leGlow->color.b = (byte)(color.b * 255);
	leGlow->color.a = 0.7f;

	//Core
	leCore->leType = LE_FADE_RGB;
	leCore->startTime = cg.time;
	leCore->endTime = cg.time + (int)trailTime;
	leCore->lifeRate = 1.0f / (leCore->endTime - leCore->startTime);
	reCore->shaderTime = cg.time / (trailTime);
	reCore->reType = RT_LINE;
	reCore->radius = 1.0f;
	reCore->customShader = trailShader;
	VectorCopy(start, &reCore->origin);
	VectorCopy(end, &reCore->oldorigin);
	reCore->shaderRGBA[0] = 255;
	reCore->shaderRGBA[1] = 255;
	reCore->shaderRGBA[2] = 255;
	reCore->shaderRGBA[3] = 255;
	VectorSet4( &leCore->color, 1.0f, 1.0f, 1.0f, 1.0f );

	AxisClear( reCore->axis );
}
