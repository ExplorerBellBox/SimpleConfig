#include "simple_config_manager.h"
#include <iostream>

int
main()
{
	auto &_manager = Simple::ConfigManager::Inst();
	const auto _file = "./config.ini";
	std::cout << _manager.GetValue(_file, 1, "section 0", "key 0") << std::endl;
	std::cout << _manager.GetValue(_file, 10, "section 0", "key 1") << std::endl;
	std::cout << _manager.GetValue(_file, 100, "section 1", "key") << std::endl;
	std::cout << _manager.GetValue(_file, 1000, "section 2", "key") << std::endl;
	_manager.SetValue(_file, "section 0", "key_0", 123, "default is 1");
	_manager.SetValue(_file, "section 0", "key_1", 234, "default is 10");
	_manager.SetValue(_file, "section 1", "key", 456, "default is 100");
	_manager.SetValue(_file, "section 2", "key", 678, "default is 1000");
	std::cout << _manager.GetValue(_file, 1, "section 0", "key_0") << std::endl;
	std::cout << _manager.GetValue(_file, 10, "section 0", "key_1") << std::endl;
	std::cout << _manager.GetValue(_file, 100, "section 1", "key") << std::endl;
	std::cout << _manager.GetValue(_file, 1000, "section 2", "key") << std::endl;
	_manager.UpdateFile(_file);
	return 0;
}
