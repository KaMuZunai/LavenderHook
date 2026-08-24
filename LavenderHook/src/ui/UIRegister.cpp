#include "../ui/UIRegister.h"
#include "../misc/Globals.h"

#include "UIWindows/DebugWindow.h"
#include "UIWindows/InfoOverlayWindow.h"
#include "UIWindows/PerformanceOverlayWindow.h"
#include "UIWindows/SettingsWindow.h"
#include "../ui/components/LavenderBackgroundDim.h"
#include "UIWindows/console.h"
#include "UIWindows/MenuLogoWindow.h"
#include "UIWindows/ProfilesWindow.h"


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
            LavenderHook::UI::Windows::DebugWindow::UpdateActions();
        },
        [] {
            LavenderHook::UI::Windows::DebugWindow::Render(
                LavenderHook::Globals::show_menu &&
                LavenderHook::Globals::show_debug_window
            );
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
            LavenderHook::UI::Windows::RenderProfilesWindow(
                LavenderHook::Globals::show_menu &&
                LavenderHook::Globals::show_profiles_window
            );
        },
        nullptr
        });

}
