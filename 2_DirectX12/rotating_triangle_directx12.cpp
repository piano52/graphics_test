// Basic DirectX 12 rotating triangle.
// Build in "x64 Native Tools Command Prompt for VS":
// cl /EHsc rotating_triangle_directx12.cpp user32.lib dxgi.lib d3d12.lib d3dcompiler.lib

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <cmath>
#include <cstring>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

static const UINT FrameCount = 2;
static HWND g_hwnd;
static UINT g_width = 800;
static UINT g_height = 600;
static float g_angle;
static const UINT VertexCount = 3;

struct Vertex {
    XMFLOAT3 position;
    XMFLOAT3 color;
};

static ComPtr<ID3D12Device> g_device;
static ComPtr<ID3D12CommandQueue> g_commandQueue;
static ComPtr<IDXGISwapChain3> g_swapChain;
static ComPtr<ID3D12DescriptorHeap> g_rtvHeap;
static ComPtr<ID3D12Resource> g_renderTargets[FrameCount];
static ComPtr<ID3D12CommandAllocator> g_commandAllocator[FrameCount];
static ComPtr<ID3D12GraphicsCommandList> g_commandList;
static ComPtr<ID3D12RootSignature> g_rootSignature;
static ComPtr<ID3D12PipelineState> g_pipelineState;
static ComPtr<ID3D12Resource> g_vertexBuffer;
static D3D12_VERTEX_BUFFER_VIEW g_vertexBufferView;
static ComPtr<ID3D12Fence> g_fence;
static HANDLE g_fenceEvent;
static UINT64 g_fenceValue[FrameCount] = {};
static UINT g_frameIndex;
static UINT g_rtvDescriptorSize;

static void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr)) {
        throw std::runtime_error("DirectX call failed");
    }
}

static D3D12_RESOURCE_BARRIER TransitionBarrier(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}

static void WaitForGpu()
{
    const UINT64 value = ++g_fenceValue[g_frameIndex];
    ThrowIfFailed(g_commandQueue->Signal(g_fence.Get(), value));
    if (g_fence->GetCompletedValue() < value) {
        ThrowIfFailed(g_fence->SetEventOnCompletion(value, g_fenceEvent));
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }
}

static void MoveToNextFrame()
{
    const UINT64 currentFenceValue = ++g_fenceValue[g_frameIndex];
    ThrowIfFailed(g_commandQueue->Signal(g_fence.Get(), currentFenceValue));

    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    if (g_fence->GetCompletedValue() < g_fenceValue[g_frameIndex]) {
        ThrowIfFailed(g_fence->SetEventOnCompletion(g_fenceValue[g_frameIndex], g_fenceEvent));
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }

    g_fenceValue[g_frameIndex] = currentFenceValue;
}

static void UpdateTriangleVertices()
{
    const float cy = cosf(g_angle);
    const float sy = sinf(g_angle);
    const float cx = cosf(g_angle * 0.65f);
    const float sx = sinf(g_angle * 0.65f);

    const XMFLOAT3 points[VertexCount] = {
        { 0.0f,  0.95f,  0.0f },
        {-0.95f, -0.65f,  0.0f },
        { 0.95f, -0.65f,  0.0f }
    };

    const XMFLOAT3 colors[VertexCount] = {
        {1.0f, 0.88f, 0.15f},
        {1.0f, 0.20f, 0.62f},
        {0.15f, 0.95f, 0.90f}
    };

    Vertex vertices[VertexCount] = {};

    for (int i = 0; i < (int)VertexCount; ++i) {
        XMFLOAT3 v = points[i];

        float x1 = v.x * cy + v.z * sy;
        float z1 = -v.x * sy + v.z * cy;
        float y1 = v.y;

        float y2 = y1 * cx - z1 * sx;
        float z2 = y1 * sx + z1 * cx;
        float cameraZ = z2 + 3.2f;
        float perspective = 1.45f / cameraZ;

        vertices[i].position.x = x1 * perspective;
        vertices[i].position.y = y2 * perspective;
        vertices[i].position.z = 0.0f;
        vertices[i].color = colors[i];
    }

    void* mappedData = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    ThrowIfFailed(g_vertexBuffer->Map(0, &readRange, &mappedData));
    memcpy(mappedData, vertices, sizeof(vertices));
    g_vertexBuffer->Unmap(0, nullptr);
}

static void LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;
    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device)));

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(g_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_commandQueue)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = g_width;
    swapChainDesc.Height = g_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        g_commandQueue.Get(),
        g_hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain));
    ThrowIfFailed(factory->MakeWindowAssociation(g_hwnd, DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(swapChain.As(&g_swapChain));
    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    ThrowIfFailed(g_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_rtvHeap)));
    g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < FrameCount; ++i) {
        ThrowIfFailed(g_swapChain->GetBuffer(i, IID_PPV_ARGS(&g_renderTargets[i])));
        g_device->CreateRenderTargetView(g_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += g_rtvDescriptorSize;
        ThrowIfFailed(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandAllocator[i])));
    }
}

static void LoadAssets()
{
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
    ThrowIfFailed(g_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&g_rootSignature)));

    const char* shaderCode =
        "struct VSInput { float3 position : POSITION; float3 color : COLOR; };"
        "struct PSInput { float4 position : SV_POSITION; float3 color : COLOR; };"
        "PSInput VSMain(VSInput input) {"
        "  PSInput result;"
        "  result.position = float4(input.position, 1.0);"
        "  result.color = input.color;"
        "  return result;"
        "}"
        "float4 PSMain(PSInput input) : SV_TARGET {"
        "  return float4(input.color, 1.0);"
        "}";

    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    ThrowIfFailed(D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vertexShader, &error));
    ThrowIfFailed(D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &pixelShader, &error));

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = g_rootSignature.Get();
    psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pipelineState)));

    ThrowIfFailed(g_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        g_commandAllocator[g_frameIndex].Get(),
        g_pipelineState.Get(),
        IID_PPV_ARGS(&g_commandList)));
    ThrowIfFailed(g_commandList->Close());

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeof(Vertex) * VertexCount;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(g_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&g_vertexBuffer)));

    g_vertexBufferView.BufferLocation = g_vertexBuffer->GetGPUVirtualAddress();
    g_vertexBufferView.StrideInBytes = sizeof(Vertex);
    g_vertexBufferView.SizeInBytes = sizeof(Vertex) * VertexCount;

    ThrowIfFailed(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)));
    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!g_fenceEvent) {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    UpdateTriangleVertices();
}

static void PopulateCommandList()
{
    ThrowIfFailed(g_commandAllocator[g_frameIndex]->Reset());
    ThrowIfFailed(g_commandList->Reset(g_commandAllocator[g_frameIndex].Get(), g_pipelineState.Get()));

    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)g_width, (float)g_height, 0.0f, 1.0f };
    D3D12_RECT scissorRect = { 0, 0, (LONG)g_width, (LONG)g_height };
    g_commandList->SetGraphicsRootSignature(g_rootSignature.Get());
    g_commandList->RSSetViewports(1, &viewport);
    g_commandList->RSSetScissorRects(1, &scissorRect);

    D3D12_RESOURCE_BARRIER barrier = TransitionBarrier(
        g_renderTargets[g_frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    g_commandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += g_frameIndex * g_rtvDescriptorSize;
    g_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    const float clearColor[] = { 0.02f, 0.18f, 0.19f, 1.0f };
    g_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_commandList->IASetVertexBuffers(0, 1, &g_vertexBufferView);
    g_commandList->DrawInstanced(VertexCount, 1, 0, 0);

    barrier = TransitionBarrier(
        g_renderTargets[g_frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    g_commandList->ResourceBarrier(1, &barrier);
    ThrowIfFailed(g_commandList->Close());
}

static void Render()
{
    g_angle -= 0.018f;
    UpdateTriangleVertices();
    PopulateCommandList();

    ID3D12CommandList* commandLists[] = { g_commandList.Get() };
    g_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
    ThrowIfFailed(g_swapChain->Present(1, 0));
    MoveToNextFrame();
}

static void Cleanup()
{
    if (g_device) {
        WaitForGpu();
    }
    CloseHandle(g_fenceEvent);
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT:
        Render();
        ValidateRect(hwnd, nullptr);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCmd)
{
    const wchar_t className[] = L"BasicDirectX12RotatingTriangle";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = className;
    RegisterClass(&wc);

    RECT rect = { 0, 0, (LONG)g_width, (LONG)g_height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowEx(
        0,
        className,
        L"DirectX 12 - Rotating Triangle",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    try {
        LoadPipeline();
        LoadAssets();
        ShowWindow(g_hwnd, showCmd);

        MSG msg = {};
        while (msg.message != WM_QUIT) {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            } else {
                Render();
            }
        }
        Cleanup();
    } catch (...) {
        MessageBox(g_hwnd, L"DirectX 12 sample failed to run.", L"Error", MB_ICONERROR);
        Cleanup();
        return 1;
    }

    return 0;
}
