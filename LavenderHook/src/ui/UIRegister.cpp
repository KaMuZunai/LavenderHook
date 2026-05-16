#include "../ui/UIRegister.h"
#include "../misc/Globals.h"
#include "../memory/aobutils/AobScanner.h"

#include "../windows/GeneralButtonsWindow.h"
#include "../windows/BuffingWindow.h"
#include "../windows/MiscButtonsWindow.h"
#include "../windows/GamepadWindow.h"
#include "../windows/PerformanceOverlayWindow.h"
#include "../windows/ToggleMenuWindow.h"
#include "../windows/ProfilesWindow.h"
#include "../ui/components/LavenderBackgroundDim.h"
#include "../ui/components/console.h"
#include "../windows/MenuLogoWindow.h"
#include "../windows/ParagonLevelWindow.h"
#include "../windows/WaveTrackerWindow.h"
#include "../windows/MacroManagerWindow.h"
#include "../windows/MacroEditorWindow.h"
#include "../windows/TravelWindow.h"
#include "../windows/WikiWindow.h"


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
            LavenderHook::UI::Windows::RenderMenuSelectorWindow(
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

}
