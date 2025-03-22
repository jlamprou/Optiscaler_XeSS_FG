#pragma once

#include "pch.h"
#include "Util.h"
#include "Config.h"
#include "Logger.h"

#include <xell.h>
#include <xell_d3d12.h>
#include "detours/detours.h"

typedef xell_result_t(*PFN_xellD3D12CreateContext)(ID3D12Device* device, xell_context_handle_t* out_context);
typedef xell_result_t(*PFN_xellDestroyContext)(xell_context_handle_t context);
typedef xell_result_t(*PFN_xellSetSleepMode)(xell_context_handle_t context, const xell_sleep_params_t* param);
typedef xell_result_t(*PFN_xellGetSleepMode)(xell_context_handle_t context, xell_sleep_params_t* param);
typedef xell_result_t(*PFN_xellSleep)(xell_context_handle_t context, uint32_t frame_id);
typedef xell_result_t(*PFN_xellAddMarkerData)(xell_context_handle_t context, uint32_t frame_id, xell_latency_marker_type_t marker);
typedef xell_result_t(*PFN_xellGetVersion)(xell_version_t* pVersion);
typedef xell_result_t(*PFN_xellSetLoggingCallback)(xell_context_handle_t hContext, xell_logging_level_t loggingLevel, xell_app_log_callback_t loggingCallback);
typedef xell_result_t(*PFN_xellGetFramesReports)(xell_context_handle_t context, xell_frame_report_t* outdata);

class XeLLProxy
{
private:
    inline static HMODULE _dll = nullptr;
    inline static xell_version_t _xellVersion{};
    inline static bool _initialized = false;

    inline static PFN_xellD3D12CreateContext _xellD3D12CreateContext = nullptr;
    inline static PFN_xellDestroyContext _xellDestroyContext = nullptr;
    inline static PFN_xellSetSleepMode _xellSetSleepMode = nullptr;
    inline static PFN_xellGetSleepMode _xellGetSleepMode = nullptr;
    inline static PFN_xellSleep _xellSleep = nullptr;
    inline static PFN_xellAddMarkerData _xellAddMarkerData = nullptr;
    inline static PFN_xellGetVersion _xellGetVersion = nullptr;
    inline static PFN_xellSetLoggingCallback _xellSetLoggingCallback = nullptr;
    inline static PFN_xellGetFramesReports _xellGetFramesReports = nullptr;

    inline static void LogCallback(const char* message, xell_logging_level_t level)
    {
        switch (level)
        {
            case XELL_LOGGING_LEVEL_DEBUG:
                LOG_DEBUG("[XeLL] {}", message);
                break;
            case XELL_LOGGING_LEVEL_INFO:
                LOG_INFO("[XeLL] {}", message);
                break;
            case XELL_LOGGING_LEVEL_WARNING:
                LOG_WARN("[XeLL] {}", message);
                break;
            case XELL_LOGGING_LEVEL_ERROR:
                LOG_ERROR("[XeLL] {}", message);
                break;
            default:
                LOG_INFO("[XeLL] {}", message);
                break;
        }
    }

public:
    static bool InitXeLL()
    {
        // if dll already loaded or functions already initialized
        if (_dll != nullptr || _xellD3D12CreateContext != nullptr)
            return true;

        spdlog::info("");

        State::Instance().upscalerDisableHook = true;

        auto dllPath = Util::DllPath();

        // we would like to prioritize file pointed at in ini
        if (Config::Instance()->XeLLLibrary.has_value())
        {
            std::filesystem::path cfgPath(Config::Instance()->XeLLLibrary.value().c_str());
            LOG_INFO("Trying to load libxell.dll from ini path: {}", cfgPath.string());

            if (!cfgPath.has_filename())
                cfgPath = cfgPath / L"libxell.dll";

            _dll = LoadLibrary(cfgPath.c_str());
        }

        if (_dll == nullptr)
        {
            std::filesystem::path libXellPath = dllPath.parent_path() / L"libxell.dll";
            LOG_INFO("Trying to load libxell.dll from dll path: {}", libXellPath.string());
            _dll = LoadLibrary(libXellPath.c_str());
        }

        if (_dll != nullptr)
        {
            _xellD3D12CreateContext = (PFN_xellD3D12CreateContext)GetProcAddress(_dll, "xellD3D12CreateContext");
            _xellDestroyContext = (PFN_xellDestroyContext)GetProcAddress(_dll, "xellDestroyContext");
            _xellSetSleepMode = (PFN_xellSetSleepMode)GetProcAddress(_dll, "xellSetSleepMode");
            _xellGetSleepMode = (PFN_xellGetSleepMode)GetProcAddress(_dll, "xellGetSleepMode");
            _xellSleep = (PFN_xellSleep)GetProcAddress(_dll, "xellSleep");
            _xellAddMarkerData = (PFN_xellAddMarkerData)GetProcAddress(_dll, "xellAddMarkerData");
            _xellGetVersion = (PFN_xellGetVersion)GetProcAddress(_dll, "xellGetVersion");
            _xellSetLoggingCallback = (PFN_xellSetLoggingCallback)GetProcAddress(_dll, "xellSetLoggingCallback");
            _xellGetFramesReports = (PFN_xellGetFramesReports)GetProcAddress(_dll, "xellGetFramesReports");
        }

        // if libxell not loaded with LoadLibrary, try with Detours
        if (_xellD3D12CreateContext == nullptr)
        {
            LOG_INFO("Trying to load libxell.dll with Detours");

            _xellD3D12CreateContext = (PFN_xellD3D12CreateContext)DetourFindFunction("libxell.dll", "xellD3D12CreateContext");
            _xellDestroyContext = (PFN_xellDestroyContext)DetourFindFunction("libxell.dll", "xellDestroyContext");
            _xellSetSleepMode = (PFN_xellSetSleepMode)DetourFindFunction("libxell.dll", "xellSetSleepMode");
            _xellGetSleepMode = (PFN_xellGetSleepMode)DetourFindFunction("libxell.dll", "xellGetSleepMode");
            _xellSleep = (PFN_xellSleep)DetourFindFunction("libxell.dll", "xellSleep");
            _xellAddMarkerData = (PFN_xellAddMarkerData)DetourFindFunction("libxell.dll", "xellAddMarkerData");
            _xellGetVersion = (PFN_xellGetVersion)DetourFindFunction("libxell.dll", "xellGetVersion");
            _xellSetLoggingCallback = (PFN_xellSetLoggingCallback)DetourFindFunction("libxell.dll", "xellSetLoggingCallback");
            _xellGetFramesReports = (PFN_xellGetFramesReports)DetourFindFunction("libxell.dll", "xellGetFramesReports");
        }

        State::Instance().upscalerDisableHook = false;

        bool loadResult = (_xellD3D12CreateContext != nullptr);
        LOG_INFO("XeLL LoadResult: {}", loadResult);

        if (loadResult && _xellGetVersion != nullptr)
        {
            _xellGetVersion(&_xellVersion);
            LOG_INFO("XeLL Version: {}.{}.{}", _xellVersion.major, _xellVersion.minor, _xellVersion.patch);
        }

        _initialized = loadResult;
        return loadResult;
    }

    static xell_version_t Version()
    {
        // Initialize version if it's not already set
        if (_xellVersion.major == 0 && _xellGetVersion != nullptr)
        {
            _xellGetVersion(&_xellVersion);
            LOG_INFO("XeLL Version: {}.{}.{}", _xellVersion.major, _xellVersion.minor, _xellVersion.patch);
        }

        return _xellVersion;
    }

    static bool IsInitialized()
    {
        return _initialized;
    }

    static PFN_xellD3D12CreateContext D3D12CreateContext() { return _xellD3D12CreateContext; }
    static PFN_xellDestroyContext DestroyContext() { return _xellDestroyContext; }
    static PFN_xellSetSleepMode SetSleepMode() { return _xellSetSleepMode; }
    static PFN_xellGetSleepMode GetSleepMode() { return _xellGetSleepMode; }
    static PFN_xellSleep Sleep() { return _xellSleep; }
    static PFN_xellAddMarkerData AddMarkerData() { return _xellAddMarkerData; }
    static PFN_xellGetVersion GetVersion() { return _xellGetVersion; }
    static PFN_xellSetLoggingCallback SetLoggingCallback() { return _xellSetLoggingCallback; }
    static PFN_xellGetFramesReports GetFramesReports() { return _xellGetFramesReports; }

    static void SetupLogging(xell_context_handle_t context)
    {
        if (_initialized && _xellSetLoggingCallback != nullptr && context != nullptr)
        {
            _xellSetLoggingCallback(context, Config::Instance()->LogLevel < 2 ? XELL_LOGGING_LEVEL_DEBUG : XELL_LOGGING_LEVEL_WARNING, LogCallback);
        }
    }

    static std::string ResultToString(xell_result_t result)
    {
        switch (result)
        {
            case XELL_RESULT_SUCCESS: return "Success";
            case XELL_RESULT_ERROR_UNSUPPORTED_DEVICE: return "Unsupported device";
            case XELL_RESULT_ERROR_UNSUPPORTED_DRIVER: return "Unsupported driver";
            case XELL_RESULT_ERROR_UNINITIALIZED: return "Uninitialized";
            case XELL_RESULT_ERROR_INVALID_ARGUMENT: return "Invalid argument";
            case XELL_RESULT_ERROR_DEVICE: return "Device error";
            case XELL_RESULT_ERROR_NOT_IMPLEMENTED: return "Not implemented";
            case XELL_RESULT_ERROR_INVALID_CONTEXT: return "Invalid context";
            case XELL_RESULT_ERROR_UNSUPPORTED: return "Unsupported";
            case XELL_RESULT_ERROR_UNKNOWN: return "Unknown error";
            default: return "Unknown result code";
        }
    }
};