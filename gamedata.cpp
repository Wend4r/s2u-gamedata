/**
 * vim: set ts=4 sw=4 tw=99 noet :
 * ======================================================
 * Universal gamedata parser for Source2 games.
 * Written by Wend4r (2023).
 * ======================================================
 */

#include "gamedata.hpp"

#include <stdlib.h>

#include <tier0/keyvalues.h>

// Windows: linkage corresponds to a final class type.
#ifdef META_IS_SOURCE2
class IVEngineServer2;
class IFileSystem;
class ISource2Server;

typedef IVEngineServer2 IVEngineServer;
typedef ISource2Server IServerGameDLL;
#else
class IVEngineServer;
class IFileSystem;
class IServerGameDLL;
#endif

extern IVEngineServer *engine;
extern IFileSystem *filesystem;
extern IServerGameDLL *server;

CModule g_aLibEngine, 
        g_aLibServer;

bool GameData::Init(char *psError, size_t nMaxLength)
{
	bool bResult = g_aLibEngine.InitFromMemory(engine);

	if(bResult)
	{
		this->m_aLibraryMap["engine"] = &g_aLibEngine;

		bResult = g_aLibServer.InitFromMemory(server);

		if(bResult)
		{
			this->m_aLibraryMap["server"] = &g_aLibServer;
		}
		else
		{
			strncpy(psError, "Failed to get server module", nMaxLength);
		}
	}
	else
	{
		strncpy(psError, "Failed to get engine module", nMaxLength);
	}

	return true;
}

void GameData::Clear()
{
	this->m_aLibraryMap.clear();
}

void GameData::Destroy()
{
	// ...
}

const CModule *GameData::FindLibrary(std::string sName) const
{
	auto itResult = this->m_aLibraryMap.find(sName);

	return itResult == this->m_aLibraryMap.cend() ? nullptr : itResult->second;
}

const char *GameData::GetSourceEngineName()
{
#if SOURCE_ENGINE == SE_CS2
	return "cs2";
#elif SOURCE_ENGINE == SE_DOTA
	return "dota";
#else
#	error "Unknown engine type"
	return "unknown";
#endif
}

GameData::Platform GameData::GetCurrentPlatform()
{
#if defined(_WINDOWS)
#	if defined(X64BITS)
	return Platform::PLAT_WINDOWS64;
#	else
	return Platform::PLAT_WINDOWS;
#	endif
#elif defined(_LINUX)
#	if defined(X64BITS)
	return Platform::PLAT_LINUX64;
#	else
	return Platform::PLAT_LINUX;
#	endif
#else
#	error Unsupported platform
	return Platform::PLAT_UNKNOWN;
#endif
}

const char *GameData::GetCurrentPlatformName()
{
	return This::GetPlatformName(This::GetCurrentPlatform());
}

const char *GameData::GetPlatformName(Platform eElm)
{
	const char *sPlatformNames[Platform::PLAT_MAX] =
	{
		"windows",
		"windows64",

		"linux",
		"linux64",

		"mac",
		"mac64"
	};

	return sPlatformNames[eElm];
}

ptrdiff_t GameData::ReadOffset(const char *pszValue)
{
	return static_cast<ptrdiff_t>(strtol(pszValue, NULL, 0));
}

GameData::Config::Config(Addresses aAddressStorage, Offsets aOffsetsStorage)
 :  m_aAddressStorage(aAddressStorage), 
    m_aOffsetStorage(aOffsetsStorage)
{
}

bool GameData::Config::Load(IGameData *pRoot, KeyValues *pGameConfig, char *psError, size_t nMaxLength)
{
	const char *pszEngineName = GameData::GetSourceEngineName();

	KeyValues *pEngineValues = pGameConfig->FindKey(pszEngineName, false);

	bool bResult = pEngineValues != nullptr;

	if(bResult)
	{
		bResult = this->LoadEngine(pRoot, pEngineValues, psError, nMaxLength);
	}
	else if(psError)
	{
		snprintf(psError, nMaxLength, "Failed to find \"%s\" section", pszEngineName);
	}

	return bResult;
}

void GameData::Config::ClearValues()
{
	this->m_aAddressStorage.ClearValues();
	this->m_aOffsetStorage.ClearValues();
}

GameData::Config::Addresses &GameData::Config::GetAddresses()
{
	return this->m_aAddressStorage;
}

GameData::Config::Offsets &GameData::Config::GetOffsets()
{
	return this->m_aOffsetStorage;
}

bool GameData::Config::LoadEngine(IGameData *pRoot, KeyValues *pEngineValues, char *psError, size_t nMaxLength)
{
	KeyValues *pSectionValues = pEngineValues->FindKey("Signatures", false);

	bool bResult = true;

	if(pSectionValues) // Ignore the section not found for result.
	{
		bResult = this->LoadEngineSignatures(pRoot, pSectionValues, psError, nMaxLength);
	}

	if(bResult)
	{
		pSectionValues = pEngineValues->FindKey("Offsets", false);

		if(pSectionValues) // Same ignore.
		{
			bResult = this->LoadEngineOffsets(pSectionValues, psError, nMaxLength);
		}
	}

	if(bResult)
	{
		pSectionValues = pEngineValues->FindKey("Addresses", false);

		if(pSectionValues)
		{
			bResult = this->LoadEngineAddresses(pSectionValues, psError, nMaxLength);
		}
	}

	return bResult;
}

bool GameData::Config::LoadEngineSignatures(IGameData *pRoot, KeyValues *pSignaturesValues, char *psError, size_t nMaxLength)
{
	KeyValues *pSigSection = pSignaturesValues->GetFirstSubKey();

	bool bResult = pSigSection != nullptr;

	if(bResult)
	{
		const char *pszLibraryKey = "library", 
		           *pszPlatformKey = GameData::GetCurrentPlatformName();

		do
		{
			const char *pszSigName = pSigSection->GetName();

			KeyValues *pLibraryValues = pSigSection->FindKey(pszLibraryKey, false);

			bResult = pLibraryValues != nullptr;

			if(bResult)
			{
				const char *pszLibraryName = pLibraryValues->GetString(nullptr, "unknown");

				const CModule *pLibModule = pRoot->FindLibrary(pszLibraryName);

				bResult = (bool)pLibModule;

				if(bResult)
				{
					KeyValues *pPlatformValues = pSigSection->FindKey(pszPlatformKey, false);

					bResult = pPlatformValues != nullptr;

					if(pPlatformValues)
					{
						const char *pszSignature = pPlatformValues->GetString(nullptr);

						CMemory pSigResult = pLibModule->FindPatternSIMD(pszSignature);

						bResult = (bool)pSigResult;

						if(bResult)
						{
							this->SetAddress(pszSigName, pSigResult);
						}
						else if(psError)
						{
							snprintf(psError, nMaxLength, "Failed to find \"%s\" signature", pszSigName);
						}
					}
					else if(psError)
					{
						snprintf(psError, nMaxLength, "Failed to get platform (\"%s\" key) at \"%s\" signature", pszPlatformKey, pszSigName);
					}
				}
				else if(psError)
				{
					snprintf(psError, nMaxLength, "Unknown \"%s\" library at \"%s\" signature", pszLibraryName, pszSigName);
				}
			}
			else if(psError)
			{
				snprintf(psError, nMaxLength, "Failed to get \"%s\" key at \"%s\" signature", pszLibraryKey, pszSigName);
			}

			pSigSection = pSigSection->GetNextKey();
		}
		while(pSigSection);
	}
	else if(psError)
	{
		strncpy(psError, "Signatures section is empty", nMaxLength);
	}

	return bResult;
}

bool GameData::Config::LoadEngineOffsets(KeyValues *pOffsetsValues, char *psError, size_t nMaxLength)
{
	KeyValues *pOffsetSection = pOffsetsValues->GetFirstSubKey();

	bool bResult = pOffsetSection != nullptr;

	if(bResult)
	{
		const char *pszPlatformKey = GameData::GetCurrentPlatformName();

		do
		{
			const char *pszOffsetName = pOffsetSection->GetName();

			KeyValues *pPlatformValues = pOffsetSection->FindKey(pszPlatformKey, false);

			bResult = pPlatformValues != nullptr;

			if(pPlatformValues)
			{
				this->SetOffset(pszOffsetName, GameData::ReadOffset(pPlatformValues->GetString()));
			}
			else if(psError)
			{
				snprintf(psError, nMaxLength, "Failed to get platform (\"%s\" key) at \"%s\" signature", pszPlatformKey, pszOffsetName);
			}

			pOffsetSection = pOffsetSection->GetNextKey();
		}
		while(pOffsetSection);
	}
	else if(psError)
	{
		strncpy(psError, "Offsets section is empty", nMaxLength);
	}

	return bResult;
}

bool GameData::Config::LoadEngineAddresses(KeyValues *pAddressesValues, char *psError, size_t nMaxLength)
{
	KeyValues *pAddrSection = pAddressesValues->GetFirstSubKey();

	bool bResult = pAddrSection != nullptr;

	if(bResult)
	{
		const char *pszSignatureKey = "signature";

		char sAddressActionsError[256];

		do
		{
			const char *pszAddressName = pAddrSection->GetName();

			KeyValues *pSignatureValues = pAddrSection->FindKey(pszSignatureKey, false);

			bResult = pSignatureValues != nullptr;

			if(bResult)
			{
				const char *pszSignatureName = pSignatureValues->GetString();

				CMemory pSigAddress = this->GetAddress(pszSignatureName);

				if(pSigAddress)
				{
					uintptr_t pAddrCur = pSigAddress.GetPtr();

					// Remove an extra keys.
					{
						pAddrSection->RemoveSubKey(pSignatureValues, true /* bDelete */, true);

						int iCurrentPlat = This::GetCurrentPlatform();

						for(int iPlat = PLAT_FIRST; iPlat < PLAT_MAX; iPlat++)
						{
							if(iCurrentPlat != iPlat)
							{
								pAddrSection->FindAndDeleteSubKey(This::GetPlatformName((This::Platform)iPlat));
							}
						}
					}

					if(this->LoadEngineAddressActions(pAddrCur, pAddrSection, (char *)sAddressActionsError, sizeof(sAddressActionsError)))
					{
						this->SetAddress(pszAddressName, pAddrCur);
					}
					else if(psError)
					{
						snprintf(psError, nMaxLength, "Failed to get \"%s\" address action: %s\n", pszAddressName, sAddressActionsError);
					}
				}
				else if(psError)
				{
					snprintf(psError, nMaxLength, "Failed to get \"%s\" signature in \"%s\" address", pszSignatureName, pszAddressName);
				}
			}
			else if(psError)
			{
				snprintf(psError, nMaxLength, "Failed to get signature (\"%s\" key) at \"%s\" address", pszSignatureKey, pszAddressName);
			}

			pAddrSection = pAddrSection->GetNextKey();
		}
		while(pAddrSection);
	}
	else if(psError)
	{
		strncpy(psError, "Signatures section is empty", nMaxLength);
	}

	return bResult;
}

bool GameData::Config::LoadEngineAddressActions(uintptr_t &pAddrCur, KeyValues *pActionSection, char *psError, size_t nMaxLength)
{
	KeyValues *pAction = pActionSection->GetFirstSubKey();

	bool bResult = pAction != nullptr;

	if(bResult)
	{
		do
		{
			const char *pszName = pAction->GetName();

			ptrdiff_t nActionValue = GameData::ReadOffset(pAction->GetString());

			if(!strcmp(pszName, "offset"))
			{
				pAddrCur += nActionValue;
			}
			else if(!strncmp(pszName, "read", 4))
			{
				if(!pszName[4])
				{
					pAddrCur = *(uintptr_t *)(pAddrCur + nActionValue);
				}
				else if(!strcmp(&pszName[4], "_offs32"))
				{
					pAddrCur = pAddrCur + nActionValue + sizeof(int32_t) + *(int32_t *)(pAddrCur + nActionValue);
				}
				else if(psError)
				{
					bResult = false;
					snprintf(psError, nMaxLength, "Unknown \"%s\" read action", pszName);
				}
			}
			else if(!strcmp(pszName, GameData::GetCurrentPlatformName()))
			{
				bResult = this->LoadEngineAddressActions(pAddrCur, pAction, psError, nMaxLength); // Recursive by platform.
			}
			else if(psError)
			{
				bResult = false;
				snprintf(psError, nMaxLength, "Unknown \"%s\" action", pszName);
			}

			pAction = pAction->GetNextKey();
		}
		while(pAction);
	}

	return bResult;
}

const CMemory &GameData::Config::GetAddress(std::string sName) const
{
	return this->m_aAddressStorage.Get(sName);
}

const ptrdiff_t &GameData::Config::GetOffset(std::string sName) const
{
	return this->m_aOffsetStorage.Get(sName);
}

void GameData::Config::SetAddress(std::string sName, CMemory aMemory)
{
	this->m_aAddressStorage.Set(sName, aMemory);
}

void GameData::Config::SetOffset(std::string sName, ptrdiff_t nValue)
{
	this->m_aOffsetStorage.Set(sName, nValue);
}
