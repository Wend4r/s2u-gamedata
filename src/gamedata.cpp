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

using Platform_t = GameData::Platform_t;
using Game_t = GameData::Game_t;

static CKV3MemberName s_aGameMemberNames[Game_t::GAME_MAX] =
{
	CKV3MemberName("csgo"), // Game_t::GAME_CSGO
	CKV3MemberName("dota"), // Game_t::GAME_DOTA
};

static CKV3MemberName s_aPlatformMemberNames[Platform_t::PLAT_MAX] =
{
	CKV3MemberName("windows"), // Platform_t::PLAT_WINDOWS
	CKV3MemberName("win64"), // Platform_t::PLAT_WINDOWS64

	CKV3MemberName("linux"), // Platform_t::PLAT_LINUX
	CKV3MemberName("linuxsteamrt64"), // Platform_t::PLAT_LINUX64

	CKV3MemberName("mac"), // Platform_t::PLAT_MAC
	CKV3MemberName("osx64"), // Platform_t::PLAT_MAC64
};

static CKV3MemberName s_aLibraryMemberName = CKV3MemberName("library"), 
                      s_aSignatureMemberName = CKV3MemberName("signature");

DLL_IMPORT IVEngineServer *engine;
DLL_IMPORT IServerGameDLL *server;

const CKV3MemberName &GameData::GetSourceEngineMemberName()
{
#if SOURCE_ENGINE == SE_CS2
	return s_aGameMemberNames[GAME_CSGO];
#elif SOURCE_ENGINE == SE_DOTA
	return s_aGameMemberNames[GAME_DOTA];
#else
#	error "Unknown engine type"
	return "unknown";
#endif
}

GameData::Platform_t GameData::GetCurrentPlatform()
{
#if defined(PLATFORM_WINDOWS)
#	if defined(X64BITS)
	return Platform_t::PLAT_WINDOWS64;
#	else
	return Platform_t::PLAT_WINDOWS;
#	endif
#elif defined(PLATFORM_LINUX)
#	if defined(X64BITS)
	return Platform_t::PLAT_LINUX64;
#	else
	return Platform_t::PLAT_LINUX;
#	endif
#elif defined(PLATFORM_OSX)
#	if defined(X64BITS)
	return Platform_t::PLAT_MAC64;
#	else
	return Platform_t::PLAT_MAC;
#	endif
#else
#	error Unsupported platform
	return Platform_t::PLAT_UNKNOWN;
#endif
}

const CKV3MemberName &GameData::GetPlatformMemberName(Platform_t eElm)
{
	return s_aPlatformMemberNames[eElm];
}

const CKV3MemberName &GameData::GetCurrentPlatformMemberName()
{
	return GetPlatformMemberName(GetCurrentPlatform());
}

ptrdiff_t GameData::ReadOffset(const char *pszValue)
{
	return static_cast<ptrdiff_t>(strtol(pszValue, NULL, 0));
}

GameData::Config::Config(CUtlSymbolTableLarge_CI &&aSymbolTable, Addresses_t &&aAddressStorage, Keys_t &&aKeysStorage, Offsets_t &&aOffsetsStorage)
 :  m_aSymbolTable(Move(aSymbolTable)),
    m_aAddressStorage(Move(aAddressStorage)), 
    m_aKeysStorage(Move(aKeysStorage)), 
    m_aOffsetStorage(Move(aOffsetsStorage))
{
}

bool GameData::Config::Load(IGameData *pRoot, KeyValues3 *pGameConfig, CBufferStringVector &vecMessages)
{
	auto aEngineMemberName = GameData::GetSourceEngineMemberName();

	const char *pszEngineKey = aEngineMemberName.GetString();

	KeyValues3 *pEngineValues = pGameConfig->FindMember(aEngineMemberName);

	if(!pEngineValues)
	{
		const char *pszMessageConcat[] = {"Failed to ", "find ", "\"", pszEngineKey, "\" section"};

		vecMessages.AddToTail(pszMessageConcat);

		return false;
	}

	return LoadEngine(pRoot, pEngineValues, vecMessages);
}

void GameData::Config::ClearValues()
{
	m_aAddressStorage.ClearValues();
	m_aKeysStorage.ClearValues();
	m_aOffsetStorage.ClearValues();
}

bool GameData::Config::LoadEngine(IGameData *pRoot, KeyValues3 *pEngineValues, CBufferStringVector &vecMessages)
{
	struct
	{
		CKV3MemberName aMember;
		bool (GameData::Config::*pfnLoadOne)(IGameData *pRoot, KeyValues3 *pValues, CBufferStringVector &vecMessages);
	} aSections[] =
	{
		{
			"Signatures",
			&GameData::Config::LoadEngineSignatures
		},
		{
			"VTables",
			&GameData::Config::LoadEngineVTables
		},
		{
			"Keys",
			&GameData::Config::LoadEngineKeys
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

	CBufferStringVector vecSubMessages;

	for(uintp n = 0, nSize = ARRAYSIZE(aSections); n < nSize; n++)
	{
		auto &aSection = aSections[n];

		auto &aSectionMember = aSection.aMember;

		KeyValues3 *pEngineMember = pEngineValues->FindMember(aSectionMember);

		if(pEngineMember && !(this->*(aSections[n].pfnLoadOne))(pRoot, pEngineMember, vecSubMessages))
		{
			const char *pszMessageConcat[] = {"Failed to ", "load \"", aSectionMember.GetString(), "\" section:"};

			vecMessages.AddToTail(pszMessageConcat);

			for(const auto &it : vecSubMessages)
			{
				const char *pszSubMessageConcat[] = {"\t", it.Get()};

				vecMessages.AddToTail(pszSubMessageConcat);
			}
		}
	}

	return true;
}

bool GameData::Config::LoadEngineSignatures(IGameData *pRoot, KeyValues3 *pSignaturesValues, CBufferStringVector &vecMessages)
{
	int nMemberCount = pSignaturesValues->GetMemberCount();

	if(!nMemberCount)
	{
		vecMessages.AddToTail("Section is empty");

		return false;
	}

	KV3MemberId_t i = 0;

	const auto &aLibraryMemberName = s_aLibraryMemberName;

	const char *pszLibraryKey = aLibraryMemberName.GetString();

	const auto &aPlatformMemberName = GameData::GetCurrentPlatformMemberName();

	const char *pszPlatformKey = aPlatformMemberName.GetString();

	do
	{
		KeyValues3 *pSigSection = pSignaturesValues->GetMember(i);

		const char *pszSigName = pSignaturesValues->GetMemberName(i);

		KeyValues3 *pLibraryValues = pSigSection->FindMember(aLibraryMemberName);

		if(!pLibraryValues)
		{
			const char *pszMessageConcat[] = {"Failed to ", "get ", "\"", pszLibraryKey, "\" key ", "at \"", pszSigName, "\""};

			vecMessages.AddToTail(pszMessageConcat);
			i++;

			continue;
		}

		const char *pszLibraryName = pLibraryValues->GetString("<none>");

		const auto *pLibModule = pRoot->FindLibrary(pszLibraryName);

		if(!pLibModule)
		{
			const char *pszMessageConcat[] = {"Unknown \"", pszLibraryName, "\" library ", "at \"", pszSigName, "\""};

			vecMessages.AddToTail(pszMessageConcat);
			i++;

			continue;
		}

		KeyValues3 *pPlatformValues = pSigSection->FindMember(aPlatformMemberName);

		if(!pPlatformValues)
		{
			const char *pszMessageConcat[] = {"Failed to ", "get ", "platform ", "(\"", pszPlatformKey, "\" key) ", "at \"", pszSigName, "\""};

			vecMessages.AddToTail(pszMessageConcat);
			i++;

			continue;
		}

		const char *pszSignature = pPlatformValues->GetString();

		const auto pSigResult = pLibModule->FindPattern(DynLibUtils::ParsePattern(pszSignature)); // Hot!

		if(!pSigResult.IsValid())
		{
			const char *pszMessageConcat[] = {"Failed to ", "find ", "\"", pszSigName, "\" by", "\"", pszSignature, "\" signature" };

			vecMessages.AddToTail(pszMessageConcat);
			i++;

			continue;
		}

		SetAddress(GetSymbol(pszSigName), pSigResult);

		i++;
	}
	while(i < nMemberCount);

	return true;
}

bool GameData::Config::LoadEngineVTables(IGameData *pRoot, KeyValues3 *pVTableValues, CBufferStringVector &vecMessages)
{
	int nMemberCount = pVTableValues->GetMemberCount();

	if(!nMemberCount)
	{
		vecMessages.AddToTail("Section is empty");

		return false;
	}

	KV3MemberId_t i = 0;

	const auto &aLibraryMemberName = s_aLibraryMemberName;

	const char *pszLibraryKey = aLibraryMemberName.GetString();

	do
	{
		KeyValues3 *pData = pVTableValues->GetMember(i);

		const char *pszVTableKey = pVTableValues->GetMemberName(i);

		KeyValues3 *pLibraryValues = pData->FindMember(aLibraryMemberName);

		if(!pLibraryValues)
		{
			const char *pszMessageConcat[] = {"Failed to ", "get ", "\"", pszLibraryKey, "\" key ", "at \"", pszVTableKey, "\""};

			vecMessages.AddToTail(pszMessageConcat);
			i++;

			continue;
		}

		const char *pszLibraryName = pLibraryValues->GetString("<none>");

		const auto *pLibModule = pRoot->FindLibrary(pszLibraryName);

		if(!pLibModule)
		{
			const char *pszMessageConcat[] = {"Unknown \"", pszLibraryName, "\" library ", "at \"", pszVTableKey, "\""};

			vecMessages.AddToTail(pszMessageConcat);
			i++;

			continue;
		}

		KeyValues3 *pVTableNameKV = pData->FindMember("name");

		const char *pszName = (pVTableNameKV && pVTableNameKV->IsString()) ? pVTableNameKV->GetString() : pszVTableKey;

		const auto pResult = pLibModule->GetVirtualTableByName(pszName);

		if(!pResult.IsValid())
		{
			const char *pszMessageConcat[] = {"Failed to ", "find ", "\"", pszVTableKey, "\" by", "\"", pszName, "\" vtable" };

			vecMessages.AddToTail(pszMessageConcat);
			i++;

			continue;
		}

		SetAddress(GetSymbol(pszVTableKey), pResult);

		i++;
	}
	while(i < nMemberCount);

	return true;
}

bool GameData::Config::LoadEngineKeys(IGameData *pRoot, KeyValues3 *pKeysValues, CBufferStringVector &vecMessages)
{
	int nMemberCount = pKeysValues->GetMemberCount();

	if(!nMemberCount)
	{
		static const char *s_pszMessageConcat[] = {"Keys section is empty"};

		vecMessages.AddToTail(s_pszMessageConcat);

		return false;
	}

	KV3MemberId_t i = 0;

	const auto &aPlatformMemberName = GameData::GetCurrentPlatformMemberName();

	const char *pszPlatformKey = aPlatformMemberName.GetString();

	do
	{
		KeyValues3 *pKeySection = pKeysValues->GetMember(i);

		const char *pszKeyName = pKeysValues->GetMemberName(i);

		KeyValues3 *pPlatformKV = pKeySection->FindMember(aPlatformMemberName);

		if(!pPlatformKV)
		{
			const char *pszMessageConcat[] = {"Failed to ", "get ", " platform ", "(\"", pszPlatformKey, "\" key)", "at \"", pszKeyName, "\""};

			vecMessages.AddToTail(pszMessageConcat);
			i++;

			continue;
		}

		SetKey(GetSymbol(pszKeyName), Move(*pPlatformKV));

		i++;
	}
	while(i < nMemberCount);

	return true;
}

bool GameData::Config::LoadEngineOffsets(IGameData *pRoot, KeyValues3 *pOffsetsValues, CBufferStringVector &vecMessages)
{
	int nMemberCount = pOffsetsValues->GetMemberCount();

	if(!nMemberCount)
	{
		static const char *s_pszMessageConcat[] = {"Offsets section is empty"};

		vecMessages.AddToTail(s_pszMessageConcat);

		return false;
	}

	KV3MemberId_t i = 0;

	const auto &aPlatformMemberName = GameData::GetCurrentPlatformMemberName();

	const char *pszPlatformKey = aPlatformMemberName.GetString();

	do
	{
		KeyValues3 *pOffsetSection = pOffsetsValues->GetMember(i);

		const char *pszOffsetName = pOffsetsValues->GetMemberName(i);

		KeyValues3 *pPlatformValues = pOffsetSection->FindMember(aPlatformMemberName);

		if(!pPlatformValues)
		{
			const char *pszMessageConcat[] = {"Failed to ", "get ", " platform ", "(\"", pszPlatformKey, "\" key)", "at \"", pszOffsetName, "\""};

			vecMessages.AddToTail(pszMessageConcat);
			i++;

			continue;
		}

		SetOffset(GetSymbol(pszOffsetName), pPlatformValues->IsString() ? GameData::ReadOffset(pPlatformValues->GetString()) : pPlatformValues->GetUInt64());

		i++;
	}
	while(i < nMemberCount);

	return true;
}

bool GameData::Config::LoadEngineAddresses(IGameData *pRoot, KeyValues3 *pAddressesValues, CBufferStringVector &vecMessages)
{
	int nMemberCount = pAddressesValues->GetMemberCount();

	if(!nMemberCount)
	{
		const char *s_pszMessageConcat[] = {"Section is empty"};

		vecMessages.AddToTail(s_pszMessageConcat);

		return false;
	}

	KV3MemberId_t i = 0;

	CBufferStringVector vecSubMessages;

	do
	{
		KeyValues3 *pAddrSection = pAddressesValues->GetMember(i);

		const char *pszAddressName = pAddressesValues->GetMemberName(i);

		uintptr_t pAddrCur {};

		if(!LoadEngineAddressActions(pRoot, pszAddressName, pAddrCur, pAddrSection, vecSubMessages))
		{
			const char *pszMessageConcat[] = {"Failed to ", "load ", "\"", pszAddressName, "\" address action:"};

			vecMessages.AddToTail(pszMessageConcat);

			for(const auto &it : vecSubMessages)
			{
				const char *pszSubMessageConcat[] = {"\t", it.Get()};

				vecMessages.AddToTail(pszSubMessageConcat);
			}

			i++;

			continue;
		}

		SetAddress(GetSymbol(pszAddressName), pAddrCur);

		i++;
	}
	while(i < nMemberCount);

	return true;
}

bool GameData::Config::LoadEngineAddressActions(IGameData *pRoot, const char *pszAddressName, uintptr_t &pAddrCur, KeyValues3 *pActionsValues, CBufferStringVector &vecMessages)
{
	int nMemberCount = pActionsValues->GetMemberCount();

	if(!nMemberCount)
	{
		static const char *s_pszMessageConcat[] = {"Section is empty"};

		vecMessages.AddToTail(s_pszMessageConcat);

		return false;
	}

	const auto &aSignatureMemberName = s_aSignatureMemberName;

	const char *pszSignatureKey = aSignatureMemberName.GetString();

	KeyValues3 *pSignatureValues = pActionsValues->FindMember(aSignatureMemberName);

	if(pSignatureValues)
	{
		const char *pszSignatureName = pSignatureValues->GetString();

		const auto &pSigAddress = GetAddress(GetSymbol(pszSignatureName));

		if(!pSigAddress)
		{
			const char *pszMessageConcat[] = {"Failed to ", "get ", "\"", pszSignatureKey, "\" signature ", "in \"", pszAddressName, "\""};

			vecMessages.AddToTail(pszMessageConcat);

			return false;
		}

		pAddrCur = pSigAddress.GetAddr();

		pActionsValues->RemoveMember(pSignatureValues);
		nMemberCount--;
	}

	// Remove an extra keys.
	{
		int iCurrentPlat = GetCurrentPlatform();

		for(int iPlat = PLAT_FIRST; iPlat < PLAT_MAX; iPlat++)
		{
			if(iCurrentPlat != iPlat)
			{
				if(pActionsValues->RemoveMember(GetPlatformMemberName((Platform_t)iPlat)))
				{
					nMemberCount--;
				}
			}
		}
	}

	if(nMemberCount < 1)
	{
		return true;
	}

	const auto &aPlatformMemberName = GameData::GetCurrentPlatformMemberName();

	const char *pszPlatformKey = aPlatformMemberName.GetString();

	KV3MemberId_t i = 0;

	do
	{
		const char *pszName = pActionsValues->GetMemberName(i);

		KeyValues3 *pAction = pActionsValues->GetMember(i);

		if(!strcmp(pszPlatformKey, pszName))
		{
			return LoadEngineAddressActions(pRoot, pszAddressName, pAddrCur, pAction, vecMessages); // Recursive by platform.
		}
		else
		{
			ptrdiff_t nActionValue = static_cast<ptrdiff_t>(pAction->GetUInt64());

			if(!strcmp(pszName, "offset"))
			{
				pAddrCur += nActionValue;
			}
			else if(!strncmp(pszName, "read", 4))
			{
				if(!pszName[4])
				{
					pAddrCur = *reinterpret_cast<uintptr_t *>(pAddrCur + nActionValue);
				}
				else if(!strcmp(&pszName[4], "_offs32"))
				{
					pAddrCur = pAddrCur + nActionValue + sizeof(int32_t) + *reinterpret_cast<int32_t *>(pAddrCur + nActionValue);
				}
				else
				{
					const char *pszMessageConcat[] = {"Unknown \"", pszName, "\" read key"};

					vecMessages.AddToTail(pszMessageConcat);
					i++;

					continue;
				}
			}
			else
			{
				const char *pszMessageConcat[] = {"Unknown \"", pszName, "\" key"};

				vecMessages.AddToTail(pszMessageConcat);
				i++;

				continue;
			}
		}

		i++;
	}
	while(i < nMemberCount);

	return true;
}
