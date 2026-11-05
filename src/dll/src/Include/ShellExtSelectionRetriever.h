#pragma once

#include <shlobj.h>
#include <wrl/client.h>

struct ShellSelectionContext
{
	bool background = false;
	Microsoft::WRL::ComPtr<IShellItem> parent;
	Microsoft::WRL::ComPtr<IShellItemArray> items;
};

class ShellExtSelectionRetriever final : public IShellExtInit, public IContextMenu
{
public:
	ShellExtSelectionRetriever();
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override;
	ULONG STDMETHODCALLTYPE AddRef() override;
	ULONG STDMETHODCALLTYPE Release() override;
	HRESULT STDMETHODCALLTYPE Initialize(PCIDLIST_ABSOLUTE folder, IDataObject *data, HKEY key) override;
	HRESULT STDMETHODCALLTYPE QueryContextMenu(HMENU menu, UINT index, UINT first, UINT last, UINT flags) override;
	HRESULT STDMETHODCALLTYPE InvokeCommand(CMINVOKECOMMANDINFO *command) override;
	HRESULT STDMETHODCALLTYPE GetCommandString(UINT_PTR id, UINT flags, UINT *reserved, LPSTR text, UINT length) override;

	static bool TakeSelection(HMENU menu, ShellSelectionContext &selection);

private:
	~ShellExtSelectionRetriever();
	volatile long _references = 1;
	ShellSelectionContext _selection;
	HMENU _menu = nullptr;
	DWORD _thread = 0;
	ShellExtSelectionRetriever *_next = nullptr;
	inline static SRWLOCK _lock = SRWLOCK_INIT;
	inline static ShellExtSelectionRetriever *_handlers = nullptr;
};
