// Include the game classes.
#include "SDK.h"

// Include the user defined skins.
#include "Skins.h"

// Define global pointers to game classes.
IVEngineClient* g_EngineClient = nullptr;
IClientEntityList* g_EntityList = nullptr;

// Define the calling convention for the FrameStageNotify function.
typedef void(__thiscall *FrameStageNotify)(void*, ClientFrameStage_t);
FrameStageNotify fnOriginalFunction = NULL;

// Function to apply skin data to weapons.
inline bool ApplyCustomSkin(DWORD hWeapon) {
	// Get the weapon entity from the provided handle.
	CBaseAttributableItem* pWeapon = (CBaseAttributableItem*)g_EntityList->GetClientEntityFromHandle(hWeapon);

	if (!pWeapon)
		return false;

	// Get the weapons item definition index.
	int nWeaponIndex = *pWeapon->GetItemDefinitionIndex();

	// Check if this weapon has a valid override defined.
	if (g_SkinChangerCfg.find(nWeaponIndex) == g_SkinChangerCfg.end())
		return false;

	// Apply our changes to the fallback variables.
	*pWeapon->GetFallbackPaintKit() = g_SkinChangerCfg[nWeaponIndex].nFallbackPaintKit;
	*pWeapon->GetEntityQuality() = g_SkinChangerCfg[nWeaponIndex].iEntityQuality;
	*pWeapon->GetFallbackSeed() = g_SkinChangerCfg[nWeaponIndex].nFallbackSeed;
	*pWeapon->GetFallbackStatTrak() = g_SkinChangerCfg[nWeaponIndex].nFallbackStatTrak;
	*pWeapon->GetFallbackWear() = g_SkinChangerCfg[nWeaponIndex].flFallbackWear;

	// If a name is defined, write it now.
	if (g_SkinChangerCfg[nWeaponIndex].szCustomName) {
		sprintf_s(pWeapon->GetCustomName(), 32, "%s", g_SkinChangerCfg[nWeaponIndex].szCustomName);
	}

	// Edit "m_iItemIDHigh" so fallback values will be used.
	*pWeapon->GetItemIDHigh() = -1;

	return true;
}

void __fastcall FrameStageNotifyThink(void* ecx, void* edx, ClientFrameStage_t Stage) {
	while (Stage == FRAME_NET_UPDATE_POSTDATAUPDATE_START) {
		// Get our player entity.
		IClientEntity* pLocal = g_EntityList->GetClientEntity(g_EngineClient->GetLocalPlayer());

		// Don't change anything if we're not alive.
		if (!pLocal || pLocal->GetLifeState() != LIFE_ALIVE)
			break;

		// Get handles to weapons we're carrying.
		UINT* hWeapons = pLocal->GetWeapons();

		if (!hWeapons)
			break;

		// Loop through weapons and run our skin function on them.
		for (int nIndex = 0; hWeapons[nIndex]; nIndex++) {
			ApplyCustomSkin(hWeapons[nIndex]);
		}

		break;
	}

	// Run the original FrameStageNotify function.
	fnOriginalFunction(ecx, Stage);
}

void Initialise() {
	// Get the "CreateInterface" function from "client.dll" and "engine.dll".
	CreateInterfaceFn fnClientFactory = (CreateInterfaceFn)GetProcAddress(GetModuleHandleA("client.dll"), "CreateInterface");
	CreateInterfaceFn fnEngineFactory = (CreateInterfaceFn)GetProcAddress(GetModuleHandleA("engine.dll"), "CreateInterface");
	
	// Call "CreateInterface" to get the IBaseClientDLL, IClientEntityList and IVEngineClient classes.
	g_EntityList = (IClientEntityList*)fnClientFactory("VClientEntityList003", NULL);
	g_EngineClient = (IVEngineClient*)fnEngineFactory("VEngineClient013", NULL);

	// We don't implement IBaseClientDLL so we won't cast it.
	void* pClientDLL = fnClientFactory("VClient017", NULL);

	// Get the virtual method table for IBaseClientDLL.
	PDWORD* pClientDLLVMT = (PDWORD*)pClientDLL;

	// Save the untouched table so we know where the original functions are.
	PDWORD pOriginalClientDLLVMT = *pClientDLLVMT;

	// Calculate the size of the table.
	size_t dwVMTSize = 0;

	while ((PDWORD)(*pClientDLLVMT)[dwVMTSize])
		dwVMTSize++;

	// Create the replacement table.
	PDWORD pNewClientDLLVMT = new DWORD[dwVMTSize];

	// Copy the original table into the replacement table.
	CopyMemory(pNewClientDLLVMT, pOriginalClientDLLVMT, (sizeof(DWORD) * dwVMTSize));

	// Change the FrameStageNotify function in the new table to point to our function.
	pNewClientDLLVMT[36] = (DWORD)FrameStageNotifyThink;

	// Backup the original function from the untouched table.
	fnOriginalFunction = (FrameStageNotify)pOriginalClientDLLVMT[36];

	// Write the virtual method table.
	*pClientDLLVMT = pNewClientDLLVMT;

	// Import skins to use.
	SetSkinConfig();
}

bool __stdcall DllMain(HINSTANCE hDLLInstance, DWORD dwReason, LPVOID lpReserved) {
	if (dwReason == DLL_PROCESS_ATTACH) {
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Initialise, 0, 0, 0);
	}

	return true;
}