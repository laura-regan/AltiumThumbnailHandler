#include <shlwapi.h>
#include <thumbcache.h> // For IThumbnailProvider.
#include <string>
#include <filesystem>
#include <fstream>

#pragma comment(lib, "shlwapi.lib")

// this thumbnail provider implements IInitializeWithStream to enable being hosted
// in an isolated process for robustness
class CAltiumThumbProvider : public IInitializeWithStream,
                             public IThumbnailProvider
{
public:
    CAltiumThumbProvider() : _cRef(1), _pStream(NULL), m_process(NULL)
    {
    }

    virtual ~CAltiumThumbProvider()
    {
        if (_pStream)
        {
            _pStream->Release();
        }
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        static const QITAB qit[] =
        {
            QITABENT(CAltiumThumbProvider, IInitializeWithStream),
            QITABENT(CAltiumThumbProvider, IThumbnailProvider),
            { 0 },
        };
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&_cRef);
    }

    IFACEMETHODIMP_(ULONG) Release()
    {
        ULONG cRef = InterlockedDecrement(&_cRef);
        if (!cRef)
        {
            delete this;
        }
        return cRef;
    }

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream *pStream, DWORD grfMode);

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha);

private:
    long _cRef;
    IStream *_pStream;     // provided during initialization.

    HANDLE m_process;
};

HRESULT CAltiumThumbProvider_CreateInstance(REFIID riid, void **ppv)
{
    CAltiumThumbProvider *pNew = new (std::nothrow) CAltiumThumbProvider();
    HRESULT hr = pNew ? S_OK : E_OUTOFMEMORY;
    if (SUCCEEDED(hr))
    {
        hr = pNew->QueryInterface(riid, ppv);
        pNew->Release();
    }
    return hr;
}

// IInitializeWithStream
IFACEMETHODIMP CAltiumThumbProvider::Initialize(IStream *pStream, DWORD)
{
    HRESULT hr = E_UNEXPECTED;  // can only be inited once
    if (_pStream == NULL)
    {
        // take a reference to the stream if we have not been inited yet
        hr = pStream->QueryInterface(&_pStream);
    }
    return hr;
}

std::wstring GetExecutablePath() {
    wchar_t modulePath[MAX_PATH];
    HMODULE hModule = NULL;

    // Get the handle to the DLL itself
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&GetExecutablePath,
        &hModule)) {
        // Get the full path of the DLL
        GetModuleFileNameW(hModule, modulePath, MAX_PATH);

        // Convert to std::wstring for easier manipulation
        std::wstring path(modulePath);

        // Find the last slash and return the directory
        auto lastSlash = path.find_last_of(L"\\/");
        return path.substr(0, lastSlash) + L"\\Support\\AltiumThumbnailProvider.exe";
    }

    // If something goes wrong, return an empty string or error value
    return L"";
}

// IThumbnailProvider
IFACEMETHODIMP CAltiumThumbProvider::GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha)
{
    char buffer[4096];
    ULONG cbRead;

    wchar_t tempPath[MAX_PATH];

    GetTempPath2W(MAX_PATH, tempPath);

    std::wstring filePath = std::wstring(tempPath) + L"\\AltiumThumnail-Temp\\";
    if (!std::filesystem::exists(filePath))
    {
        std::filesystem::create_directories(filePath);
    }

    std::wstring fileName = filePath + L"Temp";

    // Write data to tmp file
    std::fstream file;
    file.open(fileName, std::ios_base::out | std::ios_base::binary);

    if (!file.is_open())
    {
        return 0;
    }

    while (true)
    {
        auto result = _pStream->Read(buffer, 4096, &cbRead);

        file.write(buffer, cbRead);
        if (result == S_FALSE)
        {
            break;
        }
    }
    file.close();
    
    _pStream->Release();
    _pStream = NULL;

    try
    {

        STARTUPINFO info = { sizeof(info) };
        std::wstring cmdLine{ L"\"" + fileName + L"\"" };
        cmdLine += L" ";
        cmdLine += std::to_wstring(cx);

        std::wstring appPath = GetExecutablePath();

        SHELLEXECUTEINFO sei{ sizeof(sei) };
        sei.fMask = { SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI };
        sei.lpFile = appPath.c_str();
        sei.lpParameters = cmdLine.c_str();
        sei.nShow = SW_SHOWDEFAULT;
        ShellExecuteEx(&sei);
        m_process = sei.hProcess;
        WaitForSingleObject(m_process, INFINITE);
        std::filesystem::remove(fileName);
         
        
        std::wstring fileNameBmp = fileName.substr(0, fileName.find_last_of('.')) + L".bmp";
        if (std::filesystem::exists(fileNameBmp))
        {
            *phbmp = static_cast<HBITMAP>(LoadImage(NULL, fileNameBmp.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE));
            *pdwAlpha = WTS_ALPHATYPE::WTSAT_ARGB;
            std::filesystem::remove(fileNameBmp);
        }
        else
        {
            return E_FAIL;
        }
    }
    catch (std::exception& e)
    {
        throw e.what();
    }

    // ensure releasing the stream (not all if branches contain it)
    if (_pStream)
    {
        _pStream->Release();
        _pStream = NULL;
    }

    return S_OK;
}
