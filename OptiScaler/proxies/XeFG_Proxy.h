#pragma once

#include "pch.h"
#include "Util.h"
#include "Config.h"
#include "Logger.h"

#include <xefg_swapchain.h>
#include <xefg_swapchain_d3d12.h>
#include <xefg_swapchain_debug.h>
#include "detours/detours.h"

// Function pointer types for XeSS-FG API
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainGetVersion)(xefg_swapchain_version_t* pVersion);
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainGetProperties)(xefg_swapchain_handle_t hSwapChain, xefg_swapchain_properties_t* pProperties);
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainTagFrameConstants)(xefg_swapchain_handle_t hSwapChain, uint32_t presentId, const xefg_swapchain_frame_constant_data_t* pConstants);
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainSetEnabled)(xefg_swapchain_handle_t hSwapChain, uint32_t enable);
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainSetPresentId)(xefg_swapchain_handle_t hSwapChain, uint32_t presentId);
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainGetLastPresentStatus)(xefg_swapchain_handle_t hSwapChain, xefg_swapchain_present_status_t* pPresentStatus);
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainSetLoggingCallback)(xefg_swapchain_handle_t hSwapChain, xefg_swapchain_logging_level_t loggingLevel, xefg_swapchain_app_log_callback_t loggingCallback, void* userData);
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainDestroy)(xefg_swapchain_handle_t hSwapChain);
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainSetLatencyReduction)(xefg_swapchain_handle_t hSwapChain, void* hXeLLContext);
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainSetSceneChangeThreshold)(xefg_swapchain_handle_t hSwapChain, float threshold);
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainGetPipelineBuildStatus)(xefg_swapchain_handle_t hSwapChain);

// D3D12 specific functions
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainD3D12CreateContext)(ID3D12Device* pDevice, xefg_swapchain_handle_t* phSwapChain);
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainD3D12BuildPipelines)(xefg_swapchain_handle_t hSwapChain, ID3D12PipelineLibrary* pPipelineLibrary, bool blocking, uint32_t initFlags);
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainD3D12InitFromSwapChain)(xefg_swapchain_handle_t hSwapChain, ID3D12CommandQueue* pCmdQueue, const xefg_swapchain_d3d12_init_params_t* pInitParams);
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainD3D12InitFromSwapChainDesc)(xefg_swapchain_handle_t hSwapChain, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1* pSwapChainDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc, ID3D12CommandQueue* pCmdQueue, IDXGIFactory2* pDxgiFactory, const xefg_swapchain_d3d12_init_params_t* pInitParams);
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainD3D12GetSwapChainPtr)(xefg_swapchain_handle_t hSwapChain, REFIID riid, void** ppSwapChain);
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainD3D12TagFrameResource)(xefg_swapchain_handle_t hSwapChain, ID3D12CommandList* pCmdList, uint32_t presentId, const xefg_swapchain_d3d12_resource_data_t* pResData);
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainD3D12SetDescriptorHeap)(xefg_swapchain_handle_t hSwapChain, ID3D12DescriptorHeap* pDescriptorHeap, uint32_t descriptorHeapOffsetInBytes);

// Debug functions
typedef xefg_swapchain_result_t(*PFN_xefgSwapChainEnableDebugFeature)(xefg_swapchain_handle_t hSwapChain, xefg_swapchain_debug_feature_t featureId, uint32_t enable, void* pArgument);

/**
 * @brief Proxy class for Intel XeSS Frame Generation API
 *
 * This class handles loading the XeSS-FG library and initializing function pointers.
 * It provides static methods to access the XeSS-FG API functions.
 */
class XeFGProxy
{
private:
    inline static HMODULE _dll = nullptr;
    inline static xefg_swapchain_version_t _xefgVersion{};
    inline static bool _initialized = false;

    // Core API function pointers
    inline static PFN_xefgSwapChainGetVersion _xefgSwapChainGetVersion = nullptr;
    inline static PFN_xefgSwapChainGetProperties _xefgSwapChainGetProperties = nullptr;
    inline static PFN_xefgSwapChainTagFrameConstants _xefgSwapChainTagFrameConstants = nullptr;
    inline static PFN_xefgSwapChainSetEnabled _xefgSwapChainSetEnabled = nullptr;
    inline static PFN_xefgSwapChainSetPresentId _xefgSwapChainSetPresentId = nullptr;
    inline static PFN_xefgSwapChainGetLastPresentStatus _xefgSwapChainGetLastPresentStatus = nullptr;
    inline static PFN_xefgSwapChainSetLoggingCallback _xefgSwapChainSetLoggingCallback = nullptr;
    inline static PFN_xefgSwapChainDestroy _xefgSwapChainDestroy = nullptr;
    inline static PFN_xefgSwapChainSetLatencyReduction _xefgSwapChainSetLatencyReduction = nullptr;
    inline static PFN_xefgSwapChainSetSceneChangeThreshold _xefgSwapChainSetSceneChangeThreshold = nullptr;
    inline static PFN_xefgSwapChainGetPipelineBuildStatus _xefgSwapChainGetPipelineBuildStatus = nullptr;

    // D3D12 specific function pointers
    inline static PFN_xefgSwapChainD3D12CreateContext _xefgSwapChainD3D12CreateContext = nullptr;
    inline static PFN_xefgSwapChainD3D12BuildPipelines _xefgSwapChainD3D12BuildPipelines = nullptr;
    inline static PFN_xefgSwapChainD3D12InitFromSwapChain _xefgSwapChainD3D12InitFromSwapChain = nullptr;
    inline static PFN_xefgSwapChainD3D12InitFromSwapChainDesc _xefgSwapChainD3D12InitFromSwapChainDesc = nullptr;
    inline static PFN_xefgSwapChainD3D12GetSwapChainPtr _xefgSwapChainD3D12GetSwapChainPtr = nullptr;
    inline static PFN_xefgSwapChainD3D12TagFrameResource _xefgSwapChainD3D12TagFrameResource = nullptr;
    inline static PFN_xefgSwapChainD3D12SetDescriptorHeap _xefgSwapChainD3D12SetDescriptorHeap = nullptr;

    // Debug function pointers
    inline static PFN_xefgSwapChainEnableDebugFeature _xefgSwapChainEnableDebugFeature = nullptr;

    // Logging callback
    inline static void LogCallback(const char* message, xefg_swapchain_logging_level_t level, void* userData)
    {
        switch (level)
        {
        case XEFG_SWAPCHAIN_LOGGING_LEVEL_DEBUG:
            LOG_DEBUG("[XeSS-FG] {}", message);
            break;
        case XEFG_SWAPCHAIN_LOGGING_LEVEL_INFO:
            LOG_INFO("[XeSS-FG] {}", message);
            break;
        case XEFG_SWAPCHAIN_LOGGING_LEVEL_WARNING:
            LOG_WARN("[XeSS-FG] {}", message);
            break;
        case XEFG_SWAPCHAIN_LOGGING_LEVEL_ERROR:
            LOG_ERROR("[XeSS-FG] {}", message);
            break;
        default:
            LOG_INFO("[XeSS-FG] {}", message);
            break;
        }
    }

public:
    /**
     * @brief Initialize XeSS-FG by loading the library and resolving function pointers
     *
     * @return true if initialization was successful, false otherwise
     */
    static bool InitXeFG()
    {
        // If dll already loaded or functions already initialized
        if (_dll != nullptr || _xefgSwapChainGetVersion != nullptr)
            return true;

        spdlog::info("");

        State::Instance().upscalerDisableHook = true;

        auto dllPath = Util::DllPath();

        // Try to load from path specified in ini
        if (Config::Instance()->XeFGLibrary.has_value())
        {
            std::filesystem::path cfgPath(Config::Instance()->XeFGLibrary.value().c_str());
            LOG_INFO("Trying to load libxess_fg.dll from ini path: {}", cfgPath.string());

            if (!cfgPath.has_filename())
                cfgPath = cfgPath / L"libxess_fg.dll";

            _dll = LoadLibrary(cfgPath.c_str());
        }

        // Try to load from application directory
        if (_dll == nullptr)
        {
            std::filesystem::path libPath = dllPath.parent_path() / L"libxess_fg.dll";
            LOG_INFO("Trying to load libxess_fg.dll from dll path: {}", libPath.string());
            _dll = LoadLibrary(libPath.c_str());
        }

        // If loaded successfully, get function pointers
        if (_dll != nullptr)
        {
            // Core API functions
            _xefgSwapChainGetVersion = (PFN_xefgSwapChainGetVersion)GetProcAddress(_dll, "xefgSwapChainGetVersion");
            _xefgSwapChainGetProperties = (PFN_xefgSwapChainGetProperties)GetProcAddress(_dll, "xefgSwapChainGetProperties");
            _xefgSwapChainTagFrameConstants = (PFN_xefgSwapChainTagFrameConstants)GetProcAddress(_dll, "xefgSwapChainTagFrameConstants");
            _xefgSwapChainSetEnabled = (PFN_xefgSwapChainSetEnabled)GetProcAddress(_dll, "xefgSwapChainSetEnabled");
            _xefgSwapChainSetPresentId = (PFN_xefgSwapChainSetPresentId)GetProcAddress(_dll, "xefgSwapChainSetPresentId");
            _xefgSwapChainGetLastPresentStatus = (PFN_xefgSwapChainGetLastPresentStatus)GetProcAddress(_dll, "xefgSwapChainGetLastPresentStatus");
            _xefgSwapChainSetLoggingCallback = (PFN_xefgSwapChainSetLoggingCallback)GetProcAddress(_dll, "xefgSwapChainSetLoggingCallback");
            _xefgSwapChainDestroy = (PFN_xefgSwapChainDestroy)GetProcAddress(_dll, "xefgSwapChainDestroy");
            _xefgSwapChainSetLatencyReduction = (PFN_xefgSwapChainSetLatencyReduction)GetProcAddress(_dll, "xefgSwapChainSetLatencyReduction");
            _xefgSwapChainSetSceneChangeThreshold = (PFN_xefgSwapChainSetSceneChangeThreshold)GetProcAddress(_dll, "xefgSwapChainSetSceneChangeThreshold");
            _xefgSwapChainGetPipelineBuildStatus = (PFN_xefgSwapChainGetPipelineBuildStatus)GetProcAddress(_dll, "xefgSwapChainGetPipelineBuildStatus");

            // D3D12 specific functions
            _xefgSwapChainD3D12CreateContext = (PFN_xefgSwapChainD3D12CreateContext)GetProcAddress(_dll, "xefgSwapChainD3D12CreateContext");
            _xefgSwapChainD3D12BuildPipelines = (PFN_xefgSwapChainD3D12BuildPipelines)GetProcAddress(_dll, "xefgSwapChainD3D12BuildPipelines");
            _xefgSwapChainD3D12InitFromSwapChain = (PFN_xefgSwapChainD3D12InitFromSwapChain)GetProcAddress(_dll, "xefgSwapChainD3D12InitFromSwapChain");
            _xefgSwapChainD3D12InitFromSwapChainDesc = (PFN_xefgSwapChainD3D12InitFromSwapChainDesc)GetProcAddress(_dll, "xefgSwapChainD3D12InitFromSwapChainDesc");
            _xefgSwapChainD3D12GetSwapChainPtr = (PFN_xefgSwapChainD3D12GetSwapChainPtr)GetProcAddress(_dll, "xefgSwapChainD3D12GetSwapChainPtr");
            _xefgSwapChainD3D12TagFrameResource = (PFN_xefgSwapChainD3D12TagFrameResource)GetProcAddress(_dll, "xefgSwapChainD3D12TagFrameResource");
            _xefgSwapChainD3D12SetDescriptorHeap = (PFN_xefgSwapChainD3D12SetDescriptorHeap)GetProcAddress(_dll, "xefgSwapChainD3D12SetDescriptorHeap");

            // Debug functions
            _xefgSwapChainEnableDebugFeature = (PFN_xefgSwapChainEnableDebugFeature)GetProcAddress(_dll, "xefgSwapChainEnableDebugFeature");
        }

        // Try with Detours if LoadLibrary failed
        if (_xefgSwapChainGetVersion == nullptr)
        {
            LOG_INFO("Trying to load libxess_fg.dll with Detours");

            // Core API functions
            _xefgSwapChainGetVersion = (PFN_xefgSwapChainGetVersion)DetourFindFunction("libxess_fg.dll", "xefgSwapChainGetVersion");
            _xefgSwapChainGetProperties = (PFN_xefgSwapChainGetProperties)DetourFindFunction("libxess_fg.dll", "xefgSwapChainGetProperties");
            _xefgSwapChainTagFrameConstants = (PFN_xefgSwapChainTagFrameConstants)DetourFindFunction("libxess_fg.dll", "xefgSwapChainTagFrameConstants");
            _xefgSwapChainSetEnabled = (PFN_xefgSwapChainSetEnabled)DetourFindFunction("libxess_fg.dll", "xefgSwapChainSetEnabled");
            _xefgSwapChainSetPresentId = (PFN_xefgSwapChainSetPresentId)DetourFindFunction("libxess_fg.dll", "xefgSwapChainSetPresentId");
            _xefgSwapChainGetLastPresentStatus = (PFN_xefgSwapChainGetLastPresentStatus)DetourFindFunction("libxess_fg.dll", "xefgSwapChainGetLastPresentStatus");
            _xefgSwapChainSetLoggingCallback = (PFN_xefgSwapChainSetLoggingCallback)DetourFindFunction("libxess_fg.dll", "xefgSwapChainSetLoggingCallback");
            _xefgSwapChainDestroy = (PFN_xefgSwapChainDestroy)DetourFindFunction("libxess_fg.dll", "xefgSwapChainDestroy");
            _xefgSwapChainSetLatencyReduction = (PFN_xefgSwapChainSetLatencyReduction)DetourFindFunction("libxess_fg.dll", "xefgSwapChainSetLatencyReduction");
            _xefgSwapChainSetSceneChangeThreshold = (PFN_xefgSwapChainSetSceneChangeThreshold)DetourFindFunction("libxess_fg.dll", "xefgSwapChainSetSceneChangeThreshold");
            _xefgSwapChainGetPipelineBuildStatus = (PFN_xefgSwapChainGetPipelineBuildStatus)DetourFindFunction("libxess_fg.dll", "xefgSwapChainGetPipelineBuildStatus");

            // D3D12 specific functions
            _xefgSwapChainD3D12CreateContext = (PFN_xefgSwapChainD3D12CreateContext)DetourFindFunction("libxess_fg.dll", "xefgSwapChainD3D12CreateContext");
            _xefgSwapChainD3D12BuildPipelines = (PFN_xefgSwapChainD3D12BuildPipelines)DetourFindFunction("libxess_fg.dll", "xefgSwapChainD3D12BuildPipelines");
            _xefgSwapChainD3D12InitFromSwapChain = (PFN_xefgSwapChainD3D12InitFromSwapChain)DetourFindFunction("libxess_fg.dll", "xefgSwapChainD3D12InitFromSwapChain");
            _xefgSwapChainD3D12InitFromSwapChainDesc = (PFN_xefgSwapChainD3D12InitFromSwapChainDesc)DetourFindFunction("libxess_fg.dll", "xefgSwapChainD3D12InitFromSwapChainDesc");
            _xefgSwapChainD3D12GetSwapChainPtr = (PFN_xefgSwapChainD3D12GetSwapChainPtr)DetourFindFunction("libxess_fg.dll", "xefgSwapChainD3D12GetSwapChainPtr");
            _xefgSwapChainD3D12TagFrameResource = (PFN_xefgSwapChainD3D12TagFrameResource)DetourFindFunction("libxess_fg.dll", "xefgSwapChainD3D12TagFrameResource");
            _xefgSwapChainD3D12SetDescriptorHeap = (PFN_xefgSwapChainD3D12SetDescriptorHeap)DetourFindFunction("libxess_fg.dll", "xefgSwapChainD3D12SetDescriptorHeap");

            // Debug functions
            _xefgSwapChainEnableDebugFeature = (PFN_xefgSwapChainEnableDebugFeature)DetourFindFunction("libxess_fg.dll", "xefgSwapChainEnableDebugFeature");
        }

        State::Instance().upscalerDisableHook = false;

        bool loadResult = (_xefgSwapChainGetVersion != nullptr && _xefgSwapChainD3D12CreateContext != nullptr);
        LOG_INFO("XeFG LoadResult: {}", loadResult);

        if (loadResult && _xefgSwapChainGetVersion != nullptr)
        {
            _xefgSwapChainGetVersion(&_xefgVersion);
            LOG_INFO("XeSS-FG Version: {}.{}.{}", _xefgVersion.major, _xefgVersion.minor, _xefgVersion.patch);
        }

        _initialized = loadResult;
        return loadResult;
    }

    /**
     * @brief Get the XeSS-FG version
     *
     * @return xefg_swapchain_version_t The version structure
     */
    static xefg_swapchain_version_t Version()
    {
        // Initialize version if it's not already set
        if (_xefgVersion.major == 0 && _xefgSwapChainGetVersion != nullptr)
        {
            _xefgSwapChainGetVersion(&_xefgVersion);
            LOG_INFO("XeSS-FG Version: {}.{}.{}", _xefgVersion.major, _xefgVersion.minor, _xefgVersion.patch);
        }

        return _xefgVersion;
    }

    /**
     * @brief Check if XeSS-FG is initialized
     *
     * @return true if initialized successfully, false otherwise
     */
    static bool IsInitialized()
    {
        return _initialized;
    }

    // Core API function accessors
    static PFN_xefgSwapChainGetVersion GetVersion() { return _xefgSwapChainGetVersion; }
    static PFN_xefgSwapChainGetProperties GetProperties() { return _xefgSwapChainGetProperties; }
    static PFN_xefgSwapChainTagFrameConstants TagFrameConstants() { return _xefgSwapChainTagFrameConstants; }
    static PFN_xefgSwapChainSetEnabled SetEnabled() { return _xefgSwapChainSetEnabled; }
    static PFN_xefgSwapChainSetPresentId SetPresentId() { return _xefgSwapChainSetPresentId; }
    static PFN_xefgSwapChainGetLastPresentStatus GetLastPresentStatus() { return _xefgSwapChainGetLastPresentStatus; }
    static PFN_xefgSwapChainSetLoggingCallback SetLoggingCallback() { return _xefgSwapChainSetLoggingCallback; }
    static PFN_xefgSwapChainDestroy Destroy() { return _xefgSwapChainDestroy; }
    static PFN_xefgSwapChainSetLatencyReduction SetLatencyReduction() { return _xefgSwapChainSetLatencyReduction; }
    static PFN_xefgSwapChainSetSceneChangeThreshold SetSceneChangeThreshold() { return _xefgSwapChainSetSceneChangeThreshold; }
    static PFN_xefgSwapChainGetPipelineBuildStatus GetPipelineBuildStatus() { return _xefgSwapChainGetPipelineBuildStatus; }

    // D3D12 specific function accessors
    static PFN_xefgSwapChainD3D12CreateContext D3D12CreateContext() { return _xefgSwapChainD3D12CreateContext; }
    static PFN_xefgSwapChainD3D12BuildPipelines D3D12BuildPipelines() { return _xefgSwapChainD3D12BuildPipelines; }
    static PFN_xefgSwapChainD3D12InitFromSwapChain D3D12InitFromSwapChain() { return _xefgSwapChainD3D12InitFromSwapChain; }
    static PFN_xefgSwapChainD3D12InitFromSwapChainDesc D3D12InitFromSwapChainDesc() { return _xefgSwapChainD3D12InitFromSwapChainDesc; }
    static PFN_xefgSwapChainD3D12GetSwapChainPtr D3D12GetSwapChainPtr() { return _xefgSwapChainD3D12GetSwapChainPtr; }
    static PFN_xefgSwapChainD3D12TagFrameResource D3D12TagFrameResource() { return _xefgSwapChainD3D12TagFrameResource; }
    static PFN_xefgSwapChainD3D12SetDescriptorHeap D3D12SetDescriptorHeap() { return _xefgSwapChainD3D12SetDescriptorHeap; }

    // Debug function accessors
    static PFN_xefgSwapChainEnableDebugFeature EnableDebugFeature() { return _xefgSwapChainEnableDebugFeature; }

    /**
     * @brief Set up logging for XeSS-FG
     *
     * @param swapChain The XeSS-FG swap chain handle
     */
    static void SetupLogging(xefg_swapchain_handle_t swapChain)
    {
        if (_initialized && _xefgSwapChainSetLoggingCallback != nullptr && swapChain != nullptr)
        {
            _xefgSwapChainSetLoggingCallback(swapChain,
                Config::Instance()->LogLevel < 2 ? XEFG_SWAPCHAIN_LOGGING_LEVEL_DEBUG : XEFG_SWAPCHAIN_LOGGING_LEVEL_WARNING,
                LogCallback, nullptr);
        }
    }

    /**
     * @brief Convert XeSS-FG result code to string for logging
     *
     * @param result The result code
     * @return std::string Human-readable description of the result
     */
    static std::string ResultToString(xefg_swapchain_result_t result)
    {
        switch (result)
        {
        case XEFG_SWAPCHAIN_RESULT_WARNING_OLD_DRIVER: return "Warning: Old driver";
        case XEFG_SWAPCHAIN_RESULT_WARNING_TOO_FEW_FRAMES: return "Warning: Too few frames";
        case XEFG_SWAPCHAIN_RESULT_WARNING_FRAMES_ID_MISMATCH: return "Warning: Frames ID mismatch";
        case XEFG_SWAPCHAIN_RESULT_WARNING_MISSING_PRESENT_STATUS: return "Warning: Missing present status";
        case XEFG_SWAPCHAIN_RESULT_WARNING_RESOURCE_SIZES_MISMATCH: return "Warning: Resource sizes mismatch";
        case XEFG_SWAPCHAIN_RESULT_SUCCESS: return "Success";
        case XEFG_SWAPCHAIN_RESULT_ERROR_UNSUPPORTED_DEVICE: return "Error: Unsupported device";
        case XEFG_SWAPCHAIN_RESULT_ERROR_UNSUPPORTED_DRIVER: return "Error: Unsupported driver";
        case XEFG_SWAPCHAIN_RESULT_ERROR_UNINITIALIZED: return "Error: Uninitialized";
        case XEFG_SWAPCHAIN_RESULT_ERROR_INVALID_ARGUMENT: return "Error: Invalid argument";
        case XEFG_SWAPCHAIN_RESULT_ERROR_DEVICE_OUT_OF_MEMORY: return "Error: Device out of memory";
        case XEFG_SWAPCHAIN_RESULT_ERROR_DEVICE: return "Error: Device error";
        case XEFG_SWAPCHAIN_RESULT_ERROR_NOT_IMPLEMENTED: return "Error: Not implemented";
        case XEFG_SWAPCHAIN_RESULT_ERROR_INVALID_CONTEXT: return "Error: Invalid context";
        case XEFG_SWAPCHAIN_RESULT_ERROR_OPERATION_IN_PROGRESS: return "Error: Operation in progress";
        case XEFG_SWAPCHAIN_RESULT_ERROR_UNSUPPORTED: return "Error: Unsupported";
        case XEFG_SWAPCHAIN_RESULT_ERROR_CANT_LOAD_LIBRARY: return "Error: Can't load library";
        case XEFG_SWAPCHAIN_RESULT_ERROR_MISMATCH_INPUT_RESOURCES: return "Error: Mismatch input resources";
        case XEFG_SWAPCHAIN_RESULT_ERROR_INCORRECT_OUTPUT_RESOURCES: return "Error: Incorrect output resources";
        case XEFG_SWAPCHAIN_RESULT_ERROR_INCORRECT_INPUT_RESOURCES: return "Error: Incorrect input resources";
        case XEFG_SWAPCHAIN_RESULT_ERROR_LATENCY_REDUCTION_UNSUPPORTED: return "Error: Latency reduction unsupported";
        case XEFG_SWAPCHAIN_RESULT_ERROR_LATENCY_REDUCTION_FUNCTION_MISSING: return "Error: Latency reduction function missing";
        case XEFG_SWAPCHAIN_RESULT_ERROR_HRESULT_FAILURE: return "Error: HRESULT failure";
        case XEFG_SWAPCHAIN_RESULT_ERROR_DXGI_INVALID_CALL: return "Error: DXGI invalid call";
        case XEFG_SWAPCHAIN_RESULT_ERROR_POINTER_STILL_IN_USE: return "Error: Pointer still in use";
        case XEFG_SWAPCHAIN_RESULT_ERROR_INVALID_DESCRIPTOR_HEAP: return "Error: Invalid descriptor heap";
        case XEFG_SWAPCHAIN_RESULT_ERROR_WRONG_CALL_ORDER: return "Error: Wrong call order";
        case XEFG_SWAPCHAIN_RESULT_ERROR_UNKNOWN: return "Error: Unknown";
        default: return "Unknown result code";
        }
    }
};