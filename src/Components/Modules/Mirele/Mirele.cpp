#include <STDInclude.hpp>
#include "Components/Modules/Dvar.hpp"
#include "Components/Modules/Command.hpp"

namespace Components::Mirele
{
	Dvar::Var Instashoots_Dvar;
	Dvar::Var AlwaysCanswap_Dvar;
	Dvar::Var Canzoom_Dvar;

	void CheckForInstashoots(Game::pmove_s* pm, Game::PlayerEquippedWeaponState* state)
	{
		if (state == nullptr || !Instashoots_Dvar.get<bool>())
		{
			return;
		}

		if (int weaponState = pm->ps->weapState[0].weaponState; !(weaponState == Game::WEAPON_RAISING || weaponState == Game::WEAPON_RAISING_ALTSWITCH))
		{
			return;
		}

		if (state->dualWielding ? (pm->cmd.buttons & Game::CMD_BUTTON_ATTACK || pm->cmd.buttons & Game::CMD_BUTTON_THROW) : (pm->cmd.buttons & Game::CMD_BUTTON_ATTACK))
		{
			for (int i = 0; i < 2; i++)
			{
				state->needsRechamber[i] = false;

				pm->ps->weapState[i].weaponState = Game::WEAPON_READY;
				pm->ps->weapState[i].weaponTime = 0;
				pm->ps->weapState[i].weaponDelay = 0;
			}
		}
	}

	void AlwaysCanswap(Game::pmove_s* pm)
	{
		if (!AlwaysCanswap_Dvar.get<bool>())
		{
			return;
		}

		for (int i = 0; i < 15; i++)
		{
			pm->ps->weapEquippedData[i].usedBefore = false;
		}
	}

	void CheckForCanzooms(Game::pmove_s* pm, Game::PlayerEquippedWeaponState* state)
	{
		if (state == nullptr || !Canzoom_Dvar.get<bool>())
		{
			return;
		}

		if (int weaponState = pm->ps->weapState[0].weaponState; state->dualWielding || !(weaponState == Game::WEAPON_RAISING || weaponState == Game::WEAPON_RAISING_ALTSWITCH))
		{
			return;
		}

		if (pm->cmd.buttons & Game::CMD_BUTTON_ADS)
		{
			for (int i = 0; i < 2; i++)
			{
				state->needsRechamber[i] = false;

				pm->ps->weapState[i].weaponState = Game::WEAPON_READY;
				pm->ps->weapState[i].weaponTime = 0;
				pm->ps->weapState[i].weaponDelay = 0;
			}
		}
	}

	void PM_Weapon_Hk(Game::pmove_s* pm, Game::pml_t* pml)
	{
		if (pm->ps->clientNum != Game::CG_GetClientNum())
		{
			Utils::Hook::Call<void(Game::pmove_s*, Game::pml_t*)>(0x44C380)(pm, pml);
			return;
		}

		Game::PlayerEquippedWeaponState* state = Game::BG_GetEquippedWeaponState(pm->ps, pm->ps->weapCommon.weapon);
		CheckForInstashoots(pm, state);
		AlwaysCanswap(pm);
		CheckForCanzooms(pm, state);

		Utils::Hook::Call<void(Game::pmove_s*, Game::pml_t*)>(0x44C380)(pm, pml);
	}

	bool Recording = false;
	bool SavedRecording = false;
	bool PlayingRecording = false;
	unsigned int RecordingFrame = 0;
	unsigned int PredictedRecordingFrame = 0;

	struct PlayerStateRecordingData
	{
		float origin[3];
		float velocity[3];
		int bobCycle;
		int pm_type;
		int pm_time;
		int pm_flags;
		int otherFlags;
		int linkFlags;
		int gravity;
		int speed;
		int groundEntityNum;
		float vLadderVec[3];
		int jumpTime;
		float jumpOriginZ;
		int viewHeightTarget;
		float viewHeightCurrent;
		int viewHeightLerpTime;
		int viewHeightLerpTarget;
		int viewHeightLerpDown;
		float viewAngleClampBase[2];
		float viewAngleClampRange[2];
		float proneDirection;
		float proneDirectionPitch;
		float proneTorsoPitch;
		float moveSpeedScaleMultiplier;
		float meleeChargeYaw;
		int meleeChargeDist;
		int meleeChargeTime;
	};

	struct PmoveRecordingData
	{
		int tracemask; // 84
		int numtouch;
		int touchents[32];
		Game::Bounds bounds; // 220
		float xyspeed;
		int proneChange;
		float maxSprintTimeMultiplier;
		bool mantleStarted;
		float mantleEndPos[3];
		int mantleDuration;
		float fTorsoPitch;
		float fWaistPitch;
	};

	std::vector<PlayerStateRecordingData> PlayerStateRecording;
	std::vector<PmoveRecordingData> PmoveRecording;

	std::vector<PlayerStateRecordingData> PredictedPlayerStateRecording;
	std::vector<PmoveRecordingData> PredictedPmoveRecording;

	void SaveFrame(Game::pmove_s* pm, bool isPredicted)
	{
		PlayerStateRecordingData playerStateData;
		for (int i = 0; i < 3; i++)
		{
			playerStateData.origin[i] = pm->ps->origin[i];
			playerStateData.velocity[i] = pm->ps->velocity[i];
			playerStateData.vLadderVec[i] = pm->ps->vLadderVec[i];
		}

		playerStateData.bobCycle = pm->ps->bobCycle;
		playerStateData.pm_type = pm->ps->pm_type;
		playerStateData.pm_time = pm->ps->pm_time;
		playerStateData.pm_flags = pm->ps->pm_flags;
		playerStateData.otherFlags = pm->ps->otherFlags;
		playerStateData.linkFlags = pm->ps->linkFlags;
		playerStateData.gravity = pm->ps->gravity;
		playerStateData.speed = pm->ps->speed;
		playerStateData.groundEntityNum = pm->ps->groundEntityNum;
		playerStateData.jumpTime = pm->ps->jumpTime;
		playerStateData.jumpOriginZ = pm->ps->jumpOriginZ;
		playerStateData.viewHeightTarget = pm->ps->viewHeightTarget;
		playerStateData.viewHeightCurrent = pm->ps->viewHeightCurrent;
		playerStateData.viewHeightLerpTime = pm->ps->viewHeightLerpTime;
		playerStateData.viewHeightLerpTarget = pm->ps->viewHeightLerpTarget;
		playerStateData.viewHeightLerpDown = pm->ps->viewHeightLerpDown;

		for (int i = 0; i < 2; i++)
		{
			playerStateData.viewAngleClampBase[i] = pm->ps->viewAngleClampBase[i];
			playerStateData.viewAngleClampRange[i] = pm->ps->viewAngleClampRange[i];
		}

		playerStateData.proneDirection = pm->ps->proneDirection;
		playerStateData.proneDirectionPitch = pm->ps->proneDirectionPitch;
		playerStateData.proneTorsoPitch = pm->ps->proneTorsoPitch;
		playerStateData.moveSpeedScaleMultiplier = pm->ps->moveSpeedScaleMultiplier;
		playerStateData.meleeChargeYaw = pm->ps->meleeChargeYaw;
		playerStateData.meleeChargeDist = pm->ps->meleeChargeDist;
		playerStateData.meleeChargeTime = pm->ps->meleeChargeTime;

		if (isPredicted)
		{
			PredictedPlayerStateRecording.push_back(playerStateData);
		}
		else
		{
			PlayerStateRecording.push_back(playerStateData);
		}

		PmoveRecordingData pmoveData;
		pmoveData.tracemask = pm->tracemask;
		pmoveData.numtouch = pm->numtouch;
		for (int i = 0; i < 32; i++)
		{
			pmoveData.touchents[i] = pm->touchents[i];
		}

		for (int i = 0; i < 3; i++)
		{
			pmoveData.bounds.midPoint[i] = pm->bounds.midPoint[i];
			pmoveData.bounds.halfSize[i] = pm->bounds.halfSize[i];
			pmoveData.mantleEndPos[i] = pm->mantleEndPos[i];
		}

		pmoveData.xyspeed = pm->xyspeed;
		pmoveData.proneChange = pm->proneChange;
		pmoveData.maxSprintTimeMultiplier = pm->maxSprintTimeMultiplier;
		pmoveData.mantleStarted = pm->mantleStarted;
		pmoveData.mantleDuration = pm->mantleDuration;
		pmoveData.fTorsoPitch = pm->fTorsoPitch;
		pmoveData.fWaistPitch = pm->fWaistPitch;

		if (isPredicted)
		{
			PredictedPmoveRecording.push_back(pmoveData);
		}
		else
		{
			PmoveRecording.push_back(pmoveData);
		}
	}

	void PlayFrame(Game::pmove_s* pm, bool isPredicted)
	{
		if (isPredicted)
		{
			if (PredictedRecordingFrame >= PredictedPmoveRecording.size())
			{
				return;
			}
		}
		else
		{
			if (RecordingFrame >= PmoveRecording.size())
			{
				return;
			}
		}

		PlayerStateRecordingData& playerStateData = isPredicted ? PredictedPlayerStateRecording[PredictedRecordingFrame] : PlayerStateRecording[RecordingFrame];
		for (int i = 0; i < 3; i++)
		{
			pm->ps->origin[i] = playerStateData.origin[i];
			pm->ps->velocity[i] = playerStateData.velocity[i];
			pm->ps->vLadderVec[i] = playerStateData.vLadderVec[i];
		}

		pm->ps->bobCycle = playerStateData.bobCycle;
		pm->ps->pm_type = playerStateData.pm_type;
		pm->ps->pm_time = playerStateData.pm_time;
		pm->ps->pm_flags = playerStateData.pm_flags;
		pm->ps->otherFlags = playerStateData.otherFlags;
		pm->ps->linkFlags = playerStateData.linkFlags;
		pm->ps->gravity = playerStateData.gravity;
		pm->ps->speed = playerStateData.speed;
		pm->ps->groundEntityNum = playerStateData.groundEntityNum;
		pm->ps->jumpTime = playerStateData.jumpTime;
		pm->ps->jumpOriginZ = playerStateData.jumpOriginZ;
		pm->ps->viewHeightTarget = playerStateData.viewHeightTarget;
		pm->ps->viewHeightCurrent = playerStateData.viewHeightCurrent;
		pm->ps->viewHeightLerpTime = playerStateData.viewHeightLerpTime;
		pm->ps->viewHeightLerpTarget = playerStateData.viewHeightLerpTarget;
		pm->ps->viewHeightLerpDown = playerStateData.viewHeightLerpDown;
		for (int i = 0; i < 2; i++)
		{
			pm->ps->viewAngleClampBase[i] = playerStateData.viewAngleClampBase[i];
			pm->ps->viewAngleClampRange[i] = playerStateData.viewAngleClampRange[i];
		}

		pm->ps->proneDirection = playerStateData.proneDirection;
		pm->ps->proneDirectionPitch = playerStateData.proneDirectionPitch;
		pm->ps->proneTorsoPitch = playerStateData.proneTorsoPitch;
		pm->ps->moveSpeedScaleMultiplier = playerStateData.moveSpeedScaleMultiplier;
		pm->ps->meleeChargeYaw = playerStateData.meleeChargeYaw;
		pm->ps->meleeChargeDist = playerStateData.meleeChargeDist;

		PmoveRecordingData& pmoveData = isPredicted ? PredictedPmoveRecording[PredictedRecordingFrame] : PmoveRecording[RecordingFrame];
		pm->tracemask = pmoveData.tracemask;
		pm->numtouch = pmoveData.numtouch;
		for (int i = 0; i < 32; i++)
		{
			pm->touchents[i] = pmoveData.touchents[i];
		}

		for (int i = 0; i < 3; i++)
		{
			pm->bounds.midPoint[i] = pmoveData.bounds.midPoint[i];
			pm->bounds.halfSize[i] = pmoveData.bounds.halfSize[i];
			pm->mantleEndPos[i] = pmoveData.mantleEndPos[i];
		}

		pm->xyspeed = pmoveData.xyspeed;
		pm->proneChange = pmoveData.proneChange;
		pm->maxSprintTimeMultiplier = pmoveData.maxSprintTimeMultiplier;
		pm->mantleStarted = pmoveData.mantleStarted;
		pm->mantleDuration = pmoveData.mantleDuration;
		pm->fTorsoPitch = pmoveData.fTorsoPitch;
		pm->fWaistPitch = pmoveData.fWaistPitch;

		if (isPredicted)
		{
			PredictedRecordingFrame++;
		}
		else
		{
			RecordingFrame++;
		}
	}

	void Pmove_Hk(Game::pmove_s* pm)
	{
		if (pm->ps->clientNum != Game::CG_GetClientNum() || (pm == nullptr || pm->ps == nullptr))
		{
			Utils::Hook::Call<void(Game::pmove_s*)>(0x4CFEE0)(pm);
			return;
		}

		if (Recording)
		{
			SaveFrame(pm, false);
		}
		else if (PlayingRecording)
		{
			PlayFrame(pm, false);
		}

		if (PlayingRecording && PredictedRecordingFrame >= PredictedPmoveRecording.size() && RecordingFrame >= PmoveRecording.size())
		{
			PlayingRecording = false;
			PredictedRecordingFrame = 0;
			RecordingFrame = 0;
		}

		Utils::Hook::Call<void(Game::pmove_s*)>(0x4CFEE0)(pm);
	}

	void Pmove_Hk2(Game::pmove_s* pm)
	{
		if (pm->ps->clientNum != Game::CG_GetClientNum() || (pm == nullptr || pm->ps == nullptr))
		{
			Utils::Hook::Call<void(Game::pmove_s*)>(0x4CFEE0)(pm);
			return;
		}

		if (PlayingRecording)
		{
			pm->ps->bobCycle = 0;
		}

		Utils::Hook::Call<void(Game::pmove_s*)>(0x4CFEE0)(pm);

		if (PlayingRecording)
		{
			pm->ps->bobCycle = 0;
		}
	}

	struct Rectangles
	{
		float x, y, width, height;
		Game::vec4_t color;
		std::string align;

		void draw()
		{
			float newx = x;
			if (align == "center")
			{
				newx = x - width / 2;
			}
			else if (align == "right")
			{
				newx = x - width;
			}

			Game::R_AddCmdDrawStretchPic(newx, y, width, height, 0, 0, 1, 1, color, Game::cls->whiteMaterial);
		}
	};

	struct Texts
	{
		float x, y;
		float scale;
		Game::vec4_t color;
		std::string text;
		std::string font;
		std::string align;

		void draw()
		{
			float newx = x;
			Game::Font_s* fontHandle = Game::R_RegisterFont(font.data(), scale);
			if (align == "center")
			{
				newx = x - (Game::R_TextWidth(text.data(), 0x7FFFFFFF, fontHandle) * scale) / 2;
			}
			else if (align == "right")
			{
				newx = x - Game::R_TextWidth(text.data(), 0x7FFFFFFF, fontHandle) * scale;
			}

			Game::R_AddCmdDrawText(text.data(), 0x7FFFFFFF, fontHandle, newx, y, scale, scale, 0, color, 0);
		};
	};

	std::map<std::string, Rectangles> rectangles;
	std::map<std::string, Texts> texts;

	Mirele::Mirele()
	{
		Scheduler::Loop([]()
		{
			if (Game::CL_IsCgameInitialized())
			{
				for (auto& [name, rect] : rectangles)
				{
					rect.draw();
				}

				for (auto& [name, text] : texts)
				{
					text.draw();
				}
			}
			else
			{
				if (!rectangles.empty())
				{
					rectangles.clear();
				}

				if (!texts.empty())
				{
					texts.clear();
				}
			}
		}, Scheduler::Pipeline::RENDERER);

		Command::Add("togglerecording", []()
		{
			if (!Recording)
			{
				PlayerStateRecording.clear();
				PmoveRecording.clear();
				Recording = true;
				SavedRecording = false;
				PlayingRecording = false;
				RecordingFrame = 0;
				Game::SV_GameSendServerCommand(Game::CG_GetClientNum(), Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"%s\"", 0x65, "Started Recording"));
			}
			else
			{
				Recording = false;
				SavedRecording = true;
				Game::SV_GameSendServerCommand(Game::CG_GetClientNum(), Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"%s\"", 0x65, "Stopped Recording"));
			}
		});

		Command::Add("playrecording", []()
		{
			if (SavedRecording)
			{
				RecordingFrame = 0;
				PlayingRecording = true;
			}
		});

		Command::Add("xpartygo2", []()
		{
			std::string mapname = Game::Dvar_GetString("ui_mapname");
			Command::Execute("devmap " + mapname, false);
		});

		Instashoots_Dvar = Dvar::Register<bool>("instashoots", false, Game::DVAR_SAVED, "");
		AlwaysCanswap_Dvar = Dvar::Register<bool>("alwayscanswap", false, Game::DVAR_SAVED, "");
		Canzoom_Dvar = Dvar::Register<bool>("canzoom", false, Game::DVAR_SAVED, "");

		Utils::Hook(0x574960, PM_Weapon_Hk, HOOK_CALL).install()->quick();
		Utils::Hook(0x574B69, PM_Weapon_Hk, HOOK_CALL).install()->quick();
		Utils::Hook(0x574AB2, PM_Weapon_Hk, HOOK_CALL).install()->quick();

		Utils::Hook(0x493686, Pmove_Hk, HOOK_CALL).install()->quick();
		Utils::Hook(0x5900FD, Pmove_Hk2, HOOK_CALL).install()->quick();
	}
}
