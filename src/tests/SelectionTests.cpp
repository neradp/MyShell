#include <windows.h>
#include <shlobj.h>
#include <wrl/client.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <thread>
#include "Include/ClassFactory.h"
#include "Include/ShellExtSelectionRetriever.h"
#include "Include/MenuToolbar.h"

using Microsoft::WRL::ComPtr;
volatile long g_cDllRef = 0;

static void check(bool value, const char *message)
{
	if(!value)
		throw std::runtime_error(message);
}

static void succeeded(HRESULT value, const char *message)
{
	if(FAILED(value))
	{
		std::ostringstream error;
		error << message << " (HRESULT 0x" << std::hex << std::uppercase
			<< std::setw(8) << std::setfill('0') << static_cast<unsigned long>(value) << ')';
		throw std::runtime_error(error.str());
	}
}

struct Popup
{
	HMENU handle = ::CreatePopupMenu();
	~Popup() { if(handle) ::DestroyMenu(handle); }
};

struct Pidl
{
	PIDLIST_ABSOLUTE value = nullptr;
	~Pidl() { ::CoTaskMemFree(value); }
};

static void runToolbarTests()
{
	using Nilesoft::Shell::MenuToolbar;
	MenuToolbar toolbar;
	check(!toolbar.enabled && toolbar.id == 0, "Toolbar must be opt-in");
	check(toolbar.move(-1, 1) == -1 && toolbar.command(0) == 0,
		"Unavailable toolbar operations must not dispatch");
	toolbar.buttons[0] = {101, true};
	toolbar.buttons[1] = {102, true};
	toolbar.buttons[2] = {103, false};
	toolbar.buttons[4] = {105, true};
	check(!toolbar.replaces(101, true, false), "Absent toolbar must preserve text commands");
	toolbar.id = 500;
	check(toolbar.replaces(101, true, false) && toolbar.replaces(103, true, false),
		"Active toolbar must replace native text commands including disabled actions");
	check(!toolbar.replaces(101, true, true) && !toolbar.replaces(101, false, false) &&
		!toolbar.replaces(999, true, false) && !toolbar.replaces(0, true, false),
		"Toolbar must preserve submenus, custom commands, unrelated actions and invalid IDs");
	check(toolbar.command(0) == 101 && toolbar.command(4) == 105,
		"Toolbar must preserve native command IDs");
	check(toolbar.command(2) == 0 && toolbar.command(-1) == 0 &&
		toolbar.command(MenuToolbar::count) == 0, "Disabled/outside buttons must not dispatch");
	check(toolbar.move(-1, 1) == 0 && toolbar.move(-1, -1) == 4 &&
		toolbar.move(1, 1) == 4 && toolbar.move(4, 1) == 0 &&
		toolbar.move(0, -1) == 4, "Keyboard navigation must skip disabled buttons and wrap");
	toolbar.keyboard = true;
	toolbar.selected = 0;
	check(toolbar.hover(1) && toolbar.hot == 1 && toolbar.selected == 1 && !toolbar.keyboard,
		"Mouse hover must move the active button and clear keyboard focus");
	check(!toolbar.hover(1), "Stationary hover must not request another repaint");
	toolbar.keyboard = true;
	check(toolbar.hover(1) && !toolbar.keyboard,
		"Returning to mouse input on the same button must clear keyboard focus");
	check(toolbar.hover(2) && toolbar.hot == 2 && toolbar.selected == 1 &&
		toolbar.command(toolbar.hot) == 0, "Disabled hover must not activate a command");
	check(toolbar.hover(4) && toolbar.selected == 4 && toolbar.command(toolbar.selected) == 105,
		"Moving between enabled buttons must update the eventual command");
	check(toolbar.hover(-1) && toolbar.hot == -1 && !toolbar.keyboard,
		"Leaving the toolbar must clear hover without showing a keyboard focus frame");

	HDC textDC = ::CreateCompatibleDC(nullptr);
	check(textDC != nullptr, "Create toolbar label measurement DC");
	for(int dpi : {96, 144, 192, 288})
	{
		HFONT textFont = ::CreateFontW(-::MulDiv(14, dpi, 96), 0, 0, 0, FW_NORMAL,
			FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
		check(textFont != nullptr, "Create toolbar label test font");
		HGDIOBJ oldFont = ::SelectObject(textDC, textFont);
		check(oldFont && oldFont != HGDI_ERROR, "Select toolbar label test font");
		auto scale = [dpi](int value) { return ::MulDiv(value, dpi, 96); };
		int width = scale(MenuToolbar::button_width) - 2 * scale(MenuToolbar::label_inset);
		int lineHeight = MenuToolbar::label_height(textDC, L"M", 1, width);
		const wchar_t *labels[] = {L"Premenova\u0165", L"VeryLongUnbrokenToolbarLabel"};
		for(auto label : labels)
		{
			int textHeight = MenuToolbar::label_height(textDC, label, -1, width);
			check(lineHeight > 0 && textHeight >= 2 * lineHeight,
				"Long toolbar labels must wrap even without spaces");
			int rowHeight = MenuToolbar::required_row_height(scale(MenuToolbar::height),
				scale(MenuToolbar::padding), scale(MenuToolbar::icon_top), scale(20),
				scale(MenuToolbar::label_gap), textHeight, scale(MenuToolbar::label_bottom));
			int labelTop = scale(MenuToolbar::padding) + scale(MenuToolbar::icon_top) +
				scale(20) + scale(MenuToolbar::label_gap);
			RECT bounds{0, labelTop, width,
				rowHeight - scale(MenuToolbar::padding) - scale(MenuToolbar::label_bottom)};
			int renderedHeight = ::DrawTextW(textDC, label, -1, &bounds, MenuToolbar::label_format);
			check(bounds.bottom - bounds.top >= textHeight && renderedHeight == textHeight,
				"Toolbar row must fit every wrapped label line at each DPI");
		}
		::SelectObject(textDC, oldFont);
		::DeleteObject(textFont);
	}
	::DeleteDC(textDC);

	for(int dpi : {96, 144, 192, 288})
	{
		int padding = ::MulDiv(MenuToolbar::padding, dpi, 96);
		int buttonWidth = ::MulDiv(MenuToolbar::button_width, dpi, 96);
		RECT row{-300, 70, -300 + ::MulDiv(560, dpi, 96),
			70 + ::MulDiv(MenuToolbar::height, dpi, 96)};
		for(bool rtl : {false, true})
		{
			int buttonsLeft = rtl ? row.right - padding - MenuToolbar::count * buttonWidth :
				row.left + padding;
			int buttonsRight = rtl ? row.right - padding :
				row.left + padding + MenuToolbar::count * buttonWidth;
			for(int i = 0; i < MenuToolbar::count; ++i)
			{
				auto button = MenuToolbar::button_rect(row, i, padding, buttonWidth, rtl);
				POINT center{(button.left + button.right) / 2, (button.top + button.bottom) / 2};
				check(MenuToolbar::hit_test(row, center, padding, buttonWidth, rtl) == i,
					"Drawn buttons and hit testing must agree at each DPI and in RTL");
				toolbar.hover(MenuToolbar::hit_test(row, center, padding, buttonWidth, rtl));
				check(toolbar.hot == i && !toolbar.keyboard,
					"Mouse motion must track each drawn button at every DPI and in RTL");
				check(button.left >= row.left + padding && button.right <= row.right - padding,
					"Buttons must fit the menu row");
				check(button.right - button.left == buttonWidth,
					"Toolbar buttons must retain a fixed width as the menu grows");
				if(!rtl)
					check(button.left == row.left + padding + i * buttonWidth,
						"LTR buttons must remain anchored to the left menu edge");
				else
					check(button.right == row.right - padding - i * buttonWidth,
						"RTL buttons must remain anchored to the right menu edge");

				if(i < MenuToolbar::count - 1)
				{
					int separator = MenuToolbar::separator_x(row, i, padding, buttonWidth, rtl);
					auto next = MenuToolbar::button_rect(row, i + 1, padding, buttonWidth, rtl);
					check(separator == (rtl ? next.right : button.right),
						"Vertical separators must sit between adjacent buttons");
				}
			}
			check(buttonsRight - buttonsLeft == MenuToolbar::count * buttonWidth &&
				row.right - row.left > buttonsRight - buttonsLeft + 2 * padding,
				"Toolbar buttons must not stretch to fill a wider menu");
			check(MenuToolbar::hit_test(row, {row.left, row.top}, padding, buttonWidth, rtl) == -1 &&
				MenuToolbar::hit_test(row, {row.right, row.bottom}, padding, buttonWidth, rtl) == -1,
				"Toolbar padding and outside clicks must not select a command");
			auto first = MenuToolbar::button_rect(row, 0, padding, buttonWidth, rtl);
			auto second = MenuToolbar::button_rect(row, 1, padding, buttonWidth, rtl);
			POINT shared{rtl ? first.left : first.right, first.top + 1};
			check((rtl ? second.right == first.left : first.right == second.left) &&
				MenuToolbar::hit_test(row, shared, padding, buttonWidth, rtl) == (rtl ? 0 : 1),
				"Shared edges must belong to exactly one button");
		}
	}
	check(MenuToolbar::hit_test({0, 0, 8, 8}, {4, 4}, 4, 16, false) == -1,
		"Collapsed toolbar must not select a command");

	{
		RECT row{0, 0, 600, 100};
		int dividerHeight = 1 + 4;
		RECT content = MenuToolbar::content_rect(row, dividerHeight);
		check(content.bottom == row.bottom - dividerHeight && content.top == row.top &&
			content.left == row.left && content.right == row.right,
			"Divider zone must be reserved below the button area only");
		auto button = MenuToolbar::button_rect(content, 0, MenuToolbar::padding, 80, false);
		check(button.bottom == content.bottom - MenuToolbar::padding &&
			button.bottom < row.bottom - dividerHeight,
			"Buttons must end above the divider line");
		auto visual = MenuToolbar::visual_rect(button, 4);
		int separator = MenuToolbar::separator_x(content, 0, MenuToolbar::padding, 80, false);
		check(visual.left == button.left + 4 && visual.right == button.right - 4 &&
			visual.top == button.top && visual.bottom == button.bottom &&
			separator - visual.right == 4,
			"Button highlight must keep a gap from the vertical separators");
		RECT narrow{0, 0, 6, 20};
		RECT narrowVisual = MenuToolbar::visual_rect(narrow, 4);
		RECT gapless = MenuToolbar::visual_rect(button, 0);
		check(::EqualRect(&narrowVisual, &narrow) && ::EqualRect(&gapless, &button),
			"Gaps must not collapse narrow or gapless buttons");
		check(MenuToolbar::hit_test(content, {40, row.bottom - 2}, MenuToolbar::padding,
			80, false) == -1,
			"Clicks on the divider gap must not select a command");
		check(MenuToolbar::content_rect(row, 0).bottom == row.bottom &&
			MenuToolbar::content_rect({0, 0, 600, 4}, 10).bottom == 4,
			"Missing or oversized divider zones must leave the row intact");
		toolbar.row_height = 80;
		toolbar.divider_height = dividerHeight;
		check(toolbar.total_height() == 85, "Menu row must include the divider zone");
	}
}

static void runTests()
{
	runToolbarTests();
	ComPtr<IClassFactory> factory;
	check(ClassFactory::CreateClassObject(IID_IClassFactory, nullptr) == E_POINTER,
		"Null exported factory output");
	void *unsupportedFactory = reinterpret_cast<void *>(1);
	check(ClassFactory::CreateClassObject(IID_IStream, &unsupportedFactory) == E_NOINTERFACE &&
		!unsupportedFactory && g_cDllRef == 0, "Failed factory query must release the factory");
	succeeded(ClassFactory::CreateClassObject(IID_PPV_ARGS(factory.GetAddressOf())), "Create class factory");
	check(g_cDllRef == 1, "Factory must hold a DLL reference");
	check(factory->QueryInterface(IID_IUnknown, nullptr) == E_POINTER, "Null QI output");
	void *unsupported = factory.Get();
	check(factory->QueryInterface(IID_IStream, &unsupported) == E_NOINTERFACE && !unsupported,
		"Unsupported QI must clear output");
	check(factory->CreateInstance(nullptr, IID_IContextMenu, nullptr) == E_POINTER, "Null creation output");
	check(factory->CreateInstance(factory.Get(), IID_IContextMenu, &unsupported) == CLASS_E_NOAGGREGATION &&
		!unsupported, "Aggregation must fail and clear output");
	succeeded(factory->LockServer(TRUE), "LockServer");
	check(g_cDllRef == 2, "Server lock");
	succeeded(factory->LockServer(FALSE), "UnlockServer");

	ComPtr<IContextMenu> first;
	ComPtr<IShellExtInit> firstInit;
	succeeded(factory->CreateInstance(nullptr, IID_PPV_ARGS(first.GetAddressOf())), "Create handler");
	succeeded(first.As(&firstInit), "Query IShellExtInit");
	check(g_cDllRef == 2, "Handler must hold a DLL reference");
	check(firstInit->Initialize(nullptr, nullptr, nullptr) == E_INVALIDARG, "Invalid selection must fail");
	Popup firstMenu;
	Popup secondMenu;
	check(firstMenu.handle && secondMenu.handle, "Create menus");
	succeeded(first->QueryContextMenu(firstMenu.handle, 0, 100, 100, 0), "Uninitialized query");
	ShellSelectionContext selection;
	check(!ShellExtSelectionRetriever::TakeSelection(firstMenu.handle, selection), "No invalid context");

	Pidl desktop;
	Pidl documents;
	succeeded(::SHGetKnownFolderIDList(FOLDERID_Desktop, 0, nullptr, &desktop.value), "Desktop PIDL");
	succeeded(::SHGetKnownFolderIDList(FOLDERID_Documents, 0, nullptr, &documents.value), "Documents PIDL");
	succeeded(firstInit->Initialize(desktop.value, nullptr, nullptr), "Initialize background");
	auto result = first->QueryContextMenu(firstMenu.handle, 0, 100, 100, 0);
	check(result == MAKE_HRESULT(SEVERITY_SUCCESS, 0, 1),
		"Handler must reserve an ID so Shell hosts retain it until popup capture");
	check(::GetMenuItemCount(firstMenu.handle) == 0, "No visible placeholder");
	check(!ShellExtSelectionRetriever::TakeSelection(secondMenu.handle, selection), "Menu isolation");
	bool wrongThreadCaptured = false;
	std::thread other([&]()
	{
		ShellSelectionContext otherSelection;
		wrongThreadCaptured = ShellExtSelectionRetriever::TakeSelection(firstMenu.handle, otherSelection);
	});
	other.join();
	check(!wrongThreadCaptured, "Thread isolation");
	check(ShellExtSelectionRetriever::TakeSelection(firstMenu.handle, selection), "Capture background");
	check(selection.background && selection.parent && !selection.items, "Background snapshot");
	check(!ShellExtSelectionRetriever::TakeSelection(firstMenu.handle, selection), "One-shot capture");
	selection = {};
	check(first->QueryContextMenu(firstMenu.handle, 0, 100, 100, CMF_DEFAULTONLY) ==
		MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0), "Default-only query must reserve no IDs");
	check(!ShellExtSelectionRetriever::TakeSelection(firstMenu.handle, selection), "No default-only capture");

	ComPtr<IContextMenu> second;
	ComPtr<IShellExtInit> secondInit;
	succeeded(factory->CreateInstance(nullptr, IID_PPV_ARGS(second.GetAddressOf())), "Create second handler");
	succeeded(second.As(&secondInit), "Second IShellExtInit");
	succeeded(secondInit->Initialize(documents.value, nullptr, nullptr), "Second background");
	succeeded(first->QueryContextMenu(firstMenu.handle, 0, 100, 100, 0), "First association");
	succeeded(second->QueryContextMenu(secondMenu.handle, 0, 100, 100, 0), "Second association");
	ShellSelectionContext firstSelection;
	ShellSelectionContext secondSelection;
	check(ShellExtSelectionRetriever::TakeSelection(firstMenu.handle, firstSelection), "First isolated snapshot");
	check(ShellExtSelectionRetriever::TakeSelection(secondMenu.handle, secondSelection), "Second isolated snapshot");
	int comparison = 0;
	succeeded(firstSelection.parent->Compare(secondSelection.parent.Get(), SICHINT_CANONICAL, &comparison),
		"Compare snapshots");
	check(comparison != 0, "Different handlers must retain different contexts");

	Pidl documentsParent;
	documentsParent.value = ::ILCloneFull(documents.value);
	check(documentsParent.value && ::ILRemoveLastID(documentsParent.value), "Create selection parent PIDL");
	PCUITEMID_CHILD child = ::ILFindLastID(documents.value);
	check(child && child->mkid.cb != 0, "Create parent-relative selection PIDL");
	ComPtr<IDataObject> data;
	succeeded(::SHCreateDataObject(documentsParent.value, 1, &child, nullptr, IID_PPV_ARGS(data.GetAddressOf())),
		"Create real Shell data object");
	succeeded(firstInit->Initialize(nullptr, data.Get(), nullptr), "Initialize selected object");
	succeeded(first->QueryContextMenu(firstMenu.handle, 0, 100, 100, 0), "Selection association");
	check(ShellExtSelectionRetriever::TakeSelection(firstMenu.handle, selection), "Capture selected object");
	DWORD count = 0;
	check(!selection.background && selection.items, "Selection snapshot");
	succeeded(selection.items->GetCount(&count), "Selection count");
	check(count == 1, "Selection must contain exactly one item");
	ComPtr<IShellItem> selectedItem;
	ComPtr<IShellItem> expectedItem;
	ComPtr<IShellItem> expectedParent;
	succeeded(selection.items->GetItemAt(0, selectedItem.GetAddressOf()), "Read selected item");
	succeeded(::SHCreateItemFromIDList(documents.value, IID_PPV_ARGS(expectedItem.GetAddressOf())),
		"Create expected selected item");
	succeeded(::SHCreateItemFromIDList(documentsParent.value, IID_PPV_ARGS(expectedParent.GetAddressOf())),
		"Create expected selection parent");
	succeeded(selectedItem->Compare(expectedItem.Get(), SICHINT_CANONICAL, &comparison),
		"Compare selected item");
	check(comparison == 0, "Selection must preserve the original object");
	check(selection.parent.Get() != nullptr, "Selection must retain its parent");
	succeeded(selection.parent->Compare(expectedParent.Get(), SICHINT_CANONICAL, &comparison),
		"Compare selection parent");
	check(comparison == 0, "Selection must preserve the correct parent");

	succeeded(first->QueryContextMenu(firstMenu.handle, 0, 100, 100, 0), "Associate before failed reinit");
	check(firstInit->Initialize(nullptr, nullptr, nullptr) == E_INVALIDARG, "Failed reinit");
	check(!ShellExtSelectionRetriever::TakeSelection(firstMenu.handle, selection), "No stale association");
	check(first->InvokeCommand(nullptr) == E_INVALIDARG, "No invokable helper command");

	succeeded(second->QueryContextMenu(secondMenu.handle, 0, 100, 100, 0), "Associate before destruction");
	secondInit.Reset();
	second.Reset();
	check(!ShellExtSelectionRetriever::TakeSelection(secondMenu.handle, selection), "Destroyed handler removed");
}

int wmain()
{
	auto result = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if(FAILED(result))
	{
		std::cerr << "COM initialization failed\n";
		return 1;
	}
	int exitCode = 0;
	try
	{
		runTests();
		check(g_cDllRef == 0, "All COM references must be released");
		std::cout << "Selection tests passed\n";
	}
	catch(const std::exception &error)
	{
		std::cerr << error.what() << '\n';
		exitCode = 1;
	}
	::CoUninitialize();
	return exitCode;
}
