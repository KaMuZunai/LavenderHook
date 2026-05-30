#pragma once

namespace LavenderHook {
    namespace UI {
        namespace Functions {

            // Public tick entrypoints
            void TickButton21(bool enabled);  // Flash Buff
            void TickButton22(bool enabled);  // Alt Flash Buff
            void TickButton23(bool enabled);  // Pet Boost
            void TickButton24(bool enabled);  // Wrath Form
            void TickButton25(bool enabled);  // Overcharge
            void TickButton26(bool enabled);  // Defense Boost

            // Auto F (Flash Buff)
            void SetAutoFTiming(int intervalMs);
            int  GetAutoFIntervalMs();
            void SetAutoFKey(int vk);
            int  GetAutoFKey();

            // Alt Flash Buff
            void SetAltAutoFTiming(int intervalMs);
            int  GetAltAutoFIntervalMs();
            void SetAltAutoFKey(int vk);
            int  GetAltAutoFKey();

            // Pet Boost
            void SetPetBoostKey(int vk);
            int  GetPetBoostKey();
            void SetPetBoostCooldownMs(int ms);
            int  GetPetBoostCooldownMs();
            void SetPetBoostMinMana(int v);
            int  GetPetBoostMinMana();
            void SetPetBoostMaxMana(int v);
            int  GetPetBoostMaxMana();

            // Wrath Form
            void SetWrathFormKey(int vk);
            int  GetWrathFormKey();
            void SetWrathFormCooldownMs(int ms);
            int  GetWrathFormCooldownMs();
            void SetWrathFormMinMana(int v);
            int  GetWrathFormMinMana();
            void SetWrathFormMaxMana(int v);
            int  GetWrathFormMaxMana();

            // Overcharge
            void SetOverchargeKey(int vk);
            int  GetOverchargeKey();
            void SetOverchargeSpamKey(int vk);
            int  GetOverchargeSpamKey();
            void SetOverchargeCooldownMs(int ms);
            int  GetOverchargeCooldownMs();
            void SetOverchargeMinMana(int v);
            int  GetOverchargeMinMana();
            void SetOverchargeMaxMana(int v);
            int  GetOverchargeMaxMana();
            void SetOverchargeSpamDelayMs(int ms);
            int  GetOverchargeSpamDelayMs();

            // Defense Boost
            void SetDefenseBoostKey(int vk);
            int  GetDefenseBoostKey();
            void SetDefenseBoostIntervalMs(int ms);
            int  GetDefenseBoostIntervalMs();

        } // namespace Functions
    } // namespace UI
} // namespace LavenderHook
