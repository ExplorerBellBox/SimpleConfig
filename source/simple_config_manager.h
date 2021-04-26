#pragma once

#include "./simple_config.h"

namespace Simple
{

class __attribute__((visibility("default"), aligned(8))) ConfigManager final
{
private:
	using Mutex = std::mutex;
	using SafeLock = std::unique_lock<Mutex>;

public:
	static
	ConfigManager &
	Inst()
	{
		static ConfigManager _inst;
		return _inst;
	}

	~ConfigManager() noexcept = default;

	ConfigManager(const ConfigManager &) = delete;

	ConfigManager &
	operator=(const ConfigManager &) = delete;

	[[maybe_unused, nodiscard]] inline
	Config *
	GetConfig(const std::string &_file)
	{
		SafeLock _sl(m_mutex);
		return GetConfigNoLock(_file);
	}

	[[maybe_unused]]
	bool
	UpdateFile(const std::string &_file)
	{
		SafeLock _sl(m_mutex);
		auto _config = GetConfigNoLock(_file);
		return _config && _config->UpdateFile();
	}

	[[maybe_unused]]
	bool
	SetValue(const std::string &_file, const std::string &_section, const std::string &_key, const std::string &_value,
			 const std::string &_comment = "", bool _replaceExistComment = false)
	{
		SafeLock _sl(m_mutex);
		auto _config = GetConfigNoLock(_file);
		return _config && _config->SetValue(_section, _key, _value, _comment, _replaceExistComment);
	}

	template<typename T>
	[[maybe_unused]] inline
	bool
	SetValue(const std::string &_file, const std::string &_section, const std::string &_key, T _value,
			 const std::string &_comment = "", bool _replaceExistComment = false)
	{
		SafeLock _sl(m_mutex);
		auto _config = GetConfigNoLock(_file);
		return _config && _config->SetValue(_section, _key, _value, _comment, _replaceExistComment);
	}

	[[maybe_unused, nodiscard]] inline
	std::string
	GetValue(const std::string &_file,
			 const std::string &_default, const std::string &_section, const std::string &_key)
	{
		SafeLock _sl(m_mutex);
		auto _config = GetConfigNoLock(_file);
		return _config ? _config->GetValue(_default, _section, _key) : _default;
	}

	template<typename T>
	[[maybe_unused, nodiscard]] inline
	T
	GetValue(const std::string &_file, T _default, const std::string &_section, const std::string &_key)
	{
		SafeLock _sl(m_mutex);
		auto _config = GetConfigNoLock(_file);
		return _config ? _config->GetValue(_default, _section, _key) : _default;
	}

	[[maybe_unused, nodiscard]]
	std::list<Config::KeyValue_t>
	GetSection(const std::string &_file, const std::string &_section)
	{
		SafeLock _sl(m_mutex);
		auto _config = GetConfigNoLock(_file);
		return _config ? _config->GetSection(_section) : std::list<Config::KeyValue_t>{};
	}

	[[maybe_unused]]
	bool
	DeleteKey(const std::string &_file, const std::string &_section, const std::string &_key, bool _updateFile = true)
	{
		SafeLock _sl(m_mutex);
		auto _config = GetConfigNoLock(_file);
		return (nullptr == _config) || _config->DeleteKey(_section, _key, _updateFile);
	}

	[[maybe_unused]]
	bool
	DeleteSection(const std::string &_file, const std::string &_section, bool _updateFile = true)
	{
		SafeLock _sl(m_mutex);
		auto _config = GetConfigNoLock(_file);
		return (nullptr == _config) || _config->DeleteSection(_section, _updateFile);
	}

private:
	ConfigManager() = default;

	[[nodiscard]] inline
	Config *
	GetConfigNoLock(const std::string &_file)
	{
		auto iter = m_configs.find(_file);
		if (m_configs.end() != iter)
		{
			return iter->second;
		}

		auto _config = new Config;
		if (!_config->ReadFile(_file))
		{
			delete _config;
			return nullptr;
		}

		m_configs[_file] = _config;
		return _config;
	}

private:
	Mutex m_mutex;
	std::unordered_map<std::string, Config *> m_configs;
};

}