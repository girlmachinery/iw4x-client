
#include <Components/Modules/Events.hpp>

#include "ScriptExtension.hpp"
#include "Script.hpp"

namespace Components::GSC
{
	std::unordered_map<const char*, const char*> ScriptExtension::ReplacedFunctions;
	const char* ScriptExtension::ReplacedPos = nullptr;
	nlohmann::json data;

	typedef void(*FireWeapon_t)(Game::gentity_s* ent, int gametime, int shotCount);
	FireWeapon_t FireWeapon = FireWeapon_t(0x4A4D50);

	const char* ScriptExtension::GetCodePosForParam(int index)
	{
		if (static_cast<unsigned int>(index) >= Game::scrVmPub->outparamcount)
		{
			Game::Scr_ParamError(static_cast<unsigned int>(index), "GetCodePosForParam: Index is out of range!");
			return "";
		}

		const auto* value = &Game::scrVmPub->top[-index];

		if (value->type != Game::VAR_FUNCTION)
		{
			Game::Scr_ParamError(static_cast<unsigned int>(index), "GetCodePosForParam: Expects a function as parameter!");
			return "";
		}

		return value->u.codePosValue;
	}

	void ScriptExtension::GetReplacedPos(const char* pos)
	{
		if (!pos)
		{
			// This seems to happen often and there should not be pointers to NULL in our map
			return;
		}

		if (const auto itr = ReplacedFunctions.find(pos); itr != ReplacedFunctions.end())
		{
			ReplacedPos = itr->second;
		}
	}

	void ScriptExtension::SetReplacedPos(const char* what, const char* with)
	{
		if (!*what || !*with)
		{
			Logger::Warning(Game::CON_CHANNEL_SCRIPT, "Invalid parameters passed to ReplacedFunctions\n");
			return;
		}

		if (ReplacedFunctions.contains(what))
		{
			Logger::Warning(Game::CON_CHANNEL_SCRIPT, "ReplacedFunctions already contains codePosValue for a function\n");
		}

		ReplacedFunctions[what] = with;
	}

	__declspec(naked) void ScriptExtension::VMExecuteInternalStub()
	{
		__asm
		{
			pushad

			push edx
			call GetReplacedPos

			pop edx
			popad

			cmp ReplacedPos, 0
			jne SetPos

			movzx eax, byte ptr [edx]
			inc edx

		Loc1:
			cmp eax, 0x8B

			push ecx

			mov ecx, 0x2045094
			mov [ecx], eax

			mov ecx, 0x2040CD4
			mov [ecx], edx

			pop ecx

			push 0x61E944
			ret

		SetPos:
			mov edx, ReplacedPos
			mov ReplacedPos, 0

			movzx eax, byte ptr [edx]
			inc edx

			jmp Loc1
		}
	}

	void ScriptExtension::AddFunctions()
	{
		Script::AddFunction("IsArray", [] // gsc: IsArray(<object>)
		{
			auto type = Game::Scr_GetType(0);

			bool result;
			if (type == Game::VAR_POINTER)
			{
				type = Game::Scr_GetPointerType(0);
				assert(type >= Game::FIRST_OBJECT);
				result = (type == Game::VAR_ARRAY);
			}
			else
			{
				assert(type < Game::FIRST_OBJECT);
				result = false;
			}

			Game::Scr_AddBool(result);
		});

		Script::AddFunction("ReplaceFunc", [] // gsc: ReplaceFunc(<function>, <function>)
		{
			if (Game::Scr_GetNumParam() != 2)
			{
				Game::Scr_Error("ReplaceFunc: Needs two parameters!");
				return;
			}

			const auto what = GetCodePosForParam(0);
			const auto with = GetCodePosForParam(1);

			SetReplacedPos(what, with);
		});


		Script::AddFunction("GetSystemMilliseconds", [] // gsc: GetSystemMilliseconds()
		{
			SYSTEMTIME time;
			GetSystemTime(&time);

			Game::Scr_AddInt(time.wMilliseconds);
		});

		Script::AddFunction("Exec", [] // gsc: Exec(<string>)
		{
			const auto* str = Game::Scr_GetString(0);
			if (!str)
			{
				Game::Scr_ParamError(0, "Exec: Illegal parameter!");
				return;
			}

			Command::Execute(str, false);
		});

		// Allow printing to the console even when developer is 0
		Script::AddFunction("PrintConsole", [] // gsc: PrintConsole(<string>)
		{
			for (std::size_t i = 0; i < Game::Scr_GetNumParam(); ++i)
			{
				const auto* str = Game::Scr_GetString(i);
				if (!str)
				{
					Game::Scr_ParamError(i, "PrintConsole: Illegal parameter!");
					return;
				}

				Logger::Print(Game::level->scriptPrintChannel, "{}", str);
			}
		});

		Script::AddFunction("DataFileExists", []
		{
			Game::Scr_AddBool(Utils::IO::FileExists("data.json"));
		});

		Script::AddFunction("ReadDataFile", []
		{
			data = nlohmann::json::parse(Utils::IO::ReadFile("data.json"));
		});

		Script::AddFunction("WriteDataFile", []
		{
			Utils::IO::WriteFile("data.json", data.dump(4));
		});

		Script::AddFunction("SetDataValue", []
		{
			std::string playerName = Game::Scr_GetString(0);
			std::string key = Game::Scr_GetString(1);
			std::string value = Game::Scr_GetString(2);

			data[playerName][key] = value;
		});

		Script::AddFunction("DataHasValue", []
		{
			std::string playerName = Game::Scr_GetString(0);
			std::string key = Game::Scr_GetString(1);

			Game::Scr_AddBool(data.contains(playerName) && data[playerName].contains(key));
		});

		Script::AddFunction("GetDataValue", []
		{
			std::string playerName = Game::Scr_GetString(0);
			std::string key = Game::Scr_GetString(1);

			if (data.contains(playerName) && data[playerName].contains(key))
			{
				Game::Scr_AddString(data[playerName][key].get<std::string>().data());
			}
			else
			{
				Game::Scr_AddString("NO VALUE");
			}
		});

		Script::AddMethod("fire_weapon", [](Game::scr_entref_t entref)
		{
			FireWeapon(&Game::g_entities[entref.entnum], 1, 55);
		});

		Script::AddMethod("InstaShoot", [](Game::scr_entref_t entref)
		{
			auto* ent = Script::Scr_GetPlayerEntity(entref);
			if (!ent)
				return;

			for (int hand = 0; hand < 2; hand++)
			{
				ent->client->ps.weapState[hand].weaponState = 0;
				ent->client->ps.weapState[hand].weaponTime = 0;
				ent->client->ps.weapState[hand].weaponDelay = 0;
			}
		});

		Script::AddMethod("SmoothAction", [](Game::scr_entref_t entref)
		{
			auto* ent = Script::Scr_GetPlayerEntity(entref);
			if (!ent)
				return;

			for (int hand = 0; hand < 2; hand++)
			{
				ent->client->ps.weapState[hand].weaponState = 0;
				ent->client->ps.weapState[hand].weaponTime = 0;
				ent->client->ps.weapState[hand].weaponDelay = 0;
				ent->client->ps.weapState[hand].weapAnim = 1;
			}
		});

		Script::AddMethod("SetWeaponAnimation", [](Game::scr_entref_t entref)
		{
			auto* ent = Script::Scr_GetPlayerEntity(entref);
			if (!ent)
				return;

			int anim = Game::Scr_GetInt(0);
			std::string hand = Game::Scr_GetString(1);

			if (hand == "both")
			{
				for (int i = 0; i < 2; i++)
				{
					ent->client->ps.weapState[i].weapAnim = anim;
				}
			}
			else if (hand == "left")
			{
				ent->client->ps.weapState[1].weapAnim = anim;
			}
			else if (hand == "right")
			{
				ent->client->ps.weapState[0].weapAnim = anim;
			}
		});

		Script::AddMethod("set_gunlock", [](Game::scr_entref_t entref)
		{
			auto* ent = Script::Scr_GetPlayerEntity(entref);
			if (!ent)
				return;

			std::string weapon = Game::Scr_GetString(0);
			int model = Game::Scr_GetInt(1);
			auto* state = Game::BG_GetEquippedWeaponState(&ent->client->ps, Game::G_GetWeaponIndexForName(weapon.data()));
			if (state != nullptr)
			{
				state->weaponModel = (char)model;
			}
		});

		Script::AddMethod("rename_player", [](Game::scr_entref_t entref) // gsc: self rename_player(name);
		{
			auto stringValue = Game::Scr_GetString(0);
			strcpy(Game::g_entities[entref.entnum].client->sess.cs.name, stringValue);
			strcpy(Game::g_entities[entref.entnum].client->sess.newnetname, stringValue);
		});
	}

	ScriptExtension::ScriptExtension()
	{
		AddFunctions();

		Utils::Hook(0x61E92E, VMExecuteInternalStub, HOOK_JUMP).install()->quick();
		Utils::Hook::Nop(0x61E933, 1);

		Events::OnVMShutdown([]
		{
			ReplacedFunctions.clear();
		});
	}
}
