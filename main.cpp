// dear imgui - standalone example application for DirectX 11
// If you are new to dear imgui, see examples/README.txt and documentation at the top of imgui.cpp.

#include "imgui/imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>
#include <string>
#include <algorithm>
#include <math.h>
#include "DSoundTools.h"
#include "SynthOX/SynthOX.h"

// Data
static ID3D11Device*            g_pd3dDevice = NULL;
static ID3D11DeviceContext*     g_pd3dDeviceContext = NULL;
static IDXGISwapChain*          g_pSwapChain = NULL;
static ID3D11RenderTargetView*  g_mainRenderTargetView = NULL;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static const int WPosX = 100;
static const int WPosY = 100;
static const int WSizeW = 900;
static const int WSizeH = 400;
static const float PI = 3.1415926f;

bool Knob(const char * label, float & value, float minv, float maxv, float Size = 36.f, int NbSegments = 16)
{	
	const ImGuiStyle& style = ImGui::GetStyle();
	
	const ImVec2 CursorPos = ImGui::GetCursorScreenPos();
	float radius = Size*0.5f;
	const ImVec2 center {CursorPos.x + radius, CursorPos.y + radius};	
	const float gamma = PI / 4.0f;
	const float val1 = (value - minv) / (maxv - minv);
	const float alpha = (PI - gamma) * val1 * 2.0f + gamma;
	
	ImGui::InvisibleButton(label, {Size, Size + ImGui::GetTextLineHeight() + style.ItemInnerSpacing.y});

	const bool is_active = ImGui::IsItemActive();	
	if(is_active)
	{		
		const ImVec2 mp = ImGui::GetIO().MousePos;
		const float alpha2 = std::clamp(atan2f(mp.x - center.x, center.y - mp.y) + PI, gamma, 2.0f * PI - gamma);
		value = (0.5f * (alpha2 - gamma) / (PI - gamma)) * (maxv - minv) + minv;
	}
	
	const ImU32 col32 = ImGui::GetColorU32(is_active ? ImGuiCol_FrameBgActive : ImGui::IsItemHovered() ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
	const ImU32 col32line = ImGui::GetColorU32(ImGuiCol_SliderGrabActive); 
	const ImU32 col32text = ImGui::GetColorU32(ImGuiCol_Text);
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	
	auto PointOnRadius = [center](float angle, float rad) -> ImVec2 
	{
		return {-sinf(angle)*rad + center.x, cosf(angle)*rad + center.y};
	};

    if(value > 0.f)
    {
        float th = gamma;
	    const float step = (alpha - gamma) / float(NbSegments);
	    for(int i = 0; i < NbSegments; i++, th += step)
		    draw_list->AddLine(PointOnRadius(th, radius + 2.f), PointOnRadius(th + step + .02f, radius + 2.f), ImGui::GetColorU32(ImVec4(.6f, .15f, .11f, 1.f)), 4.f);
        th = gamma;
	    for(int i = 0; i < NbSegments; i++, th += step)
		    draw_list->AddLine(PointOnRadius(th, radius + 1.f), PointOnRadius(th + step + .02f, radius + 1.f), ImGui::GetColorU32(ImVec4(9.f, .4f, .2f, 1.f)), 2.f);
    }

    draw_list->AddCircleFilled(center, radius, col32, NbSegments);

	const ImVec2 CursotTip = PointOnRadius(alpha, radius);
	draw_list->AddLine({.5f * (center.x + CursotTip.x), .5f * (center.y + CursotTip.y)}, CursotTip, col32line, 3);
	
	ImVec2 TSize = ImGui::CalcTextSize(label);
	draw_list->AddText({CursorPos.x + .5f*(Size - TSize.x), CursorPos.y + Size + style.ItemInnerSpacing.y}, col32text, label);

	//if(is_active)
	//	draw_list->AddText(nullptr, 11.f, CursorPos, col32text, std::to_string(value).c_str());
	
	return is_active;
}

// Main code
int main(int, char**)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ShyntOlGui"), NULL };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("SynthOlGui Window"), WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU, WPosX, WPosY, WPosX + WSizeW, WPosY + WSizeH, NULL, NULL, wc.hInstance, NULL);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

	SynthOX::Synth Synth;
	SynthOX::AnalogSourceData AnalogSourceData;
    AnalogSourceData.m_LeftVolume = .7f;
    AnalogSourceData.m_RightVolume = .7f;
    AnalogSourceData.m_ADSR_Attack = .1f;
    AnalogSourceData.m_ADSR_Decay = .1f;
    AnalogSourceData.m_ADSR_Sustain = .7f;
    AnalogSourceData.m_ADSR_Release = .3f;
    AnalogSourceData.m_OscillatorTab[0].m_ModulationType = SynthOX::ModulationType::Mix;
    AnalogSourceData.m_OscillatorTab[0].m_LFOTab[(int)SynthOX::LFODest::Morph].m_BaseValue = .5f;
    AnalogSourceData.m_OscillatorTab[1].m_ModulationType = SynthOX::ModulationType::Mix;
    AnalogSourceData.m_OscillatorTab[1].m_LFOTab[(int)SynthOX::LFODest::Morph].m_BaseValue = .5f;
    AnalogSourceData.m_OscillatorTab[1].m_LFOTab[(int)SynthOX::LFODest::Volume].m_BaseValue = .0f;
	SynthOX::AnalogSource AnalogSource(&Synth.m_OutBuf, 0, &AnalogSourceData);
	Synth.BindSource(AnalogSource);

    DSoundTools::Init(hwnd);

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    ImFont * LUCONFont = io.Fonts->AddFontFromFileTTF("LUCON.TTF", 12.0f);
    //IM_ASSERT(font != NULL);


    // Our state
    ImVec4 clear_color = ImVec4(0.145f, 0.155f, 0.160f, 1.00f);

    // Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::PushFont(LUCONFont);

        {
            static float MasterVolume = .5f;
//            static int counter = 0;

            ImGui::SetNextWindowSize({WSizeW, WSizeH});
            ImGui::SetNextWindowPos({0.f, 0.f});

            bool bDummy = false;
            ImGui::Begin("SynthOl", &bDummy, ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove);                          // Create a window called "Hello, world!" and append into it.

//            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state

            int Id = 0;
            auto VSlider = [&Id](float & Val, bool SameLine=true) 
            {
                ImGui::PushID(Id++);
                if(SameLine)
                    ImGui::SameLine(); 
                ImGui::VSliderFloat("##v", {10.f, 80.0f}, &Val, 0.0f, 1.0f, "");  
                ImGui::PopID();
            };

            ImGui::PushID("set1");

            VSlider(AnalogSourceData.m_ADSR_Attack, false);
            VSlider(AnalogSourceData.m_ADSR_Decay);
            VSlider(AnalogSourceData.m_ADSR_Sustain);
            VSlider(AnalogSourceData.m_ADSR_Release);
           
            ImGui::PopID();

//            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

//            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
//                counter++;

//            ImGui::SameLine(); ImGui::Text("counter = %d", counter);
            ImGui::SameLine(0, 15.f); Knob("Morph",  AnalogSourceData.m_OscillatorTab[0].m_LFOTab[(int)SynthOX::LFODest::Morph].m_BaseValue, 0.f, 1.f, 32.f, 16);
            ImGui::SameLine(0, 15.f); Knob("Squish", AnalogSourceData.m_OscillatorTab[0].m_LFOTab[(int)SynthOX::LFODest::Squish].m_BaseValue, 0.f, 1.f, 32.f, 16);
            ImGui::SameLine(0, 15.f); Knob("Drive",  AnalogSourceData.m_OscillatorTab[0].m_LFOTab[(int)SynthOX::LFODest::Distort].m_BaseValue, 0.f, 1.f, 32.f, 16);
            static float Pan = .5f;
            ImGui::SameLine(0, 15.f); Knob("Pan", Pan, 0.f, 1.f, 32.f, 16);
            AnalogSourceData.m_RightVolume = min(Pan * 2.f, 1.f);
            AnalogSourceData.m_LeftVolume = min((1.f - Pan) * 2.f, 1.f);

            ImGui::SameLine(0, 15.f); Knob("Master", MasterVolume, 0.f, 1.f, 45.f, 16);
            
            auto Scope = AnalogSource.RenderScope(0, 400);
            for(auto & X : Scope)
                X = .5f*X + .5f;
            ImGui::PlotLines("", Scope.data(), Scope.size(), 0, nullptr, 0.0f, 1.0f, ImVec2(0, 180.0f));

  //          ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();

			if(ImGui::IsKeyPressed(VK_SPACE, false))
                Synth.NoteOn(0, 69, 1.f);
			if(ImGui::IsKeyReleased(VK_SPACE))
                Synth.NoteOff(0, 69);
		    
            DSoundTools::Render(Synth, MasterVolume);
        }

        bool ShowDemo = false;
        if(ShowDemo)
            ImGui::ShowDemoWindow(&ShowDemo);

        // Rendering
        ImGui::PopFont();
        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync
    }

    // Cleanup
	DSoundTools::Release();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
