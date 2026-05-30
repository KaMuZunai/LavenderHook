#include "SoundPlayer.h"
#include <windows.h>
#include <vector>
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio/miniaudio.h"
#include "../assets/resources/resource.h"
#include "../misc/Globals.h"

namespace LavenderHook {
    namespace Audio {

        struct SoundData {
            std::vector<uint8_t> raw; // full WAV file bytes
        };

        static ma_engine s_engine;
        static bool s_engineInited = false;

        // Simple theme sounds
        static ma_sound s_soundOn;
        static ma_sound s_soundOff;
        static ma_sound s_soundFail;
        static ma_sound s_soundHide;
        static ma_decoder s_decoderOn;
        static ma_decoder s_decoderOff;
        static ma_decoder s_decoderFail;
        static ma_decoder s_decoderHide;
        static bool s_onLoaded = false;
        static bool s_offLoaded = false;
        static bool s_failLoaded = false;
        static bool s_hideLoaded = false;
        static SoundData s_onData;
        static SoundData s_offData;
        static SoundData s_failData;
        static SoundData s_hideData;

        // Polished theme sounds
        static ma_sound s_soundOnPol;
        static ma_sound s_soundOffPol;
        static ma_sound s_soundFailPol;
        static ma_sound s_soundHidePol;
        static ma_decoder s_decoderOnPol;
        static ma_decoder s_decoderOffPol;
        static ma_decoder s_decoderFailPol;
        static ma_decoder s_decoderHidePol;
        static bool s_onPolLoaded = false;
        static bool s_offPolLoaded = false;
        static bool s_failPolLoaded = false;
        static bool s_hidePolLoaded = false;
        static SoundData s_onDataPol;
        static SoundData s_offDataPol;
        static SoundData s_failDataPol;
        static SoundData s_hideDataPol;

        static void InitSoundFromData(const SoundData& data, ma_decoder* decoder, ma_sound* sound, bool& loaded)
        {
            if (data.raw.empty()) return;
            ma_decoder_config dcfg = ma_decoder_config_init(ma_format_unknown, 0, 0);
            if (ma_decoder_init_memory(data.raw.data(), data.raw.size(), &dcfg, decoder) == MA_SUCCESS) {
                if (ma_sound_init_from_data_source(&s_engine, (ma_data_source*)decoder, MA_SOUND_FLAG_DECODE, NULL, sound) == MA_SUCCESS) {
                    loaded = true;
                } else {
                    ma_decoder_uninit(decoder);
                }
            }
        }

        static bool LoadWavFromResource(UINT id, SoundData& out)
        {
            HMODULE mod = NULL;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)&LoadWavFromResource, &mod))
                mod = GetModuleHandle(NULL);

            HRSRC rc = FindResourceW(mod, MAKEINTRESOURCEW(id), L"WAVE");
            if (!rc) return false;
            HGLOBAL hg = LoadResource(mod, rc);
            if (!hg) return false;
            void* ptr = LockResource(hg);
            DWORD sz = SizeofResource(mod, rc);
            if (!ptr || sz == 0) return false;

            uint8_t* p = (uint8_t*)ptr;
            out.raw.resize(sz);
            memcpy(out.raw.data(), p, sz);
            return true;
        }

        static void EnsureInit()
        {
            if (s_engineInited) return;
            if (ma_engine_init(NULL, &s_engine) != MA_SUCCESS) return;
            s_engineInited = true;

            LoadWavFromResource(TOGGLE_ON, s_onData);
            LoadWavFromResource(TOGGLE_OFF, s_offData);
            LoadWavFromResource(STOP_ON_FAIL, s_failData);
            LoadWavFromResource(HIDE_NOTIF, s_hideData);

            InitSoundFromData(s_onData,   &s_decoderOn,   &s_soundOn,   s_onLoaded);
            InitSoundFromData(s_offData,  &s_decoderOff,  &s_soundOff,  s_offLoaded);
            InitSoundFromData(s_failData, &s_decoderFail, &s_soundFail, s_failLoaded);
            InitSoundFromData(s_hideData, &s_decoderHide, &s_soundHide, s_hideLoaded);

            LoadWavFromResource(TOGGLE_ON_POLISHED, s_onDataPol);
            LoadWavFromResource(TOGGLE_OFF_POLISHED, s_offDataPol);
            LoadWavFromResource(STOP_ON_FAIL_POLISHED, s_failDataPol);
            LoadWavFromResource(HIDE_NOTIF_POLISHED, s_hideDataPol);

            InitSoundFromData(s_onDataPol,   &s_decoderOnPol,   &s_soundOnPol,   s_onPolLoaded);
            InitSoundFromData(s_offDataPol,  &s_decoderOffPol,  &s_soundOffPol,  s_offPolLoaded);
            InitSoundFromData(s_failDataPol, &s_decoderFailPol, &s_soundFailPol, s_failPolLoaded);
            InitSoundFromData(s_hideDataPol, &s_decoderHidePol, &s_soundHidePol, s_hidePolLoaded);

            SetVolumePercent(LavenderHook::Globals::sound_volume);
        }

        void PlayFailSound()
        {
            EnsureInit();
            if (!s_engineInited) return;

            // Only play fail sound if stop_on_fail is enabled and not muted
            if (!LavenderHook::Globals::stop_on_fail) return;
            if (LavenderHook::Globals::mute_fail) return;

            float vol = (float)LavenderHook::Globals::sound_volume / 100.0f;
            bool polished = LavenderHook::Globals::use_polished_overlay;
            if (polished) {
                if (!s_failPolLoaded) return;
                ma_sound_set_volume(&s_soundFailPol, vol);
                ma_sound_start(&s_soundFailPol);
            } else {
                if (!s_failLoaded) return;
                ma_sound_set_volume(&s_soundFail, vol);
                ma_sound_start(&s_soundFail);
            }
        }

        void PlayToggleSound(bool on)
        {
            EnsureInit();
            if (!s_engineInited) return;
            // Respect global mute for button sounds
            if (LavenderHook::Globals::mute_buttons)
                return;

            float vol = (float)LavenderHook::Globals::sound_volume / 100.0f;
            bool polished = LavenderHook::Globals::use_polished_overlay;
            if (on) {
                if (polished) {
                    if (!s_onPolLoaded) return;
                    ma_sound_set_volume(&s_soundOnPol, vol);
                    ma_sound_start(&s_soundOnPol);
                } else {
                    if (!s_onLoaded) return;
                    ma_sound_set_volume(&s_soundOn, vol);
                    ma_sound_start(&s_soundOn);
                }
            } else {
                if (polished) {
                    if (!s_offPolLoaded) return;
                    ma_sound_set_volume(&s_soundOffPol, vol);
                    ma_sound_start(&s_soundOffPol);
                } else {
                    if (!s_offLoaded) return;
                    ma_sound_set_volume(&s_soundOff, vol);
                    ma_sound_start(&s_soundOff);
                }
            }
        }

        void PlayHideWindowSound()
        {
            EnsureInit();
            if (!s_engineInited) return;
            if (LavenderHook::Globals::mute_buttons) return;

            float vol = (float)LavenderHook::Globals::sound_volume / 100.0f;
            bool polished = LavenderHook::Globals::use_polished_overlay;
            if (polished) {
                if (!s_hidePolLoaded) return;
                ma_sound_set_volume(&s_soundHidePol, vol);
                ma_sound_start(&s_soundHidePol);
            } else {
                if (!s_hideLoaded) return;
                ma_sound_set_volume(&s_soundHide, vol);
                ma_sound_start(&s_soundHide);
            }
        }

        void SetVolumePercent(int pct) {
            if (pct < 0) pct = 0; if (pct > 100) pct = 100;
            LavenderHook::Globals::sound_volume = pct;
            float vol = (float)pct / 100.0f;
            if (s_onLoaded)   ma_sound_set_volume(&s_soundOn,   vol);
            if (s_offLoaded)  ma_sound_set_volume(&s_soundOff,  vol);
            if (s_failLoaded) ma_sound_set_volume(&s_soundFail, vol);
            if (s_hideLoaded) ma_sound_set_volume(&s_soundHide, vol);
            if (s_onPolLoaded)   ma_sound_set_volume(&s_soundOnPol,   vol);
            if (s_offPolLoaded)  ma_sound_set_volume(&s_soundOffPol,  vol);
            if (s_failPolLoaded) ma_sound_set_volume(&s_soundFailPol, vol);
            if (s_hidePolLoaded) ma_sound_set_volume(&s_soundHidePol, vol);
        }

        int GetVolumePercent() { return LavenderHook::Globals::sound_volume; }

    }
}
