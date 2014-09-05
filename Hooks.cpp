#include "sptlib-stdafx.hpp"

#include <unordered_map>

#include <detours.h>

#include "hooks.hpp"
#include "detoursutils.hpp"
#include "memutils.hpp"
#include "sptlib.hpp"

HMODULE WINAPI HOOKED_LoadLibraryA(LPCSTR lpFileName)
{
	return Hooks::getInstance().HOOKED_LoadLibraryA_Func(lpFileName);
}

HMODULE WINAPI HOOKED_LoadLibraryW(LPCWSTR lpFileName)
{
	return Hooks::getInstance().HOOKED_LoadLibraryW_Func(lpFileName);
}

HMODULE WINAPI HOOKED_LoadLibraryExA(LPCSTR lpFileName, HANDLE hFile, DWORD dwFlags)
{
	return Hooks::getInstance().HOOKED_LoadLibraryExA_Func(lpFileName, hFile, dwFlags);
}

HMODULE WINAPI HOOKED_LoadLibraryExW(LPCWSTR lpFileName, HANDLE hFile, DWORD dwFlags)
{
	return Hooks::getInstance().HOOKED_LoadLibraryExW_Func(lpFileName, hFile, dwFlags);
}

BOOL WINAPI HOOKED_FreeLibrary(HMODULE hModule)
{
	return Hooks::getInstance().HOOKED_FreeLibrary_Func(hModule);
}

void Hooks::Init()
{
	_EngineDevMsg("SPTLib version " SPTLIB_VERSION ".\n");
	EngineDevMsg("Modules contain %d entries.\n", modules.size());

	// Try hooking each module in case it is already loaded
	for (auto it = modules.cbegin(); it != modules.cend(); ++it)
	{
		(*it)->TryHookAll();
	}

	ORIG_LoadLibraryA = LoadLibraryA;
	ORIG_LoadLibraryW = LoadLibraryW;
	ORIG_LoadLibraryExA = LoadLibraryExA;
	ORIG_LoadLibraryExW = LoadLibraryExW;
	ORIG_FreeLibrary = FreeLibrary;

	DetoursUtils::AttachDetours(L"WinAPI", {
		{ (PVOID *)(&ORIG_LoadLibraryA), HOOKED_LoadLibraryA },
		{ (PVOID *)(&ORIG_LoadLibraryW), HOOKED_LoadLibraryW },
		{ (PVOID *)(&ORIG_LoadLibraryExA), HOOKED_LoadLibraryExA },
		{ (PVOID *)(&ORIG_LoadLibraryExW), HOOKED_LoadLibraryExW },
		{ (PVOID *)(&ORIG_FreeLibrary), HOOKED_FreeLibrary }
	});
}

void Hooks::Free()
{
	EngineDevMsg("Modules contain %d entries.\n", modules.size());

	// Unhook everything
	for (auto it = modules.begin(); it != modules.end(); ++it)
	{
		EngineDevMsg("Unhooking %s...\n", string_converter.to_bytes((*it)->GetHookedModuleName()).c_str());
		(*it)->Unhook();
	}

	DetoursUtils::DetachDetours(L"WinAPI", {
		{ (PVOID *) (&ORIG_LoadLibraryA), HOOKED_LoadLibraryA },
		{ (PVOID *) (&ORIG_LoadLibraryW), HOOKED_LoadLibraryW },
		{ (PVOID *) (&ORIG_LoadLibraryExA), HOOKED_LoadLibraryExA },
		{ (PVOID *) (&ORIG_LoadLibraryExW), HOOKED_LoadLibraryExW },
		{ (PVOID *) (&ORIG_FreeLibrary), HOOKED_FreeLibrary }
	});

	Clear();
}

void Hooks::Clear()
{
	ORIG_LoadLibraryA = nullptr;
	ORIG_LoadLibraryW = nullptr;
	ORIG_LoadLibraryExA = nullptr;
	ORIG_LoadLibraryExW = nullptr;
	ORIG_FreeLibrary = nullptr;

	modules.clear();
}

void Hooks::HookModule(std::wstring moduleName)
{
	HMODULE module = NULL;
	uintptr_t start = 0;
	size_t size = 0;

	for (auto it = modules.cbegin(); it != modules.cend(); ++it)
	{
		if ((*it)->CanHook(moduleName))
		{
			if ((module != NULL) || (MemUtils::GetModuleInfo(moduleName, &module, &start, &size)))
			{
				EngineDevMsg("Hooking %s (start: %p; size: %x)...\n", string_converter.to_bytes(moduleName).c_str(), start, size);
				(*it)->Hook(moduleName, module, start, size);
			}
			else
			{
				EngineWarning("Unable to obtain the %s module info!\n", string_converter.to_bytes(moduleName).c_str());
				return;
			}
		}
	}

	if (module == NULL)
	{
		EngineDevMsg("Tried to hook an unlisted module: %s\n", string_converter.to_bytes(moduleName).c_str());
	}
}

void Hooks::UnhookModule(std::wstring moduleName)
{
	bool unhookedSomething = false;
	for (auto it = modules.cbegin(); it != modules.cend(); ++it)
	{
		if ((*it)->GetHookedModuleName().compare(moduleName) == 0)
		{
			EngineDevMsg("Unhooking %s...\n", string_converter.to_bytes(moduleName).c_str());
			(*it)->Unhook();
			unhookedSomething = true;
		}
	}
	
	if (!unhookedSomething)
	{
		EngineDevMsg("Tried to unhook an unlisted module: %s\n", string_converter.to_bytes(moduleName).c_str());
	}
}

void Hooks::AddToHookedModules(IHookableModule* module)
{
	if (!module)
	{
		EngineWarning("Tried to add a nullptr module!\n");
		return;
	}

	modules.push_back(module);
}

HMODULE WINAPI Hooks::HOOKED_LoadLibraryA_Func(LPCSTR lpFileName)
{
	HMODULE rv = ORIG_LoadLibraryA(lpFileName);

	EngineDevMsg("Engine call: LoadLibraryA( \"%s\" ) => %p\n", lpFileName, rv);

	if (rv != NULL)
	{
		HookModule( string_converter.from_bytes(lpFileName) );
	}

	return rv;
}

HMODULE WINAPI Hooks::HOOKED_LoadLibraryW_Func(LPCWSTR lpFileName)
{
	HMODULE rv = ORIG_LoadLibraryW(lpFileName);

	EngineDevMsg("Engine call: LoadLibraryW( \"%s\" ) => %p\n", string_converter.to_bytes(lpFileName).c_str(), rv);

	if (rv != NULL)
	{
		HookModule( std::wstring(lpFileName) );
	}

	return rv;
}

HMODULE WINAPI Hooks::HOOKED_LoadLibraryExA_Func(LPCSTR lpFileName, HANDLE hFile, DWORD dwFlags)
{
	HMODULE rv = ORIG_LoadLibraryExA(lpFileName, hFile, dwFlags);

	EngineDevMsg("Engine call: LoadLibraryExA( \"%s\" ) => %p\n", lpFileName, rv);

	if (rv != NULL)
	{
		HookModule( string_converter.from_bytes(lpFileName) );
	}

	return rv;
}

HMODULE WINAPI Hooks::HOOKED_LoadLibraryExW_Func(LPCWSTR lpFileName, HANDLE hFile, DWORD dwFlags)
{
	HMODULE rv = ORIG_LoadLibraryExW(lpFileName, hFile, dwFlags);

	EngineDevMsg("Engine call: LoadLibraryExW( \"%s\" ) => %p\n", string_converter.to_bytes(lpFileName).c_str(), rv);

	if (rv != NULL)
	{
		HookModule( std::wstring(lpFileName) );
	}

	return rv;
}

BOOL WINAPI Hooks::HOOKED_FreeLibrary_Func(HMODULE hModule)
{
	for (auto it = modules.cbegin(); it != modules.cend(); ++it)
	{
		if ((*it)->GetModule() == hModule)
			(*it)->Unhook();
	}

	BOOL rv = ORIG_FreeLibrary(hModule);

	EngineDevMsg("Engine call: FreeLibrary( %p ) => %s\n", hModule, (rv ? "true" : "false"));

	return rv;
}
