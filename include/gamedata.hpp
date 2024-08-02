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
#define _INCLUDE_GAMEDATA_HPP_

#define MAX_GAMEDATA_SECTION_MESSAGE_LENGTH 256
#define MAX_GAMEDATA_ENGINE_ADDRESSES_SECTION_MESSAGE_LENGTH MAX_GAMEDATA_SECTION_MESSAGE_LENGTH
#define MAX_GAMEDATA_ENGINE_SECTION_MESSAGE_LENGTH (MAX_GAMEDATA_SECTION_MESSAGE_LENGTH + MAX_GAMEDATA_ENGINE_ADDRESSES_SECTION_MESSAGE_LENGTH)
#define MAX_GAMEDATA_MESSAGE_LENGTH (MAX_GAMEDATA_SECTION_MESSAGE_LENGTH + MAX_GAMEDATA_ENGINE_SECTION_MESSAGE_LENGTH + MAX_GAMEDATA_ENGINE_ADDRESSES_SECTION_MESSAGE_LENGTH)

#include <dynlibutils/module.hpp>
#include <dynlibutils/memaddr.hpp>

#include <functional>

#include <tier0/platform.h>

#include <tier0/bufferstring.h>
#include <tier0/dbg.h>
#include <tier1/utlmap.h>
#include <tier1/utlrbtree.h>
#include <tier1/utlsymbollarge.h>
#include <tier1/utlvector.h>

#define INVALID_GAMEDATA_INDEX(map) \
	decltype(map)::InvalidIndex()

#define IS_VALID_GAMEDATA_INDEX(i, map) \
	(i != INVALID_GAMEDATA_INDEX(map))

class KeyValues3;

class IGameData
{
public:
	virtual const DynLibUtils::CModule *FindLibrary(CUtlSymbolLarge sName) const = 0;
}; // IGameData

namespace GameData
{
	using CBufferStringSection = CBufferStringGrowable<MAX_GAMEDATA_SECTION_MESSAGE_LENGTH>;

	class CBufferStringConcat : public CBufferStringSection
	{
	public:
		using Base = CBufferStringSection;
		using Base::Base;

		template<uintp N>
		CBufferStringConcat(const char *(&pszSplit)[N])
		{
			AppendConcat(N, pszSplit, NULL);
		}

		template<uintp N>
		CBufferStringConcat(const char *pszStartWtih, const char *(&pszSplit)[N])
		{
			Insert(0, pszStartWtih);
			AppendConcat(N, pszSplit, NULL);
		}
	}; // GameData::CBufferStringConcat

	using CBufferStringVector = CUtlVector<CBufferStringConcat>;

	static const char *GetSourceEngineName();

	enum Platform : int
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
	}; // GameData::Platform

	inline static Platform GetCurrentPlatform();
	inline static const char *GetCurrentPlatformName();
	inline static const char *GetPlatformName(Platform eElm);

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

			Storage(IListener *pFirstListener)
			{
				m_vecListeners.AddToTail(pFirstListener);
			}

		public:
			using OnCollectorChangedCallback = std::function<void (const K &, const V &)>;

			template<typename T>
			class BaseListenerCollector : public IListener
			{
			private:
				using Base = IListener; // Root interface.

			public:
				BaseListenerCollector()
				 :  m_mapCallbacks(CDefOps<CUtlSymbolLarge>::LessFunc)
				{
				}

			public:
				virtual void Insert(const K &aKey, const OnCollectorChangedCallback &funcCallback) = 0;
				virtual bool Remove(const K &aKey) = 0;

				virtual void RemoveAll()
				{
					m_mapCallbacks.Purge();
				}

			protected:
				CUtlMap<K, T> m_mapCallbacks;
			}; // GameData::Config::Storage::BaseListenerCollector

			class ListenerCallbacksCollector : public BaseListenerCollector<OnCollectorChangedCallback>
			{
			private:
				using Base = BaseListenerCollector<OnCollectorChangedCallback>;
				using Base::m_mapCallbacks;

			public:
				ListenerCallbacksCollector() = default;

			public: // BaseListenerCollector<>
				void Insert(const K &aKey, const OnCollectorChangedCallback &funcCallback) override
				{
					auto &map = m_mapCallbacks;

					auto iFoundIndex = map.Find(aKey);

					if(IS_VALID_GAMEDATA_INDEX(iFoundIndex, m_mapCallbacks))
					{
						auto &it = map.Element(iFoundIndex);

						it = funcCallback;
					}
					else
					{
						map.Insert(aKey, funcCallback);
					}
				}

				bool Remove(const K &aKey) override
				{
					auto &map = m_mapCallbacks;

					auto iFoundIndex = map.Find(aKey);

					bool bResult = IS_VALID_GAMEDATA_INDEX(iFoundIndex, m_mapCallbacks);

					if(bResult)
					{
						map.RemoveAt(iFoundIndex);
					}

					return bResult;
				}

			public: // IListener
				void OnChanged(const K &aKey, const V &aValue) override
				{
					auto &map = m_mapCallbacks;

					auto iFoundIndex = map.Find(aKey);

					Assert(IS_VALID_GAMEDATA_INDEX(iFoundIndex, m_mapCallbacks));

					auto &it = map.Element(iFoundIndex);

					it(aKey, aValue);
				}
			}; // GameData::Config::Storage::ListenerCallbacksCollector

			using ListenerMultipleCallbacksCollectorVector = CUtlVector<OnCollectorChangedCallback>;

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

					auto iFoundIndex = map.Find(aKey);

					if(IS_VALID_GAMEDATA_INDEX(iFoundIndex, m_mapCallbacks))
					{
						auto &it = map.Element(iFoundIndex);

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
				void Insert(K aKey, OnCollectorChangedCallback funcCallback) override
				{
					auto &map = m_mapCallbacks;

					auto iFoundIndex = map.Find(aKey);

					if(IS_VALID_GAMEDATA_INDEX(iFoundIndex, m_mapCallbacks))
					{
						auto &it = map.Element(iFoundIndex);

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

					auto iFoundIndex = map.Find(aKey);

					bool bResult = IS_VALID_GAMEDATA_INDEX(iFoundIndex, m_mapCallbacks);

					if(bResult)
					{
						map.RemoveAt(iFoundIndex);
					}

					return bResult;
				}

			public: // IListener
				void OnChanged(const K &aKey, const V &aValue) override
				{
					auto &map = m_mapCallbacks;

					auto iFoundIndex = map.Find(aKey);

					Assert(IS_VALID_GAMEDATA_INDEX(iFoundIndex, m_mapCallbacks));

					auto &itVec = map.Element(iFoundIndex);

					FOR_EACH_VEC(itVec, i)
					{
						auto &it = itVec[i];

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

				auto iFoundIndex = map.Find(aKey);

				return map.Element(iFoundIndex);
			}

			const V &Get(const K &aKey) const
			{
				auto &map = m_mapValues;

				auto iFoundIndex = map.Find(aKey);

				Assert(IS_VALID_GAMEDATA_INDEX(iFoundIndex, m_mapValues));

				return map.Element(iFoundIndex);
			}

			void TriggerCallbacks()
			{
				for(auto const &[aKey, aVal] : m_mapValues)
				{
					OnChanged(aKey, aVal);
				}
			}

		protected:
			void Set(const K &aKey, const V &aValue)
			{
				auto &map = m_mapValues;

				auto iFoundIndex = map.Find(aKey);

				Assert(IS_VALID_GAMEDATA_INDEX(iFoundIndex, m_mapValues));

				auto &it = map.Element(iFoundIndex);

				it = aValue;
				OnChanged(aKey, it);
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

				const auto iInvalidIndex = INVALID_GAMEDATA_INDEX(m_vecListeners);

				auto iFoundIndex = iInvalidIndex;

				FOR_EACH_VEC(vec, i)
				{
					auto &it = vec[i];

					if(it == pListener)
					{
						iFoundIndex = i;

						break;
					}
				}

				bool bResult = iFoundIndex != iInvalidIndex;

				if(bResult)
				{
					vec.FastRemove(iFoundIndex);
				}

				return bResult;
			}

		private:
			CUtlMap<K, V> m_mapValues;
			CUtlVector<IListener *> m_vecListeners;
		}; // GameData::Config::Storage

	public:
		using Addresses = Storage<CUtlSymbolLarge, DynLibUtils::CMemory>;
		using Offsets = Storage<CUtlSymbolLarge, ptrdiff_t>;

	public:
		Config(const Addresses &aInitAddressStorage, const Offsets &aInitOffsetsStorage);

	public:
		bool Load(IGameData *pRoot, KeyValues3 *pGameConfig, CBufferStringVector &vecMessages);
		void ClearValues();

	public:
		Addresses &GetAddresses();
		Offsets &GetOffsets();

	protected:
		bool LoadEngine(IGameData *pRoot, KeyValues3 *pEngineValues, CBufferStringVector &vecMessages);

		bool LoadEngineSignatures(IGameData *pRoot, KeyValues3 *pSignaturesValues, CBufferStringVector &vecMessages);
		bool LoadEngineOffsets(IGameData *pRoot, KeyValues3 *pOffsetsValues, CBufferStringVector &vecMessages);

		// Step #2 - addresses.
		bool LoadEngineAddresses(IGameData *pRoot, KeyValues3 *pAddressesValues, CBufferStringVector &vecMessages);
		bool LoadEngineAddressActions(IGameData *pRoot, uintptr_t &pAddrCur, KeyValues3 *pActionValues,  CBufferStringVector &vecMessages);

	public:
		const DynLibUtils::CMemory &GetAddress(CUtlSymbolLarge sName) const;
		const ptrdiff_t &GetOffset(CUtlSymbolLarge sName) const;

	protected:
		void SetAddress(CUtlSymbolLarge sName, DynLibUtils::CMemory aMemory);
		void SetOffset(CUtlSymbolLarge sName, ptrdiff_t nValue);

	private:
		Addresses m_aAddressStorage;
		Offsets m_aOffsetStorage;
	}; // GameData::Config
}; // GameData

#endif //_INCLUDE_GAMEDATA_HPP_
