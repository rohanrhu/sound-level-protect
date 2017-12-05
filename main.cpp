/*
* https://github.com/rohanrhu/volume-level-protect
*
* volume-level-protect is a protector for audio volume level
* during using headphones in on Win32 platform.
*
* Copyright (C) 2017 Oðuzhan Eroðlu <rohanrhu2@gmail.com>
* Licensed under The MIT License (MIT)
*
*/

#include <objbase.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <atlbase.h>
#include <thread>
#include <mutex>
#include <csignal>

#include "main.h"

#pragma comment(lib, "uuid.lib")

#define MAX_AUDIO_VOLUME_LEVEL 0.7

typedef struct {
	IMMDevice* defaultDevice;
	IAudioEndpointVolume* endPointVolume;
	bool is_jack;
} OutDev;

OutDev* dev;
CVolumeNotification* notification;

bool is_warning = false;
bool is_allow = false;

std::mutex lock1;
std::mutex lock2;

OutDev* OpenAudioDevice()
{
	HRESULT hr;

	CoInitialize(NULL);
	IMMDeviceEnumerator* deviceEnumerator = NULL;

	hr = CoCreateInstance(
		__uuidof(MMDeviceEnumerator),
		NULL,
		CLSCTX_INPROC_SERVER,
		__uuidof(IMMDeviceEnumerator),
		(LPVOID *)&deviceEnumerator
	);

	if (!SUCCEEDED(hr)) return NULL;

	IMMDevice *defaultDevice = NULL;

	hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
	deviceEnumerator->Release();

	if (!SUCCEEDED(hr)) return NULL;

	IAudioEndpointVolume* endpointVolume = NULL;
	hr = defaultDevice->Activate(
		__uuidof(IAudioEndpointVolume),
		CLSCTX_INPROC_SERVER,
		NULL,
		(LPVOID *)&endpointVolume
	);

	if (!SUCCEEDED(hr)) return NULL;

	OutDev* oDev = new OutDev;
	oDev->defaultDevice = defaultDevice;
	oDev->endPointVolume= endpointVolume;
	oDev->is_jack = true;

	return oDev;
}

bool IsJackPlugged(void)
{
	HRESULT hr;

	GUID IDevice_FriendlyName = { 0xa45c254e, 0xdf1c, 0x4efd,{ 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } };

	static PROPERTYKEY key;
	key.pid = 14;
	key.fmtid = IDevice_FriendlyName;

	CComPtr<IPropertyStore> store;
	hr = dev->defaultDevice->OpenPropertyStore(STGM_READ, &store);
	if (!SUCCEEDED(hr))
	{
		return false;
	}

	CComPtr<IDeviceTopology> topology;
	hr = dev->defaultDevice->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, NULL, (LPVOID *)&topology);
	if (!SUCCEEDED(hr))
	{
		return false;
	}

	CComPtr<IConnector> connector;
	hr = topology->GetConnector(0, &connector);
	if (!SUCCEEDED(hr))
	{
		return false;
	}

	CComPtr<IConnector> connectedTo;
	hr = connector->GetConnectedTo(&connectedTo);
	if (!SUCCEEDED(hr))
	{
		return false;
	}

	CComPtr<IPart> part;
	hr = connectedTo->QueryInterface(&part);
	if (!SUCCEEDED(hr))
	{
		return false;
	}

	CComPtr<IKsJackDescription> jack;
	hr = part->Activate(CLSCTX_ALL, IID_PPV_ARGS(&jack));
	if (!SUCCEEDED(hr))
	{
		return false;
	}

	UINT jnum = 1;

	KSJACK_DESCRIPTION desc = { 0 };
	jack->GetJackDescription(jnum, &desc);

	return desc.IsConnected;
}

void ShowWarning(void)
{
	if (is_warning) {
		return;
	}

	lock1.lock();

	is_warning = true;

	int result = MessageBox(
		NULL,
		"High sound can damage your ears. Do you really want increase volume to unsafe level?",
		"Warning!",
		MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2
	);

	if (result == IDYES) {
		lock2.lock();
		is_allow = true;
		lock2.unlock();
	}

	is_warning = false;
	lock1.unlock();
}

CVolumeNotification::CVolumeNotification(void) : m_RefCount(1)
{
}

STDMETHODIMP_(ULONG) CVolumeNotification::AddRef()
{
	return InterlockedIncrement(&m_RefCount);
}

STDMETHODIMP_(ULONG) CVolumeNotification::Release()
{
	LONG ref = InterlockedDecrement(&m_RefCount);
	if (ref == 0)
		delete this;
	return ref;
}

STDMETHODIMP CVolumeNotification::QueryInterface(REFIID IID, void **ReturnValue)
{
	if (IID == IID_IUnknown || IID == __uuidof(IAudioEndpointVolumeCallback))
	{
		*ReturnValue = static_cast<IUnknown*>(this);
		AddRef();
		return S_OK;
	}
	*ReturnValue = NULL;
	return E_NOINTERFACE;
}

STDMETHODIMP CVolumeNotification::OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA notificationData)
{
	bool _is_allow;

	lock2.lock();
	_is_allow = is_allow;
	lock2.unlock();

	if (IsJackPlugged() && !_is_allow && notificationData->fMasterVolume > MAX_AUDIO_VOLUME_LEVEL) {
		dev->endPointVolume->SetMasterVolumeLevelScalar(MAX_AUDIO_VOLUME_LEVEL, NULL);
		std::thread(ShowWarning).detach();
	}

	return S_OK;
}


void Listener(void)
{
	dev = OpenAudioDevice();

	HRESULT result;

	notification = new CVolumeNotification();

	result = dev->endPointVolume->RegisterControlChangeNotify(notification);
}

void signalHandler(int sigid)
{
	MessageBox(
		NULL,
		"Volume Level Protector is exiting now..",
		"Volume Level Protector",
		MB_ICONINFORMATION
	);
}

int WINAPI WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow
)
{
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGABRT, signalHandler);

	std::thread(Listener).detach();

	std::this_thread::sleep_for(std::chrono::seconds(INFINITE));
}