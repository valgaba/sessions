#include "audiometer.h"

#include <algorithm>
#include <cmath>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

// MinGW ships endpointvolume.h without the full IAudioMeterInformation
// definition, so declare the interface and its IID here.
static const IID IID_IAudioMeterInformationSessions =
    {0xC02216F6, 0x8C67, 0x4B5B, {0x9D, 0x00, 0xD0, 0x08, 0xE7, 0x3E, 0x00, 0x64}};
struct IAudioMeterInformationSessions : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float *peak) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT *count) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT32 count, float *values) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD *mask) = 0;
};
#endif

struct AudioMeter::Private {
#ifdef Q_OS_WIN
    IMMDeviceEnumerator *enumerator = nullptr;
    IMMDevice *device = nullptr;
    IAudioMeterInformationSessions *meter = nullptr;
    bool comInitialized = false;
    bool comReady = false;
#endif
};

AudioMeter::AudioMeter(QObject *parent) : QObject(parent), d(std::make_unique<Private>()) {
#ifdef Q_OS_WIN
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    d->comInitialized = SUCCEEDED(hr);
    d->comReady = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    if (d->comReady) {
        CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                         IID_PPV_ARGS(&d->enumerator));
    }
#endif
    timer.setInterval(33);
    timer.setTimerType(Qt::PreciseTimer);
    connect(&timer, &QTimer::timeout, this, &AudioMeter::poll);
}

AudioMeter::~AudioMeter() {
    close();
#ifdef Q_OS_WIN
    if (d->enumerator) d->enumerator->Release();
    if (d->comInitialized) CoUninitialize();
#endif
}

void AudioMeter::setDeviceId(const QString &deviceId) {
    if (currentDeviceId == deviceId && (!active || isMetering())) return;
    currentDeviceId = deviceId;
    if (active) open();
}

void AudioMeter::setActive(bool value) {
    if (active == value) return;
    active = value;
    if (active) open();
    else close();
}

bool AudioMeter::isMetering() const {
#ifdef Q_OS_WIN
    return d->meter != nullptr;
#else
    return false;
#endif
}

void AudioMeter::open() {
    close();
#ifdef Q_OS_WIN
    if (!d->comReady || !d->enumerator) return;
    HRESULT hr;
    if (currentDeviceId.isEmpty()) {
        hr = d->enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &d->device);
    } else {
        const std::wstring id = currentDeviceId.toStdWString();
        hr = d->enumerator->GetDevice(id.c_str(), &d->device);
    }
    if (FAILED(hr) || !d->device) { d->device = nullptr; return; }
    hr = d->device->Activate(IID_IAudioMeterInformationSessions, CLSCTX_ALL, nullptr,
                             reinterpret_cast<void **>(&d->meter));
    if (FAILED(hr) || !d->meter) { d->meter = nullptr; d->device->Release(); d->device = nullptr; return; }
    timer.start();
#endif
}

void AudioMeter::close() {
    timer.stop();
#ifdef Q_OS_WIN
    if (d->meter) { d->meter->Release(); d->meter = nullptr; }
    if (d->device) { d->device->Release(); d->device = nullptr; }
#endif
}

void AudioMeter::poll() {
#ifdef Q_OS_WIN
    if (!d->meter) return;
    const auto clean = [](float value) {
        return value < 0.0001f ? qreal(0) : std::clamp<qreal>(value, 0, 1);
    };
    UINT channels = 0;
    if (FAILED(d->meter->GetMeteringChannelCount(&channels)) || channels == 0) {
        emit levelsChanged(0, 0);
        return;
    }
    std::vector<float> values(channels, 0.f);
    if (FAILED(d->meter->GetChannelsPeakValues(channels, values.data()))) {
        float peak = 0.f;
        if (FAILED(d->meter->GetPeakValue(&peak))) peak = 0.f;
        emit levelsChanged(clean(peak), clean(peak));
        return;
    }
    emit levelsChanged(clean(values[0]), clean(channels > 1 ? values[1] : values[0]));
#endif
}
