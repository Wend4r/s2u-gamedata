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

#include <tier0/commonmacros.h>
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
	return GetPlatformName(GetCurrentPlatform());
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

bool GameData::Config::Load(IGameData *pRoot, KeyValues3 *pGameConfig, CUtlVector<CBufferStringConcat> &vecMessages)
{
	const char *pszEngineName = GameData::GetSourceEngineName();

	KeyValues3 *pEngineValues = pGameConfig->FindMember(pszEngineName);

	bool bResult = pEngineValues != nullptr;

	if(bResult)
	{
		bResult = LoadEngine(pRoot, pEngineValues, vecMessages);
	}
	else
	{
		const char *pszMessageConcat[] = {"Failed to ", "find ", "\"", pszEngineName, "\" section"};

		vecMessages.AddToTail({pszMessageConcat});
	}

	return bResult;
}

void GameData::Config::ClearValues()
{
	m_aAddressStorage.ClearValues();
	m_aOffsetStorage.ClearValues();
}

GameData::Config::Addresses &GameData::Config::GetAddresses()
{
	return m_aAddressStorage;
}

GameData::Config::Offsets &GameData::Config::GetOffsets()
{
	return m_aOffsetStorage;
}

bool GameData::Config::LoadEngine(IGameData *pRoot, KeyValues3 *pEngineValues, CUtlVector<CBufferStringConcat> &vecMessages)
{
	struct
	{
		CKV3MemberName aMember;
		bool (GameData::Config::*pfnLoadOne)(IGameData *pRoot, KeyValues3 *pValues, CUtlVector<CBufferStringConcat> &vecMessages);
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

	CUtlVector<CBufferStringConcat> vecSubMessages;

	for(std::size_t n = 0, nSize = ARRAYSIZE(aSections); n < nSize; n++)
	{
		auto &aSection = aSections[n];

		auto &aSectionMember = aSection.aMember;

		KeyValues3 *pEngineMember = pEngineValues->FindMember(aSectionMember);

		if(pEngineMember && !(this->*(aSections[n].pfnLoadOne))(pRoot, pEngineMember, vecSubMessages))
		{
			const char *pszMessageConcat[] = {"Failed to ", "load \"", aSectionMember.GetString(), "\" section:"};

			vecMessages.AddToTail(pszMessageConcat);

			FOR_EACH_VEC(vecSubMessages, i)
			{
				const auto &it = vecSubMessages[i];

				const char *pszSubMessageConcat[] = {"\t", it.Get()};

				vecMessages.AddToTail(pszSubMessageConcat);
			}
		}
	}

	return true;
}

bool GameData::Config::LoadEngineSignatures(IGameData *pRoot, KeyValues3 *pSignaturesValues, CUtlVector<CBufferStringConcat> &vecMessages)
{
	int iMemberCount = pSignaturesValues->GetMemberCount();

	if(!iMemberCount)
	{
		static const char *s_pszMessageConcat[] = {"Section is empty"};

		vecMessages.AddToTail(s_pszMessageConcat);

		return false;
	}

	KV3MemberId_t i = 0;

	const char *pszLibraryKey = "library", 
	           *pszPlatformKey = GameData::GetCurrentPlatformName();

	do
	{
		KeyValues3 *pSigSection = pSignaturesValues->GetMember(i);

		const char *pszSigName = pSignaturesValues->GetMemberName(i);

		KeyValues3 *pLibraryValues = pSigSection->FindMember(pszLibraryKey);

		if(!pLibraryValues)
		{
			const char *pszMessageConcat[] = {"Failed to ", "get ", "\"", pszLibraryKey, "\" key ", "at \"", pszSigName, "\""};

			vecMessages.AddToTail(pszMessageConcat);

			continue;
		}

		const char *pszLibraryName = pLibraryValues->GetString("<none>");

		const auto *pLibModule = pRoot->FindLibrary(pszLibraryName);

		if(!pLibModule)
		{
			const char *pszMessageConcat[] = {"Unknown \"", pszLibraryName, "\" library ", "at \"", pszSigName, "\""};

			vecMessages.AddToTail(pszMessageConcat);

			continue;
		}

		KeyValues3 *pPlatformValues = pSigSection->FindMember(pszPlatformKey);

		if(!pPlatformValues)
		{
			const char *pszMessageConcat[] = {"Failed to ", "get ", "platform ", "(\"", pszPlatformKey, "\" key) ", "at \"", pszSigName, "\""};

			vecMessages.AddToTail(pszMessageConcat);

			continue;
		}

		const char *pszSignature = pPlatformValues->GetString();

		const auto pSigResult = pLibModule->FindPattern(pszSignature);

		if(!pSigResult)
		{
			const char *pszMessageConcat[] = {"Failed to ", "find ", "\"", pszSigName, "\""};

			vecMessages.AddToTail(pszMessageConcat);

			continue;
		}

		SetAddress(pszSigName, pSigResult);

		i++;
	}
	while(i < iMemberCount);

	return true;
}

bool GameData::Config::LoadEngineOffsets(IGameData *pRoot, KeyValues3 *pOffsetsValues, CUtlVector<CBufferStringConcat> &vecMessages)
{
	int iMemberCount = pOffsetsValues->GetMemberCount();

	if(!iMemberCount)
	{
		static const char *s_pszMessageConcat[] = {"Offsets section is empty"};

		vecMessages.AddToTail(s_pszMessageConcat);

		return false;
	}

	KV3MemberId_t i = 0;

	const char *pszPlatformKey = GameData::GetCurrentPlatformName();

	do
	{
		KeyValues3 *pOffsetSection = pOffsetsValues->GetMember(i);

		const char *pszOffsetName = pOffsetsValues->GetMemberName(i);

		KeyValues3 *pPlatformValues = pOffsetSection->FindMember(pszPlatformKey);

		if(!pPlatformValues)
		{
			const char *pszMessageConcat[] = {"Failed to ", "get ", " platform ", "(\"", pszPlatformKey, "\" key)", "at \"", pszOffsetName, "\""};

			vecMessages.AddToTail(pszMessageConcat);

			continue;
		}

		SetOffset(pszOffsetName, pPlatformValues->GetType() == KV3_TYPE_STRING ? GameData::ReadOffset(pPlatformValues->GetString()) : pPlatformValues->GetUInt64());

		i++;
	}
	while(i < iMemberCount);

	return true;
}

bool GameData::Config::LoadEngineAddresses(IGameData *pRoot, KeyValues3 *pAddressesValues, CUtlVector<CBufferStringConcat> &vecMessages)
{
	int iMemberCount = pAddressesValues->GetMemberCount();

	if(!iMemberCount)
	{
		const char *s_pszMessageConcat[] = {"Section is empty"};

		vecMessages.AddToTail(s_pszMessageConcat);

		return false;
	}

	KV3MemberId_t i = 0;

	const char *pszSignatureKey = "signature";

	CUtlVector<CBufferStringConcat> vecSubMessages;

	do
	{
		KeyValues3 *pAddrSection = pAddressesValues->GetMember(i);

		const char *pszAddressName = pAddressesValues->GetMemberName(i);

		KeyValues3 *pSignatureValues = pAddrSection->FindMember(pszSignatureKey);

		if(!pSignatureValues)
		{
			const char *pszMessageConcat[] = {"Failed to ", "get ", "\"", pszSignatureKey, "\" key ", "at \"", pszAddressName, "\""};

			vecMessages.AddToTail(pszMessageConcat);

			continue;
		}

		const char *pszSignatureName = pSignatureValues->GetString();

		const auto &pSigAddress = GetAddress(pszSignatureName);

		if(!pSigAddress)
		{
			const char *pszMessageConcat[] = {"Failed to ", "get ", "\"", pszSignatureKey, "\" signature ", "in \"", pszAddressName, "\""};

			vecMessages.AddToTail(pszMessageConcat);

			continue;
		}

		uintptr_t pAddrCur = pSigAddress.GetPtr();

		// Remove an extra keys.
		{
			pAddrSection->RemoveMember(pSignatureValues);

			int iCurrentPlat = GetCurrentPlatform();

			for(int iPlat = PLAT_FIRST; iPlat < PLAT_MAX; iPlat++)
			{
				if(iCurrentPlat != iPlat)
				{
					pAddrSection->RemoveMember(GetPlatformName((Platform)iPlat));
				}
			}
		}

		if(!LoadEngineAddressActions(pRoot, pAddrCur, pAddrSection, vecSubMessages))
		{
			const char *pszMessageConcat[] = {"Failed to ", "load ", "\"", pszSignatureKey, "\" address action:"};

			vecMessages.AddToTail(pszMessageConcat);

			FOR_EACH_VEC(vecSubMessages, i)
			{
				const auto &it = vecSubMessages[i];

				const char *pszSubMessageConcat[] = {"\t", it.Get()};

				vecMessages.AddToTail(pszSubMessageConcat);
			}

			continue;
		}

		SetAddress(pszAddressName, pAddrCur);

		i++;
	}
	while(i < iMemberCount);

	return true;
}

bool GameData::Config::LoadEngineAddressActions(IGameData *pRoot, uintptr_t &pAddrCur, KeyValues3 *pActionsValues, CUtlVector<CBufferStringConcat> &vecMessages)
{
	int iMemberCount = pActionsValues->GetMemberCount();

	if(!iMemberCount)
	{
		static const char *s_pszMessageConcat[] = {"Section is empty"};

		vecMessages.AddToTail(s_pszMessageConcat);

		return false;
	}

	KV3MemberId_t i = 0;

	do
	{
		KeyValues3 *pAction = pActionsValues->GetMember(i);

		const char *pszName = pActionsValues->GetMemberName(i);

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
				const char *pszMessageConcat[] = {"Unknown \"", pszName, "\" read key"};

				vecMessages.AddToTail(pszMessageConcat);

				continue;
			}
		}
		else if(!strcmp(pszName, GameData::GetCurrentPlatformName()))
		{
			return LoadEngineAddressActions(pRoot, pAddrCur, pAction, vecMessages); // Recursive by platform.
		}
		else
		{
			const char *pszMessageConcat[] = {"Unknown \"", pszName, "\" key"};

			vecMessages.AddToTail(pszMessageConcat);

			continue;
		}

		i++;
	}
	while(i < iMemberCount);

	return true;
}

const DynLibUtils::CMemory &GameData::Config::GetAddress(const std::string &sName) const
{
	return m_aAddressStorage.Get(sName);
}

const ptrdiff_t &GameData::Config::GetOffset(const std::string &sName) const
{
	return m_aOffsetStorage.Get(sName);
}

void GameData::Config::SetAddress(const std::string &sName, DynLibUtils::CMemory aMemory)
{
	m_aAddressStorage.Set(sName, aMemory);
}

void GameData::Config::SetOffset(const std::string &sName, ptrdiff_t nValue)
{
	m_aOffsetStorage.Set(sName, nValue);
}
