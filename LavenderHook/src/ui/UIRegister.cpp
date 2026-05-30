#include "../ui/UIRegister.h"
#include "../misc/Globals.h"
#include "../memory/aobutils/AobScanner.h"

#include "UIWindows/GeneralButtonsWindow.h"
#include "UIWindows/BuffingWindow.h"
#include "UIWindows/MiscButtonsWindow.h"
#include "UIWindows/GamepadWindow.h"
#include "UIWindows/PerformanceOverlayWindow.h"
#include "UIWindows/SettingsWindow.h"
#include "UIWindows/ProfilesWindow.h"
#include "../ui/components/LavenderBackgroundDim.h"
#include "../ui/components/console.h"
#include "UIWindows/MenuLogoWindow.h"
#include "UIWindows/ParagonLevelWindow.h"
#include "UIWindows/WaveTrackerWindow.h"
#include "UIWindows/MacroManagerWindow.h"
#include "UIWindows/MacroEditorWindow.h"
#include "UIWindows/TravelWindow.h"
#include "UIWindows/WikiWindow.h"
#include "UIWindows/InfoOverlayWindow.h"
#include "../webhook/WebhookManager.h"


void RegisterUIWindows()
{
    auto& ui = UIRegistry::Get();

    static LavenderHook::UI::LavenderBackgroundDim s_menuDim;

    ui.Register(UIWindowEntry{
        [] {
            s_menuDim.Tick(LavenderHook::Globals::show_menu);
        },
        [] {
            s_menuDim.Render();
        },
        nullptr
        });

    ui.Register(UIWindowEntry{
        [] {
            LavenderHook::Memory::Tick();
            LavenderHook::UI::Windows::GeneralButtonsWindow::UpdateActions();
        },
        [] {
            LavenderHook::UI::Windows::GeneralButtonsWindow::Render(
                LavenderHook::Globals::show_menu &&
                LavenderHook::Globals::show_general_window
            );
        },
        nullptr
        });

    ui.Register(UIWindowEntry{
        [] {
            LavenderHook::UI::Windows::BuffingWindow::UpdateActions();
        },
        [] {
            LavenderHook::UI::Windows::BuffingWindow::Render(
                LavenderHook::Globals::show_menu &&
                LavenderHook::Globals::show_buffing_window
            );
        },
        nullptr
        });

    ui.Register(UIWindowEntry{
        [] {
            LavenderHook::UI::Windows::MiscButtonsWindow::UpdateActions();
        },
        [] {
            LavenderHook::UI::Windows::MiscButtonsWindow::Render(
                LavenderHook::Globals::show_menu &&
                LavenderHook::Globals::show_misc_window
            );
        },
        nullptr
        });

    ui.Register(UIWindowEntry{
        [] {
            LavenderHook::UI::Windows::MacroManagerWindow::UpdateActions();
        },
        [] {
            LavenderHook::UI::Windows::MacroManagerWindow::Render(
                LavenderHook::Globals::show_menu &&
                LavenderHook::Globals::show_macro_window
            );
        },
        nullptr
        });

    ui.Register(UIWindowEntry{
        [] {
            LavenderHook::UI::Windows::MacroEditorWindow::UpdateActions();
        },
        [] {
            LavenderHook::UI::Windows::MacroEditorWindow::Render(
                LavenderHook::Globals::show_menu &&
                LavenderHook::Globals::show_macro_window
            );
        },
        nullptr
        });

    ui.Register(UIWindowEntry{
        nullptr,
        [] {
            LavenderHook::UI::Windows::GamepadWindow::Render(
                LavenderHook::Globals::show_menu &&
                LavenderHook::Globals::show_gamepad_window
            );
        },
        nullptr
        });

    ui.Register(UIWindowEntry{
        nullptr,
        [] {
            LavenderHook::UI::Windows::PerformanceOverlayWindow::Render();
        },
        nullptr
        });

    ui.Register(UIWindowEntry{
        nullptr,
        [] {
            LavenderHook::UI::Windows::RenderSettingsWindow(
                LavenderHook::Globals::show_menu &&
                LavenderHook::Globals::show_menu_selector_window
            );
        },
        nullptr
        });

    ui.Register(UIWindowEntry{
        nullptr,
        [] {
            LavenderConsole::GetInstance().Render(
                LavenderHook::Globals::show_menu &&
                LavenderHook::Globals::show_console
            );
        },
        nullptr
        });

    ui.Register(UIWindowEntry{
        nullptr,
        [] {
            LavenderHook::UI::Windows::ImageDragWindow::Render(
                LavenderHook::Globals::show_menu &&
                LavenderHook::Globals::show_menu_logo
            );
        },
        nullptr
        });

    ui.Register(UIWindowEntry{
        nullptr,
        [] {
            LavenderHook::UI::Windows::ParagonLevelWindow::Render(
                LavenderHook::Globals::show_paragon_level_window
            );
        },
        nullptr
        });

    ui.Register(UIWindowEntry{
        nullptr,
        [] {
            LavenderHook::UI::Windows::ProfilesWindow::Render(
                LavenderHook::Globals::show_menu &&
                LavenderHook::Globals::show_profiles_window
            );
        },
        nullptr
        });

    ui.Register(UIWindowEntry{
        nullptr,
        [] {
            LavenderHook::UI::Windows::WaveTrackerWindow::Render();
        },
        nullptr
        });

    ui.Register(UIWindowEntry{
        [] {
            LavenderHook::UI::Windows::WikiWindow::Update();
        },
        [] {
            LavenderHook::UI::Windows::WikiWindow::Render(
                LavenderHook::Globals::show_menu &&
                LavenderHook::Globals::show_wiki_window
            );
        },
        nullptr
        });

    ui.Register(UIWindowEntry{
        nullptr,
        [] {
            LavenderHook::UI::Windows::WikiWindow::RenderFavoriteOverlay();
        },
        nullptr
        });

    ui.Register(UIWindowEntry{
        nullptr,
        [] {
            LavenderHook::UI::Windows::InfoOverlayWindow::Render();
        },
        nullptr
        });

    ui.Register(UIWindowEntry{
        [] {
            LavenderHook::Webhook::Update();
        },
        nullptr,
        nullptr
        });

}
