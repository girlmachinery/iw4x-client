#include "..\STDInclude.hpp"

namespace Components
{
	Dvar::Var::Var(std::string dvarName) : Var()
	{
		this->dvar = Game::Dvar_FindVar(dvarName.data());

		if (!this->dvar)
		{
			// Register the dvar?
		}
	}

	template <> Game::dvar_t* Dvar::Var::Get()
	{
		return this->dvar;
	}
	template <> char* Dvar::Var::Get()
	{
		if (this->dvar && this->dvar->type == Game::dvar_type::DVAR_TYPE_STRING && this->dvar->current.string)
		{
			return this->dvar->current.string;
		}

		return "";
	}
	template <> const char* Dvar::Var::Get()
	{
		return this->Get<char*>();
	}
	template <> int Dvar::Var::Get()
	{
		if (this->dvar && this->dvar->type == Game::dvar_type::DVAR_TYPE_INT)
		{
			return this->dvar->current.integer;
		}

		return 0;
	}
	template <> float Dvar::Var::Get()
	{
		if (this->dvar && this->dvar->type == Game::dvar_type::DVAR_TYPE_FLOAT)
		{
			return this->dvar->current.value;
		}

		return 0;
	}
	template <> float* Dvar::Var::Get()
	{
		static float val[4] = { 0 };

		if (this->dvar && (this->dvar->type == Game::dvar_type::DVAR_TYPE_FLOAT_2 || this->dvar->type == Game::dvar_type::DVAR_TYPE_FLOAT_3 || this->dvar->type == Game::dvar_type::DVAR_TYPE_FLOAT_4))
		{
			return this->dvar->current.vec4;
		}

		return val;
	}
	template <> bool Dvar::Var::Get()
	{
		if (this->dvar && this->dvar->type == Game::dvar_type::DVAR_TYPE_BOOL)
		{
			return this->dvar->current.boolean;
		}

		return false;
	}

	void Dvar::Var::Set(char* string)
	{
		this->Set(reinterpret_cast<const char*>(string));
	}
	void Dvar::Var::Set(const char* string)
	{
		if (this->dvar && this->dvar->name)
		{
			Game::Dvar_SetCommand(this->dvar->name, string);
		}
	}
	void Dvar::Var::Set(std::string string)
	{
		this->Set(string.data());
	}
	void Dvar::Var::Set(int integer)
	{
		if (this->dvar && this->dvar->name)
		{
			Game::Dvar_SetCommand(this->dvar->name, Utils::VA("%i", integer));
		}
	}
	void Dvar::Var::Set(float value)
	{
		if (this->dvar && this->dvar->name)
		{
			Game::Dvar_SetCommand(this->dvar->name, Utils::VA("%f", value));
		}
	}

	template<> static Dvar::Var Dvar::Var::Register(const char* name, bool value, Game::dvar_flag flag, const char* description)
	{
		return Game::Dvar_RegisterBool(name, value, flag, description);
	}
	template<> static Dvar::Var Dvar::Var::Register(const char* name, const char* value, Game::dvar_flag flag, const char* description)
	{
		return Game::Dvar_RegisterString(name, value, flag, description);
	}

	Game::dvar_t* Dvar::RegisterName(const char* name, const char* default, Game::dvar_flag flag, const char* description)
	{
		// TODO: Register string dvars here

		return Dvar::Var::Register<const char*>(name, "Unknown Soldier", (Game::dvar_flag)(flag | Game::dvar_flag::DVAR_FLAG_SAVED), description).Get<Game::dvar_t*>();
	}

	Dvar::Dvar()
	{
		// set flags of cg_drawFPS to archive
		Utils::Hook::Or<BYTE>(0x4F8F69, Game::dvar_flag::DVAR_FLAG_SAVED);

		// un-cheat cg_fov and add archive flags
		Utils::Hook::Xor<BYTE>(0x4F8E35, Game::dvar_flag::DVAR_FLAG_CHEAT | Game::dvar_flag::DVAR_FLAG_SAVED);

		// set cg_fov max to 90.0
		static float cgFov90 = 90.0f;
		Utils::Hook::Set<float*>(0x4F8E28, &cgFov90);

		// Hook dvar 'name' registration
		Utils::Hook(0x40531C, Dvar::RegisterName, HOOK_CALL).Install()->Quick();
	}
}
