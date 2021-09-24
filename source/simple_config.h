#pragma once

#include <string>
#include <sstream>
#include <list>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <cassert>
#include <regex>
#include <unordered_map>

#define E_MAYBE_UNSUED                 [[maybe_unused]]
#define E_NODISCARD                    [[nodiscard]]
#define E_MAYBE_UNSUED_NODISCARD       [[maybe_unused, nodiscard]]

namespace Simple
{

#ifdef _MSC_VER
class Config final
#else
class __attribute__((visibility("default"), aligned(8))) Config final
#endif
{
public:
	struct KeyValue_t final
	{
		explicit
		KeyValue_t(std::string _key = "", std::string _value = "", std::string _comment = "") :
			strKey(std::move(_key)), strValue(std::move(_value)), strComment(std::move(_comment)) {}

		KeyValue_t(const KeyValue_t &) = default;

		KeyValue_t &
		operator=(const KeyValue_t &) = default;

		KeyValue_t(KeyValue_t &&_src) noexcept
		{
			strKey = std::move(_src.strKey);
			strValue = std::move(_src.strValue);
			strComment = std::move(_src.strComment);
		}

		KeyValue_t &
		operator=(KeyValue_t &&_rhs) noexcept
		{
			if (this != &_rhs)
			{
				strKey = std::move(_rhs.strKey);
				strValue = std::move(_rhs.strValue);
				strComment = std::move(_rhs.strComment);
			}
			return *this;
		}

		std::string strKey;
		std::string strValue;
		std::string strComment;
	};

private:
	struct Section_t final
	{
		explicit
		Section_t(std::string _section) : strSection(std::move(_section)) {}

		Section_t(const Section_t &) = default;

		Section_t &
		operator=(const Section_t &) = default;

		Section_t(Section_t &&_src) noexcept
		{
			strSection = std::move(_src.strSection);
			listKeyValue = std::move(_src.listKeyValue);
		}

		Section_t &
		operator=(Section_t &&_rhs) noexcept
		{
			if (this != &_rhs)
			{
				strSection = std::move(_rhs.strSection);
				listKeyValue = std::move(_rhs.listKeyValue);
			}
			return *this;
		}

		std::string strSection;
		std::list<KeyValue_t> listKeyValue;
	};

	using Config_t = std::list<Section_t>;
	using Mutex = std::recursive_mutex;
	using SafeLock = std::unique_lock<Mutex>;

public:
	Config() = default;

	~Config() noexcept = default;

	Config(const Config &) = delete;

	Config &
	operator=(const Config &) = delete;

	E_NODISCARD
	bool
	ReadFile(const std::string &_file)
	{
		SafeLock _sl(m_mutex);
		assert(!_file.empty());
		assert(m_strFile.empty());
		m_strFile = _file;

		{
			std::fstream _fs;
			_fs.open(m_strFile, std::ios_base::in);
			if (!_fs.good())
			{
				_fs.close();
				// try create the config file if not exist
				_fs.open(m_strFile, std::ios_base::out | std::ios_base::app);
				return _fs.good();
			}

			std::string _line;
			std::string _section;
			KeyValue_t _kv;
			while (getline(_fs, _line))
			{
				_line = TrimLeftRightSpace(_line);
				if (_line.empty()
					|| IsCommentLine(_line)
					|| IsSectionLine(_section, _line))
				{
					continue;
				}
				else if (!_section.empty() && IsKeyValueLine(_kv, _line))
				{
					InsertKeyValue(_kv, _section);
				}
			}
		}

		return true;
	}

	E_MAYBE_UNSUED_NODISCARD inline
	const std::string &
	GetFile() const { return m_strFile; }

	bool
	UpdateFile()
	{
		SafeLock _sl(m_mutex);
		assert(!m_strFile.empty());
		const auto strConfig = GetConfigString(m_config);
		{
			std::ofstream ofs;
			ofs.open(m_strFile, std::ios_base::out | std::ios_base::trunc);
			if (!ofs.good())
			{
				return false;
			}
			ofs.write(strConfig.c_str(), static_cast<std::streamsize>(strConfig.size()));
		}
		return true;
	}

	E_MAYBE_UNSUED
	bool
	SetValue(const std::string &_section, const std::string &_key, const std::string &_value,
			 const std::string &_comment = "", bool _replaceExistComment = false)
	{
		if (!IsValidSectionString(_section) || !IsValidKeyString(_key))
		{
			return false;
		}

		auto _commentTmp = TrimLeftRightSpace(_comment);
		if (!_commentTmp.empty()
			&& (_commentTmp[0] != ';')
			&& (_commentTmp[0] != '#'))
		{
			_commentTmp = std::string("# ") + _commentTmp;
		}

		{
			SafeLock _sl(m_mutex);
			auto pKeyValue = FindKeyValue(_key, _section, m_config);
			if (nullptr == pKeyValue)
			{
				KeyValue_t _kv{_key, _value, _commentTmp};
				InsertKeyValue(_kv, _section);
			}
			else
			{
				pKeyValue->strValue = _value;
				if (_replaceExistComment || !_commentTmp.empty())
				{
					pKeyValue->strComment = _commentTmp;
				}
			}
		}
		return true;
	}

	E_MAYBE_UNSUED
	bool
	SetValue(const std::string &_section, const std::string &_key, const char *__restrict _value,
			 const std::string &_comment = "", bool _replaceExistComment = false)
	{
		assert(_value);
		return SetValue(_section, _key, std::string(_value), _comment, _replaceExistComment);
	}

	template<typename T>
	E_MAYBE_UNSUED inline
	bool
	SetValue(const std::string &_section, const std::string &_key, T _value,
			 const std::string &_comment = "", bool _replaceExistComment = false)
	{
		return SetValue(_section, _key, std::to_string(_value), _comment, _replaceExistComment);
	}

	E_MAYBE_UNSUED_NODISCARD inline
	std::string
	GetValue(const std::string &_default, const std::string &_section, const std::string &_key)
	{
		SafeLock _sl(m_mutex);
		auto _kv = FindKeyValue(_key, _section, m_config);
		return (_kv && !(_kv->strValue.empty())) ? _kv->strValue : _default;
	}

	E_MAYBE_UNSUED_NODISCARD [[maybe_unused]] inline
	std::string
	GetValue(const char *__restrict _default, const std::string &_section, const std::string &_key)
	{
		return GetValue(std::string(_default ? _default : ""), _section, _key);
	}

	template<typename T>
	E_MAYBE_UNSUED_NODISCARD inline
	T
	GetValue(T _default, const std::string &_section, const std::string &_key)
	{
		assert(std::is_arithmetic<T>::value);
		auto _value = GetValue(std::string{}, _section, _key);
		if (_value.empty())
		{
			return _default;
		}
		T _v = 0;
		return StringToNumber(_v, _value.c_str()) ? _v : _default;
	}

	E_MAYBE_UNSUED_NODISCARD
	std::list<KeyValue_t>
	GetSection(const std::string &_section)
	{
		SafeLock _sl(m_mutex);
		const auto pSection = FindSection(_section, m_config);
		return pSection ? pSection->listKeyValue : std::list<KeyValue_t>{};
	}

	E_MAYBE_UNSUED
	bool
	DeleteKey(const std::string &_section, const std::string &_key, bool _updateFile = true)
	{
		SafeLock _sl(m_mutex);
		for (auto &section: m_config)
		{
			if (_section == section.strSection)
			{
				for (auto iter = section.listKeyValue.begin(); iter != section.listKeyValue.end(); ++iter)
				{
					if (_key == iter->strKey)
					{
						section.listKeyValue.erase(iter);
						break;
					}
				}
				break;
			}
		}
		return !_updateFile || UpdateFile();
	}

	E_MAYBE_UNSUED
	bool
	DeleteSection(const std::string &_section, bool _updateFile = true)
	{
		SafeLock _sl(m_mutex);
		for (auto iter = m_config.begin(); iter != m_config.end(); ++iter)
		{
			if (_section == iter->strSection)
			{
				m_config.erase(iter);
				break;
			}
		}
		return !_updateFile || UpdateFile();
	}

private:
	void
	InsertKeyValue(KeyValue_t &_kv, const std::string &_section)
	{
		auto pSection = FindSection(_section, m_config);
		if (nullptr == pSection)
		{
			auto &refSection = m_config.emplace_back(Section_t{_section});
			refSection.listKeyValue.emplace_back(std::move(_kv));
		}
		else
		{
			auto pKeyValue = FindKeyValue(_kv.strKey, *pSection);
			if (nullptr == pKeyValue)
			{
				pSection->listKeyValue.push_back(std::move(_kv));
			}
			else
			{
				*pKeyValue = std::move(_kv);
			}
		}
	}

	E_NODISCARD static inline
	bool
	IsCommentLine(const std::string &_str)
	{
		return (';' == _str[0]) || ('#' == _str[0]);
	}

	E_NODISCARD static inline
	bool
	IsSectionLine(std::string &_section, const std::string &_str)
	{
		if ((_str.size() < 3) || ('[' != _str[0]) || (']' != _str[_str.size() - 1]))
		{
			return false;
		}
		static const std::regex _reg{R"+(\[[\t ]*([A-Za-z_][A-Za-z_0-9- ]*)[\t ]*\])+"};
		std::smatch _m;
		if (!std::regex_match(_str, _m, _reg))
		{
			return false;
		}
		_section = TrimLeftRightSpace(_m[1]);
		return true;
	}

	E_NODISCARD static inline
	bool
	IsKeyValueLine(KeyValue_t &_kv, const std::string &_str)
	{
		static const std::regex _reg{R"+(([^=#;]+)[\t ]*=[\t ]*([^=#;]*)(.*))+"};
		std::smatch _m;
		if (!std::regex_match(_str, _m, _reg))
		{
			return false;
		}
		_kv.strKey = TrimLeftRightSpace(_m[1]);
		_kv.strValue = TrimLeftRightSpace(_m[2]);
		_kv.strComment = TrimLeftRightSpace(_m[3]);
		return true;
	}

	E_NODISCARD static inline
	bool
	IsValidSectionString(const std::string &_section)
	{
		if (_section.empty() || (' ' == _section[_section.size() - 1]))
		{
			return false;
		}
		static const std::regex _reg{R"+([A-Za-z_][A-Za-z_0-9\- ]*)+"};
		std::smatch _m;
		return std::regex_match(_section, _m, _reg);
	}

	E_NODISCARD static inline
	bool
	IsValidKeyString(const std::string &_key)
	{
		if (_key.empty() || (' ' == _key[_key.size() - 1]))
		{
			return false;
		}
		static const std::regex _reg{R"+([A-Za-z_][\w_\.]*)+"};
		std::smatch _m;
		return std::regex_match(_key, _m, _reg);
	}

	E_NODISCARD static inline
	Section_t *
	FindSection(const std::string &_section, Config_t &_config)
	{
		for (auto &_item: _config)
		{
			if (_section == _item.strSection)
			{
				return &_item;
			}
		}
		return nullptr;
	}

	E_NODISCARD static inline
	KeyValue_t *
	FindKeyValue(const std::string &_key, Section_t &_section)
	{
		for (auto &_kv: _section.listKeyValue)
		{
			if (_key == _kv.strKey)
			{
				return &_kv;
			}
		}
		return nullptr;
	}

	E_NODISCARD static inline
	KeyValue_t *
	FindKeyValue(const std::string &_key, const std::string &_section, Config_t &_config)
	{
		auto pSection = FindSection(_section, _config);
		return pSection ? FindKeyValue(_key, *pSection) : nullptr;
	}

	E_NODISCARD
	std::string
	GetConfigString(Config_t &_config)
	{
		std::stringstream _ss;
		{
			SafeLock _sl(m_mutex);
			for (const auto &_section: _config)
			{
				_ss << '[' << _section.strSection << "]\n";
				for (const auto &_kv: _section.listKeyValue)
				{
					_ss << _kv.strKey << '=' << _kv.strValue;
					if (!_kv.strComment.empty())
					{
						_ss << "\t\t" << _kv.strComment;
					}
					_ss << '\n';
				}
				_ss << '\n';
			}
		}
		return _ss.str();
	}

	E_NODISCARD static
	std::string
	TrimLeftRightSpace(const std::string &_src)
	{
		if (_src.empty())
		{
			return _src;
		}
		const char *_l = _src.c_str();
		const char *_r = _l + _src.size();
		while ((_l < _r)
			&& ((' ' == *_l)
				|| ('\t' == *_l)
				|| ('\n' == *_l)
				|| ('\r' == *_l)))
		{
			++_l;
		}

		--_r;
		while ((_l <= _r)
			&& ((' ' == *_r)
				|| ('\t' == *_r)
				|| ('\n' == *_r)
				|| ('\r' == *_r)))
		{
			--_r;
		}

		return std::string{_l, static_cast<std::string::size_type>(_r - _l + 1)};
	}

	E_MAYBE_UNSUED static inline
	bool
	StringToNumber(short &_v, const char *__restrict _str)
	{
		char *_end = nullptr;
		_v = static_cast<short>(std::strtol(_str, &_end, 0));
		return ((_str != _end) && ('\0' == *_end));
	}

	E_MAYBE_UNSUED static inline
	bool
	StringToNumber(unsigned short &_v, const char *__restrict _str)
	{
		char *_end = nullptr;
		_v = static_cast<unsigned short>(std::strtoul(_str, &_end, 0));
		return ((_str != _end) && ('\0' == *_end));
	}

	E_MAYBE_UNSUED static inline
	bool
	StringToNumber(int &_v, const char *__restrict _str)
	{
		char *_end = nullptr;
		_v = static_cast<int>(std::strtol(_str, &_end, 0));
		return ((_str != _end) && ('\0' == *_end));
	}

	E_MAYBE_UNSUED static inline
	bool
	StringToNumber(unsigned int &_v, const char *__restrict _str)
	{
		char *_end = nullptr;
		_v = static_cast<unsigned int>(std::strtoul(_str, &_end, 0));
		return ((_str != _end) && ('\0' == *_end));
	}

	E_MAYBE_UNSUED static inline
	bool
	StringToNumber(long long &_v, const char *__restrict _str)
	{
		char *_end = nullptr;
		_v = std::strtoll(_str, &_end, 0);
		return ((_str != _end) && ('\0' == *_end));
	}

	E_MAYBE_UNSUED static inline
	bool
	StringToNumber(unsigned long long &_v, const char *__restrict _str)
	{
		char *_end = nullptr;
		_v = std::strtoull(_str, &_end, 0);
		return ((_str != _end) && ('\0' == *_end));
	}

	E_MAYBE_UNSUED static inline
	bool
	StringToNumber(unsigned long &_v, const char *__restrict _str)
	{
		char *_end = nullptr;
		_v = std::strtoul(_str, &_end, 0);
		return ((_str != _end) && ('\0' == *_end));
	}

	E_MAYBE_UNSUED static inline
	bool
	StringToNumber(float &_v, const char *__restrict _str)
	{
		char *_end = nullptr;
		_v = std::strtof(_str, &_end);
		return ((_str != _end) && ('\0' == *_end));
	}

	E_MAYBE_UNSUED static inline
	bool
	StringToNumber(double &_v, const char *__restrict _str)
	{
		char *_end = nullptr;
		_v = std::strtod(_str, &_end);
		return ((_str != _end) && ('\0' == *_end));
	}

	E_MAYBE_UNSUED static inline
	bool
	StringToNumber(long double &_v, const char *__restrict _str)
	{
		char *_end = nullptr;
		_v = std::strtold(_str, &_end);
		return ((_str != _end) && ('\0' == *_end));
	}

private:
	std::string m_strFile;
	Config_t m_config;
	Mutex m_mutex;
};

}