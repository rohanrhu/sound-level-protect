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

#pragma once

#include <endpointvolume.h>

class CVolumeNotification : public IAudioEndpointVolumeCallback
{
	LONG m_RefCount;
public:
	CVolumeNotification(void);

	~CVolumeNotification(void) {};

	STDMETHODIMP_(ULONG) AddRef();

	STDMETHODIMP_(ULONG) Release();

	STDMETHODIMP QueryInterface(REFIID IID, void **ReturnValue);

	STDMETHODIMP OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA notificationData);
};