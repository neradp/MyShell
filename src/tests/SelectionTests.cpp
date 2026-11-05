#include <windows.h>
#include <shlobj.h>
#include <wrl/client.h>
#include <iostream>
#include <stdexcept>
#include <thread>
#include "Include/ClassFactory.h"
#include "Include/ShellExtSelectionRetriever.h"

using Microsoft::WRL::ComPtr;
volatile long g_cDllRef = 0;

static void check(bool value, const char *message)
{
	if(!value)
		throw std::runtime_error(message);
}

static void succeeded(HRESULT value, const char *message)
{
	check(SUCCEEDED(value), message);
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

static void runTests()
{
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
	check(result == MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0), "Handler must reserve zero IDs");
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
	succeeded(first->QueryContextMenu(firstMenu.handle, 0, 100, 100, CMF_DEFAULTONLY), "Default-only query");
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

	PCUITEMID_CHILD child = documents.value;
	ComPtr<IDataObject> data;
	succeeded(::SHCreateDataObject(nullptr, 1, &child, nullptr, IID_PPV_ARGS(data.GetAddressOf())),
		"Create real Shell data object");
	succeeded(firstInit->Initialize(nullptr, data.Get(), nullptr), "Initialize selected object");
	succeeded(first->QueryContextMenu(firstMenu.handle, 0, 100, 100, 0), "Selection association");
	check(ShellExtSelectionRetriever::TakeSelection(firstMenu.handle, selection), "Capture selected object");
	DWORD count = 0;
	check(!selection.background && selection.items, "Selection snapshot");
	succeeded(selection.items->GetCount(&count), "Selection count");
	check(count == 1, "Selection must contain exactly one item");

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
