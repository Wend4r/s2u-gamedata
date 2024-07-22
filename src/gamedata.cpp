/**
 * vim: set ts=4 sw=4 tw=99 noet :
 * ======================================================
 * Universal gamedata parser for Source2 games.
 * Written by Wend4r (2023).
 * ======================================================

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <gamedata.hpp>

#include <stdlib.h>
#include <iterator>

#include <tier0/platform.h>
#include <tier1/keyvalues3.h>

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

DLL_IMPORT IVEngineServer *engine;
DLL_IMPORT IServerGameDLL *server;

DynLibUtils::CModule g_aLibEngine, 
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

const DynLibUtils::CModule *GameData::FindLibrary(std::string sName) const
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

bool GameData::Config::Load(IGameData *pRoot, KeyValues3 *pGameConfig, char *psError, size_t nMaxLength)
{
	const char *pszEngineName = GameData::GetSourceEngineName();

	KeyValues3 *pEngineValues = pGameConfig->FindMember(pszEngineName);

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

bool GameData::Config::LoadEngine(IGameData *pRoot, KeyValues3 *pEngineValues, char *psError, size_t nMaxLength)
{
	struct
	{
		CKV3MemberName aMember;
		bool (GameData::Config::*pfnLoadOne)(IGameData *pRoot, KeyValues3 *pValues, char *psError, size_t nMaxLength);
	} aSections[] =
	{
		{
			"Signatures",
			&GameData::Config::LoadEngineSignatures
		},
		{
			"Offsets",
			&GameData::Config::LoadEngineOffsets
		},
		{
			"Addresses",
			&GameData::Config::LoadEngineAddresses
		}
	};

	char sSubError[MAX_GAMEDATA_ERROR_LENGTH];

	for(size_t n = 0, nSize = std::size(aSections); n < nSize; n++)
	{
		auto &aSection = aSections[n];

		auto &aSectionMember = aSection.aMember;

		KeyValues3 *pEngineMember = pEngineValues->FindMember(aSectionMember);

		if(pEngineMember && !(this->*(aSections[n].pfnLoadOne))(pRoot, pEngineMember, (char *)sSubError, sizeof(sSubError)))
		{
			if(psError)
			{
				snprintf(psError, nMaxLength, "Failed to load \"%s\" section: %s", aSectionMember.GetString(), sSubError);
			}

			return false;
		}
	}

	return true;
}

bool GameData::Config::LoadEngineSignatures(IGameData *pRoot, KeyValues3 *pSignaturesValues, char *psError, size_t nMaxLength)
{
	int iMemberCount = pSignaturesValues->GetMemberCount();

	if(!iMemberCount)
	{
		if(psError)
		{
			strncpy(psError, "Section is empty", nMaxLength);
		}

		return false;
	}

	KV3MemberId_t i = 0;

	const char *pszLibraryKey = "library", 
	           *pszPlatformKey = GameData::GetCurrentPlatformName();

	do
	{
		KeyValues3 *pSigSection = pSignaturesValues->GetMember(i);

		const char *pszSigName = pSigSection->GetString("<none>");

		KeyValues3 *pLibraryValues = pSigSection->FindMember(pszLibraryKey);

		if(!pLibraryValues)
		{
			if(psError)
			{
				snprintf(psError, nMaxLength, "Failed to get \"%s\" key at \"%s\"", pszLibraryKey, pszSigName);
			}

			return false;
		}

		const char *pszLibraryName = pLibraryValues->GetString("<none>");

		const auto *pLibModule = pRoot->FindLibrary(pszLibraryName);

		if(!pLibModule)
		{
			if(psError)
			{
				snprintf(psError, nMaxLength, "Unknown \"%s\" library at \"%s\"", pszLibraryName, pszSigName);
			}

			return false;
		}

		KeyValues3 *pPlatformValues = pSigSection->FindMember(pszPlatformKey);

		if(!pPlatformValues)
		{
			if(psError)
			{
				snprintf(psError, nMaxLength, "Failed to get platform (\"%s\" key) at \"%s\"", pszPlatformKey, pszSigName);
			}

			return false;
		}

		const char *pszSignature = pPlatformValues->GetString();

		const auto pSigResult = pLibModule->FindPattern(pszSignature);

		if(!pSigResult)
		{
			if(psError)
			{
				snprintf(psError, nMaxLength, "Failed to find \"%s\"", pszSigName);
			}

			return false;
		}

		this->SetAddress(pszSigName, pSigResult);

		i++;
	}
	while(i < iMemberCount);

	return true;
}

bool GameData::Config::LoadEngineOffsets(IGameData *pRoot, KeyValues3 *pOffsetsValues, char *psError, size_t nMaxLength)
{
	int iMemberCount = pOffsetsValues->GetMemberCount();

	if(!iMemberCount)
	{
		if(psError)
		{
			strncpy(psError, "Offsets section is empty", nMaxLength);
		}

		return false;
	}

	KV3MemberId_t i = 0;

	const char *pszPlatformKey = GameData::GetCurrentPlatformName();

	do
	{
		KeyValues3 *pOffsetSection = pOffsetsValues->GetMember(i);

		const char *pszOffsetName = pOffsetSection->GetString();

		KeyValues3 *pPlatformValues = pOffsetSection->FindMember(pszPlatformKey);

		if(!pPlatformValues)
		{
			if(psError)
			{
				snprintf(psError, nMaxLength, "Failed to get platform (\"%s\" key) at \"%s\" offset", pszPlatformKey, pszOffsetName);
			}

			return false;
		}

		this->SetOffset(pszOffsetName, GameData::ReadOffset(pPlatformValues->GetString()));

		i++;
	}
	while(i < iMemberCount);

	return true;
}

bool GameData::Config::LoadEngineAddresses(IGameData *pRoot, KeyValues3 *pAddressesValues, char *psError, size_t nMaxLength)
{
	int iMemberCount = pAddressesValues->GetMemberCount();

	if(!iMemberCount && psError)
	{
		strncpy(psError, "Section is empty", nMaxLength);

		return false;
	}

	KV3MemberId_t i = 0;

	const char *pszSignatureKey = "signature";

	char sAddressActionsError[256];

	do
	{
		KeyValues3 *pAddrSection = pAddressesValues->GetMember(i);

		const char *pszAddressName = pAddrSection->GetString();

		KeyValues3 *pSignatureValues = pAddrSection->FindMember(pszSignatureKey);

		if(!pSignatureValues)
		{
			if(psError)
			{
				snprintf(psError, nMaxLength, "Failed to get \"%s\" signature at \"%s\" address", pszSignatureKey, pszAddressName);
			}

			return false;
		}

		const char *pszSignatureName = pSignatureValues->GetString();

		const auto &pSigAddress = this->GetAddress(pszSignatureName);

		if(!pSigAddress)
		{
			if(psError)
			{
				snprintf(psError, nMaxLength, "Failed to get \"%s\" signature in \"%s\" address", pszSignatureName, pszAddressName);
			}

			return false;
		}

		uintptr_t pAddrCur = pSigAddress.GetPtr();

		// Remove an extra keys.
		{
			pAddrSection->RemoveMember(pSignatureValues);

			int iCurrentPlat = This::GetCurrentPlatform();

			for(int iPlat = PLAT_FIRST; iPlat < PLAT_MAX; iPlat++)
			{
				if(iCurrentPlat != iPlat)
				{
					pAddrSection->RemoveMember(This::GetPlatformName((This::Platform)iPlat));
				}
			}
		}

		if(!this->LoadEngineAddressActions(pRoot, pAddrCur, pAddrSection, (char *)sAddressActionsError, sizeof(sAddressActionsError)))
		{
			if(psError)
			{
				snprintf(psError, nMaxLength, "Failed to get \"%s\" address action: %s\n", pszAddressName, sAddressActionsError);
			}

			return false;
		}

		this->SetAddress(pszAddressName, pAddrCur);

		i++;
	}
	while(i < iMemberCount);

	return true;
}

bool GameData::Config::LoadEngineAddressActions(IGameData *pRoot, uintptr_t &pAddrCur, KeyValues3 *pActionsValues, char *psError, size_t nMaxLength)
{
	int iMemberCount = pActionsValues->GetMemberCount();

	if(!iMemberCount)
	{
		if(psError)
		{
			strncpy(psError, "Section is empty", nMaxLength);
		}

		return false;
	}

	KV3MemberId_t i = 0;

	do
	{
		KeyValues3 *pAction = pActionsValues->GetMember(i);

		const char *pszName = pAction->GetString();

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
			else
			{
				if(psError)
				{
					snprintf(psError, nMaxLength, "Unknown \"%s\" read key", pszName);
				}

				return false;
			}
		}
		else if(!strcmp(pszName, GameData::GetCurrentPlatformName()))
		{
			return this->LoadEngineAddressActions(pRoot, pAddrCur, pAction, psError, nMaxLength); // Recursive by platform.
		}
		else
		{
			if(psError)
			{
				snprintf(psError, nMaxLength, "Unknown \"%s\" key", pszName);
			}

			return false;
		}

		i++;
	}
	while(i < iMemberCount);

	return true;
}

const DynLibUtils::CMemory &GameData::Config::GetAddress(const std::string &sName) const
{
	return this->m_aAddressStorage.Get(sName);
}

const ptrdiff_t &GameData::Config::GetOffset(const std::string &sName) const
{
	return this->m_aOffsetStorage.Get(sName);
}

void GameData::Config::SetAddress(const std::string &sName, DynLibUtils::CMemory aMemory)
{
	this->m_aAddressStorage.Set(sName, aMemory);
}

void GameData::Config::SetOffset(const std::string &sName, ptrdiff_t nValue)
{
	this->m_aOffsetStorage.Set(sName, nValue);
}
