////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_monster_brain.cpp
//	Created 	: 06.10.2005
//  Modified 	: 22.11.2005
//	Author		: Dmitriy Iassenev
//	Description : ALife monster brain class
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "alife_monster_brain.h"
#include "object_broker.h"
#include "xrServer_Objects_ALife_Monsters.h"

#ifdef XRGAME_EXPORTS
#	include "alife_monster_movement_manager.h"
#	include "alife_monster_detail_path_manager.h"
#	include "alife_monster_patrol_path_manager.h"
#	include "ai_space.h"
#	include "ef_storage.h"
#	include "ef_primary.h"
#	include "alife_simulator.h"
#	include "alife_graph_registry.h"
#	include "movement_manager_space.h"
#	include "alife_smart_terrain_registry.h"
#	include "alife_time_manager.h"
#	include "date_time.h"
#	ifdef DEBUG
#		include "level.h"
#		include "map_location.h"
#		include "map_manager.h"
#	endif
#endif

#define MAX_ITEM_FOOD_COUNT			3
#define MAX_ITEM_MEDIKIT_COUNT		3
#define MAX_AMMO_ATTACH_COUNT		10

CALifeMonsterBrain::CALifeMonsterBrain		(object_type *object)
{
	VERIFY							(object);
	m_object						= object;
	m_last_search_time				= 0;
	m_smart_terrain					= 0;

#ifdef XRGAME_EXPORTS
	m_movement_manager				= xr_new<CALifeMonsterMovementManager>(object);
#endif
	
#ifdef XRGAME_EXPORTS
	u32								hours,minutes,seconds;
	sscanf							(pSettings->r_string(this->object().name(),"smart_terrain_choose_interval"),"%d:%d:%d",&hours,&minutes,&seconds);
	m_time_interval					= (u32)generate_time(1,1,1,hours,minutes,seconds);
#endif

	m_can_choose_alife_tasks		= true;
}

CALifeMonsterBrain::~CALifeMonsterBrain			()
{
#ifdef XRGAME_EXPORTS
	xr_delete						(m_movement_manager);
#endif
}

void CALifeMonsterBrain::on_state_write		(NET_Packet &packet)
{
}

void CALifeMonsterBrain::on_state_read		(NET_Packet &packet)
{
}

#ifdef XRGAME_EXPORTS

bool CALifeMonsterBrain::perform_attack		()
{
	return							(false);
}

ALife::EMeetActionType CALifeMonsterBrain::action_type	(CSE_ALifeSchedulable *tpALifeSchedulable, const int &iGroupIndex, const bool &bMutualDetection)
{
	return							(ALife::eMeetActionTypeIgnore);
}

void CALifeMonsterBrain::on_register			()
{
}

void CALifeMonsterBrain::on_unregister		()
{
}

void CALifeMonsterBrain::on_location_change	()
{
}

IC	CSE_ALifeSmartZone &CALifeMonsterBrain::smart_terrain	()
{
	VERIFY							(object().m_smart_terrain_id != 0xffff);
	if (m_smart_terrain && (object().m_smart_terrain_id == m_smart_terrain->ID))
		return						(*m_smart_terrain);

	m_smart_terrain					= ai().alife().smart_terrains().object(object().m_smart_terrain_id);
	VERIFY							(m_smart_terrain);
	return							(*m_smart_terrain);
}

void CALifeMonsterBrain::process_task			()
{
	CALifeSmartTerrainTask			*task = smart_terrain().task(&object());
	//THROW3							(task,"smart terrain returned nil task, while npc is registered in it",smart_terrain().name_replace());
	if (!task)
	{
		Msg("! ERROR smart returned [%s] nil task, while npc [%s] is registered in it.",smart_terrain().name_replace(),object().name_replace());
		FATAL("ENGINE Crush. See log for details.");
	}

	movement().path_type			(MovementManager::ePathTypeGamePath);
	movement().detail().target		(*task);
}

#define MEAS_BEGIN(timer) if (g_measure) { timer.Begin(); }
#define MEAS_END(timer) if (g_measure) { timer.End(); }
#define MEAS(timer) g_measure ? timer : g_dummy

extern bool g_measure;
CTimerStat g_brain_select_task;
CTimerStat g_brain_st;
CTimerStat g_brain_al;
CTimerStat g_brain_for;
CTimerStat g_brain_for_en;
CTimerStat g_brain_reg;
CTimerStat g_dummy;

void CALifeMonsterBrain::select_task			()
{
	CTimerStatScoped _(MEAS(g_brain_select_task));

	{
		CTimerStatScoped _l(MEAS(g_brain_st));
		if (object().m_smart_terrain_id != 0xffff)
			return;
	}

	{
		CTimerStatScoped _l(MEAS(g_brain_al));
		if (!can_choose_alife_tasks())
			return;
	}
	ALife::_TIME_ID					current_time = ai().alife().time_manager().game_time();

	if (m_last_search_time + m_time_interval > current_time)
		return;

	m_last_search_time				= current_time;

	float							best_value = flt_min;
	CALifeSmartTerrainRegistry::OBJECTS::const_iterator	I = ai().alife().smart_terrains().objects().begin();
	CALifeSmartTerrainRegistry::OBJECTS::const_iterator	E = ai().alife().smart_terrains().objects().end();
	{
		CTimerStatScoped _l(MEAS(g_brain_for));
		for ( ; I != E; ++I) 
		{
			CTimerStatScoped _l_en(MEAS(g_brain_for_en));
			if (!(*I).second->enabled(&object()))
				continue;

			float						value = (*I).second->suitable(&object());
			if (value > best_value) {
				best_value				= value;
				object().m_smart_terrain_id	= (*I).second->ID;
			}
		}
	}

	{
		CTimerStatScoped _l(MEAS(g_brain_reg));
		if (object().m_smart_terrain_id != 0xffff) {
			smart_terrain().register_npc	(&object());
			m_last_search_time				= 0;
		}
	}
}

void CALifeMonsterBrain::update				()
{
#if 0//def DEBUG
	if (!Level().MapManager().HasMapLocation("debug_stalker",object().ID)) {
		CMapLocation				*map_location = 
			Level().MapManager().AddMapLocation(
				"debug_stalker",
				object().ID
			);

		map_location->SetHint		(object().name_replace());
	}
#endif

	select_task						();
	
	if (object().m_smart_terrain_id != 0xffff)
		process_task				();
	else
		default_behaviour			();

	movement().update				();
}

void CALifeMonsterBrain::default_behaviour	()
{
	movement().path_type			(MovementManager::ePathTypeNoPath);
}

void CALifeMonsterBrain::on_switch_online	()
{
	movement().on_switch_online		();
}

void CALifeMonsterBrain::on_switch_offline	()
{
	movement().on_switch_offline	();
}

#endif // XRGAME_EXPORTS