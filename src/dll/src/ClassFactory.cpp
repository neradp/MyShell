#include <windows.h>
#include "Include/ClassFactory.h"
#include "Include/ShellExtSelectionRetriever.h"
#include <new>

ClassFactory::ClassFactory()
{
	::InterlockedIncrement(&g_cDllRef);
}

ClassFactory::~ClassFactory()
{
	::InterlockedDecrement(&g_cDllRef);
}

HRESULT STDMETHODCALLTYPE ClassFactory::QueryInterface(REFIID riid, void **ppv)
{
	if(!ppv)
		return E_POINTER;
	*ppv = nullptr;
	if(riid != IID_IUnknown && riid != IID_IClassFactory)
		return E_NOINTERFACE;
	*ppv = static_cast<IClassFactory *>(this);
	AddRef();
	return S_OK;
}

ULONG STDMETHODCALLTYPE ClassFactory::AddRef()
{
	return ::InterlockedIncrement(&_references);
}

ULONG STDMETHODCALLTYPE ClassFactory::Release()
{
	auto references = ::InterlockedDecrement(&_references);
	if(references == 0)
		delete this;
	return references;
}

HRESULT STDMETHODCALLTYPE ClassFactory::CreateInstance(IUnknown *outer, REFIID riid, void **ppv)
{
	if(!ppv)
		return E_POINTER;
	*ppv = nullptr;
	if(outer)
		return CLASS_E_NOAGGREGATION;
	auto handler = new(std::nothrow) ShellExtSelectionRetriever;
	if(!handler)
		return E_OUTOFMEMORY;
	auto result = handler->QueryInterface(riid, ppv);
	handler->Release();
	return result;
}

HRESULT STDMETHODCALLTYPE ClassFactory::LockServer(BOOL lock)
{
	if(lock)
		::InterlockedIncrement(&g_cDllRef);
	else
		::InterlockedDecrement(&g_cDllRef);
	return S_OK;
}
