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

#ifndef _INCLUDE_GAMEDATA_HPP_
#	define _INCLUDE_GAMEDATA_HPP_

#	include <dynlibutils/module.hpp>
#	include <dynlibutils/memaddr.hpp>

#	include <stddef.h>

#	include <functional>
#	include <memory>

#	include <tier0/platform.h>

#	include <tier0/bufferstring.h>
#	include <tier0/dbg.h>
#	include <tier1/utlsymbollarge.h>
#	include <tier1/utlmap.h>
#	include <tier1/utlrbtree.h>
#	include <tier1/utlvector.h>
#	include <tier1/keyvalues3.h>

#	define MAX_GAMEDATA_SECTION_MESSAGE_LENGTH 256
#	define MAX_GAMEDATA_ENGINE_ADDRESSES_SECTION_MESSAGE_LENGTH MAX_GAMEDATA_SECTION_MESSAGE_LENGTH
#	define MAX_GAMEDATA_ENGINE_SECTION_MESSAGE_LENGTH (MAX_GAMEDATA_SECTION_MESSAGE_LENGTH + MAX_GAMEDATA_ENGINE_ADDRESSES_SECTION_MESSAGE_LENGTH)
#	define MAX_GAMEDATA_MESSAGE_LENGTH (MAX_GAMEDATA_SECTION_MESSAGE_LENGTH + MAX_GAMEDATA_ENGINE_SECTION_MESSAGE_LENGTH + MAX_GAMEDATA_ENGINE_ADDRESSES_SECTION_MESSAGE_LENGTH)

#	define INVALID_GAMEDATA_INDEX(map) \
	decltype(map)::InvalidIndex()

#	define IS_VALID_GAMEDATA_INDEX(map, i) \
	(map.IsValidIndex(i))

#	define GAMEDATA_ADDRESS_LAMBDA_CAPTURE(fieldAddressVar) \
	[&_address = fieldAddressVar](const CUtlSymbolLarge &aKey, const DynLibUtils::CMemory &aAddress) \
	{ \
		_address = aAddress.RCast<decltype(fieldAddressVar)>(); \
	}
#	define GAMEDATA_KEY_LAMBDA_CAPTURE(fieldKeyVar) \
	[&_key = fieldKeyVar](const CUtlSymbolLarge &aKey, const CUtlString &sKey) \
	{ \
		_key = sKey; \
	}
#	define GAMEDATA_OFFSET_LAMBDA_CAPTURE(fieldOffsetVar) \
	[&_offset = fieldOffsetVar](const CUtlSymbolLarge &aKey, const ptrdiff_t &nOffset) \
	{ \
		_offset = nOffset; \
	}

#	define GAMEDATA_ADDRESS_SHARED_LAMBDA_CAPTURE(fieldAddressVar) {GAMEDATA_ADDRESS_LAMBDA_CAPTURE(fieldAddressVar)}
#	define GAMEDATA_KEY_SHARED_LAMBDA_CAPTURE(fieldKeyVar) {GAMEDATA_KEY_LAMBDA_CAPTURE(fieldKeyVar)}
#	define GAMEDATA_OFFSET_SHARED_LAMBDA_CAPTURE(fieldOffsetVar) {GAMEDATA_OFFSET_LAMBDA_CAPTURE(fieldOffsetVar)}

class KeyValues3;

class IGameData
{
public:
	virtual const DynLibUtils::CModule *FindLibrary(const char *pszName) const = 0;
}; // IGameData

namespace GameData
{
	using CBufferStringVector = CUtlVector<CBufferString>;

	static const CKV3MemberName &GetSourceEngineMemberName();

	enum Platform_t : int
	{
		PLAT_UNKNOWN = -1,
		PLAT_FIRST = 0,

		PLAT_WINDOWS = 0,
		PLAT_WINDOWS64,

		PLAT_LINUX,
		PLAT_LINUX64,

		PLAT_MAC,
		PLAT_MAC64,

		PLAT_MAX
	}; // GameData::Platform_t

	enum Game_t : int
	{
		GAME_UNKNOWN = -1,
		GAME_FIRST = 0,

		GAME_CSGO = 0,
		GAME_DOTA,

		GAME_MAX
	}; // GameData::Game_t

	inline static Platform_t GetCurrentPlatform();
	inline static const CKV3MemberName &GetCurrentPlatformMemberName();
	inline static const CKV3MemberName &GetPlatformMemberName(Platform_t eElm);

	static ptrdiff_t ReadOffset(const char *pszValue);

	class Config
	{
	public:
		template<typename K, typename V>
		class Storage
		{
			friend class Config;

		public:
			class IListener
			{
			public:
				virtual void OnChanged(const K &aKey, const V &aValue) = 0;
			}; // GameData::Config::Storage::IListener

			Storage()
			 :  m_mapValues(DefLessFunc(const K))
			{
			}

			explicit Storage(IListener *pFirstListener)
			{
				m_vecListeners.AddToTail(pFirstListener);
			}

		public:
			using OnCollectorChangedCallback_t = std::function<void (const K &, const V &)>;
			using OnCollectorChangedCallbackShared_t = std::shared_ptr<OnCollectorChangedCallback_t>;

			class CollectorChangedSharedCallback
			{
			public:
				CollectorChangedSharedCallback()
				 :  m_pCallback(std::make_shared<OnCollectorChangedCallback_t>(nullptr))
				{
				}

				CollectorChangedSharedCallback(const OnCollectorChangedCallbackShared_t &funcSharedCallback)
				 :  m_pCallback(funcSharedCallback)
				{
				}

				CollectorChangedSharedCallback(const OnCollectorChangedCallback_t &funcCallback)
				 :  m_pCallback(std::make_shared<OnCollectorChangedCallback_t>(funcCallback))
				{
				}

				operator OnCollectorChangedCallbackShared_t() const
				{
					return m_pCallback;
				}

				operator OnCollectorChangedCallback_t() const
				{
					return *m_pCallback;
				}

			private:
				OnCollectorChangedCallbackShared_t m_pCallback;
			}; // GameData::Config::Storage::CollectorChangedSharedCallback

			template<typename T>
			class BaseListenerCollector : public IListener
			{
			private:
				using Base = IListener; // Root interface.

			public:
				BaseListenerCollector()
				 :  m_mapCallbacks(DefLessFunc(const K))
				{
				}

			public:
				virtual void Insert(const K &aKey, const CollectorChangedSharedCallback &funcCallback) = 0;
				virtual bool Remove(const K &aKey) = 0;

				virtual void RemoveAll()
				{
					m_mapCallbacks.Purge();
				}

			protected:
				CUtlMap<K, T> m_mapCallbacks;
			}; // GameData::Config::Storage::BaseListenerCollector

			class ListenerCallbacksCollector : public BaseListenerCollector<CollectorChangedSharedCallback>
			{
			private:
				using Base = BaseListenerCollector<CollectorChangedSharedCallback>;
				using Base::m_mapCallbacks;

			public:
				ListenerCallbacksCollector() = default;

			public: // BaseListenerCollector<>
				void Insert(const K &aKey, const CollectorChangedSharedCallback &funcCallback) override
				{
					m_mapCallbacks.InsertOrReplace(aKey, funcCallback);
				}

				bool Remove(const K &aKey) override
				{
					auto &map = m_mapCallbacks;

					auto iFound = map.Find(aKey);

					bool bResult = IS_VALID_GAMEDATA_INDEX(map, iFound);

					if(bResult)
					{
						map.RemoveAt(iFound);
					}

					return bResult;
				}

			public: // IListener
				void OnChanged(const K &aKey, const V &aValue) override
				{
					auto &map = m_mapCallbacks;

					auto iFound = map.Find(aKey);

					if(IS_VALID_GAMEDATA_INDEX(map, iFound))
					{
						OnCollectorChangedCallback_t it = map.Element(iFound);

						it(aKey, aValue);
					}
				}
			}; // GameData::Config::Storage::ListenerCallbacksCollector

			using ListenerMultipleCallbacksCollectorVector = CUtlVector<CollectorChangedSharedCallback>;

			class ListenerMultipleCallbacksCollector : public BaseListenerCollector<ListenerMultipleCallbacksCollectorVector>
			{
			private:
				using BaseVector = ListenerMultipleCallbacksCollectorVector;
				using Base = BaseListenerCollector<BaseVector>;
				using Base::m_mapCallbacks;

			public:
				ListenerMultipleCallbacksCollector() = default;

				// Adapter.
				void Insert(const K &aKey, const BaseVector &vecCallbacks) override
				{
					auto &map = m_mapCallbacks;

					auto iFound = map.Find(aKey);

					if(IS_VALID_GAMEDATA_INDEX(map, iFound))
					{
						auto &it = map.Element(iFound);

						it.AddVectorToTail(vecCallbacks);
					}
					else
					{
						BaseVector vecNewOne;

						vecNewOne.AddVectorToTail(vecCallbacks);
						map.Insert(vecNewOne);
					}
				}

			public: // BaseListenerCollector<>
				void Insert(K aKey, const ListenerCallbacksCollector &funcCallback) override
				{
					auto &map = m_mapCallbacks;

					auto iFound = map.Find(aKey);

					if(IS_VALID_GAMEDATA_INDEX(map, iFound))
					{
						auto &it = map.Element(iFound);

						it.AddToTail(funcCallback);
					}
					else
					{
						BaseVector vecNewOne;

						vecNewOne.AddToTail(funcCallback);
						map.Insert(vecNewOne);
					}
				}

				bool Remove(K aKey)
				{
					auto &map = m_mapCallbacks;

					auto iFound = map.Find(aKey);

					bool bResult = IS_VALID_GAMEDATA_INDEX(map, iFound);

					if(bResult)
					{
						map.RemoveAt(iFound);
					}

					return bResult;
				}

			public: // IListener
				void OnChanged(const K &aKey, const V &aValue) override
				{
					auto &map = m_mapCallbacks;

					auto iFound = map.Find(aKey);

					Assert(IS_VALID_GAMEDATA_INDEX(map, iFound));

					auto &itVec = map.Element(iFound);

					FOR_EACH_VEC(itVec, i)
					{
						OnCollectorChangedCallback_t it = itVec[i];

						it(aKey, aValue);
					}
				}
			}; // GameData::Config::Storage::ListenerMultipleCallbacksCollector
		
		public:
			void ClearValues()
			{
				m_mapValues.Purge();
			}

			void ClearListeners()
			{
				m_vecListeners.Purge();
			}

		public:
			const V &operator[](const K &aKey) const
			{
				auto &map = m_mapValues;

				auto iFound = map.Find(aKey);

				return map.Element(iFound);
			}

			const V &Get(const K &aKey, const V &aDefaultValue = {}) const
			{
				auto &map = m_mapValues;

				auto iFound = map.Find(aKey);

				return IS_VALID_GAMEDATA_INDEX(m_mapValues, iFound) ? map.Element(iFound) : aDefaultValue;
			}

			void TriggerCallbacks()
			{
				for(auto const &[aKey, aVal] : m_mapValues)
				{
					OnChanged(aKey, aVal);
				}
			}

		public:
			void Set(const K &aKey, const V &aValue)
			{
				auto &map = m_mapValues;

				auto iFound = map.Find(aKey);

				if(IS_VALID_GAMEDATA_INDEX(m_mapValues, iFound))
				{
					auto &it = map.Element(iFound);

					it = aValue;
				}
				else
				{
					map.Insert(aKey, aValue);
				}

				OnChanged(aKey, aValue);
			}

		private:
			void OnChanged(const K &aKey, const V &aValue)
			{
				auto &vec = m_vecListeners;

				for(const auto it : vec)
				{
					it->OnChanged(aKey, aValue);
				}
			}

		public:
			virtual void AddListener(IListener *pListener)
			{
				m_vecListeners.AddToTail(pListener);
			}

			virtual bool RemoveListener(IListener *pListener)
			{
				auto &vec = m_vecListeners;

				const auto iInvalid = INVALID_GAMEDATA_INDEX(m_vecListeners);

				auto iFound = iInvalid;

				FOR_EACH_VEC(vec, i)
				{
					auto &it = vec[i];

					if(it == pListener)
					{
						iFound = i;

						break;
					}
				}

				bool bResult = iFound != iInvalid;

				if(bResult)
				{
					vec.FastRemove(iFound);
				}

				return bResult;
			}

		private:
			CUtlMap<K, V> m_mapValues;
			CUtlVector<IListener *> m_vecListeners;
		}; // GameData::Config::Storage

	public:
		using Addresses = Storage<CUtlSymbolLarge, DynLibUtils::CMemory>;
		using Keys = Storage<CUtlSymbolLarge, CUtlString>;
		using Offsets = Storage<CUtlSymbolLarge, ptrdiff_t>;

	public:
		Config() = default;
		explicit Config(const Addresses &aInitAddressStorage, const Keys &aInitKeysStorage, const Offsets &aInitOffsetsStorage);

	public:
		bool Load(IGameData *pRoot, KeyValues3 *pGameConfig, CBufferStringVector &vecMessages);
		void ClearValues();

	public:
		Addresses &GetAddresses()
		{
			return m_aAddressStorage;
		}

		Keys &GetKeys()
		{
			return m_aKeysStorage;
		}

		Offsets &GetOffsets()
		{
			return m_aOffsetStorage;
		}

	protected:
		bool LoadEngine(IGameData *pRoot, KeyValues3 *pEngineValues, CBufferStringVector &vecMessages);

		bool LoadEngineSignatures(IGameData *pRoot, KeyValues3 *pSignaturesValues, CBufferStringVector &vecMessages);
		bool LoadEngineKeys(IGameData *pRoot, KeyValues3 *pKeysValues, CBufferStringVector &vecMessages);
		bool LoadEngineOffsets(IGameData *pRoot, KeyValues3 *pOffsetsValues, CBufferStringVector &vecMessages);

		// Step #2 - addresses.
		bool LoadEngineAddresses(IGameData *pRoot, KeyValues3 *pAddressesValues, CBufferStringVector &vecMessages);
		bool LoadEngineAddressActions(IGameData *pRoot, const char *pszAddressSection, uintptr_t &pAddrCur, KeyValues3 *pActionValues,  CBufferStringVector &vecMessages);

	public:
		CUtlSymbolLarge GetSymbol(const char *pszText)
		{
			return m_aSymbolTable.AddString(pszText);
		}

		CUtlSymbolLarge FindSymbol(const char *pszText) const
		{
			return m_aSymbolTable.Find(pszText);
		}

	public:
		const DynLibUtils::CMemory &GetAddress(const CUtlSymbolLarge &sName) const
		{
			return m_aAddressStorage.Get(sName);
		}

		const CUtlString &GetKey(const CUtlSymbolLarge &sName) const
		{
			return m_aKeysStorage.Get(sName);
		}

		const ptrdiff_t &GetOffset(const CUtlSymbolLarge &sName) const
		{
			return m_aOffsetStorage.Get(sName);
		}

	protected:
		void SetAddress(const CUtlSymbolLarge &sName, const DynLibUtils::CMemory &aMemory)
		{
			m_aAddressStorage.Set(sName, aMemory);
		}

		void SetKey(const CUtlSymbolLarge &sName, const CUtlString &sValue)
		{
			m_aKeysStorage.Set(sName, sValue);
		}

		void SetOffset(const CUtlSymbolLarge &sName, const ptrdiff_t &nValue)
		{
			m_aOffsetStorage.Set(sName, nValue);
		}

	private:
		CUtlSymbolTableLarge_CI m_aSymbolTable;

		Addresses m_aAddressStorage;
		Keys m_aKeysStorage;
		Offsets m_aOffsetStorage;
	}; // GameData::Config
}; // GameData

#endif //_INCLUDE_GAMEDATA_HPP_
