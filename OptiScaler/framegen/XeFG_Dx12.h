#pragma once

#include <framegen/IFGFeature_Dx12.h>
#include <proxies/XeLL_Proxy.h>
#include <proxies/XeFG_Proxy.h>

#include <xefg_swapchain.h>
#include <xefg_swapchain_d3d12.h>
#include <xefg_swapchain_debug.h>
#include <xell.h>
#include <xell_d3d12.h>

class XeFG_Dx12 : public virtual IFGFeature_Dx12
{
private:
    // XeLL context
    xell_context_handle_t _xellContext = nullptr;
    
    // XeSS-FG context
    xefg_swapchain_handle_t _xefgSwapChain = nullptr;
    
    // UI mode for XeSS-FG
    xefg_swapchain_ui_mode_t _uiMode = XEFG_SWAPCHAIN_UI_MODE_AUTO;
    
    // Present status
    xefg_swapchain_present_status_t _lastPresentStatus = {};
    
    // SwapChain from XeSS-FG
    IDXGISwapChain4* _xefgProxySwapChain = nullptr;

    // Resource validity
    xefg_swapchain_resource_validity_t GetResourceValidity(ID3D12GraphicsCommandList* cmdList);
    
    // Mark frames for XeLL
    void MarkXeLLFrames(uint32_t frameId);

public:
    // Inherited via IFGFeature_Dx12
    UINT64 UpscaleStart() final;
    void UpscaleEnd() final;

    feature_version Version() final;
    const char* Name() final;

    bool Dispatch(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* output, double frameTime) final;
    bool DispatchHudless(bool useHudless, double frameTime) final;

    void StopAndDestroyContext(bool destroy, bool shutDown, bool useMutex) final;

    bool CreateSwapchain(IDXGIFactory* factory, ID3D12CommandQueue* cmdQueue, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** swapChain) final;
    bool CreateSwapchain1(IDXGIFactory* factory, ID3D12CommandQueue* cmdQueue, HWND hwnd, DXGI_SWAP_CHAIN_DESC1* desc, DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc, IDXGISwapChain1** swapChain) final;
    bool ReleaseSwapchain(HWND hwnd) final;

    void CreateContext(ID3D12Device* device, IFeature* upscalerContext) final;

    // XeFG specific methods
    bool InitXeLL(ID3D12Device* device);
    void ConfigureXeFG(bool enableDebug = false);
    void SetUIMode(xefg_swapchain_ui_mode_t mode);
    xefg_swapchain_present_status_t GetLastPresentStatus();

    // Inherited via IFGFeature_Dx12
    void FgDone() override;

    XeFG_Dx12() = default;
    ~XeFG_Dx12();
};