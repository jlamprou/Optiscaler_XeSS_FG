#include "XeFG_Dx12.h"

#include <State.h>
#include <Config.h>
#include <menu/menu_overlay_dx.h>

// Also need to update the destructor to ensure proper cleanup order
XeFG_Dx12::~XeFG_Dx12()
{
    LOG_DEBUG("XeFG_Dx12 destructor called");

    // Ensure proper destruction order
    StopAndDestroyContext(true, true, true);
}


UINT64 XeFG_Dx12::UpscaleStart()
{
    LOG_DEBUG("Upsclale Start");
    // Add sleep for the current frame at the beginning
    if (_xellContext != nullptr && XeLLProxy::IsInitialized() && XeLLProxy::Sleep())
    {
        XeLLProxy::Sleep()(_xellContext, static_cast<uint32_t>(_frameCount));
    }
    _frameCount++;

    if (IsActive())
    {
        auto frameIndex = GetIndex();
        auto allocator = _commandAllocators[frameIndex];
        auto result = allocator->Reset();
        result = _commandList[frameIndex]->Reset(allocator, nullptr);
        LOG_DEBUG("_commandList[{}]->Reset()", frameIndex);
    }

    return _frameCount;
}

void XeFG_Dx12::UpscaleEnd()
{
    LOG_DEBUG("Upsclale end");
}

feature_version XeFG_Dx12::Version()
{
    if (XeFGProxy::InitXeFG())
    {
        xefg_swapchain_version_t ver = XeFGProxy::Version();
        return { ver.major, ver.minor, ver.patch };
    }

    return { 0, 0, 0 };
}

const char* XeFG_Dx12::Name()
{
    return "XeSS-FG";
}

bool XeFG_Dx12::Dispatch(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* output, double frameTime)
{
    LOG_DEBUG("(XeFG) running, frame: {0}", _frameCount);
    auto frameIndex = _frameCount % BUFFER_COUNT;
    uint32_t currentFrameId = static_cast<uint32_t>(_frameCount);

    // Mark the simulation phase start - typically should happen earlier in the frame,
    // but we can at least place it at the start of our function
    MarkXeLLSimulationStart(currentFrameId);

    if (Config::Instance()->FGUseMutexForSwaphain.value_or_default())
    {
        LOG_TRACE("Waiting Mutex 1, current: {}", Mutex.getOwner());
        Mutex.lock(1);
        LOG_TRACE("Acquired Mutex: {}", Mutex.getOwner());
    }

    // Mark simulation end - in a real implementation, this would happen after CPU work is done
    MarkXeLLSimulationEnd(currentFrameId);

    // Mark render submit start - we're about to prepare frame data for the GPU
    MarkXeLLRenderSubmitStart(currentFrameId);

    // Prepare frame constants
    xefg_swapchain_frame_constant_data_t frameConstants = {};

    // Create identity matrices if not provided
    for (int i = 0; i < 16; i++)
    {
        frameConstants.viewMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f; // Identity matrix
        frameConstants.projectionMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f; // Identity matrix
    }

    // Set jitter and motion vector scale from base class
    frameConstants.jitterOffsetX = _jitterX;
    frameConstants.jitterOffsetY = _jitterY;
    frameConstants.motionVectorScaleX = _mvScaleX;
    frameConstants.motionVectorScaleY = _mvScaleY;
    frameConstants.resetHistory = _reset;
    frameConstants.frameRenderTime = static_cast<float>(frameTime);

    // Tag frame constants
    if (XeFGProxy::TagFrameConstants() && _xefgSwapChain)
    {
        XeFGProxy::TagFrameConstants()(_xefgSwapChain, currentFrameId, &frameConstants);
    }

    // Tag resources
    if (XeFGProxy::D3D12TagFrameResource() && _xefgSwapChain)
    {
        // Determine resource validity based on command list
        auto validity = GetResourceValidity(cmdList);

        // Tag motion vectors
        if (_paramVelocity[frameIndex])
        {
            xefg_swapchain_d3d12_resource_data_t velocityData = {};
            velocityData.type = XEFG_SWAPCHAIN_RES_MOTION_VECTOR;
            velocityData.validity = validity;
            velocityData.resourceBase = { 0, 0 };

            auto desc = _paramVelocity[frameIndex]->GetDesc();
            velocityData.resourceSize = { static_cast<uint32_t>(desc.Width), static_cast<uint32_t>(desc.Height) };
            velocityData.pResource = _paramVelocity[frameIndex];
            velocityData.incomingState = validity == XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT ?
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE;

            xefg_swapchain_result_t result = XeFGProxy::D3D12TagFrameResource()(_xefgSwapChain, cmdList, currentFrameId, &velocityData);
            if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
            {
                LOG_WARN("Failed to tag motion vectors: {}", XeFGProxy::ResultToString(result));
            }
        }

        // Tag depth
        if (_paramDepth[frameIndex])
        {
            xefg_swapchain_d3d12_resource_data_t depthData = {};
            depthData.type = XEFG_SWAPCHAIN_RES_DEPTH;
            depthData.validity = validity;
            depthData.resourceBase = { 0, 0 };

            auto desc = _paramDepth[frameIndex]->GetDesc();
            depthData.resourceSize = { static_cast<uint32_t>(desc.Width), static_cast<uint32_t>(desc.Height) };
            depthData.pResource = _paramDepth[frameIndex];
            depthData.incomingState = validity == XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT ?
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE;

            xefg_swapchain_result_t result = XeFGProxy::D3D12TagFrameResource()(_xefgSwapChain, cmdList, currentFrameId, &depthData);
            if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
            {
                LOG_WARN("Failed to tag depth: {}", XeFGProxy::ResultToString(result));
            }
        }

        // Tag output if provided
        if (output)
        {
            DXGI_SWAP_CHAIN_DESC scDesc{};
            if (_swapChain && _swapChain->GetDesc(&scDesc) == S_OK)
            {
                auto desc = output->GetDesc();
                if (desc.Format == scDesc.BufferDesc.Format)
                {
                    xefg_swapchain_d3d12_resource_data_t outputData = {};
                    outputData.type = XEFG_SWAPCHAIN_RES_BACKBUFFER;
                    outputData.validity = validity;
                    outputData.resourceBase = { 0, 0 };
                    outputData.resourceSize = { static_cast<uint32_t>(desc.Width), static_cast<uint32_t>(desc.Height) };
                    outputData.pResource = output;
                    outputData.incomingState = validity == XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT ?
                        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE;

                    xefg_swapchain_result_t result = XeFGProxy::D3D12TagFrameResource()(_xefgSwapChain, cmdList, currentFrameId, &outputData);
                    if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
                    {
                        LOG_WARN("Failed to tag output: {}", XeFGProxy::ResultToString(result));
                    }
                }
            }
        }
    }

    // Mark render submit end - we've finished preparing the GPU workload
    MarkXeLLRenderSubmitEnd(currentFrameId);

    // Set present ID for XeSS-FG
    if (XeFGProxy::SetPresentId() && _xefgSwapChain)
    {
        XeFGProxy::SetPresentId()(_xefgSwapChain, currentFrameId);
    }

    if (Config::Instance()->FGUseMutexForSwaphain.value_or_default())
    {
        LOG_TRACE("Releasing Mutex: {}", Mutex.getOwner());
        Mutex.unlockThis(1);
    }

    return true;
}

bool XeFG_Dx12::DispatchHudless(bool useHudless, double frameTime)
{
    LOG_DEBUG("useHudless: {}, frameTime: {}", useHudless, frameTime);
    uint32_t currentFrameId = static_cast<uint32_t>(_frameCount);

    // Mark the simulation phase start
    MarkXeLLSimulationStart(currentFrameId);

    if (Config::Instance()->FGUseMutexForSwaphain.value_or_default() && Mutex.getOwner() != 2)
    {
        LOG_TRACE("Waiting XeFG->Mutex 1, current: {}", Mutex.getOwner());
        Mutex.lock(1);
        LOG_TRACE("Acquired XeFG->Mutex: {}", Mutex.getOwner());
    }

    // Mark simulation end
    MarkXeLLSimulationEnd(currentFrameId);

    // Mark render submit start
    MarkXeLLRenderSubmitStart(currentFrameId);

    // hudless captured for this frame
    auto fIndex = GetIndex();

    if (useHudless && _paramHudless[fIndex] != nullptr)
    {
        LOG_TRACE("Using hudless: {:X}", (size_t)_paramHudless[fIndex]);

        // Determine resource validity
        auto validity = XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;

        // Tag hudless resource
        if (XeFGProxy::D3D12TagFrameResource() && _xefgSwapChain)
        {
            xefg_swapchain_d3d12_resource_data_t hudlessData = {};
            hudlessData.type = XEFG_SWAPCHAIN_RES_HUDLESS_COLOR;
            hudlessData.validity = validity;
            hudlessData.resourceBase = { 0, 0 };

            auto desc = _paramHudless[fIndex]->GetDesc();
            hudlessData.resourceSize = { static_cast<uint32_t>(desc.Width), static_cast<uint32_t>(desc.Height) };
            hudlessData.pResource = _paramHudless[fIndex];
            hudlessData.incomingState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

            xefg_swapchain_result_t result = XeFGProxy::D3D12TagFrameResource()(_xefgSwapChain, _commandList[fIndex], currentFrameId, &hudlessData);
            if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
            {
                LOG_WARN("Failed to tag hudless: {}", XeFGProxy::ResultToString(result));
            }
        }
    }

    // Prepare frame constants
    xefg_swapchain_frame_constant_data_t frameConstants = {};

    // Create identity matrices if not provided
    for (int i = 0; i < 16; i++)
    {
        frameConstants.viewMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f; // Identity matrix
        frameConstants.projectionMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f; // Identity matrix
    }

    // Set jitter and motion vector scale from base class
    frameConstants.jitterOffsetX = _jitterX;
    frameConstants.jitterOffsetY = _jitterY;
    frameConstants.motionVectorScaleX = _mvScaleX;
    frameConstants.motionVectorScaleY = _mvScaleY;
    frameConstants.resetHistory = _reset;
    frameConstants.frameRenderTime = static_cast<float>(frameTime);

    // Tag frame constants
    if (XeFGProxy::TagFrameConstants() && _xefgSwapChain)
    {
        XeFGProxy::TagFrameConstants()(_xefgSwapChain, currentFrameId, &frameConstants);
    }

    // Tag resources
    if (XeFGProxy::D3D12TagFrameResource() && _xefgSwapChain)
    {
        // Tag motion vectors
        if (_paramVelocity[fIndex])
        {
            xefg_swapchain_d3d12_resource_data_t velocityData = {};
            velocityData.type = XEFG_SWAPCHAIN_RES_MOTION_VECTOR;
            velocityData.validity = XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;
            velocityData.resourceBase = { 0, 0 };

            auto desc = _paramVelocity[fIndex]->GetDesc();
            velocityData.resourceSize = { static_cast<uint32_t>(desc.Width), static_cast<uint32_t>(desc.Height) };
            velocityData.pResource = _paramVelocity[fIndex];
            velocityData.incomingState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

            xefg_swapchain_result_t result = XeFGProxy::D3D12TagFrameResource()(_xefgSwapChain, _commandList[fIndex], currentFrameId, &velocityData);
            if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
            {
                LOG_WARN("Failed to tag motion vectors: {}", XeFGProxy::ResultToString(result));
            }
        }

        // Tag depth
        if (_paramDepth[fIndex])
        {
            xefg_swapchain_d3d12_resource_data_t depthData = {};
            depthData.type = XEFG_SWAPCHAIN_RES_DEPTH;
            depthData.validity = XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;
            depthData.resourceBase = { 0, 0 };

            auto desc = _paramDepth[fIndex]->GetDesc();
            depthData.resourceSize = { static_cast<uint32_t>(desc.Width), static_cast<uint32_t>(desc.Height) };
            depthData.pResource = _paramDepth[fIndex];
            depthData.incomingState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

            xefg_swapchain_result_t result = XeFGProxy::D3D12TagFrameResource()(_xefgSwapChain, _commandList[fIndex], currentFrameId, &depthData);
            if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
            {
                LOG_WARN("Failed to tag depth: {}", XeFGProxy::ResultToString(result));
            }
        }
    }

    // Mark render submit end
    MarkXeLLRenderSubmitEnd(currentFrameId);

    // Set present ID for XeSS-FG
    if (XeFGProxy::SetPresentId() && _xefgSwapChain)
    {
        XeFGProxy::SetPresentId()(_xefgSwapChain, currentFrameId);
    }

    if (Config::Instance()->FGUseMutexForSwaphain.value_or_default() && Mutex.getOwner() == 1)
    {
        LOG_TRACE("Releasing XeFG->Mutex: {}", Mutex.getOwner());
        Mutex.unlockThis(1);
    }

    return true;
}

void XeFG_Dx12::StopAndDestroyContext(bool destroy, bool shutDown, bool useMutex)
{
    _frameCount = 0;

    LOG_DEBUG("");

    if (Config::Instance()->FGUseMutexForSwaphain.value_or_default() && useMutex)
    {
        LOG_TRACE("Waiting Mutex 1, current: {}", Mutex.getOwner());
        Mutex.lock(1);
        LOG_TRACE("Acquired Mutex: {}", Mutex.getOwner());
    }

    if (!(shutDown || State::Instance().isShuttingDown))
    {
        // Disable XeSS-FG if it's currently active
        if (_xefgSwapChain != nullptr && _isActive && XeFGProxy::SetEnabled())
        {
            XeFGProxy::SetEnabled()(_xefgSwapChain, 0);
            _isActive = false;
        }
    }

    if (destroy)
    {
        // First, release proxy swap chain if it exists
        if (_xefgProxySwapChain != nullptr)
        {
            _xefgProxySwapChain->Release();
            _xefgProxySwapChain = nullptr;
        }

        // Then destroy XeSS-FG swap chain
        if (_xefgSwapChain != nullptr && XeFGProxy::Destroy())
        {
            LOG_INFO("Destroying XeSS-FG SwapChain");
            XeFGProxy::Destroy()(_xefgSwapChain);
            _xefgSwapChain = nullptr;
            State::Instance().XeFgSwapChain = nullptr;
        }

        // IMPORTANT: Destroy XeLL AFTER XeSS-FG has been destroyed
        if (_xellContext != nullptr && XeLLProxy::DestroyContext())
        {
            LOG_INFO("Destroying XeLL Context");
            XeLLProxy::DestroyContext()(_xellContext);
            _xellContext = nullptr;
        }
    }

    if ((shutDown || State::Instance().isShuttingDown) || destroy)
    {
        ReleaseObjects();
    }

    if (Config::Instance()->FGUseMutexForSwaphain.value_or_default() && useMutex)
    {
        LOG_TRACE("Releasing Mutex: {}", Mutex.getOwner());
        Mutex.unlockThis(1);
    }
}


// Update the initialization order in dllmain.cpp for global cleanup
void CleanupLibraries()
{
    // Clean up in the correct order - XeSS-FG first, then XeLL

    // First, clean up XeSS-FG
    if (State::Instance().XeFgSwapChain != nullptr && XeFGProxy::Destroy())
    {
        XeFGProxy::Destroy()(State::Instance().XeFgSwapChain);
        State::Instance().XeFgSwapChain = nullptr;
    }

    // Then, clean up XeLL
    if (State::Instance().XeLLContext != nullptr && XeLLProxy::DestroyContext())
    {
        XeLLProxy::DestroyContext()(State::Instance().XeLLContext);
        State::Instance().XeLLContext = nullptr;
    }
}
bool XeFG_Dx12::CreateSwapchain(IDXGIFactory* factory, ID3D12CommandQueue* cmdQueue, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** swapChain)
{
    LOG_DEBUG("Creating swapchain from DXGI_SWAP_CHAIN_DESC");
    LOG_INFO("XeFG_Dx12::CreateSwapchain called, XeFG enabled");

    // Store reference to command queue and swapchain for later use
    _gameCommandQueue = cmdQueue;
    _hwnd = desc->OutputWindow;

    // Initialize XeLL if needed
    if (_xellContext == nullptr)
    {
        InitXeLL(State::Instance().currentD3D12Device);
    }

    // Initialize XeSS-FG
    if (_xefgSwapChain == nullptr)
    {
        if (!XeFGProxy::InitXeFG() || !XeFGProxy::D3D12CreateContext())
        {
            LOG_ERROR("Failed to initialize XeFG");
            return false;
        }

        XeFGProxy::D3D12CreateContext()(State::Instance().currentD3D12Device, &_xefgSwapChain);
        if (_xefgSwapChain == nullptr)
        {
            LOG_ERROR("Failed to create XeFG context");
            return false;
        }

        // Set up logging
        XeFGProxy::SetupLogging(_xefgSwapChain);

        // Link XeLL with XeSS-FG
        if (_xellContext != nullptr && XeFGProxy::SetLatencyReduction())
        {
            XeFGProxy::SetLatencyReduction()(_xefgSwapChain, _xellContext);
        }
    }

    // Create XeSS-FG swapchain from desc
    if (_xefgSwapChain != nullptr)
    {
        // Prepare full screen desc
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsDesc = {};
        fsDesc.RefreshRate = desc->BufferDesc.RefreshRate;
        fsDesc.ScanlineOrdering = desc->BufferDesc.ScanlineOrdering;
        fsDesc.Scaling = desc->BufferDesc.Scaling;
        fsDesc.Windowed = desc->Windowed;

        // Convert DXGI_SWAP_CHAIN_DESC to DXGI_SWAP_CHAIN_DESC1
        DXGI_SWAP_CHAIN_DESC1 desc1 = {};
        desc1.Width = desc->BufferDesc.Width;
        desc1.Height = desc->BufferDesc.Height;
        desc1.Format = desc->BufferDesc.Format;
        desc1.Stereo = FALSE;
        desc1.SampleDesc = desc->SampleDesc;
        desc1.BufferUsage = desc->BufferUsage;
        desc1.BufferCount = desc->BufferCount;
        desc1.Scaling = DXGI_SCALING_STRETCH;
        desc1.SwapEffect = desc->SwapEffect;
        desc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        desc1.Flags = desc->Flags;

        // Create the XeSS-FG swapchain
        IDXGIFactory2* factory2 = nullptr;
        HRESULT hr = factory->QueryInterface(__uuidof(IDXGIFactory2), (void**)&factory2);
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to get IDXGIFactory2 interface: 0x{:X}", (unsigned int)hr);
            return false;
        }

        // Prepare init params for XeSS-FG
        xefg_swapchain_d3d12_init_params_t initParams = {};
        initParams.pApplicationSwapChain = nullptr; // Will be created by XeSS-FG
        initParams.maxInterpolatedFrames = 1;
        initParams.uiMode = _uiMode;
        initParams.initFlags = 0;

        // Set flags based on configuration
        if (Config::Instance()->XeFGInvertedDepth.value_or_default())
            initParams.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_INVERTED_DEPTH;
        
        if (Config::Instance()->XeFGUseNDCVelocity.value_or_default())
            initParams.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_USE_NDC_VELOCITY;
            
        if (Config::Instance()->XeFGJitteredMV.value_or_default())
            initParams.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_JITTERED_MV;

        // Initialize XeSS-FG swapchain
        xefg_swapchain_result_t result = XeFGProxy::D3D12InitFromSwapChainDesc()(
            _xefgSwapChain, desc->OutputWindow, &desc1, &fsDesc, cmdQueue, factory2, &initParams);

        factory2->Release();

        if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
        {
            LOG_ERROR("Failed to initialize XeFG swapchain: {}", XeFGProxy::ResultToString(result));
            return false;
        }

        // Get the proxy swapchain
        if (XeFGProxy::D3D12GetSwapChainPtr()(_xefgSwapChain, 
            __uuidof(IDXGISwapChain4), (void**)&_xefgProxySwapChain) != XEFG_SWAPCHAIN_RESULT_SUCCESS)
        {
            LOG_ERROR("Failed to get XeFG proxy swapchain");
            return false;
        }

        // Assign proxy swapchain to output parameter
        *swapChain = _xefgProxySwapChain;
        _swapChain = _xefgProxySwapChain;
        // Add proper reference counting
        if (_xefgProxySwapChain) {
            _xefgProxySwapChain->AddRef();
        }
        LOG_INFO("Successfully created XeFG swapchain");
        return true;
    }

    return false;
}

bool XeFG_Dx12::CreateSwapchain1(IDXGIFactory* factory, ID3D12CommandQueue* cmdQueue, HWND hwnd, DXGI_SWAP_CHAIN_DESC1* desc, DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc, IDXGISwapChain1** swapChain)
{
    LOG_DEBUG("Creating swapchain from DXGI_SWAP_CHAIN_DESC1");

    // Store reference to command queue and hwnd for later use
    _gameCommandQueue = cmdQueue;
    _hwnd = hwnd;

    // Initialize XeLL if needed
    if (_xellContext == nullptr)
    {
        InitXeLL(State::Instance().currentD3D12Device);
    }

    // Initialize XeSS-FG
    if (_xefgSwapChain == nullptr)
    {
        if (!XeFGProxy::InitXeFG() || !XeFGProxy::D3D12CreateContext())
        {
            LOG_ERROR("Failed to initialize XeFG");
            return false;
        }

        XeFGProxy::D3D12CreateContext()(State::Instance().currentD3D12Device, &_xefgSwapChain);
        if (_xefgSwapChain == nullptr)
        {
            LOG_ERROR("Failed to create XeFG context");
            return false;
        }

        // Set up logging
        XeFGProxy::SetupLogging(_xefgSwapChain);

        // Link XeLL with XeSS-FG
        if (_xellContext != nullptr && XeFGProxy::SetLatencyReduction())
        {
            XeFGProxy::SetLatencyReduction()(_xefgSwapChain, _xellContext);
        }
    }

    // Create XeSS-FG swapchain
    if (_xefgSwapChain != nullptr)
    {
        // Create the XeSS-FG swapchain
        IDXGIFactory2* factory2 = nullptr;
        HRESULT hr = factory->QueryInterface(__uuidof(IDXGIFactory2), (void**)&factory2);
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to get IDXGIFactory2 interface: 0x{:X}", (unsigned int)hr);
            return false;
        }

        // Prepare init params for XeSS-FG
        xefg_swapchain_d3d12_init_params_t initParams = {};
        initParams.pApplicationSwapChain = nullptr; // Will be created by XeSS-FG
        initParams.maxInterpolatedFrames = 1;
        initParams.uiMode = _uiMode;
        initParams.initFlags = 0;

        // Set flags based on configuration
        if (Config::Instance()->XeFGInvertedDepth.value_or_default())
            initParams.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_INVERTED_DEPTH;
        
        if (Config::Instance()->XeFGUseNDCVelocity.value_or_default())
            initParams.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_USE_NDC_VELOCITY;
            
        if (Config::Instance()->XeFGJitteredMV.value_or_default())
            initParams.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_JITTERED_MV;

        // Initialize XeSS-FG swapchain
        xefg_swapchain_result_t result = XeFGProxy::D3D12InitFromSwapChainDesc()(
            _xefgSwapChain, hwnd, desc, pFullscreenDesc, cmdQueue, factory2, &initParams);

        factory2->Release();

        if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
        {
            LOG_ERROR("Failed to initialize XeFG swapchain: {}", XeFGProxy::ResultToString(result));
            return false;
        }

        // Get the proxy swapchain
        if (XeFGProxy::D3D12GetSwapChainPtr()(_xefgSwapChain, 
            __uuidof(IDXGISwapChain4), (void**)&_xefgProxySwapChain) != XEFG_SWAPCHAIN_RESULT_SUCCESS)
        {
            LOG_ERROR("Failed to get XeFG proxy swapchain");
            return false;
        }

        // Cast to IDXGISwapChain1
        IDXGISwapChain1* proxySwapChain1 = nullptr;
        hr = _xefgProxySwapChain->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&proxySwapChain1);
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to get IDXGISwapChain1 interface: 0x{:X}", (unsigned int)hr);
            return false;
        }

        // Assign proxy swapchain to output parameter
        *swapChain = proxySwapChain1;
        _swapChain = _xefgProxySwapChain;
        // Add proper reference counting
        if (_xefgProxySwapChain) {
            _xefgProxySwapChain->AddRef();
        }
        LOG_INFO("Successfully created XeFG swapchain");
        return true;
    }

    return false;
}

bool XeFG_Dx12::ReleaseSwapchain(HWND hwnd)
{
    if (hwnd != _hwnd || _hwnd == NULL)
        return false;

    LOG_DEBUG("");

    if (Config::Instance()->FGUseMutexForSwaphain.value_or_default())
    {
        LOG_TRACE("Waiting Mutex 1, current: {}", Mutex.getOwner());
        Mutex.lock(1);
        LOG_TRACE("Acquired Mutex: {}", Mutex.getOwner());
    }

    MenuOverlayDx::CleanupRenderTarget(true, NULL);

    // Stop and destroy contexts
    if (_xefgSwapChain != nullptr)
        StopAndDestroyContext(true, true, false);

    if (Config::Instance()->FGUseMutexForSwaphain.value_or_default())
    {
        LOG_TRACE("Releasing Mutex: {}", Mutex.getOwner());
        Mutex.unlockThis(1);
    }

    return true;
}

void XeFG_Dx12::CreateContext(ID3D12Device* device, IFeature* upscalerContext)
{
    LOG_DEBUG("");

    // Create command queues and allocators
    CreateObjects(device);
    
    // Initialize XeLL if needed
    if (_xellContext == nullptr)
    {
        InitXeLL(device);
    }

    // If XeSS-FG is already initialized, just enable it
    if (_xefgSwapChain != nullptr)
    {
        if (XeFGProxy::SetEnabled())
        {
            LOG_DEBUG("Re-enabling XeFG");
            XeFGProxy::SetEnabled()(_xefgSwapChain, 1);
        }

        _isActive = true;
        return;
    }

    // Initialize XeSS-FG
    if (!XeFGProxy::InitXeFG() || !XeFGProxy::D3D12CreateContext())
    {
        LOG_ERROR("Failed to initialize XeFG");
        return;
    }

    // Create XeSS-FG context
    XeFGProxy::D3D12CreateContext()(device, &_xefgSwapChain);
    if (_xefgSwapChain == nullptr)
    {
        LOG_ERROR("Failed to create XeFG context");
        return;
    }

    // Set up logging
    XeFGProxy::SetupLogging(_xefgSwapChain);

    // Link XeLL with XeSS-FG
    if (_xellContext != nullptr && XeFGProxy::SetLatencyReduction())
    {
        XeFGProxy::SetLatencyReduction()(_xefgSwapChain, _xellContext);
    }

    // Configure XeSS-FG based on upscaler context
    ConfigureXeFG(Config::Instance()->FGDebugView.value_or_default());

    _isActive = true;
}

bool XeFG_Dx12::InitXeLL(ID3D12Device* device)
{
    // Initialize XeLL
    if (!XeLLProxy::InitXeLL())
    {
        LOG_ERROR("Failed to initialize XeLL");
        return false;
    }

    // Create XeLL context
    if (XeLLProxy::D3D12CreateContext()(device, &_xellContext) != XELL_RESULT_SUCCESS)
    {
        LOG_ERROR("Failed to create XeLL context");
        return false;
    }

    // Set up logging for XeLL
    XeLLProxy::SetupLogging(_xellContext);

    // Configure XeLL according to settings
    xell_sleep_params_t sleepParams = {};
    sleepParams.bLowLatencyMode = 1; // Enable low latency mode for frame generation

    // Configure boost if enabled in configuration
    sleepParams.bLowLatencyBoost = Config::Instance()->XeLLBoost.value_or_default() ? 1 : 0;

    // Set frame limiter if specified (in microseconds)
    uint32_t intervalMs = Config::Instance()->XeLLIntervalUs.value_or_default();
    if (intervalMs > 0)
    {
        sleepParams.minimumIntervalUs = intervalMs;
    }

    // Apply the configuration
    xell_result_t result = XeLLProxy::SetSleepMode()(_xellContext, &sleepParams);
    if (result != XELL_RESULT_SUCCESS)
    {
        LOG_WARN("Failed to set XeLL sleep mode: {}", XeLLProxy::ResultToString(result));
        // We continue even if this fails, as basic functionality should still work
    }

    // Store the current parameters in State for reference
    if (XeLLProxy::GetSleepMode() && State::Instance().XeLLCurrentParams.bLowLatencyMode != sleepParams.bLowLatencyMode)
    {
        XeLLProxy::GetSleepMode()(_xellContext, &State::Instance().XeLLCurrentParams);
    }

    LOG_INFO("XeLL initialized successfully");
    return true;
}

void XeFG_Dx12::ConfigureXeFG(bool enableDebug)
{
    if (_xefgSwapChain == nullptr)
        return;

    // Set scene change threshold if available
    if (XeFGProxy::SetSceneChangeThreshold())
    {
        float threshold = Config::Instance()->XeFGSceneChangeThreshold.value_or_default();
        XeFGProxy::SetSceneChangeThreshold()(_xefgSwapChain, threshold);
    }

    // Enable debug features if requested
    if (enableDebug && XeFGProxy::EnableDebugFeature())
    {
        if (Config::Instance()->XeFGDebugShowOnlyInterpolation.value_or_default())
        {
            XeFGProxy::EnableDebugFeature()(_xefgSwapChain, 
                XEFG_SWAPCHAIN_DEBUG_FEATURE_SHOW_ONLY_INTERPOLATION, 1, nullptr);
        }

        if (Config::Instance()->XeFGDebugTagInterpolatedFrames.value_or_default())
        {
            XeFGProxy::EnableDebugFeature()(_xefgSwapChain, 
                XEFG_SWAPCHAIN_DEBUG_FEATURE_TAG_INTERPOLATED_FRAMES, 1, nullptr);
        }

        if (Config::Instance()->XeFGDebugPresentFailedInterpolation.value_or_default())
        {
            XeFGProxy::EnableDebugFeature()(_xefgSwapChain, 
                XEFG_SWAPCHAIN_DEBUG_FEATURE_PRESENT_FAILED_INTERPOLATION, 1, nullptr);
        }
    }
}

void XeFG_Dx12::SetUIMode(xefg_swapchain_ui_mode_t mode)
{
    _uiMode = mode;
}

xefg_swapchain_present_status_t XeFG_Dx12::GetLastPresentStatus()
{
    if (_xefgSwapChain && XeFGProxy::GetLastPresentStatus())
    {
        XeFGProxy::GetLastPresentStatus()(_xefgSwapChain, &_lastPresentStatus);
    }
    return _lastPresentStatus;
}

void XeFG_Dx12::FgDone()
{
    // Nothing to do
}

xefg_swapchain_resource_validity_t XeFG_Dx12::GetResourceValidity(ID3D12GraphicsCommandList* cmdList)
{
    // If no command list is provided, resources will be valid until next present
    if (cmdList == nullptr)
    {
        return XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;
    }
    
    // Check if the command list is one of our internal command lists
    for (size_t i = 0; i < BUFFER_COUNT; i++)
    {
        if (_commandList[i] == cmdList)
        {
            return XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;
        }
    }
    
    // External command list - resource only valid now
    return XEFG_SWAPCHAIN_RV_ONLY_NOW;
}

bool XeFG_Dx12::InitXeLLAndSleep(ID3D12Device* device, uint32_t frameId)
{
    // Initialize XeLL if needed
    if (_xellContext == nullptr && !InitXeLL(device))
    {
        LOG_WARN("Failed to initialize XeLL, proceeding without latency reduction");
        return false;
    }

    // Call Sleep at the beginning of the frame for the current frame
    if (_xellContext != nullptr && XeLLProxy::Sleep())
    {
        // Sleep before starting the frame processing
        xell_result_t result = XeLLProxy::Sleep()(_xellContext, frameId);
        if (result != XELL_RESULT_SUCCESS)
        {
            LOG_WARN("XeLL Sleep failed: {}", XeLLProxy::ResultToString(result));
            return false;
        }
        return true;
    }

    return false;
}

void XeFG_Dx12::MarkXeLLSimulationStart(uint32_t frameId)
{
    if (_xellContext == nullptr || !XeLLProxy::IsInitialized() || !XeLLProxy::AddMarkerData())
        return;

    XeLLProxy::AddMarkerData()(_xellContext, frameId, XELL_SIMULATION_START);
}

void XeFG_Dx12::MarkXeLLSimulationEnd(uint32_t frameId)
{
    if (_xellContext == nullptr || !XeLLProxy::IsInitialized() || !XeLLProxy::AddMarkerData())
        return;

    XeLLProxy::AddMarkerData()(_xellContext, frameId, XELL_SIMULATION_END);
}

void XeFG_Dx12::MarkXeLLRenderSubmitStart(uint32_t frameId)
{
    if (_xellContext == nullptr || !XeLLProxy::IsInitialized() || !XeLLProxy::AddMarkerData())
        return;

    XeLLProxy::AddMarkerData()(_xellContext, frameId, XELL_RENDERSUBMIT_START);
}

void XeFG_Dx12::MarkXeLLRenderSubmitEnd(uint32_t frameId)
{
    if (_xellContext == nullptr || !XeLLProxy::IsInitialized() || !XeLLProxy::AddMarkerData())
        return;

    XeLLProxy::AddMarkerData()(_xellContext, frameId, XELL_RENDERSUBMIT_END);
}

void XeFG_Dx12::MarkXeLLPresentStart(uint32_t frameId)
{
    if (_xellContext == nullptr || !XeLLProxy::IsInitialized() || !XeLLProxy::AddMarkerData())
        return;

    XeLLProxy::AddMarkerData()(_xellContext, frameId, XELL_PRESENT_START);
}

void XeFG_Dx12::MarkXeLLPresentEnd(uint32_t frameId)
{
    if (_xellContext == nullptr || !XeLLProxy::IsInitialized() || !XeLLProxy::AddMarkerData())
        return;

    XeLLProxy::AddMarkerData()(_xellContext, frameId, XELL_PRESENT_END);
}
