#pragma once

namespace LavenderHook {
    namespace Audio {
        void PlayToggleSound(bool on);
        void PlayFailSound();
        void PlayHideWindowSound();
        void SetVolumePercent(int pct);
        int GetVolumePercent();
    }
}
