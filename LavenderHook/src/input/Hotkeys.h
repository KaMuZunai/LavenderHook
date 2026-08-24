#pragma once
#include <string>

namespace LavenderHook {
	namespace Input {

		void Update();
		bool IsPressed(const std::string& name, bool requireForeground = true);
		bool IsDown(const std::string& name, bool requireForeground = true);
		void Clear();
	}
} // namespace
