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

#include <dynlibutils/module.hpp>
#include <dynlibutils/memaddr.hpp>

#include <stddef.h>

#include <functional>
#include <map>
#include <utility>
#include <string>
#include <vector>

class KeyValues;

class IGameData
{
public:
	virtual const DynLibUtils::CModule *FindLibrary(std::string sName) const = 0;
}; // IGameData

class GameData : public IGameData
{
	using This = GameData;

public:
	bool Init(char *psError, size_t nMaxLength);
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
				this->m_vecListeners.push_back(pFirstListener);
			}

		public:
			using OnCollectorChangedCallback = std::function<void (const K &, const V &)>;

			template<typename T>
			class BaseListenerCollector : public IListener
			{
			private:
				using Super = IListener; // Root interface.
				using This = BaseListenerCollector;

			public:
				virtual void Insert(const K &aKey, const OnCollectorChangedCallback &funcCallback) = 0;
				virtual bool Remove(const K &aKey) = 0;

				virtual void RemoveAll()
				{
					this->m_mapCallbacks.clear();
				}

			protected:
				std::unordered_map<K, T> m_mapCallbacks;
			}; // GameData::Config::Storage::BaseListenerCollector

			class ListenerCallbacksCollector : public BaseListenerCollector<OnCollectorChangedCallback>
			{
			private:
				using Super = BaseListenerCollector<OnCollectorChangedCallback>;
				using This = ListenerCallbacksCollector;

			public:
				ListenerCallbacksCollector() = default;

			public: // BaseListenerCollector<>
				void Insert(const K &aKey, const OnCollectorChangedCallback &funcCallback) override
				{
					this->m_mapCallbacks[aKey] = funcCallback;
				}

				bool Remove(const K &aKey) override
				{
					auto &map = this->m_mapCallbacks;

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
					auto &map = this->m_mapCallbacks;

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
				using Super = BaseListenerCollector<std::vector<OnCollectorChangedCallback>>;
				using This = ListenerMultipleCallbacksCollector;

			public:
				ListenerMultipleCallbacksCollector() = default;

				// Adapter.
				void Insert(const K &aKey, const std::vector<OnCollectorChangedCallback> &vecCallbacks) override
				{
					auto &map = this->m_mapCallbacks;

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
					auto &map = this->m_mapCallbacks;

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
					auto &map = this->m_mapCallbacks;

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
					auto &map = this->m_mapCallbacks;

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
				this->m_mapValues.clear();
			}

			void ClearListeners()
			{
				this->m_vecListeners.clear();
			}

		public:
			const V &operator[](const K &aKey) const
			{
				return this->m_mapValues.at(aKey);
			}

			const V &Get(const K &aKey) const
			{
				return this->operator[](aKey);
			}

			void TriggerCallbacks()
			{
				for(auto const &[aKey, aVal] : this->m_mapValues)
				{
					this->OnChanged(aKey, aVal);
				}
			}

		protected:
			void Set(const K &aKey, const V &aValue)
			{
				this->m_mapValues[aKey] = aValue;

				this->OnChanged(aKey, aValue);
			}

		private:
			void OnChanged(const K &aKey, const V &aValue)
			{
				auto &vec = this->m_vecListeners;

				for(const auto it : vec)
				{
					it->OnChanged(aKey, aValue);
				}
			}

		public:
			virtual void AddListener(IListener *pListener)
			{
				this->m_vecListeners.push_back(pListener);
			}

			virtual bool RemoveListener(IListener *pListener)
			{
				auto &vec = this->m_vecListeners;

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
		bool Load(IGameData *pRoot, KeyValues *pGameConfig, char *psError = NULL, size_t nMaxLength = 0);
		void ClearValues();

	public:
		Addresses &GetAddresses();
		Offsets &GetOffsets();

		void TriggerCallbacks();

	protected:
		bool LoadEngine(IGameData *pRoot, KeyValues *pEngineValues, char *psError = NULL, size_t nMaxLength = 0);

		bool LoadEngineSignatures(IGameData *pRoot, KeyValues *pSignaturesValues, char *psError = NULL, size_t nMaxLength = 0);
		bool LoadEngineOffsets(KeyValues *pOffsetsValues, char *psError = NULL, size_t nMaxLength = 0);

		// Step #2 - addresses.
		bool LoadEngineAddresses(KeyValues *pAddressesValues, char *psError = NULL, size_t nMaxLength = 0);
		bool LoadEngineAddressActions(uintptr_t &pAddrCur, KeyValues *pActionValues, char *psError = NULL, size_t nMaxLength = 0);

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
