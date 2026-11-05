#pragma once

#include <unknwn.h>

extern volatile long g_cDllRef;

class ClassFactory final : public IClassFactory
{
public:
	ClassFactory();
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override;
	ULONG STDMETHODCALLTYPE AddRef() override;
	ULONG STDMETHODCALLTYPE Release() override;
	HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *outer, REFIID riid, void **ppv) override;
	HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override;

private:
	~ClassFactory();
	volatile long _references = 1;
};
