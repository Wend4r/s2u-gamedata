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

#include <cstddef>
#include <functional>
#include <map>
#include <utility>
#include <string>
#include <vector>

#include <tier0/bufferstring.h>
#include <tier1/utlvector.h>

class CBufferString;
class KeyValues3;

class IGameData
{
public:
	virtual const DynLibUtils::CModule *FindLibrary(std::string sName) const = 0;
}; // IGameData

class GameData : public IGameData
{
public:
	using This = GameData;

	using CBufferStringSection = CBufferStringGrowable<MAX_GAMEDATA_SECTION_MESSAGE_LENGTH>;

	class CBufferStringConcat : public CBufferStringSection
	{
	public:
		using Base = CBufferStringSection;
		using Base::Base;

		template<std::size_t N>
		CBufferStringConcat(const char *(&pszSplit)[N])
		{
			AppendConcat(N, pszSplit, NULL);
		}

		template<std::size_t N>
		CBufferStringConcat(const char *pszStartWtih, const char *(&pszSplit)[N])
		{
			Insert(0, pszStartWtih);
			AppendConcat(N, pszSplit, NULL);
		}
	};

public:
	bool Init(CUtlVector<CBufferStringConcat> &vecMessages);
	void Clear();
	void Destroy();

public: // IGameData
	const DynLibUtils::CModule *FindLibrary(std::string sName) const;

protected:
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

public:
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

			Storage() = default;
			Storage(IListener *pFirstListener)
			{
				m_vecListeners.push_back(pFirstListener);
			}

		public:
			using OnCollectorChangedCallback = std::function<void (const K &, const V &)>;

			template<typename T>
			class BaseListenerCollector : public IListener
			{
			private:
				using Base = IListener; // Root interface.
				using This = BaseListenerCollector;

			public:
				virtual void Insert(const K &aKey, const OnCollectorChangedCallback &funcCallback) = 0;
				virtual bool Remove(const K &aKey) = 0;

				virtual void RemoveAll()
				{
					m_mapCallbacks.clear();
				}

			protected:
				std::unordered_map<K, T> m_mapCallbacks;
			}; // GameData::Config::Storage::BaseListenerCollector

			class ListenerCallbacksCollector : public BaseListenerCollector<OnCollectorChangedCallback>
			{
			private:
				using Base = BaseListenerCollector<OnCollectorChangedCallback>;
				using This = ListenerCallbacksCollector;

				using Base::m_mapCallbacks;

			public:
				ListenerCallbacksCollector() = default;

			public: // BaseListenerCollector<>
				void Insert(const K &aKey, const OnCollectorChangedCallback &funcCallback) override
				{
					m_mapCallbacks[aKey] = funcCallback;
				}

				bool Remove(const K &aKey) override
				{
					auto &map = m_mapCallbacks;

					const auto it = map.find(aKey);

					bool bResult = it != map.cend();

					if(bResult)
					{
						map.erase(it);
					}

					return bResult;
				}

			public: // IListener
				void OnChanged(const K &aKey, const V &aValue) override
				{
					auto &map = m_mapCallbacks;

					auto it = map.find(aKey);

					if(it != map.cend())
					{
						(it->second)(aKey, aValue);
					}
				}
			}; // GameData::Config::Storage::ListenerCallbacksCollector

			class ListenerMultipleCallbacksCollector : public BaseListenerCollector<std::vector<OnCollectorChangedCallback>>
			{
			private:
				using Base = BaseListenerCollector<std::vector<OnCollectorChangedCallback>>;
				using This = ListenerMultipleCallbacksCollector;

				using Base::m_mapCallbacks;

			public:
				ListenerMultipleCallbacksCollector() = default;

				// Adapter.
				void Insert(const K &aKey, const std::vector<OnCollectorChangedCallback> &vecCallbacks) override
				{
					auto &map = m_mapCallbacks;

					auto it = map.find(aKey);

					if(it != map.cend())
					{
						it.insert(it.end(), vecCallbacks.begin(), vecCallbacks.end());
					}
					else
					{
						map[aKey] = vecCallbacks;
					}
				}

			public: // BaseListenerCollector<>
				void Insert(K aKey, OnCollectorChangedCallback funcCallback) override
				{
					auto &map = m_mapCallbacks;

					auto it = map.find(aKey);

					if(it != map.cend())
					{
						it.second.push_back(funcCallback);
					}
					else
					{
						map[aKey] = {funcCallback};
					}
				}

				bool Remove(K aKey)
				{
					auto &map = m_mapCallbacks;

					const auto it = map.find(aKey);

					bool bResult = it != map.cend();

					if(bResult)
					{
						map.erase(it);
					}

					return bResult;
				}

			public: // IListener
				void OnChanged(const K &aKey, const V &aValue) override
				{
					auto &map = m_mapCallbacks;

					auto it = map.find(aKey);

					if(it != map.cend())
					{
						auto &vec = it.second;

						for(size_t n = 0, nLength = vec.size(); n < nLength; n++)
						{
							vec[n](aKey, aValue);
						}
					}
				}
			}; // GameData::Config::Storage::ListenerMultipleCallbacksCollector
		
		public:
			void ClearValues()
			{
				m_mapValues.clear();
			}

			void ClearListeners()
			{
				m_vecListeners.clear();
			}

		public:
			const V &operator[](const K &aKey) const
			{
				return m_mapValues[aKey];
			}

			const V &Get(const K &aKey) const
			{
				return m_mapValues.at(aKey);
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
				m_mapValues[aKey] = aValue;

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
				m_vecListeners.push_back(pListener);
			}

			virtual bool RemoveListener(IListener *pListener)
			{
				auto &vec = m_vecListeners;

				auto it = std::find(vec.begin(), vec.end(), pListener);

				bool bResult = it != vec.cend();

				if(bResult)
				{
					vec.erase(it);
				}

				return bResult;
			}

		private:
			std::map<K, V> m_mapValues;
			std::vector<IListener *> m_vecListeners;
		}; // GameData::Config::Storage

	public:
		using Addresses = Storage<std::string, DynLibUtils::CMemory>;
		using Offsets = Storage<std::string, ptrdiff_t>;

	public:
		Config() = default;
		Config(Addresses aInitAddressStorage, Offsets aInitOffsetsStorage);

	public:
		bool Load(IGameData *pRoot, KeyValues3 *pGameConfig, CUtlVector<CBufferStringConcat> &vecMessages);
		void ClearValues();

	public:
		Addresses &GetAddresses();
		Offsets &GetOffsets();

	protected:
		bool LoadEngine(IGameData *pRoot, KeyValues3 *pEngineValues, CUtlVector<CBufferStringConcat> &vecMessages);

		bool LoadEngineSignatures(IGameData *pRoot, KeyValues3 *pSignaturesValues, CUtlVector<CBufferStringConcat> &vecMessages);
		bool LoadEngineOffsets(IGameData *pRoot, KeyValues3 *pOffsetsValues, CUtlVector<CBufferStringConcat> &vecMessages);

		// Step #2 - addresses.
		bool LoadEngineAddresses(IGameData *pRoot, KeyValues3 *pAddressesValues, CUtlVector<CBufferStringConcat> &vecMessages);
		bool LoadEngineAddressActions(IGameData *pRoot, uintptr_t &pAddrCur, KeyValues3 *pActionValues,  CUtlVector<CBufferStringConcat> &vecMessages);

	public:
		const DynLibUtils::CMemory &GetAddress(const std::string &sName) const;
		const ptrdiff_t &GetOffset(const std::string &sName) const;

	protected:
		void SetAddress(const std::string &sName, DynLibUtils::CMemory aMemory);
		void SetOffset(const std::string &sName, ptrdiff_t nValue);

	private:
		Addresses m_aAddressStorage;
		Offsets m_aOffsetStorage;
	}; // GameData::Config

private:
	std::map<std::string, const DynLibUtils::CModule *> m_aLibraryMap;
}; // GameData

#endif //_INCLUDE_GAMEDATA_HPP_
