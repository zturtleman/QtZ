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

/*****************************************************************************
 * name:		ai_team.c
 *
 * desc:		Quake3 bot AI
 *
 * $Archive: /MissionPack/code/game/ai_team.c $
 *
 *****************************************************************************/

#include "g_local.h"
#include "../botlib/botlib.h"
#include "../botlib/be_aas.h"
#include "../botlib/be_ea.h"
#include "../botlib/be_ai_char.h"
#include "../botlib/be_ai_chat.h"
#include "../botlib/be_ai_gen.h"
#include "../botlib/be_ai_goal.h"
#include "../botlib/be_ai_move.h"
#include "../botlib/be_ai_weap.h"
//
#include "ai_main.h"
#include "ai_dmq3.h"
#include "ai_chat.h"
#include "ai_cmd.h"
#include "ai_dmnet.h"
#include "ai_team.h"
#include "ai_vcmd.h"

#include "match.h"

// for the voice chats
#include "../../build/qtz/ui/menudef.h"

//ctf task preferences for a client
typedef struct bot_ctftaskpreference_s
{
	char		name[36];
	int			preference;
} bot_ctftaskpreference_t;

bot_ctftaskpreference_t ctftaskpreferences[MAX_CLIENTS];

int BotNumTeamMates(bot_state_t *bs) {
	int i, numplayers;
	char buf[MAX_INFO_STRING];
	static int maxclients;

	if (!maxclients)
		maxclients = trap->Cvar_VariableIntegerValue("sv_maxclients");

	numplayers = 0;
	for (i = 0; i < maxclients && i < MAX_CLIENTS; i++) {
		trap->SV_GetConfigstring(CS_PLAYERS+i, buf, sizeof(buf));
		//if no config string or no name
		if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "n"))) continue;
		//skip spectators
		if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR) continue;
		//
		if (BotSameTeam(bs, i)) {
			numplayers++;
		}
	}
	return numplayers;
}

int BotClientTravelTimeToGoal(int client, bot_goal_t *goal) {
	playerState_t ps;
	int areanum;

	BotAI_GetClientState(client, &ps);
	areanum = BotPointAreaNum(&ps.origin);
	if (!areanum) return 1;
	return trap->aas->AAS_AreaTravelTimeToGoalArea(areanum, &ps.origin, goal->areanum, TFL_DEFAULT);
}

int BotSortTeamMatesByBaseTravelTime(bot_state_t *bs, int *teammates, int maxteammates) {

	int i, j, k, numteammates, traveltime;
	char buf[MAX_INFO_STRING];
	static int maxclients;
	int traveltimes[MAX_CLIENTS];
	bot_goal_t *goal = NULL;

	if (gametype == GT_FLAGS || gametype == GT_TROJAN)
	{
		if (BotTeam(bs) == TEAM_RED)
			goal = &ctf_redflag;
		else
			goal = &ctf_blueflag;
	}
	else {
		if (BotTeam(bs) == TEAM_RED)
			goal = &redobelisk;
		else
			goal = &blueobelisk;
	}
	if (!maxclients)
		maxclients = trap->Cvar_VariableIntegerValue("sv_maxclients");

	numteammates = 0;
	for (i = 0; i < maxclients && i < MAX_CLIENTS; i++) {
		trap->SV_GetConfigstring(CS_PLAYERS+i, buf, sizeof(buf));
		//if no config string or no name
		if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "n"))) continue;
		//skip spectators
		if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR) continue;
		//
		if (BotSameTeam(bs, i)) {
			//
			traveltime = BotClientTravelTimeToGoal(i, goal);
			//
			for (j = 0; j < numteammates; j++) {
				if (traveltime < traveltimes[j]) {
					for (k = numteammates; k > j; k--) {
						traveltimes[k] = traveltimes[k-1];
						teammates[k] = teammates[k-1];
					}
					break;
				}
			}
			traveltimes[j] = traveltime;
			teammates[j] = i;
			numteammates++;
			if (numteammates >= maxteammates) break;
		}
	}
	return numteammates;
}

void BotSetTeamMateTaskPreference(bot_state_t *bs, int teammate, int preference) {
	char teammatename[MAX_NETNAME];

	ctftaskpreferences[teammate].preference = preference;
	ClientName(teammate, teammatename, sizeof(teammatename));
	strcpy(ctftaskpreferences[teammate].name, teammatename);
}

int BotGetTeamMateTaskPreference(bot_state_t *bs, int teammate) {
	char teammatename[MAX_NETNAME];

	if (!ctftaskpreferences[teammate].preference) return 0;
	ClientName(teammate, teammatename, sizeof(teammatename));
	if (Q_stricmp(teammatename, ctftaskpreferences[teammate].name)) return 0;
	return ctftaskpreferences[teammate].preference;
}

int BotSortTeamMatesByTaskPreference(bot_state_t *bs, int *teammates, int numteammates) {
	int defenders[MAX_CLIENTS], numdefenders;
	int attackers[MAX_CLIENTS], numattackers;
	int roamers[MAX_CLIENTS], numroamers;
	int i, preference;

	numdefenders = numattackers = numroamers = 0;
	for (i = 0; i < numteammates; i++) {
		preference = BotGetTeamMateTaskPreference(bs, teammates[i]);
		if (preference & TEAMTP_DEFENDER) {
			defenders[numdefenders++] = teammates[i];
		}
		else if (preference & TEAMTP_ATTACKER) {
			attackers[numattackers++] = teammates[i];
		}
		else {
			roamers[numroamers++] = teammates[i];
		}
	}
	numteammates = 0;
	//defenders at the front of the list
	memcpy(&teammates[numteammates], defenders, numdefenders * sizeof(int));
	numteammates += numdefenders;
	//roamers in the middle
	memcpy(&teammates[numteammates], roamers, numroamers * sizeof(int));
	numteammates += numroamers;
	//attacker in the back of the list
	memcpy(&teammates[numteammates], attackers, numattackers * sizeof(int));
	numteammates += numattackers;

	return numteammates;
}

void BotSayTeamOrderAlways(bot_state_t *bs, int toclient) {
	char teamchat[MAX_MESSAGE_SIZE];
	char buf[MAX_MESSAGE_SIZE];
	char name[MAX_NETNAME];

	//if the bot is talking to itself
	if (bs->client == toclient) {
		//don't show the message just put it in the console message queue
		trap->ai->BotGetChatMessage(bs->cs, buf, sizeof(buf));
		ClientName(bs->client, name, sizeof(name));
		Com_sprintf(teamchat, sizeof(teamchat), "(%s): %s", name, buf);
		trap->ai->BotQueueConsoleMessage(bs->cs, CMS_CHAT, teamchat);
	}
	else {
		trap->ai->BotEnterChat(bs->cs, toclient, CHAT_TELL);
	}
}

void BotSayTeamOrder(bot_state_t *bs, int toclient) {
	// voice chats only
	char buf[MAX_MESSAGE_SIZE];

	trap->ai->BotGetChatMessage(bs->cs, buf, sizeof(buf));
}

void BotVoiceChat(bot_state_t *bs, int toclient, char *voicechat) {
	if (toclient == -1)
		// voice only say team
		trap->ea->EA_Command(bs->client, va("vsay_team %s", voicechat));
	else
		// voice only tell single player
		trap->ea->EA_Command(bs->client, va("vtell %d %s", toclient, voicechat));
}

void BotVoiceChatOnly(bot_state_t *bs, int toclient, char *voicechat) {
	if (toclient == -1)
		// voice only say team
		trap->ea->EA_Command(bs->client, va("vosay_team %s", voicechat));
	else
		// voice only tell single player
		trap->ea->EA_Command(bs->client, va("votell %d %s", toclient, voicechat));
}

void BotSayVoiceTeamOrder(bot_state_t *bs, int toclient, char *voicechat) {
	BotVoiceChat(bs, toclient, voicechat);
}

void BotCTFOrders_BothFlagsNotAtBase(bot_state_t *bs) {
	int numteammates, defenders, attackers, i, other;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME], carriername[MAX_NETNAME];

	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	//different orders based on the number of team mates
	switch(bs->numteammates) {
		case 1: break;
		case 2:
		{
			//tell the one not carrying the flag to attack the enemy base
			if (teammates[0] != bs->flagcarrier) other = teammates[0];
			else other = teammates[1];
			ClientName(other, name, sizeof(name));
			BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
			BotSayTeamOrder(bs, other);
			BotSayVoiceTeamOrder(bs, other, VOICECHAT_GETFLAG);
			break;
		}
		case 3:
		{
			//tell the one closest to the base not carrying the flag to accompany the flag carrier
			if (teammates[0] != bs->flagcarrier) other = teammates[0];
			else other = teammates[1];
			ClientName(other, name, sizeof(name));
			if ( bs->flagcarrier != -1 ) {
				ClientName(bs->flagcarrier, carriername, sizeof(carriername));
				if (bs->flagcarrier == bs->client) {
					BotAI_BotInitialChat(bs, "cmd_accompanyme", name, NULL);
					BotSayVoiceTeamOrder(bs, other, VOICECHAT_FOLLOWME);
				}
				else {
					BotAI_BotInitialChat(bs, "cmd_accompany", name, carriername, NULL);
					BotSayVoiceTeamOrder(bs, other, VOICECHAT_FOLLOWFLAGCARRIER);
				}
			}
			else {
				//
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayVoiceTeamOrder(bs, other, VOICECHAT_GETFLAG);
			}
			BotSayTeamOrder(bs, other);
			//tell the one furthest from the the base not carrying the flag to get the enemy flag
			if (teammates[2] != bs->flagcarrier) other = teammates[2];
			else other = teammates[1];
			ClientName(other, name, sizeof(name));
			BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
			BotSayTeamOrder(bs, other);
			BotSayVoiceTeamOrder(bs, other, VOICECHAT_RETURNFLAG);
			break;
		}
		default:
		{
			defenders = (int)((float)numteammates * 0.4f + 0.5f);
			if (defenders > 4) defenders = 4;
			attackers = (int)((float)numteammates * 0.5f + 0.5f);
			if (attackers > 5) attackers = 5;
			if (bs->flagcarrier != -1) {
				ClientName(bs->flagcarrier, carriername, sizeof(carriername));
				for (i = 0; i < defenders; i++) {
					//
					if (teammates[i] == bs->flagcarrier) {
						continue;
					}
					//
					ClientName(teammates[i], name, sizeof(name));
					if (bs->flagcarrier == bs->client) {
						BotAI_BotInitialChat(bs, "cmd_accompanyme", name, NULL);
						BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_FOLLOWME);
					}
					else {
						BotAI_BotInitialChat(bs, "cmd_accompany", name, carriername, NULL);
						BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_FOLLOWFLAGCARRIER);
					}
					BotSayTeamOrder(bs, teammates[i]);
				}
			}
			else {
				for (i = 0; i < defenders; i++) {
					//
					if (teammates[i] == bs->flagcarrier) {
						continue;
					}
					//
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_GETFLAG);
					BotSayTeamOrder(bs, teammates[i]);
				}
			}
			for (i = 0; i < attackers; i++) {
				//
				if (teammates[numteammates - i - 1] == bs->flagcarrier) {
					continue;
				}
				//
				ClientName(teammates[numteammates - i - 1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
				BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_RETURNFLAG);
			}
			//
			break;
		}
	}
}

void BotCTFOrders_FlagNotAtBase(bot_state_t *bs) {
	int numteammates, defenders, attackers, i;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME];

	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	//passive strategy
	if (!(bs->ctfstrategy & CTFS_AGRESSIVE)) {
		//different orders based on the number of team mates
		switch(bs->numteammates) {
			case 1: break;
			case 2:
			{
				// keep one near the base for when the flag is returned
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				break;
			}
			case 3:
			{
				//keep one near the base for when the flag is returned
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the other two get the flag
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				//
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				//keep some people near the base for when the flag is returned
				defenders = (int)((float)numteammates * 0.3f + 0.5f);
				if (defenders > 3) defenders = 3;
				attackers = (int)((float)numteammates * 0.6f + 0.5f);
				if (attackers > 6) attackers = 6;
				for (i = 0; i < defenders; i++) {
					//
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					//
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_GETFLAG);
				}
				//
				break;
			}
		}
	}
	else {
		//different orders based on the number of team mates
		switch(bs->numteammates) {
			case 1: break;
			case 2:
			{
				//both will go for the enemy flag
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_GETFLAG);
				//
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				break;
			}
			case 3:
			{
				//everyone go for the flag
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_GETFLAG);
				//
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				//
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				//keep some people near the base for when the flag is returned
				defenders = (int)((float)numteammates * 0.2f + 0.5f);
				if (defenders > 2) defenders = 2;
				attackers = (int)((float)numteammates * 0.7f + 0.5f);
				if (attackers > 7) attackers = 7;
				for (i = 0; i < defenders; i++) {
					//
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					//
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
				}
				//
				break;
			}
		}
	}
}

void BotCTFOrders_EnemyFlagNotAtBase(bot_state_t *bs) {
	int numteammates, defenders, attackers, i, other;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME], carriername[MAX_NETNAME];

	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	//different orders based on the number of team mates
	switch(numteammates) {
		case 1: break;
		case 2:
		{
			//tell the one not carrying the flag to defend the base
			if (teammates[0] == bs->flagcarrier) other = teammates[1];
			else other = teammates[0];
			ClientName(other, name, sizeof(name));
			BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
			BotSayTeamOrder(bs, other);
			BotSayVoiceTeamOrder(bs, other, VOICECHAT_DEFEND);
			break;
		}
		case 3:
		{
			//tell the one closest to the base not carrying the flag to defend the base
			if (teammates[0] != bs->flagcarrier) other = teammates[0];
			else other = teammates[1];
			ClientName(other, name, sizeof(name));
			BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
			BotSayTeamOrder(bs, other);
			BotSayVoiceTeamOrder(bs, other, VOICECHAT_DEFEND);
			//tell the other also to defend the base
			if (teammates[2] != bs->flagcarrier) other = teammates[2];
			else other = teammates[1];
			ClientName(other, name, sizeof(name));
			BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
			BotSayTeamOrder(bs, other);
			BotSayVoiceTeamOrder(bs, other, VOICECHAT_DEFEND);
			break;
		}
		default:
		{
			//60% will defend the base
			defenders = (int)((float)numteammates * 0.6f + 0.5f);
			if (defenders > 6) defenders = 6;
			//30% accompanies the flag carrier
			attackers = (int)((float)numteammates * 0.3f + 0.5f);
			if (attackers > 3) attackers = 3;
			for (i = 0; i < defenders; i++) {
				//
				if (teammates[i] == bs->flagcarrier) {
					continue;
				}
				ClientName(teammates[i], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[i]);
				BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
			}
			// if we have a flag carrier
			if ( bs->flagcarrier != -1 ) {
				ClientName(bs->flagcarrier, carriername, sizeof(carriername));
				for (i = 0; i < attackers; i++) {
					//
					if (teammates[numteammates - i - 1] == bs->flagcarrier) {
						continue;
					}
					//
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					if (bs->flagcarrier == bs->client) {
						BotAI_BotInitialChat(bs, "cmd_accompanyme", name, NULL);
						BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_FOLLOWME);
					}
					else {
						BotAI_BotInitialChat(bs, "cmd_accompany", name, carriername, NULL);
						BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_FOLLOWFLAGCARRIER);
					}
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
				}
			}
			else {
				for (i = 0; i < attackers; i++) {
					//
					if (teammates[numteammates - i - 1] == bs->flagcarrier) {
						continue;
					}
					//
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
				}
			}
			//
			break;
		}
	}
}


void BotCTFOrders_BothFlagsAtBase(bot_state_t *bs) {
	int numteammates, defenders, attackers, i;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME];

	//sort team mates by travel time to base
	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	//sort team mates by CTF preference
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	//passive strategy
	if (!(bs->ctfstrategy & CTFS_AGRESSIVE)) {
		//different orders based on the number of team mates
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the other will get the flag
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				break;
			}
			case 3:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the second one closest to the base will defend the base
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_DEFEND);
				//the other will get the flag
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				defenders = (int)((float)numteammates * 0.5f + 0.5f);
				if (defenders > 5) defenders = 5;
				attackers = (int)((float)numteammates * 0.4f + 0.5f);
				if (attackers > 4) attackers = 4;
				for (i = 0; i < defenders; i++) {
					//
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					//
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
				}
				//
				break;
			}
		}
	}
	else {
		//different orders based on the number of team mates
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the other will get the flag
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				break;
			}
			case 3:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the others should go for the enemy flag
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				//
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				defenders = (int)((float)numteammates * 0.4f + 0.5f);
				if (defenders > 4) defenders = 4;
				attackers = (int)((float)numteammates * 0.5f + 0.5f);
				if (attackers > 5) attackers = 5;
				for (i = 0; i < defenders; i++) {
					//
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					//
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
				}
				//
				break;
			}
		}
	}
}

void BotCTFOrders(bot_state_t *bs) {
	int flagstatus;

	//
	if (BotTeam(bs) == TEAM_RED) flagstatus = bs->redflagstatus * 2 + bs->blueflagstatus;
	else flagstatus = bs->blueflagstatus * 2 + bs->redflagstatus;
	//
	switch(flagstatus) {
		case 0: BotCTFOrders_BothFlagsAtBase(bs); break;
		case 1: BotCTFOrders_EnemyFlagNotAtBase(bs); break;
		case 2: BotCTFOrders_FlagNotAtBase(bs); break;
		case 3: BotCTFOrders_BothFlagsNotAtBase(bs); break;
	}
}

// X% defend the base, Y% get the flag
void BotTrojanOrders_FlagAtCenter(bot_state_t *bs) {
	int numteammates, defenders, attackers, i;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME];

	//sort team mates by travel time to base
	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	//sort team mates by CTF preference
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	//passive strategy
	if (!(bs->ctfstrategy & CTFS_AGRESSIVE)) {
		//different orders based on the number of team mates
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the other will get the flag
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				break;
			}
			case 3:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the second one closest to the base will defend the base
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the other will get the flag
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				//50% defend the base
				defenders = (int)((float)numteammates * 0.5f + 0.5f);
				if (defenders > 5) defenders = 5;
				//40% get the flag
				attackers = (int)((float)numteammates * 0.4f + 0.5f);
				if (attackers > 4) attackers = 4;
				for (i = 0; i < defenders; i++) {
					//
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					//
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
				}
				//
				break;
			}
		}
	}
	else { //agressive
		//different orders based on the number of team mates
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the other will get the flag
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				break;
			}
			case 3:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the others should go for the enemy flag
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				//
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				//30% defend the base
				defenders = (int)((float)numteammates * 0.3f + 0.5f);
				if (defenders > 3) defenders = 3;
				//60% get the flag
				attackers = (int)((float)numteammates * 0.6f + 0.5f);
				if (attackers > 6) attackers = 6;
				for (i = 0; i < defenders; i++) {
					//
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					//
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
				}
				//
				break;
			}
		}
	}
}

// X% towards neutral flag
//	Y% go towards enemy base and accompany flag carrier if visible
void BotTrojanOrders_TeamHasFlag(bot_state_t *bs) {
	int numteammates, defenders, attackers, i, other;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME], carriername[MAX_NETNAME];

	//sort team mates by travel time to base
	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	//sort team mates by CTF preference
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	//passive strategy
	if (!(bs->ctfstrategy & CTFS_AGRESSIVE)) {
		//different orders based on the number of team mates
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				//tell the one not carrying the flag to attack the enemy base
				if (teammates[0] == bs->flagcarrier) other = teammates[1];
				else other = teammates[0];
				ClientName(other, name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_attackenemybase", name, NULL);
				BotSayTeamOrder(bs, other);
				BotSayVoiceTeamOrder(bs, other, VOICECHAT_OFFENSE);
				break;
			}
			case 3:
			{
				//tell the one closest to the base not carrying the flag to defend the base
				if (teammates[0] != bs->flagcarrier) other = teammates[0];
				else other = teammates[1];
				ClientName(other, name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, other);
				BotSayVoiceTeamOrder(bs, other, VOICECHAT_DEFEND);
				//tell the one furthest from the base not carrying the flag to accompany the flag carrier
				if (teammates[2] != bs->flagcarrier) other = teammates[2];
				else other = teammates[1];
				ClientName(other, name, sizeof(name));
				if ( bs->flagcarrier != -1 ) {
					ClientName(bs->flagcarrier, carriername, sizeof(carriername));
					if (bs->flagcarrier == bs->client) {
						BotAI_BotInitialChat(bs, "cmd_accompanyme", name, NULL);
						BotSayVoiceTeamOrder(bs, other, VOICECHAT_FOLLOWME);
					}
					else {
						BotAI_BotInitialChat(bs, "cmd_accompany", name, carriername, NULL);
						BotSayVoiceTeamOrder(bs, other, VOICECHAT_FOLLOWFLAGCARRIER);
					}
				}
				else {
					//
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayVoiceTeamOrder(bs, other, VOICECHAT_GETFLAG);
				}
				BotSayTeamOrder(bs, other);
				break;
			}
			default:
			{
				//30% will defend the base
				defenders = (int)((float)numteammates * 0.3f + 0.5f);
				if (defenders > 3) defenders = 3;
				//70% accompanies the flag carrier
				attackers = (int)((float)numteammates * 0.7f + 0.5f);
				if (attackers > 7) attackers = 7;
				for (i = 0; i < defenders; i++) {
					//
					if (teammates[i] == bs->flagcarrier) {
						continue;
					}
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				if (bs->flagcarrier != -1) {
					ClientName(bs->flagcarrier, carriername, sizeof(carriername));
					for (i = 0; i < attackers; i++) {
						//
						if (teammates[numteammates - i - 1] == bs->flagcarrier) {
							continue;
						}
						//
						ClientName(teammates[numteammates - i - 1], name, sizeof(name));
						if (bs->flagcarrier == bs->client) {
							BotAI_BotInitialChat(bs, "cmd_accompanyme", name, NULL);
							BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_FOLLOWME);
						}
						else {
							BotAI_BotInitialChat(bs, "cmd_accompany", name, carriername, NULL);
							BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_FOLLOWFLAGCARRIER);
						}
						BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					}
				}
				else {
					for (i = 0; i < attackers; i++) {
						//
						if (teammates[numteammates - i - 1] == bs->flagcarrier) {
							continue;
						}
						//
						ClientName(teammates[numteammates - i - 1], name, sizeof(name));
						BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
						BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
						BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
					}
				}
				//
				break;
			}
		}
	}
	else { //agressive
		//different orders based on the number of team mates
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				//tell the one not carrying the flag to defend the base
				if (teammates[0] == bs->flagcarrier) other = teammates[1];
				else other = teammates[0];
				ClientName(other, name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, other);
				BotSayVoiceTeamOrder(bs, other, VOICECHAT_DEFEND);
				break;
			}
			case 3:
			{
				//tell the one closest to the base not carrying the flag to defend the base
				if (teammates[0] != bs->flagcarrier) other = teammates[0];
				else other = teammates[1];
				ClientName(other, name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, other);
				BotSayVoiceTeamOrder(bs, other, VOICECHAT_DEFEND);
				//tell the one furthest from the base not carrying the flag to accompany the flag carrier
				if (teammates[2] != bs->flagcarrier) other = teammates[2];
				else other = teammates[1];
				ClientName(other, name, sizeof(name));
				ClientName(bs->flagcarrier, carriername, sizeof(carriername));
				if (bs->flagcarrier == bs->client) {
					BotAI_BotInitialChat(bs, "cmd_accompanyme", name, NULL);
					BotSayVoiceTeamOrder(bs, other, VOICECHAT_FOLLOWME);
				}
				else {
					BotAI_BotInitialChat(bs, "cmd_accompany", name, carriername, NULL);
					BotSayVoiceTeamOrder(bs, other, VOICECHAT_FOLLOWFLAGCARRIER);
				}
				BotSayTeamOrder(bs, other);
				break;
			}
			default:
			{
				//20% will defend the base
				defenders = (int)((float)numteammates * 0.2f + 0.5f);
				if (defenders > 2) defenders = 2;
				//80% accompanies the flag carrier
				attackers = (int)((float)numteammates * 0.8f + 0.5f);
				if (attackers > 8) attackers = 8;
				for (i = 0; i < defenders; i++) {
					//
					if (teammates[i] == bs->flagcarrier) {
						continue;
					}
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				ClientName(bs->flagcarrier, carriername, sizeof(carriername));
				for (i = 0; i < attackers; i++) {
					//
					if (teammates[numteammates - i - 1] == bs->flagcarrier) {
						continue;
					}
					//
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					if (bs->flagcarrier == bs->client) {
						BotAI_BotInitialChat(bs, "cmd_accompanyme", name, NULL);
						BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_FOLLOWME);
					}
					else {
						BotAI_BotInitialChat(bs, "cmd_accompany", name, carriername, NULL);
						BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_FOLLOWFLAGCARRIER);
					}
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
				}
				//
				break;
			}
		}
	}
}

// X% defend the base
//	Y% towards neutral flag
void BotTrojanOrders_EnemyHasFlag(bot_state_t *bs) {
	int numteammates, defenders, attackers, i;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME];

	//sort team mates by travel time to base
	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	//sort team mates by CTF preference
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	//passive strategy
	if (!(bs->ctfstrategy & CTFS_AGRESSIVE)) {
		//different orders based on the number of team mates
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				//both defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_DEFEND);
				break;
			}
			case 3:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the second one closest to the base will defend the base
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_DEFEND);
				//the other will also defend the base
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_DEFEND);
				break;
			}
			default:
			{
				//80% will defend the base
				defenders = (int)((float)numteammates * 0.8f + 0.5f);
				if (defenders > 8) defenders = 8;
				//10% will try to return the flag
				attackers = (int)((float)numteammates * 0.1f + 0.5f);
				if (attackers > 1) attackers = 1;
				for (i = 0; i < defenders; i++) {
					//
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					//
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_returnflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
				}
				//
				break;
			}
		}
	}
	else { //agressive
		//different orders based on the number of team mates
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the other will get the flag
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_DEFEND);
				break;
			}
			case 3:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the others should go for the enemy flag
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_DEFEND);
				//
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_returnflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				//70% defend the base
				defenders = (int)((float)numteammates * 0.7f + 0.5f);
				if (defenders > 7) defenders = 7;
				//20% try to return the flag
				attackers = (int)((float)numteammates * 0.2f + 0.5f);
				if (attackers > 2) attackers = 2;
				for (i = 0; i < defenders; i++) {
					//
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					//
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_returnflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
				}
				//
				break;
			}
		}
	}
}

// X% defend the base
//	Y% get the flag
void BotTrojanOrders_EnemyDroppedFlag(bot_state_t *bs) {
	int numteammates, defenders, attackers, i;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME];

	//sort team mates by travel time to base
	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	//sort team mates by CTF preference
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	//passive strategy
	if (!(bs->ctfstrategy & CTFS_AGRESSIVE)) {
		//different orders based on the number of team mates
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the other will get the flag
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				break;
			}
			case 3:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the second one closest to the base will defend the base
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_DEFEND);
				//the other will get the flag
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				//50% defend the base
				defenders = (int)((float)numteammates * 0.5f + 0.5f);
				if (defenders > 5) defenders = 5;
				//40% get the flag
				attackers = (int)((float)numteammates * 0.4f + 0.5f);
				if (attackers > 4)
					attackers = 4;
				for (i = 0; i < defenders; i++) {
					//
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					//
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
				}
				//
				break;
			}
		}
	}
	else { //agressive
		//different orders based on the number of team mates
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the other will get the flag
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				break;
			}
			case 3:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the others should go for the enemy flag
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				//
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				//30% defend the base
				defenders = (int)((float)numteammates * 0.3f + 0.5f);
				if (defenders > 3) defenders = 3;
				//60% get the flag
				attackers = (int)((float)numteammates * 0.6f + 0.5f);
				if (attackers > 6) attackers = 6;
				for (i = 0; i < defenders; i++) {
					//
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					//
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
				}
				//
				break;
			}
		}
	}
}

void BotTrojanOrders(bot_state_t *bs) {
	switch(bs->neutralflagstatus) {
		case 0: BotTrojanOrders_FlagAtCenter(bs); break;
		case 1: BotTrojanOrders_TeamHasFlag(bs); break;
		case 2: BotTrojanOrders_EnemyHasFlag(bs); break;
		case 3: BotTrojanOrders_EnemyDroppedFlag(bs); break;
	}
}

// X% in defence
//	Y% in offence
void BotObeliskOrders(bot_state_t *bs) {
	int numteammates, defenders, attackers, i;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME];

	//sort team mates by travel time to base
	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	//sort team mates by CTF preference
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	//passive strategy
	if (!(bs->ctfstrategy & CTFS_AGRESSIVE)) {
		//different orders based on the number of team mates
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the other will attack the enemy base
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_attackenemybase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_OFFENSE);
				break;
			}
			case 3:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the one second closest to the base also defends the base
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_DEFEND);
				//the other one attacks the enemy base
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_attackenemybase", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_OFFENSE);
				break;
			}
			default:
			{
				//50% defend the base
				defenders = (int)((float)numteammates * 0.5f + 0.5f);
				if (defenders > 5) defenders = 5;
				//40% attack the enemy base
				attackers = (int)((float)numteammates * 0.4f + 0.5f);
				if (attackers > 4) attackers = 4;
				for (i = 0; i < defenders; i++) {
					//
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					//
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_attackenemybase", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_OFFENSE);
				}
				//
				break;
			}
		}
	}
	else {
		//different orders based on the number of team mates
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the other will attack the enemy base
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_attackenemybase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_OFFENSE);
				break;
			}
			case 3:
			{
				//the one closest to the base will defend the base
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				//the others attack the enemy base
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_attackenemybase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_OFFENSE);
				//
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_attackenemybase", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_OFFENSE);
				break;
			}
			default:
			{
				//30% defend the base
				defenders = (int)((float)numteammates * 0.3f + 0.5f);
				if (defenders > 3) defenders = 3;
				//70% attack the enemy base
				attackers = (int)((float)numteammates * 0.7f + 0.5f);
				if (attackers > 7) attackers = 7;
				for (i = 0; i < defenders; i++) {
					//
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					//
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_attackenemybase", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_OFFENSE);
				}
				//
				break;
			}
		}
	}
}
