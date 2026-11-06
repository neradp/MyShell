#include <pch.h>
#include "Include/ContextMenu.h"
#include "Library/detours.h"
#include "RegistryConfig.h"
#include <UIAutomation.h>
#include "Include/ClassFactory.h"
using namespace Nilesoft::Shell;
using namespace Diagnostics;

#ifdef _DEBUG
#include <crtdbg.h>
#endif

#pragma comment(lib, "UxTheme.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "WindowsCodecs.lib")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Shcore.lib")
#pragma comment(lib, "plutosvg-x64.lib")
#pragma comment(lib, "detours-x64.lib")

#pragma region Entry Point

HINSTANCE _hInstance{};
volatile long g_cDllRef = 0;
Initializer _initializer;
extern Logger &_log = Logger::Instance();

WindowsHook _taskbar_mouse;

IATHook iathook_NtUserTrackPopupMenuEx;
std::vector<IATHook> iathook_TrackPopupMenu;
Detours<decltype(::CoCreateInstance)> _CoCreateInstance;

std::unordered_map<HWND, Window> _window_taskbar;

LRESULT __stdcall TaskbarProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

__inline auto is_registered() -> bool
{
	return RegistryConfig::IsRegistered();
}

struct
{
	string path;
	bool explorer{};
	bool contextmenuhandler{};
	HMODULE handle{};

	bool init()
	{
		handle = ::GetModuleHandleW(nullptr);
		if(!(explorer) && path.empty())
		{
			path = Path::Module(nullptr);
			explorer = path.ends_with(def_EXPLORER, true);
		}
		return explorer;
	}

	explicit operator bool() const { return explorer; }

} _loader;

struct taskbar_t
{
	inline static IComPtr<IUIAutomation> _IUIAutomation;
	inline static auto wShell_TrayWnd = ::RegisterWindowMessageW(Windows::WC_Shell_TrayWnd);
	inline static auto wShell_SecondaryTrayWnd = ::RegisterWindowMessageW(Windows::WC_Shell_SecondaryTrayWnd);

	static void hook_all()
	{
		::EnumWindows([](HWND hWnd, LPARAM)->BOOL {
			auto atom = Window::Atom(hWnd);
			if(atom == wShell_TrayWnd || atom == wShell_SecondaryTrayWnd)
			{
				Window window = hWnd;
				if(!window.is_prop(UxSubclass))
				{
					if(!_window_taskbar.contains(window))
					{
						auto proc = window.get_long(GWLP_WNDPROC);
						window.prop(UxSubclass, proc);
						window.set_long(GWLP_WNDPROC, TaskbarProc);
						_window_taskbar[window] = window;
						return true;
					}
				}
			}
			return TRUE;
		}, 0);
	}

	static void hook_all(uint32_t delay)
	{
		::SetTimer(::GetShellWindow(), 0, delay, [](HWND hWnd, UINT, UINT_PTR id, DWORD)
		{						
			static int atttempt = 0;
			if(atttempt++ >= 10)
				::KillTimer(hWnd, id);
			
			taskbar_t::hook_all();
		});
	}
	
	static void unhook(HWND hWnd)
	{
		for(auto &window : _window_taskbar)
		{
			if(window.first == hWnd)
			{
				window.second.set_long(GWLP_WNDPROC, window.second.prop(UxSubclass));
				window.second.remove_prop(UxSubclass);
				_window_taskbar.erase(hWnd);
				break;
			}
		}
	}
	static void unhook_all()
	{
		for(auto &window : _window_taskbar)
		{
			window.second.set_long(GWLP_WNDPROC, window.second.prop(UxSubclass));
			window.second.remove_prop(UxSubclass);
		}
		_window_taskbar.clear();
	}

	static bool is_allowed_area(HWND hTaskbar, const Point &pt)
	{
		bool ret = false;

		if(!_IUIAutomation)
			_IUIAutomation.CreateInstance(__uuidof(CUIAutomation), CLSCTX_INPROC_SERVER);

		if(_IUIAutomation)
		{
			IComPtr<IUIAutomationElement> pIUIAutomationElement;
			if(SUCCEEDED(_IUIAutomation->ElementFromPoint(pt, pIUIAutomationElement)) && pIUIAutomationElement)
			{
				struct elem_t {
					BSTR type = nullptr;
					BSTR name = nullptr;
					BSTR id = nullptr;
					~elem_t() {
						freeString(type);
						freeString(name);
						freeString(id);
					}
					void freeString(BSTR s) {
						if(s) ::SysFreeString(s);
					}
				} elem;
				
				pIUIAutomationElement->get_CurrentAutomationId(&elem.id);
				pIUIAutomationElement->get_CurrentClassName(&elem.type);
				pIUIAutomationElement->get_CurrentName(&elem.name);
				/*
				id=[TaskbarFrame], type=[Taskbar.TaskbarFrameAutomationPeer], name=[]
				id=[StartButton], type=[ToggleButton], name=[Start]
				id=[SearchButton], type=[ToggleButton], name=[Search]
				id=[SystemTrayIcon], type=[SystemTray.NormalButton], name=[Show Hidden Icons]
				id=[SystemTrayIcon], type=[SystemTray.AccentButton], name=[Network wifi-1 Internet access]
				id=[SystemTrayIcon], type=[SystemTray.OmniButton], name=[Clock 7:04 PM 3/20/2023]
				id=[SystemTrayIcon], type=[SystemTray.ShowDesktopButton], name=[Show Desktop]
				*/
				if(elem.name && elem.type && elem.id)
				{
					if(!::lstrcmpiW(elem.id, L"TaskbarFrame"))
						ret = !::lstrcmpiW(elem.name, L"") && !::lstrcmpiW(elem.type, L"Taskbar.TaskbarFrameAutomationPeer");
					else if(is_secondary(hTaskbar))
					{
						if(!::lstrcmpiW(elem.id, L"SystemTrayIcon"))
							ret = !::lstrcmpiW(elem.type, L"SystemTray.OmniButton");
					}
				}
			}
		}
		return ret;
	}

	static bool is_primary(HWND hTaskbar)
	{
		if(hTaskbar)
		{
			Window taskbar = hTaskbar;
			if(taskbar.hash() == WindowClass::WC_Shell_TrayWnd)
			{
				// Only the primary taskbar has a RebarWindow32
				return taskbar.find(Windows::WC_ReBarWindow32);
			}
		}
		return false;
	}

	static bool is_secondary(HWND hTaskbar)
	{
		return hTaskbar && (Window::class_hash(hTaskbar) == WindowClass::WC_Shell_SecondaryTrayWnd);
	}
};

HRESULT __stdcall CoCreateInstanceHook(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID *ppv)
{
	auto is_local_server = (dwClsContext & CLSCTX_LOCAL_SERVER) != 0;
	auto is_inproc_server = (dwClsContext & CLSCTX_INPROC_SERVER) != 0;

	if(_initializer.cache && (is_inproc_server || is_local_server))
	{
		auto hr = E_NOINTERFACE;
		auto process = true;

		if(rclsid == IID_FileExplorerContextMenu)
		{
			if(_initializer.cache->settings.priority)
			{
				Context context;
				Object obj = context.Eval(_initializer.cache->settings.priority).move();
				if(obj.is_number())
					process = obj;
			}

			if(process)
				return hr;
		}
		else
		{
			auto is_UWP = false;
			process = false;
			if(is_inproc_server)
				process = riid == IID_IContextMenu || riid == IID_IContextMenu2 || riid == IID_IContextMenu3 || riid == IID_IExplorerCommand;
			else
			{
				process = riid == IID_IExplorerCommand;
				is_UWP = is_local_server && process;
			}

			if(process)
			{
				Context context;
				this_item _this; context._this = &_this;
				_this.is_uwp = is_UWP;
				_this.clsid = Guid(rclsid).to_string(2);

				bool test = Keyboard::IsKeyDown(VK_MENU);
				if(test)
				{
					Timer t;
					t.start();
					hr = _CoCreateInstance.invoke(rclsid, pUnkOuter, dwClsContext, riid, ppv);
					t.stop();
					auto elapsed = (int)t.elapsed_milliseconds();
					_log.write(L"%d%s\t%s\t%s\r\n", elapsed, elapsed > 0 ? L"ms" : L"" , _this.clsid.c_str(), is_UWP ? L"UWP":L"");
					return hr;
				}

				for(auto si : _initializer.cache->statics)
				{
					if(si->clsid.empty() || (si->where && !context.eval_bool(si->where)))
						continue;

					if(si->has_clsid)
					{
						for(auto &id : si->clsid)
						{
							if(id.equals(rclsid))
							{
								if(test && *ppv)
								{
									((IUnknown *)*ppv)->Release();
									*ppv = nullptr;
								}
								return E_NOINTERFACE;
							}
						}
					}
				}

				if(test)
					return hr;
			}
		}
	}

	return _CoCreateInstance.invoke(rclsid, pUnkOuter, dwClsContext, riid, ppv);
}

bool is_in_taskbar;

BOOL WINAPI TrackPopupMenuProc(HMENU hMenu, uint32_t uFlags, int x, int y, int nReserved, HWND hWnd, const RECT *prcRect);
BOOL WINAPI TrackPopupMenuExProc(HMENU hMenu, uint32_t uFlags, int x, int y, HWND hWnd, LPTPMPARAMS lptpm);

BOOL WINAPI NtUserTrackPopupMenu(HMENU hMenu, uint32_t uFlags, int x, int y, HWND hWnd, LPTPMPARAMS lptpm, int tfunc)
{
	auto result = FALSE;
	auto invoked = false;

	auto invoke = [&](HMENU hmenu, uint32_t flag, Point pt)
	{
		if(!invoked)
		{
			if(tfunc == 0)
				result = ::TrackPopupMenu(hmenu, flag, pt.x, pt.y, 0, hWnd, nullptr);
			else if(iathook_NtUserTrackPopupMenuEx.installed())
				result = iathook_NtUserTrackPopupMenuEx.invoke<decltype(TrackPopupMenuExProc)>(hmenu, flag, pt.x, pt.y, hWnd, lptpm);
			else
				result = ::TrackPopupMenuEx(hmenu, flag, pt.x, pt.y, hWnd, lptpm);
			invoked = true;
		}
	};

	__try
	{
		__try
		{

			if(!is_registered())
			{
				__trace(L"TrackPopupMenu unregistered");
				__leave;
			}

			auto has_inited = Initializer::OnState(is_in_taskbar);

			if(!_initializer.Status.Disabled)
			{
				// is injected from explorer
				if(!_initializer.Status.Loaded)
				{
					if(_initializer.has_error() || has_inited)
						__leave;
					_initializer.init();
				}

				auto ctx = ContextMenu::CreateAndInitialize(hWnd, hMenu, { x, y }, _loader.explorer, _loader.contextmenuhandler);
				if(ctx != nullptr)
				{
					Flag<uint32_t> flag(uFlags);
					flag.remove(TPM_NONOTIFY);

					if(ctx->is_layoutRTL == 1)
						flag.add(TPM_LAYOUTRTL);
					else if(ctx->is_layoutRTL == 0)
						flag.remove(TPM_LAYOUTRTL);

					auto z = ctx->_theme.border.size + ctx->_theme.border.size;

					if(ctx->Selected.Window.parent == WINDOW_TASKBAR)
					{
						HWND hTaskbar = hWnd;
						if(ctx->Selected.Window.id == WINDOW_START)
							hTaskbar = ::WindowFromPoint({ x, y });
						else
							hTaskbar = ::GetAncestor(hWnd, GA_ROOTOWNER);

						if(!hTaskbar)
							hTaskbar = hWnd;

						Monitor monitor(hTaskbar);
						monitor.info();

						Rect rcW = monitor.rcWork;
						Rect rcM = monitor.rcMonitor;
						Rect rc = hTaskbar;

						if(ctx->Selected.Window.id == WINDOW_START)
						{
							if(rc.top > 0)
								y = (rc.top + 5) - (z + ctx->_theme.border.padding.height());
							else if(rc.left == 0)
								y = rc.bottom;
							else
								x = rc.left;
						}
						else
						{
							if(rcM.top == rc.top && rcM.bottom == rc.bottom) // Vertical
							{
								if(rcM.left == rc.left)
									x = rc.right;
								else if(rcM.right == rc.right)
									x = rc.left - 20;
							}
							else if(rcM.left == rc.left && rcM.right == rc.right) // Horizontal
							{
								if(rcM.top == rc.top)
									y = rc.bottom;
								else if(rcM.bottom == rc.bottom)
									y = (rc.top + 5) - (z + ctx->_theme.border.padding.height());
							}
						}
					}
					else
					{
						y -= z;
					}
					
					invoke(ctx->MenuHandle(), flag, { x, y });
					result = ctx->InvokeCommand(result);
					__leave;
				}
			}
		}
		except
		{
#ifdef _DEBUG
			_log.exception(__func__);
#endif
		}
	}
	__finally
	{
		invoke(hMenu, uFlags, { x, y });
		_loader.contextmenuhandler = false;
		is_in_taskbar = false;
		return result;
	}
}

BOOL WINAPI TrackPopupMenuProc(HMENU hMenu, uint32_t uFlags, int x, int y, [[maybe_unused]] int nReserved, HWND hWnd, [[maybe_unused]] const RECT *prcRect)
{
	return NtUserTrackPopupMenu(hMenu, uFlags, x, y, hWnd, nullptr, 0);
}

BOOL WINAPI TrackPopupMenuExProc(HMENU hMenu, uint32_t uFlags, int x, int y, HWND hWnd, LPTPMPARAMS lptpm)
{
	return NtUserTrackPopupMenu(hMenu, uFlags, x, y, hWnd, lptpm, 1);
}

bool ShowTaskbarContextMenu(HWND hTaskbar, const Point &pt, uint32_t uMsg)
{
	auto ret = false;
	{
		struct menu {
			HMENU handle{};
			menu() { handle = ::CreatePopupMenu(); }
			~menu() { if(handle) ::DestroyMenu(handle); }
		}_menu;

		if(_menu.handle)
		{
			is_in_taskbar = true;
			auto uFlags = TPM_RETURNCMD | TPM_RIGHTBUTTON | (::GetSystemMetrics(SM_MIDEASTENABLED) ? TPM_LAYOUTRTL : 0);
			if(uMsg == WM_CONTEXTMENU)
			{
				TrackPopupMenuExProc(_menu.handle, uFlags, pt.x, pt.y, hTaskbar, nullptr);
			}
			else 
			{
				iathook_NtUserTrackPopupMenuEx.invoke<decltype(TrackPopupMenuExProc)>(_menu.handle, uFlags, 0, 0, hTaskbar, nullptr);
			}
			::PostMessageW(hTaskbar, WM_NULL, 0, 0); // send benign message to window to make sure the menu goes away.
			ret = true;
		}
	}
	return ret;
}

LRESULT __stdcall TaskbarProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	auto wndproc = (WNDPROC)::GetPropW(hWnd, UxSubclass);
	
	if(wndproc == nullptr)
		return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);

	if(uMsg == WM_CONTEXTMENU)
	{
		auto pt = Point::CursorPos();
		if(taskbar_t::is_allowed_area(hWnd, pt))
		{
			if(ShowTaskbarContextMenu(hWnd, pt, wParam == 0 && lParam == 0 ? WM_RBUTTONDOWN : WM_CONTEXTMENU))
				return 0;
		}
	}
	else if(uMsg == WM_MOUSEACTIVATE)
	{
		auto msg = HIWORD(lParam);
		if(msg != WM_RBUTTONDOWN)
			return wndproc(hWnd, uMsg, wParam, lParam);

		auto pt = Point::CursorPos();

		if(taskbar_t::is_allowed_area(hWnd, pt))
		{
			int disabled_taskbar = 0;
			if(RegistryConfig::get(L"\\disable", L"taskbar", disabled_taskbar) && disabled_taskbar == 1)
				return wndproc(hWnd, uMsg, wParam, lParam);

			if(msg == WM_RBUTTONDOWN)
			{
				Keyboard kb;
				if(auto count = kb.get_keys_excloude_contextmenu(); count > 0)
				{
					if(kb.key_ctrl())
					{
						if(count == 2 && kb.key_win())
							return wndproc(hWnd, uMsg, wParam, lParam);

						if(count == 1 && !_initializer.has_error())
						{
							if(!_initializer.Status.Loaded)
								_initializer.OnState();
						}
					}
				}
			}

			if(_initializer.has_error())
			{
				_initializer.OnState();
				if(_initializer.has_error())
					return wndproc(hWnd, uMsg, wParam, lParam);
			}
			else
			{
				if(_initializer.Status.Disabled)
					return wndproc(hWnd, uMsg, wParam, lParam);
			}

			if(!is_registered())
			{
				return wndproc(hWnd, uMsg, wParam, lParam);
			}

			::SetFocus(hWnd);
			::SendMessageW(hWnd, WM_CONTEXTMENU, 0, 0);

			return MA_ACTIVATEANDEAT;
		}
	}
	else if(uMsg == WM_SETTINGCHANGE)
	{
		if(SPI_SETWORKAREA == wParam && ::GetSystemMetrics(SM_CMONITORS) > 1)
		{
			taskbar_t::hook_all(1000);
		}
	}
	else if(uMsg == WM_DESTROY)
	{
		taskbar_t::unhook(hWnd);
		taskbar_t::hook_all(1000);
	}

	return wndproc(hWnd, uMsg, wParam, lParam);
}

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID)
{
	try
	{
		if(dwReason == DLL_PROCESS_ATTACH)
		{
			::DisableThreadLibraryCalls(hInstance);

			_hInstance = hInstance;
			_initializer.HInstance = _hInstance;

#if _DEBUG
			// detect memory leaks
			_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG
						   | _CRTDBG_CHECK_ALWAYS_DF
						   | _CRTDBG_LEAK_CHECK_DF
						   | _CRTDBG_ALLOC_MEM_DF);
#endif
			_loader.init();

			if(!_loader.explorer && !_loader.path.ends_with(L"\\shell.exe"))
			{
				int disabled_3rdparty = 0;
				if(RegistryConfig::get(L"\\disable", L"3rdparty", disabled_3rdparty) && disabled_3rdparty == 1)
					return 0;
			}

			if(_initializer.init(_hInstance))
			{
				// DisableComExceptionHandling
				IComPtr<IGlobalOptions> globalOptions;
				if(globalOptions.CreateInstance(CLSID_GlobalOptions, CLSCTX_SERVER))
					globalOptions->Set(COMGLB_EXCEPTION_HANDLING, COMGLB_EXCEPTION_DONOT_HANDLE_ANY);

				_initializer.process.hModule = _loader.handle;
				_initializer.process.path = Path::Module(_loader.handle).move();
				_initializer.process.name = Path::Title(_initializer.process.path).move();
				_initializer.process.id = ::GetCurrentProcessId();
				_initializer.process.handle = ::GetCurrentProcess();

				// Host IATs and detached worker threads point into this image; never let COM unload it.
				HMODULE pinned{};
				::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
					reinterpret_cast<LPCWSTR>(&DllMain), &pinned);

				ContextMenu::RegisterLayer();

				iathook_NtUserTrackPopupMenuEx
					.init(L"user32.dll", "win32u.dll", "NtUserTrackPopupMenuEx", TrackPopupMenuExProc)
					.install();

				auto hook = []()
				{
					__trace(L"hook all the modules in '%s' process", _initializer.process.name.c_str());

					auto user32 = "user32.dll";

					for(auto hModule : Process::Modules(_initializer.process.handle))
					{
						if(_hInstance == hModule)
							continue;

						bool module_exists = false;
						for(const auto &m : iathook_TrackPopupMenu)
						{
							if(m._hModule == hModule)
							{
								module_exists = true;
								break;
							}
						}

						if(!module_exists)
						{
							iathook_TrackPopupMenu.emplace_back(hModule, user32, ::TrackPopupMenu, TrackPopupMenuProc).install();
							iathook_TrackPopupMenu.emplace_back(hModule, user32, ::TrackPopupMenuEx, TrackPopupMenuExProc).install();
						}
					}
				};

				if(_loader.explorer)
				{
					_CoCreateInstance.Begin();
					_CoCreateInstance.init(::CoCreateInstance, CoCreateInstanceHook).hook();
					_CoCreateInstance.Commit();

					if(!iathook_NtUserTrackPopupMenuEx.installed())
					{
						std::thread([=]() { ::Sleep(2000); hook(); }).detach();
					}

					taskbar_t::hook_all(1000);
				}
				else if(!iathook_NtUserTrackPopupMenuEx.install())
				{
					hook();
				}
			}

			return TRUE;
		}
		else if(dwReason == DLL_PROCESS_DETACH)
		{
			if(!is_registered())
			{
				if(_loader.explorer)
				{
					_taskbar_mouse.unhook();
					taskbar_t::unhook_all();

					_CoCreateInstance.Begin();
					_CoCreateInstance.unhook();
					_CoCreateInstance.Commit();
				}

				for(auto &d : iathook_TrackPopupMenu)
					d.uninstall(true);

				ContextMenu::UnRegisterLayer();
			}

			taskbar_t::_IUIAutomation.release();

			_log.close();

			//::SendNotifyMessageW(HWND_BROADCAST, WM_NULL, 0, 0);	

			return TRUE;
		}
	}
	catch(...) // handle all exceptions
	{
#ifdef _DEBUG
		_log.exception(__func__);
		_log.close();
#endif
	}
	return FALSE;
}

_Check_return_
STDAPI DllGetClassObject(_In_ REFCLSID rclsid, [[maybe_unused]] _In_ REFIID riid, [[maybe_unused]] _Outptr_ LPVOID FAR *ppv)
{
	auto hr = E_NOTIMPL;
	Guid iid = rclsid;
	if(!ppv)
		return E_POINTER;
	*ppv = nullptr;

	__try
	{
		if(iid.equals(IID_FolderExtensions))
			return E_NOTIMPL;

		if(!iid.equals({ IID_ContextMenu, IID_IconOverlay }))
			return hr;

		if(Initializer::Status.Disabled)
			return hr;

		if(!is_registered())
			return hr;
		
		if(_initializer.has_error())
			return hr;
		
		if(rclsid == IID_ContextMenu)
		{
			_loader.contextmenuhandler = true;
			Selections::point.GetCursorPos();
			hr = ClassFactory::CreateClassObject(riid, ppv);
		}

		if(!_initializer.Status.Loaded)
			_initializer.init();
	}
	except
	{
#ifdef _DEBUG
		_log.exception(__func__);
#endif
	}
	_log.close();

	return hr;
}

// Advisory only: the image is pinned in DllMain once hooks are installed.
__control_entrypoint(DllExport)
STDAPI DllCanUnloadNow(void)
{
	if(::InterlockedCompareExchange(&g_cDllRef, 0, 0) != 0)
		return S_FALSE;
	if(!_loader.explorer || !is_registered())
		return S_OK;
	return S_FALSE;
}

#pragma endregion

uint32_t ImmersiveColor::GetColorByColorType(uint32_t colorType)
{
	auto colorSet = GetImmersiveUserColorSetPreference(false, false);
	return GetImmersiveColorFromColorSetEx(colorSet, colorType, false, 0);
}

uint32_t ImmersiveColor::GetColorByName(const wchar_t *name)
{
	auto colorSet = GetImmersiveUserColorSetPreference(false, false);
	auto colorType = GetImmersiveColorTypeFromName(name);
	return GetImmersiveColorFromColorSetEx(colorSet, colorType, false, 0);
}

uint32_t ImmersiveColor::GetImmersiveUserColorSetPreference(bool bForceCheckRegistry, bool bSkipCheckOnFail)
{
	return DLL::Invoke<uint32_t>(hUxTheme, 98, bForceCheckRegistry, bSkipCheckOnFail);
}

uint32_t ImmersiveColor::GetImmersiveColorFromColorSetEx(uint32_t dwImmersiveColorSet, uint32_t dwImmersiveColorType, bool bIgnoreHighContrast, uint32_t dwHighContrastCacheMode)
{
	return DLL::Invoke<uint32_t>(hUxTheme, 95, dwImmersiveColorSet, dwImmersiveColorType, bIgnoreHighContrast, dwHighContrastCacheMode);
}

uint32_t ImmersiveColor::GetImmersiveColorTypeFromName(const wchar_t *name)
{
	return DLL::Invoke<uint32_t>(hUxTheme, 96, name);
}

uint32_t ImmersiveColor::GetImmersiveColorSetCount()
{
	return DLL::Invoke<uint32_t>(hUxTheme, 94);
}

bool ImmersiveColor::IsSupported()
{
	return  ::IsCompositionActive() && DLL::IsFunc(hUxTheme, 95);
}
