#include <System.h>
#include "Include/ClassFactory.h"
#include "Include/ShellExtSelectionRetriever.h"

using Microsoft::WRL::ComPtr;
using Nilesoft::Logger;

ShellExtSelectionRetriever::ShellExtSelectionRetriever()
{
	::InterlockedIncrement(&g_cDllRef);
	::AcquireSRWLockExclusive(&_lock);
	_next = _handlers;
	_handlers = this;
	::ReleaseSRWLockExclusive(&_lock);
}

ShellExtSelectionRetriever::~ShellExtSelectionRetriever()
{
	::AcquireSRWLockExclusive(&_lock);
	bool pending = _menu != nullptr;
	auto link = &_handlers;
	while(*link && *link != this)
		link = &(*link)->_next;
	if(*link)
		*link = _next;
	::ReleaseSRWLockExclusive(&_lock);
	if(pending)
		Logger::Instance().warning(L"Selection helper released before popup capture (process %lu, thread %lu).",
			::GetCurrentProcessId(), _thread);
	_selection.items.Reset();
	_selection.parent.Reset();
	::InterlockedDecrement(&g_cDllRef);
}

HRESULT STDMETHODCALLTYPE ShellExtSelectionRetriever::QueryInterface(REFIID riid, void **ppv)
{
	if(!ppv)
		return E_POINTER;
	*ppv = nullptr;
	if(riid == IID_IUnknown || riid == IID_IContextMenu)
		*ppv = static_cast<IContextMenu *>(this);
	else if(riid == IID_IShellExtInit)
		*ppv = static_cast<IShellExtInit *>(this);
	else
		return E_NOINTERFACE;
	AddRef();
	return S_OK;
}

ULONG STDMETHODCALLTYPE ShellExtSelectionRetriever::AddRef()
{
	return ::InterlockedIncrement(&_references);
}

ULONG STDMETHODCALLTYPE ShellExtSelectionRetriever::Release()
{
	auto references = ::InterlockedDecrement(&_references);
	if(references == 0)
		delete this;
	return references;
}

HRESULT STDMETHODCALLTYPE ShellExtSelectionRetriever::Initialize(PCIDLIST_ABSOLUTE folder, IDataObject *data, HKEY)
{
	::AcquireSRWLockExclusive(&_lock);
	_menu = nullptr;
	::ReleaseSRWLockExclusive(&_lock);
	_selection = {};

	auto fail = [](HRESULT result)
	{
		Logger::Error(L"Shell selection initialization failed (HRESULT 0x%08X).", result);
		return result;
	};

	ShellSelectionContext selection;
	selection.background = data == nullptr;
	if(folder)
	{
		auto result = ::SHCreateItemFromIDList(folder, IID_PPV_ARGS(selection.parent.GetAddressOf()));
		if(FAILED(result))
			return fail(result);
	}
	if(selection.background)
	{
		if(!selection.parent)
			return fail(E_INVALIDARG);
	}
	else
	{
		auto result = ::SHCreateShellItemArrayFromDataObject(data, IID_PPV_ARGS(selection.items.GetAddressOf()));
		if(FAILED(result))
			return fail(result);
		DWORD count = 0;
		result = selection.items->GetCount(&count);
		if(FAILED(result))
			return fail(result);
		if(count == 0)
			return fail(E_INVALIDARG);
		if(!selection.parent)
		{
			ComPtr<IShellItem> firstParent;
			bool sameParent = true;
			for(DWORD i = 0; i < count; ++i)
			{
				ComPtr<IShellItem> item;
				result = selection.items->GetItemAt(i, item.GetAddressOf());
				if(FAILED(result))
					return fail(result);
				ComPtr<IShellItem> parent;
				result = item->GetParent(parent.GetAddressOf());
				if(result == MK_E_NOOBJECT)
				{
					sameParent = false;
					continue;
				}
				if(FAILED(result))
					return fail(result);
				if(i == 0)
					firstParent = parent;
				else if(sameParent)
				{
					int comparison = 0;
					result = firstParent->Compare(parent.Get(), SICHINT_CANONICAL, &comparison);
					if(FAILED(result))
						return fail(result);
					sameParent = comparison == 0;
				}
			}
			if(sameParent)
				selection.parent = firstParent;
		}
	}
	_selection = std::move(selection);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ShellExtSelectionRetriever::QueryContextMenu(HMENU menu, UINT, UINT, UINT, UINT flags)
{
	::AcquireSRWLockExclusive(&_lock);
	_menu = nullptr;
	if(!(flags & CMF_DEFAULTONLY) && (_selection.items || _selection.parent))
	{
		_menu = menu;
		_thread = ::GetCurrentThreadId();
	}
	::ReleaseSRWLockExclusive(&_lock);
	Logger::Instance().info(L"Selection helper queried (process %lu, thread %lu, menu %p, flags 0x%08X, background %d, items %d).",
		::GetCurrentProcessId(), ::GetCurrentThreadId(), menu, flags,
		_selection.background, _selection.items.Get() != nullptr);
	// The handler captures context only; it never adds a user-visible command.
	return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
}

bool ShellExtSelectionRetriever::TakeSelection(HMENU menu, ShellSelectionContext &selection)
{
	ShellExtSelectionRetriever *handler = nullptr;
	::AcquireSRWLockExclusive(&_lock);
	for(auto current = _handlers; current; current = current->_next)
	{
		if(current->_menu == menu && current->_thread == ::GetCurrentThreadId())
		{
			current->AddRef();
			current->_menu = nullptr;
			handler = current;
			break;
		}
	}
	::ReleaseSRWLockExclusive(&_lock);
	if(!handler)
		return false;
	selection = handler->_selection;
	handler->Release();
	Logger::Instance().info(L"Selection captured (process %lu, thread %lu, menu %p).",
		::GetCurrentProcessId(), ::GetCurrentThreadId(), menu);
	return true;
}

HRESULT STDMETHODCALLTYPE ShellExtSelectionRetriever::InvokeCommand(CMINVOKECOMMANDINFO *)
{
	return E_INVALIDARG;
}

HRESULT STDMETHODCALLTYPE ShellExtSelectionRetriever::GetCommandString(UINT_PTR, UINT, UINT *, LPSTR, UINT)
{
	return E_NOTIMPL;
}
