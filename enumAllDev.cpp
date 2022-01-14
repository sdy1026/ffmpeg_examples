#include <windows.h>
#include <dshow.h>
#include <vector>
#include <comutil.h>
#include <string>
using namespace std;
#pragma comment(lib,"comsuppw")
#pragma comment(lib, "strmiids")
#pragma comment(lib, "ole32")
#pragma comment(lib, "oleaut32")

HRESULT EnumerateDevices(REFGUID category, IEnumMoniker** ppEnum)
{
	// Create the System Device Enumerator.
	ICreateDevEnum* pDevEnum;
	HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr,
		CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));
	if (SUCCEEDED(hr))
	{
		// Create an enumerator for the category.
		hr = pDevEnum->CreateClassEnumerator(category, ppEnum, 0);
		if (hr == S_FALSE)
		{
			hr = VFW_E_NOT_FOUND;  // The category is empty. Treat as an error.
		}
		pDevEnum->Release();
	}
	return hr;
}
void DisplayDeviceInformation(IEnumMoniker* pEnum, vector<string>& list)
{
	IMoniker* pMoniker = nullptr;

	while (pEnum->Next(1, &pMoniker, nullptr) == S_OK)
	{
		IPropertyBag* pPropBag;
		HRESULT hr = pMoniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&pPropBag));
		if (FAILED(hr))
		{
			pMoniker->Release();
			continue;
		}

		VARIANT var;
		VariantInit(&var);

		// Get description or friendly name.
		hr = pPropBag->Read(L"Description", &var, nullptr);
		if (FAILED(hr))
		{
			hr = pPropBag->Read(L"FriendlyName", &var, nullptr);
		}
		if (SUCCEEDED(hr))
		{
			std::string name = _com_util::ConvertBSTRToString(var.bstrVal);
			printf("%s\n", name.c_str());//设备名称
			list.push_back(name);
			VariantClear(&var);
		}
		hr = pPropBag->Write(L"FriendlyName", &var);
		// WaveInID applies only to audio capture devices.
		hr = pPropBag->Read(L"WaveInID", &var, nullptr);
		if (SUCCEEDED(hr))
		{
			//printf("WaveIn ID: %d\n", var.lVal);//设备ID
			VariantClear(&var);
		}
		hr = pPropBag->Read(L"DevicePath", &var, nullptr);
		if (SUCCEEDED(hr))
		{
			// The device path is not intended for display.
//            printf("Device path: %S\n", var.bstrVal);
			VariantClear(&var);
		}

		pPropBag->Release();
		pMoniker->Release();
	}
}

void GetCameraName(vector<std::string>& list)
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (SUCCEEDED(hr)) {
		IEnumMoniker* pEnum;
		hr = EnumerateDevices(CLSID_VideoInputDeviceCategory, &pEnum);
		if (SUCCEEDED(hr)) {
			DisplayDeviceInformation(pEnum, list);
			pEnum->Release();
		}
		//麦克风...........
		hr = EnumerateDevices(CLSID_AudioInputDeviceCategory, &pEnum);
		if (SUCCEEDED(hr)) {
			DisplayDeviceInformation(pEnum, list);
			pEnum->Release();
		}
		CoUninitialize();
	}
}

void getAllDevNames()
{
	vector<string> list;
	GetCameraName(list);

	for (int i = 0; i < list.size(); ++i)
	{
		printf("Device: %s\n", list[i].c_str());
	}
}