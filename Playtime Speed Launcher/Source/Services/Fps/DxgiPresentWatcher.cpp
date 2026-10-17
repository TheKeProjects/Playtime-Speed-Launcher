#include "Services/Fps/DxgiPresentWatcher.h"
#include <windows.h>
#include <evntcons.h>
#include <evntrace.h>
#include <vector>

#pragma comment(lib, "advapi32.lib")

namespace SpeedrunLauncher::Services {

namespace {

// Microsoft-Windows-DXGI and Microsoft-Windows-D3D9
const GUID kDxgiProvider = { 0xCA11C036, 0x0102, 0x4A2D, { 0xA6, 0xAD, 0xF0, 0x3C, 0xFE, 0xD5, 0xD3, 0xC9 } };
const GUID kD3D9Provider = { 0x783ACA0A, 0x790E, 0x4D7F, { 0x84, 0x51, 0xAA, 0x85, 0x05, 0x11, 0xC6, 0xB9 } };
constexpr USHORT kDxgiPresentStart = 42;   // Present_Start (paired with Present_Stop = 43)
constexpr USHORT kD3D9PresentStart = 1;

std::vector<BYTE> MakeProperties(const std::wstring& name) {
    std::vector<BYTE> buffer(sizeof(EVENT_TRACE_PROPERTIES) + (name.size() + 1) * sizeof(wchar_t) * 2);
    auto* p = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buffer.data());
    p->Wnode.BufferSize = static_cast<ULONG>(buffer.size());
    p->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    p->Wnode.ClientContext = 1;   // QueryPerformanceCounter timestamps
    p->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    p->FlushTimer = 1;
    p->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    return buffer;
}

void WINAPI EventCallback(PEVENT_RECORD record) {
    if (record && record->UserContext) static_cast<DxgiPresentWatcher*>(record->UserContext)->OnEvent(record);
}

} // namespace

DxgiPresentWatcher::DxgiPresentWatcher(uint32_t targetPid, std::function<void(double)> callback)
    : pid(targetPid), onFrame(std::move(callback)) {
    sessionName = L"PlaytimeFpsMon_" + std::to_wstring(targetPid) + L"_" + std::to_wstring(GetTickCount64());
    LARGE_INTEGER freq{}, now{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    qpcToMs = 1000.0 / static_cast<double>(freq.QuadPart);
    startQpc = now.QuadPart;
}

DxgiPresentWatcher::~DxgiPresentWatcher() { Stop(); }

bool DxgiPresentWatcher::Start() {
    auto props = MakeProperties(sessionName);
    TRACEHANDLE handle = 0;
    ULONG status = StartTraceW(&handle, sessionName.c_str(), reinterpret_cast<EVENT_TRACE_PROPERTIES*>(props.data()));
    if (status == ERROR_ALREADY_EXISTS) {
        auto stop = MakeProperties(sessionName);
        ControlTraceW(0, sessionName.c_str(), reinterpret_cast<EVENT_TRACE_PROPERTIES*>(stop.data()), EVENT_TRACE_CONTROL_STOP);
        props = MakeProperties(sessionName);
        status = StartTraceW(&handle, sessionName.c_str(), reinterpret_cast<EVENT_TRACE_PROPERTIES*>(props.data()));
    }
    if (status != ERROR_SUCCESS) return false;
    session = handle;
    for (const GUID* provider : { &kDxgiProvider, &kD3D9Provider })
        EnableTraceEx2(handle, provider, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_VERBOSE, ~0ull, 0, 0, nullptr);

    EVENT_TRACE_LOGFILEW log{};
    log.LoggerName = const_cast<wchar_t*>(sessionName.c_str());
    log.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
    log.EventRecordCallback = &EventCallback;
    log.Context = this;
    const TRACEHANDLE opened = OpenTraceW(&log);
    if (opened == INVALID_PROCESSTRACE_HANDLE) {
        Stop();
        return false;
    }
    consumer = opened;
    running = true;
    processThread = std::thread([this] {
        TRACEHANDLE h = consumer;
        ProcessTrace(&h, 1, nullptr, nullptr);
    });
    // Real-time sessions only deliver events when buffers flush; flushing every 100 ms keeps the
    // stream smooth instead of multi-second bursts (which read as 0 FPS, then a spike).
    flushThread = std::thread([this] {
        while (running) {
            Sleep(100);
            if (!running) break;
            auto flush = MakeProperties(sessionName);
            ControlTraceW(session, nullptr, reinterpret_cast<EVENT_TRACE_PROPERTIES*>(flush.data()), EVENT_TRACE_CONTROL_FLUSH);
        }
    });
    return true;
}

void DxgiPresentWatcher::Stop() {
    running = false;
    if (flushThread.joinable()) flushThread.join();
    if (session) {
        auto props = MakeProperties(sessionName);
        ControlTraceW(session, nullptr, reinterpret_cast<EVENT_TRACE_PROPERTIES*>(props.data()), EVENT_TRACE_CONTROL_STOP);
        session = 0;
    }
    if (consumer) {
        CloseTrace(consumer);
        consumer = 0;
    }
    if (processThread.joinable()) {
        // ProcessTrace returns once the session stops / the handle closes
        processThread.join();
    }
}

void DxgiPresentWatcher::OnEvent(const void* raw) {
    const auto* r = static_cast<const EVENT_RECORD*>(raw);
    if (r->EventHeader.ProcessId != pid) return;
    const USHORT id = r->EventHeader.EventDescriptor.Id;
    const bool dxgi = IsEqualGUID(r->EventHeader.ProviderId, kDxgiProvider) != 0;
    const bool d3d9 = IsEqualGUID(r->EventHeader.ProviderId, kD3D9Provider) != 0;
    if ((dxgi && id == kDxgiPresentStart) || (d3d9 && id == kD3D9PresentStart)) {
        const double ms = static_cast<double>(r->EventHeader.TimeStamp.QuadPart - startQpc) * qpcToMs;
        if (onFrame) onFrame(ms);
    }
}

} // namespace SpeedrunLauncher::Services
