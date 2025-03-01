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
#include "RtMidi/RtMidi.h"

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
static const int WSizeW = 1600;
static const int WSizeH = 160;
static const float PI = 3.1415926f;

bool Knob(const char * label, float & value, float minv, float maxv, float Size = 36.f, int NbSegments = 16, bool bCentered=false)
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

	if(ImGui::IsItemHovered())
		ImGui::SetTooltip("%8.4g", value);

	const ImU32 col32 = ImGui::GetColorU32(is_active ? ImGuiCol_FrameBgActive : ImGui::IsItemHovered() ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
	const ImU32 col32line = ImGui::GetColorU32(ImGuiCol_SliderGrabActive); 
	const ImU32 col32text = ImGui::GetColorU32(ImGuiCol_Text);
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	
	auto PointOnRadius = [center](float angle, float rad) -> ImVec2 
	{
		return {-sinf(angle)*rad + center.x, cosf(angle)*rad + center.y};
	};

	float th = bCentered ? PI : gamma;
	const float step = (alpha - (bCentered ? PI : gamma)) / float(NbSegments);
	for(int i = 0; i < NbSegments; i++, th += step)
		draw_list->AddLine(PointOnRadius(th, radius + 1.f), PointOnRadius(th + step + (value >= 0.f ? .02f : -.02f), radius + 1.f), ImGui::GetColorU32(ImVec4(9.f, .4f, .2f, 1.f)), 2.f);

	draw_list->AddCircleFilled(center, radius, col32, NbSegments);

	const ImVec2 CursotTip = PointOnRadius(alpha, radius);
	draw_list->AddLine({.5f * (center.x + CursotTip.x), .5f * (center.y + CursotTip.y)}, CursotTip, col32line, 3);
	
	ImVec2 TSize = ImGui::CalcTextSize(label);
	draw_list->AddText({CursorPos.x + .5f*(Size - TSize.x), CursorPos.y + Size + style.ItemInnerSpacing.y}, col32text, label);

	//if(is_active)
	//	draw_list->AddText(nullptr, 11.f, CursorPos, col32text, std::to_string(value).c_str());
	
	return is_active;
}

SynthOX::Synth Synth;

void MIDICallback( double deltatime, std::vector< unsigned char > *message, void *userData )
{
	unsigned int nBytes = (unsigned int)message->size();
	for ( unsigned int i=0; i<nBytes; i++ )
		std::cout << "Byte " << i << " = " << (int)message->at(i) << ", ";
	if ( nBytes > 0 )
		std::cout << "stamp = " << deltatime << std::endl;

	if(nBytes > 0)
	{
		auto Message = *message;
		switch(Message[0])
		{
		case 144: // Note On/Off
			if(Message[2] == 0)
				Synth.NoteOff(0, Message[1]);
			else
				Synth.NoteOn(0, Message[1], std::powf( float(Message[2]) / 127.f, .5f) );

			break;
		case 224: // PitchBend
			Synth.m_PitchBend = float(Message[2] - 64) / 64.f;
			break;
		}
	}
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

	SynthOX::AnalogSourceData AnalogSourceData;
	AnalogSourceData.m_LeftVolume = .7f;
	AnalogSourceData.m_RightVolume = .7f;
	AnalogSourceData.m_AmpADSR.m_Attack = .1f;
	AnalogSourceData.m_AmpADSR.m_Decay = .1f;
	AnalogSourceData.m_AmpADSR.m_Sustain = .7f;
	AnalogSourceData.m_AmpADSR.m_Release = .3f;
	AnalogSourceData.m_FilterADSR.m_Attack = .1f;
	AnalogSourceData.m_FilterADSR.m_Decay = .1f;
	AnalogSourceData.m_FilterADSR.m_Sustain = .7f;
	AnalogSourceData.m_FilterADSR.m_Release = .3f;

	AnalogSourceData.m_OscillatorTab[0].m_ModulationType = SynthOX::ModulationType::Mix;
	AnalogSourceData.m_OscillatorTab[0].m_LFOTab[(int)SynthOX::LFODest::Morph].m_BaseValue = .5f;
	AnalogSourceData.m_OscillatorTab[0].m_LFOTab[(int)SynthOX::LFODest::Squish].m_BaseValue = .5f;
	AnalogSourceData.m_OscillatorTab[0].m_LFOTab[(int)SynthOX::LFODest::Distort].m_BaseValue = .707f;
	AnalogSourceData.m_OscillatorTab[0].m_LFOTab[(int)SynthOX::LFODest::Tune].m_BaseValue = .0f;
	AnalogSourceData.m_OscillatorTab[0].m_LFOTab[(int)SynthOX::LFODest::Decat].m_BaseValue = .0f;
	
	AnalogSourceData.m_OscillatorTab[1].m_ModulationType = SynthOX::ModulationType::Mix;
	AnalogSourceData.m_OscillatorTab[1].m_LFOTab[(int)SynthOX::LFODest::Morph].m_BaseValue = .41f;
	AnalogSourceData.m_OscillatorTab[1].m_LFOTab[(int)SynthOX::LFODest::Squish].m_BaseValue = .5f;
	AnalogSourceData.m_OscillatorTab[1].m_LFOTab[(int)SynthOX::LFODest::Distort].m_BaseValue = .74f;
	AnalogSourceData.m_OscillatorTab[1].m_LFOTab[(int)SynthOX::LFODest::Volume].m_BaseValue = .5f;
	AnalogSourceData.m_OscillatorTab[1].m_LFOTab[(int)SynthOX::LFODest::Tune].m_BaseValue = .1f;
	AnalogSourceData.m_OscillatorTab[1].m_LFOTab[(int)SynthOX::LFODest::Decat].m_BaseValue = .0f;
	
	AnalogSourceData.m_PolyphonyMode = SynthOX::PolyphonyMode::Portamento;
	AnalogSourceData.m_PortamentoTime = .2f;
	AnalogSourceData.m_ArpeggioPeriod = .2f;
	
	SynthOX::AnalogSource AnalogSource(&Synth.m_OutBuf, 0, &AnalogSourceData);
	Synth.BindSource(AnalogSource);

	RtMidiIn midiin;
	unsigned int nPorts = midiin.getPortCount();
	if ( nPorts == 0 ) 
	{
		std::cout << "No MIDI ports available!\n";
	}
	else
	{
		try
		{
			midiin.openPort( 0 );
			std::cout << midiin.getPortName( 1 ) << '\n';

			midiin.setCallback( &MIDICallback );
			midiin.ignoreTypes( false, false, false );
		}
		catch(std::exception e)
		{
			std::cout << e.what();
			return 1;
		}
	}

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
	ImFont * LUCONFont = io.Fonts->AddFontFromFileTTF("LUCON.TTF", 10.0f);
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

			ImGui::SetNextWindowSize({WSizeW, 2*WSizeH});
			ImGui::SetNextWindowPos({0.f, 0.f});

			bool bDummy = false;
			ImGui::Begin("SynthOX", &bDummy, ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove);                          // Create a window called "Hello, world!" and append into it.

			int Id = 0;
			auto VSlider = [&Id](float & Val, bool SameLine=true) 
			{
				ImGui::PushID(Id++);
				if(SameLine)	ImGui::SameLine(); 
				ImGui::VSliderFloat("##v", {10.f, 120.0f}, &Val, 0.0f, 1.f, "");  
				ImGui::PopID();
			};

			static const float KnobSize = 40.f;
			static float NoteOffset[2];
			static float OctaveOffset[2];
			static bool bMul = false;

			auto OscillatorUI = [&](int Osc)
			{
				ImGui::PushID(Osc);
				Knob("Morph",  AnalogSourceData.m_OscillatorTab[Osc].m_LFOTab[(int)SynthOX::LFODest::Morph].m_BaseValue, 0.f, 1.f, KnobSize, 16);
				ImGui::SameLine(0, 15.f); Knob("Squish", AnalogSourceData.m_OscillatorTab[Osc].m_LFOTab[(int)SynthOX::LFODest::Squish].m_BaseValue, 0.f, 1.f, KnobSize, 16);
				ImGui::SameLine(0, 15.f); Knob("Crank",  AnalogSourceData.m_OscillatorTab[Osc].m_LFOTab[(int)SynthOX::LFODest::Distort].m_BaseValue, 0.f, 1.f, KnobSize, 16);
				ImGui::SameLine(0, 15.f); Knob("Tune",  AnalogSourceData.m_OscillatorTab[Osc].m_LFOTab[(int)SynthOX::LFODest::Tune].m_BaseValue, -1.f, 1.f, KnobSize, 16, true);
				ImGui::SameLine(0, 15.f); Knob("Decat",  AnalogSourceData.m_OscillatorTab[Osc].m_LFOTab[(int)SynthOX::LFODest::Decat].m_BaseValue, 0.f, 1.f, KnobSize, 16);
				ImGui::SameLine(0, 15.f); Knob("Amp",  AnalogSourceData.m_OscillatorTab[Osc].m_LFOTab[(int)SynthOX::LFODest::Volume].m_BaseValue, 0.f, 1.f, KnobSize, 16);
				
				ImGui::SameLine(0, 15.f); Knob("Note", NoteOffset[Osc], -12.f, 12.f, KnobSize, 16, true);
				AnalogSourceData.m_OscillatorTab[Osc].m_NoteOffset = char(NoteOffset[Osc]);
				ImGui::SameLine(0, 15.f); Knob("Oct", OctaveOffset[Osc], -4.f, 4.f, KnobSize, 16, true);
				AnalogSourceData.m_OscillatorTab[Osc].m_OctaveOffset = char(OctaveOffset[Osc]);

				auto Scope = AnalogSource.RenderScope(Osc, 100);
				for(auto & X : Scope) X = .5f*X + .5f;
				ImGui::SameLine(0, 15.f); ImGui::PlotLines("", Scope.data(), (int)Scope.size(), 0, nullptr, 0.0f, 1.0f, ImVec2(120.f, 60.0f));

				if(Osc == 1) 
				{ 
					ImGui::SameLine(0, 15.f); ImGui::Checkbox("Mod", &bMul); 
					AnalogSourceData.m_OscillatorTab[1].m_ModulationType = bMul ? SynthOX::ModulationType::Mul : SynthOX::ModulationType::Mix;
				}

				ImGui::PopID();
			};
			
			ImGui::BeginGroup();
			static int PolyMode = 0;
			ImGui::SetNextItemWidth(100.f);
	        ImGui::Combo("PolyMode", &PolyMode, "Poly\0Arpeggio\0Portamento\0\0");
			AnalogSourceData.m_PolyphonyMode = SynthOX::PolyphonyMode(PolyMode);
			ImGui::SameLine(0, 15.f); Knob("Port", AnalogSourceData.m_PortamentoTime, 0.f, 12.f, KnobSize*.8f, 16);
			ImGui::SameLine(0, 15.f); Knob("Arp", AnalogSourceData.m_ArpeggioPeriod, 0.f, 1.f, KnobSize*.8f, 16);
//			ImGui::SameLine(0, 15.f); Knob("Arp", AnalogSourceData.m_PortamentoTime, 0.f, 12.f, KnobSize*.8f, 16);
//			else if(AnalogSourceData.m_PolyphonyMode == SynthOX::PolyphonyMode::Portamento)
//				ImGui::SameLine(0, 15.f); Knob("Speed", AnalogSourceData., 0.f, 12.f, KnobSize*.8f, 16);
			ImGui::EndGroup();
			
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.f);
			ImGui::BeginGroup();
			OscillatorUI(0);
			OscillatorUI(1);
			ImGui::EndGroup();
			
			ImGui::SameLine();
			ImGui::BeginGroup();
			ImGui::PushID("FilterADSR");
			VSlider(AnalogSourceData.m_FilterADSR.m_Attack);
			VSlider(AnalogSourceData.m_FilterADSR.m_Decay);
			VSlider(AnalogSourceData.m_FilterADSR.m_Sustain);
			VSlider(AnalogSourceData.m_FilterADSR.m_Release, true);           
			ImGui::SameLine(0, 15.f); Knob("F-Amount", AnalogSourceData.m_FilterDrive, 0.f, 1.f, KnobSize, 16);
			ImGui::SameLine(0, 15.f); Knob("F-Freq", AnalogSourceData.m_FilterFreq, 0.f, .5f, KnobSize, 16);
			ImGui::SameLine(0, 15.f); Knob("F-Reso", AnalogSourceData.m_FilterReso, 0.f, 1.f, KnobSize, 16);
			ImGui::SameLine(0, 15.f); ImGui::Checkbox("Inv", &AnalogSourceData.m_InvFilterEnv); 
			ImGui::PopID();
			ImGui::EndGroup();

			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20.f);

			ImGui::PushID("AmpADSR");
			VSlider(AnalogSourceData.m_AmpADSR.m_Attack, false);
			VSlider(AnalogSourceData.m_AmpADSR.m_Decay);
			VSlider(AnalogSourceData.m_AmpADSR.m_Sustain);
			VSlider(AnalogSourceData.m_AmpADSR.m_Release, true);           
			ImGui::PopID();

			static float Pan = .0f;
			ImGui::SameLine(0, 15.f); Knob("Pan", Pan, -1.f, 1.f, KnobSize, 16, true);
			AnalogSourceData.m_RightVolume = std::clamp(1.f + Pan, .0f, 1.f);
			AnalogSourceData.m_LeftVolume = std::clamp(1.f - Pan, .0f, 1.f);

			ImGui::SameLine(0, 15.f); Knob("Master", MasterVolume, 0.f, 1.f, 64.f, 24);

			{
				std::array<float, 1000> X;
				for(int i = 0; i < 1000; i++)
					X[i] = DSoundTools::Oscillo[(DSoundTools::OscilloCursor + i) % DSoundTools::Oscillo.size()];
				ImGui::SameLine(0, 15.f); ImGui::PlotLines("Osc", X.data(), (int)X.size(), 0, nullptr, -1.0f, 1.0f, ImVec2(420.f, 120.0f));
			}
			
			ImGui::End();

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
