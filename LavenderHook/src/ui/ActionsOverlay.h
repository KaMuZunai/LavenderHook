#pragma once
#include <string>
#include <vector>

namespace LavenderHook {
	namespace UI {
		namespace Actions {

			void SetActive(const std::string& label, bool on);

			void ClearAll();
			std::vector<std::string> GetActiveList();

		}
	}
}
