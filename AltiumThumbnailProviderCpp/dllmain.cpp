#include <objbase.h>
#include <shlwapi.h>
#include <thumbcache.h> // For IThumbnailProvider.
#include <shlobj.h>     // For SHChangeNotify
#include <new>

extern HRESULT CAltiumThumbProvider_CreateInstance(REFIID riid, void **ppv);

#define SZ_CLSID_ALTIUMTHUMBHANDLER     L"{3A6F47E1-138E-4BF8-90C0-3BC0CAC146D9}"
#define SZ_ALTIUMTHUMBHANDLER           L"Altium Thumbnail Handler"

// {3A6F47E1-138E-4BF8-90C0-3BC0CAC146D9}
const CLSID CLSID_AltiumThumbHandler    = { 0x3a6f47e1, 0x138e, 0x4bf8, { 0x90, 0xc0, 0x3b, 0xc0, 0xca, 0xc1, 0x46, 0xd9 } };

typedef HRESULT (*PFNCREATEINSTANCE)(REFIID riid, void **ppvObject);
struct CLASS_OBJECT_INIT
{
    const CLSID *pClsid;
    PFNCREATEINSTANCE pfnCreate;
};

// add classes supported by this module here
const CLASS_OBJECT_INIT c_rgClassObjectInit[] =
{
    { &CLSID_AltiumThumbHandler, CAltiumThumbProvider_CreateInstance }
};


long g_cRefModule = 0;

// Handle the the DLL's module
HINSTANCE g_hInst = NULL;

// Standard DLL functions
STDAPI_(BOOL) DllMain(HINSTANCE hInstance, DWORD dwReason, void *)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        g_hInst = hInstance;
        DisableThreadLibraryCalls(hInstance);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow()
{
    // Only allow the DLL to be unloaded after all outstanding references have been released
    return (g_cRefModule == 0) ? S_OK : S_FALSE;
}

void DllAddRef()
{
    InterlockedIncrement(&g_cRefModule);
}

void DllRelease()
{
    InterlockedDecrement(&g_cRefModule);
}

class CClassFactory : public IClassFactory
{
public:
    static HRESULT CreateInstance(REFCLSID clsid, const CLASS_OBJECT_INIT *pClassObjectInits, size_t cClassObjectInits, REFIID riid, void **ppv)
    {
        *ppv = NULL;
        HRESULT hr = CLASS_E_CLASSNOTAVAILABLE;
        for (size_t i = 0; i < cClassObjectInits; i++)
        {
            if (clsid == *pClassObjectInits[i].pClsid)
            {
                IClassFactory *pClassFactory = new (std::nothrow) CClassFactory(pClassObjectInits[i].pfnCreate);
                hr = pClassFactory ? S_OK : E_OUTOFMEMORY;
                if (SUCCEEDED(hr))
                {
                    hr = pClassFactory->QueryInterface(riid, ppv);
                    pClassFactory->Release();
                }
                break; // match found
            }
        }
        return hr;
    }

    CClassFactory(PFNCREATEINSTANCE pfnCreate) : _cRef(1), _pfnCreate(pfnCreate)
    {
        DllAddRef();
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void ** ppv)
    {
        static const QITAB qit[] =
        {
            QITABENT(CClassFactory, IClassFactory),
            { 0 }
        };
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&_cRef);
    }

    IFACEMETHODIMP_(ULONG) Release()
    {
        long cRef = InterlockedDecrement(&_cRef);
        if (cRef == 0)
        {
            delete this;
        }
        return cRef;
    }

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown *punkOuter, REFIID riid, void **ppv)
    {
        return punkOuter ? CLASS_E_NOAGGREGATION : _pfnCreate(riid, ppv);
    }

    IFACEMETHODIMP LockServer(BOOL fLock)
    {
        if (fLock)
        {
            DllAddRef();
        }
        else
        {
            DllRelease();
        }
        return S_OK;
    }

private:
    ~CClassFactory()
    {
        DllRelease();
    }

    long _cRef;
    PFNCREATEINSTANCE _pfnCreate;
};

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **ppv)
{
    return CClassFactory::CreateInstance(clsid, c_rgClassObjectInit, ARRAYSIZE(c_rgClassObjectInit), riid, ppv);
}

// A struct to hold the information required for a registry entry

struct REGISTRY_ENTRY
{
    HKEY   hkeyRoot;
    PCWSTR pszKeyName;
    PCWSTR pszValueName;
    PCWSTR pszData;
};

// Creates a registry key (if needed) and sets the default value of the key

HRESULT CreateRegKeyAndSetValue(const REGISTRY_ENTRY *pRegistryEntry)
{
    HKEY hKey;
    HRESULT hr = HRESULT_FROM_WIN32(RegCreateKeyExW(pRegistryEntry->hkeyRoot, pRegistryEntry->pszKeyName,
                                0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL));
    if (SUCCEEDED(hr))
    {
        hr = HRESULT_FROM_WIN32(RegSetValueExW(hKey, pRegistryEntry->pszValueName, 0, REG_SZ,
                            (LPBYTE) pRegistryEntry->pszData,
                            ((DWORD) wcslen(pRegistryEntry->pszData) + 1) * sizeof(WCHAR)));
        RegCloseKey(hKey);
    }
    return hr;
}

//
// Registers this COM server
//
STDAPI DllRegisterServer()
{
    HRESULT hr;

    WCHAR szModuleName[MAX_PATH];

    if (!GetModuleFileNameW(g_hInst, szModuleName, ARRAYSIZE(szModuleName)))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }
    else
    {
        // List of registry entries we want to create
        const REGISTRY_ENTRY rgRegistryEntries[] =
        {
            // RootKey            KeyName                                                                ValueName                     Data
            {HKEY_CURRENT_USER,   L"Software\\Classes\\CLSID\\" SZ_CLSID_ALTIUMTHUMBHANDLER,                                 NULL,                           SZ_ALTIUMTHUMBHANDLER},
            {HKEY_CURRENT_USER,   L"Software\\Classes\\CLSID\\" SZ_CLSID_ALTIUMTHUMBHANDLER L"\\InProcServer32",             NULL,                           szModuleName},
            {HKEY_CURRENT_USER,   L"Software\\Classes\\CLSID\\" SZ_CLSID_ALTIUMTHUMBHANDLER L"\\InProcServer32",             L"ThreadingModel",              L"Apartment"},
            {HKEY_CURRENT_USER,   L"Software\\Classes\\.SchLib\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}",            NULL,                           SZ_CLSID_ALTIUMTHUMBHANDLER},
            {HKEY_CURRENT_USER,   L"Software\\Classes\\.PcbLib\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}",            NULL,                           SZ_CLSID_ALTIUMTHUMBHANDLER},
        };

        hr = S_OK;
        for (int i = 0; i < ARRAYSIZE(rgRegistryEntries) && SUCCEEDED(hr); i++)
        {
            hr = CreateRegKeyAndSetValue(&rgRegistryEntries[i]);
        }
    }
    if (SUCCEEDED(hr))
    {
        // This tells the shell to invalidate the thumbnail cache.  This is important because any .recipe files
        // viewed before registering this handler would otherwise show cached blank thumbnails.
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }
    return hr;
}

//
// Unregisters this COM server
//
STDAPI DllUnregisterServer()
{
    HRESULT hr = S_OK;

    const PCWSTR rgpszKeys[] =
    {
        L"Software\\Classes\\CLSID\\" SZ_CLSID_ALTIUMTHUMBHANDLER,
        L"Software\\Classes\\.SchLib\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}"
        L"Software\\Classes\\.PcbLib\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}"
    };

    // Delete the registry entries
    for (int i = 0; i < ARRAYSIZE(rgpszKeys) && SUCCEEDED(hr); i++)
    {
        hr = HRESULT_FROM_WIN32(RegDeleteTreeW(HKEY_CURRENT_USER, rgpszKeys[i]));
        if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        {
            // If the registry entry has already been deleted, say S_OK.
            hr = S_OK;
        }
    }
    return hr;
}
