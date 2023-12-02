#define WIDTH 1920
#define HEIGHT 1080               

#include <d3d11.h>
#include <d3dcompiler.h>

extern "C" int _fltused = 0;

//	a fullscreen window with stretched content is more compatible than oldskool fullscreen mode these days
static int SwapChainDesc[] = {
	WIDTH, HEIGHT, 0, 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, DXGI_MODE_SCALING_STRETCHED,  // w,h,refreshrate,format, scanline, (fullscreen)scaling
	1, 0, // sampledesc (count, quality)
	DXGI_USAGE_RENDER_TARGET_OUTPUT, // usage
	1, // buffercount
	0, // hwnd
	0, 0, 0 }; // windowed, swap_discard, flags

static D3D11_BUFFER_DESC ConstBufferDesc = { 16, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 }; // 256,0,4,0,0,0

//	a vertex/geoemetry/pixel shader pipeline
const char shader[] =
"float t; float r;"
"struct VS_INPUT{uint ID : SV_VERTEXID;};"
"struct VS_OUTPUT{float4 position : SV_POSITION;};"
"struct PS_INPUT{float4 position : SV_POSITION; float3 normal: TEXCOORD0;};"
"VS_OUTPUT VSMain(VS_INPUT input){VS_OUTPUT output;output.position=float4(0.0, 0.0, 0.0, 0.0);return output;}"
"static const float3 cubeVertices[24] = {"
"1.0, -1.0, 1.0,1.0, 1.0, 1.0,-1.0, -1.0, 1.0,-1.0, 1.0, 1.0,"
"-1.0, 1.0, 1.0,-1.0, 1.0, -1.0,-1.0, -1.0, 1.0,-1.0, -1.0, -1.0,"
"1.0, 1.0, -1.0,1.0, 1.0, 1.0,1.0, -1.0, -1.0,1.0, -1.0, 1.0,"
"-1.0, 1.0, -1.0,1.0, 1.0, -1.0,-1.0, -1.0, -1.0,1.0, -1.0, -1.0,"
"1.0, 1.0, 1.0, 1.0, 1.0, -1.0,-1.0, 1.0, 1.0,-1.0, 1.0, -1.0,"
"-1.0,-1.0,-1.0,1.0, -1.0, -1.0,-1.0, -1.0, 1.0,1.0, -1.0, 1.0};"
"static const float3 cubeNormals[6] = {"
"0.0,0.0,1.0,-1.0,0.0,0.0,1.0,0.0,0.0,"
"0.0,0.0,-1.0,0.0,1.0,0.0,0.0,-1.0,0.0};"
"[maxvertexcount(24)] void GSMain(point VS_OUTPUT input[1], inout TriangleStream<PS_INPUT> tri_stream){"
"float3 vforward = normalize(float3(cos(0.2*t), sin(0.3*t), sin(0.2*t)));float3 vup = float3(0.0,1.0,0.0);float3 vright=normalize(cross(vforward,vup));"
"vup=normalize(cross(vforward,vright));float s = 0.1;"
"for (int i = 0; i < 6; i++){for (int j = 0; j < 4; j++){"
"PS_INPUT output;float3 pos = 5.0*cubeVertices[4 * i + j] - (5.0+2.0*sin(0.1*t)) * vforward; float d = 0.2-0.02*dot(pos,vforward);"
"output.position = float4(float3(d*s*dot(pos, vright), d*s*r*dot(pos, vup), 0.5), 1.0); output.normal = cubeNormals[i]; tri_stream.Append(output);}"
"tri_stream.RestartStrip(); }}"
"float4 PSMain(PS_INPUT input) : SV_TARGET{return float4(0.5+0.5*input.normal, 1.0);}";

//	background clear color
float BACKGROUND_COL[4] = { 0.0,0.0,0.0,1.0 };

// device variables
HWND  hWnd;
ID3D11Device* pd3dDevice;
ID3D11DeviceContext* pImmediateContext;
IDXGISwapChain* pSwapChain;

// gpu resources
ID3D11Texture2D* frameBuffer;
ID3D11RenderTargetView* frameBufferView;
ID3D11Buffer* pConstants;

//	::blob for shader stuff
ID3DBlob* pBlob;

//	::vertex, geometry and pixel shader
ID3D11VertexShader* pVS;
ID3D11GeometryShader* pGS;
ID3D11PixelShader* pPS;

//	::viewport for render
D3D11_VIEWPORT viewport = { 0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 1.0f };


#ifndef _DEBUG
void WinMainCRTStartup()
{
	ShowCursor(0);
	hWnd = CreateWindowExA(0,(LPCSTR)0xC018, 0, WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, WIDTH, HEIGHT, 0, 0, 0, 0); //"edit"
#else
#include <stdio.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "math.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
int WinMain(HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpszArgument, int nFunsterStil)
{
	HWND hWnd = CreateWindowA("edit", 0, WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, WIDTH, HEIGHT, 0, 0, 0, 0);
#endif
	SwapChainDesc[11] = (int)hWnd;
#ifdef _DEBUG	
	D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, 0, 0, D3D11_SDK_VERSION, (DXGI_SWAP_CHAIN_DESC*)&SwapChainDesc[0], &pSwapChain, &pd3dDevice, NULL, &pImmediateContext);
#else
	D3D11CreateDeviceAndSwapChain( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, 0,0, D3D11_SDK_VERSION, (DXGI_SWAP_CHAIN_DESC*)&SwapChainDesc[0], &pSwapChain, &pd3dDevice, NULL, &pImmediateContext );
#endif

	//	get backbuffer view
	pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&frameBuffer);
	pd3dDevice->CreateRenderTargetView(frameBuffer, nullptr, &frameBufferView);

	//	create global constant buffer and attach to shaders
	pd3dDevice->CreateBuffer(&ConstBufferDesc, NULL, &pConstants);
	pImmediateContext->VSSetConstantBuffers(0, 1, &pConstants);
	pImmediateContext->GSSetConstantBuffers(0, 1, &pConstants);
	pImmediateContext->PSSetConstantBuffers(0, 1, &pConstants);

	//	setup global variables: two floats. Time and aspect ratio.
	float c[2] = { 0.0f, 0.0f };
	c[1] = 1.0 * WIDTH / HEIGHT;

#ifdef _DEBUG
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	ImGui::StyleColorsDark();
	ImGui_ImplDX11_Init(pd3dDevice, pImmediateContext);
	ImGui_ImplWin32_Init(hWnd);
#endif

	//	compilation and creation of shaders
	D3DCompile(shader, sizeof(shader), 0, 0, 0, "VSMain", "vs_5_0", 0, 0, &pBlob, nullptr);
	//	for sanity checks save the D3DCompile in HRESULT hr variable. If failed, gather the shader compilation 
	//	error from last parameter in D3DCompile function (ID3D10Blob* pError)
	pd3dDevice->CreateVertexShader((void*)(((int*)pBlob)[3]), ((int*)pBlob)[2], NULL, &pVS);
	D3DCompile(shader, sizeof(shader), 0, 0, 0, "GSMain", "gs_5_0", 0, 0, &pBlob, nullptr);
	pd3dDevice->CreateGeometryShader((void*)(((int*)pBlob)[3]), ((int*)pBlob)[2], NULL, &pGS);
	D3DCompile(shader, sizeof(shader), 0, 0, 0, "PSMain", "ps_5_0", 0, 0, &pBlob, nullptr);
	pd3dDevice->CreatePixelShader((void*)(((int*)pBlob)[3]), ((int*)pBlob)[2], NULL, &pPS);

	//	we active the shaders for the loop (we can do here because there is no need for doing in the update)
	pImmediateContext->VSSetShader(pVS, nullptr, 0);
	pImmediateContext->GSSetShader(pGS, nullptr, 0);
	pImmediateContext->PSSetShader(pPS, nullptr, 0);
	pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	pImmediateContext->IASetInputLayout(nullptr);

	do
	{
		//	update the time with a fixed delta time
		c[0] += 0.01f;

#ifdef _DEBUG
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGui::SetNextWindowSize(ImVec2(299, 100), ImGuiCond_FirstUseEver);
		ImGui::Begin("Info");
			ImGui::Text("Current time: %.3f", c[0]);
			float dir[3] = { cosf(0.3 * c[0]),sinf(0.2 * c[0]),sinf(0.3 * c[0]) };
			ImGui::Text("Camera direction: %.3f %.3f %.3f", dir[0],dir[1],dir[2]);
		ImGui::End();
		ImGui::Render();
		MSG msg;
		while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (ImGui_ImplWin32_WndProcHandler(hWnd, msg.message, msg.wParam, msg.lParam)) {
				continue;
			}

			DispatchMessageA(&msg);
		}
#endif
		//	essentially we DRAW "a vertex". This one will go from vertex to geometry shader where a box will be created. 
		//	then the pixel shader will give some color based on a given normal
		pImmediateContext->ClearRenderTargetView(frameBufferView, BACKGROUND_COL);
		pImmediateContext->OMSetRenderTargets(1, &frameBufferView, nullptr);
		pImmediateContext->UpdateSubresource(pConstants, 0, 0, &c, 0, 0);
		pImmediateContext->RSSetViewports(1, &viewport);
		pImmediateContext->Draw(1,0);

#ifdef _DEBUG
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#endif
		//	vsync
		pSwapChain->Present(1, 0);

	} 
	while (!GetAsyncKeyState(VK_ESCAPE));

	ExitProcess(0);  // actually not needed in this setup, but may be smaller..
}
