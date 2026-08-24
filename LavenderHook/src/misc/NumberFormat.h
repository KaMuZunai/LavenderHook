#pragma once

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace LavenderHook::Misc
{

    inline std::string FormatCompactNumber(double value)
    {
        if (std::isnan(value) || std::isinf(value))
            return "???";

        const double absV = std::fabs(value);
        if (absV < 10000.0)
        {
            return std::to_string(static_cast<long long>(value));
        }

        static constexpr const char* suffixes[] = { "", "k", "M", "B", "T", "Qa", "Qi" };
        constexpr int maxSuffix = static_cast<int>(sizeof(suffixes) / sizeof(suffixes[0])) - 1;

        double scaled = absV;
        int suffixIdx = 0;
        while (scaled >= 1000.0 && suffixIdx < maxSuffix)
        {
            scaled /= 1000.0;
            ++suffixIdx;
        }

        if (value < 0.0)
            scaled = -scaled;

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << scaled << suffixes[suffixIdx];
        return oss.str();
    }
}
