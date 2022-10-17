#include <stdio.h>
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include <d3d9.h>
#include <d3dx9.h>
#include "NativeWindow.h"
#include <chrono>
#include "drawing.h"
#include <iostream>
#include <thread>
#include "jpge.h"
#include "jpgd.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

/// <summary>
/// 
/// D3D9 Test environment w/ corrected dependancies
/// CHANGE TO X64 MODE OR IT WONT COMPILE
/// 
/// </summary>

WNDPROC oWndProc;
std::chrono::high_resolution_clock hrTimer;
char exTimeTxt[256]{ 0 };

IDirect3D9* pD3d = nullptr;
IDirect3DDevice9* pDevice = nullptr;
ID3DXFont* pFont = nullptr;
ID3DXSprite* pSprite = nullptr;

IDirect3DSurface9* surface;
IDirect3DSurface9* smsurface;
IDirect3DSurface9* backbuffer;

const RECT thermalWindow = { 25, 25, WND_WIDTH - 50, WND_HEIGHT - 50 };

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


typedef struct tagRGBTRI {
	BYTE    rgbBlue;
	BYTE    rgbGreen;
	BYTE    rgbRed;
} RGBTRI;

LRESULT APIENTRY hWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
		return true;
	}

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

int nScreenWidth;
int nScreenHeight;
HWND hDesktopWnd;
HDC hDesktopDC;
HDC hCaptureDC;
HBITMAP hCaptureBitmap;

RGBQUAD* pPixelsFront = 0;
RGBQUAD* pPixelsPrev = 0;
RGBQUAD* pPixelsDifference = 0;
RGBQUAD* pPixelsDecompressed = 0;
//RGBQUAD* pPixelsBack = 0;

IDirect3DTexture9* textureFront;
//IDirect3DTexture9* textureBack;

D3DLOCKED_RECT drawCanvas;

BITMAPINFO bmi = { 0 };

LPDIRECT3DSURFACE9 l_RenderTarget, l_Surface;

byte* jpgDataPile = 0;

bool compRgbQuad(RGBQUAD a, RGBQUAD b) {
	if (a.rgbRed == b.rgbRed) {
		if (a.rgbGreen == b.rgbGreen) {
			if (a.rgbBlue == b.rgbBlue) {
				if (a.rgbReserved == b.rgbReserved) {
					return true;
				}
			}
		}
	}
	return false;
}

bool InitD3D(HWND hWnd, UINT uWidth, UINT uHeight)
{
	pD3d = Direct3DCreate9( D3D_SDK_VERSION );
	D3DPRESENT_PARAMETERS dp{ 0 };
	//dp.BackBufferWidth = uWidth;
	//dp.BackBufferHeight = uHeight;
	//dp.hDeviceWindow = hWnd;
	//dp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	//dp.Windowed = TRUE;

	dp.Windowed = TRUE;
	dp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	dp.BackBufferFormat = D3DFMT_UNKNOWN;
	dp.EnableAutoDepthStencil = TRUE;
	dp.AutoDepthStencilFormat = D3DFMT_D16;
	dp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGuiIO& io = ImGui::GetIO();

	HRESULT hr = pD3d->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &dp, &pDevice );
	if (FAILED( hr ) || !pDevice)
	{
		MessageBox( NULL, _T( "Failed to create D3D Device." ), _T( "Error" ), MB_ICONERROR | MB_OK );
		return false;
	}

	nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	nScreenHeight = GetSystemMetrics(SM_CYSCREEN);
	hDesktopWnd = GetDesktopWindow();
	hDesktopDC = GetDC(hDesktopWnd);
	hCaptureDC = CreateCompatibleDC(hDesktopDC);

	bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
	bmi.bmiHeader.biWidth = nScreenWidth;
	bmi.bmiHeader.biHeight = nScreenHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	jpgDataPile = new byte[500000];

	pPixelsFront = new RGBQUAD[nScreenWidth * nScreenHeight];
	pPixelsPrev = new RGBQUAD[nScreenWidth * nScreenHeight];
	pPixelsDifference = new RGBQUAD[nScreenWidth * nScreenHeight];
	pPixelsDecompressed = new RGBQUAD[nScreenWidth * nScreenHeight];
	pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);

	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX9_Init(pDevice);

	return true;
}


bool tKill = false;
bool cKill = false;
int updates = 0;

void createDifferenceOnlyImage(RGBQUAD* output, RGBQUAD* current, RGBQUAD* old, int resX, int resY) {
	int pt = 0;
	for (int x = 0; x < resX; x++) {
		for (int y = 0; y < resY; y++) {
			if (!compRgbQuad(pPixelsFront[pt], pPixelsPrev[pt])) {
				output[pt] = current[pt];
			}
			else {
				output[pt] = { 0, 0, 0, 0 };
			}
			pt++;
		}
	}
}

int createJpgCompressedImage(uintptr_t* pxOut, RGBQUAD* pxIn, int resX, int resY) {
	jpge::params pr;
	pr.m_quality = 25;
	pr.m_subsampling = jpge::subsampling_t::H2V2;
	pr.m_no_chroma_discrim_flag = false;
	pr.m_two_pass_flag = false;
	pr.m_use_std_tables = true;

	int dataSize = 500000;
	int c_resX = resX;
	int c_resY = resY;
	int compsOut = 0;
	//jpge::compress_image_to_jpeg_file("spoog.jpg", resX, resY, 4, (jpge::uint8*)pxIn, pr);

	if (!jpge::compress_image_to_jpeg_file_in_memory(jpgDataPile, dataSize, resX, resY, 4, (jpge::uint8*)pxIn, pr)) {
		dataSize = -1;
		return dataSize;
	}

	*pxOut = (uintptr_t)jpgd::decompress_jpeg_image_from_memory(jpgDataPile, dataSize, &c_resX, &c_resY, &compsOut, 4);

	return dataSize;
}


void updateScreenshot(bool* kill) {
	updates = 0;
	auto startTime = hrTimer.now();
	GetDIBits(hCaptureDC, hCaptureBitmap, 0, nScreenHeight, pPixelsFront, &bmi, DIB_RGB_COLORS);
	//createDifferenceOnlyImage(pPixelsDifference, pPixelsFront, pPixelsPrev, nScreenWidth, nScreenHeight);

	uintptr_t decompressed = 0;
	printf_s("Compressed size: %0.5f\n", (float)(createJpgCompressedImage(&decompressed, pPixelsFront, nScreenWidth, nScreenHeight) / 1024000.f));
	RGBQUAD* decompressedRGBQ = (RGBQUAD*)decompressed;
	printf_s("PDCOM* : %p\n", (void*)decompressed);
	RECT frameDrawingTemplate = { 0, 0, nScreenWidth, nScreenHeight };

	//textureFront->LockRect(0, &drawCanvas, NULL, D3DUSAGE_WRITEONLY);
	surface->LockRect(&drawCanvas, &frameDrawingTemplate, D3DLOCK_DISCARD);

	char* data = (char*)drawCanvas.pBits;

	for (int y = 0; y < nScreenHeight; y++) {
		DWORD* row = (DWORD*)data;
		for (int x = 0; x < nScreenWidth; x++) {
			int p = (nScreenHeight - y - 1) * nScreenWidth + x; // upside down

			if (decompressedRGBQ) {
				updates++;
				char r = decompressedRGBQ[p].rgbRed;
				char g = decompressedRGBQ[p].rgbGreen;
				char b = decompressedRGBQ[p].rgbBlue;
				DWORD tRGB = 0xFFFFFFFF;
				byte* bpRGB = (byte*)&tRGB;
				bpRGB[0] = b;
				bpRGB[1] = g;
				bpRGB[2] = r;
				*row++ = tRGB;

			}



			//if (!compRgbQuad(pPixelsFront[p], pPixelsPrev[p])) {
			//	updates++;
			//	char r = pPixelsFront[p].rgbRed;
			//	char g = pPixelsFront[p].rgbGreen;
			//	char b = pPixelsFront[p].rgbBlue;
			//	DWORD tRGB = 0xFFFFFFFF;
			//	byte* bpRGB = (byte*)&tRGB;
			//	bpRGB[0] = b;
			//	bpRGB[1] = g;
			//	bpRGB[2] = r;
			//	*row++ = tRGB;
			//}
			//else {
			//	char r = pPixelsPrev[p].rgbRed;
			//	char g = pPixelsPrev[p].rgbGreen;
			//	char b = pPixelsPrev[p].rgbBlue;
			//	DWORD tRGB = 0xFF000000;
			//	//byte* bpRGB = (byte*)&tRGB;
			//	//bpRGB[0] = b;
			//	//bpRGB[1] = g;
			//	//bpRGB[2] = r;
			//	*row++ = tRGB;
			//}
		}
		data += drawCanvas.Pitch;
	}

	//textureFront->UnlockRect(0);
	surface->UnlockRect();
	free((void*)decompressed);

	//printf_s("MBs updated: %0.2f\n", ((updates * 3) / 1024000.f));

	memcpy(pPixelsPrev, pPixelsFront, (sizeof(RGBQUAD) * (nScreenWidth * nScreenHeight)));

	auto endTime = hrTimer.now();
	float scsTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() / 1000.f;
	//printf_s("Time MS: %f\n", scsTime);
	if (scsTime < 66.6f) {
		//printf_s("Sleep MS: %f\n", 66.6f - scsTime);
		Sleep(66.6f - scsTime);
	}
}


void Render(NativeWindow& wnd)
{
	auto startTime = hrTimer.now();
	pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
	pDevice->BeginScene();
	pDevice->Clear( 1, nullptr, D3DCLEAR_TARGET, 0x00111111, 1.0f, 0 );
	hCaptureBitmap = CreateCompatibleBitmap(hDesktopDC, nScreenWidth, nScreenHeight);
	SelectObject(hCaptureDC, hCaptureBitmap);
	BitBlt(hCaptureDC, 0, 0, nScreenWidth, nScreenHeight, hDesktopDC, 0, 0, SRCCOPY | CAPTUREBLT);

	//begin ImGui frame
	//ImGui_ImplDX9_NewFrame();
	//ImGui_ImplWin32_NewFrame();
	//ImGui::NewFrame();

	//START
	
	if (!surface) {
		pDevice->CreateOffscreenPlainSurface(nScreenWidth, nScreenHeight, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &surface, NULL);
		pDevice->CreateOffscreenPlainSurface(nScreenWidth, nScreenHeight, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &smsurface, NULL);
		pDevice->CreateTexture(nScreenWidth, nScreenHeight, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &textureFront, NULL);
		//std::thread scsThread = std::thread(updateScreenshot, &tKill);		
		//scsThread.detach();
	}


	pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
	pDevice->StretchRect(surface, NULL, backbuffer, &thermalWindow, D3DTEXF_LINEAR);

	updateScreenshot(&tKill);

	/*ImGui::Begin("Screen");

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2 posMin = ImGui::GetWindowContentRegionMin();
	ImVec2 posMax = ImGui::GetWindowContentRegionMax();
	ImVec2 windowPos = ImGui::GetWindowPos();
	ImVec2 adjustedPosMin = ImVec2(windowPos.x + posMin.x, windowPos.y + posMin.y);
	ImVec2 adjustedPosMax = ImVec2(windowPos.x + posMax.x, windowPos.y + posMax.y);
	drawList->AddImage(textureFront, adjustedPosMin, adjustedPosMax);


	ImGui::End();

	DrawTextC(exTimeTxt, 10, 10, 15, white, pDevice);
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());*/
	pDevice->EndScene();
	pDevice->Present( 0, 0, 0, 0 );
	auto endTime = hrTimer.now();
	float eTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() / 1000.f;
	sprintf_s(exTimeTxt, 256, "FPS    : %i", (int)(1000.f / eTime));
}

FILE* createConsole() {
	AllocConsole();
	std::wstring strW = L"screenCapTesting";
	SetConsoleTitle(strW.c_str());
	HWND console = GetConsoleWindow();
	DeleteMenu(GetSystemMenu(console, false), SC_CLOSE, MF_BYCOMMAND);
	SetWindowLong(console, GWL_STYLE, GetWindowLong(console, GWL_STYLE) & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX);
	FILE* f;
	freopen_s(&f, "CONOUT$", "w", stdout);
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	std::cout.precision(2);
	SMALL_RECT tmp = { 0, 0, 120, 15 };
	SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), true, &tmp);
	return f;
}

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	createConsole();
	NativeWindow wnd;
	wnd.Create( hInstance, nCmdShow );
	if (!InitD3D(wnd.GetHandle(), WND_WIDTH, WND_HEIGHT)) { return 1; }

	oWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(wnd.GetHandle(), GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hWndProc)));
	MSG m;
	while (true)
	{
		while (PeekMessage( &m, NULL, 0, 0, PM_REMOVE ) && m.message != WM_QUIT)
		{
			TranslateMessage( &m );
			DispatchMessage( &m );
		}
		if (m.message == WM_QUIT) {
			tKill = true;
			while (!cKill) {}
			break;
		}
		Render(wnd);
	}

	return 0;
}