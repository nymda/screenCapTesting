#include "drawing.h"
#include <vector>
#include <iostream>

void DrawFilledRect(int x, int y, int w, int h, D3DCOLOR color, IDirect3DDevice9* dev)
{
	D3DRECT BarRect = { x, y, x + w, y + h };
	dev->Clear(1, &BarRect, D3DCLEAR_TARGET | D3DCLEAR_TARGET, color, 0, 0);
}

ID3DXLine* LineL;
void DrawLine(int x1, int y1, int x2, int y2, int thickness, D3DCOLOR color, IDirect3DDevice9* dev) {

	if (!LineL) {
		D3DXCreateLine(dev, &LineL);
	}

	D3DXVECTOR2 Line[2];
	Line[0] = D3DXVECTOR2(x1, y1);
	Line[1] = D3DXVECTOR2(x2, y2);
	LineL->SetWidth(thickness);
	LineL->Draw(Line, 2, color);
}

//draws a skeletal circle using drawPrimitiveUP
void drawCircleD3D(float x, float y, float radius, int sides, float width, D3DCOLOR color, LPDIRECT3DDEVICE9 pDevice)
{
	float angle = D3DX_PI * 2 / sides;
	float _cos = cos(angle);
	float _sin = sin(angle);
	float x1 = radius, y1 = 0, x2, y2;
	Vert* cVerts = new Vert[(sides+1)];

	for (int i = 0; i < sides; i++)
	{
		x2 = _cos * x1 - _sin * y1 + x;
		y2 = _sin * x1 + _cos * y1 + y;
		x1 += x;
		y1 += y;
		cVerts[i] = { x1, y1, 0.f, 1.f, color };
		x1 = x2 - x;
		y1 = y2 - y;
	}
	cVerts[sides] = cVerts[0];
	pDevice->DrawPrimitiveUP(D3DPT_LINESTRIP, sides, cVerts, sizeof(Vert));
}

//draws a filled circle using drawPrimitiveUP
void drawCircleFilledD3D(float x, float y, float radius, int sides, float width, D3DCOLOR color, LPDIRECT3DDEVICE9 pDevice)
{
	float angle = D3DX_PI * 2 / sides;
	float _cos = cos(angle);
	float _sin = sin(angle);
	float x1 = radius, y1 = 0, x2, y2;
	Vert* cVerts = new Vert[(sides * 3 + 3)];

	for (int i = 0; i < sides * 3; i++)
	{
		//every third loop, insert the center point for vertex 3 of this triangle
		if (i % 3 == 0) {
			cVerts[i] = { x, y, 0.f, 1.f, color };
		}
		else { //add the two other vertecies
			x2 = _cos * x1 - _sin * y1 + x;
			y2 = _sin * x1 + _cos * y1 + y;
			x1 += x;
			y1 += y;
			cVerts[i] = { x1, y1, 0.f, 1.f, color };
			x1 = x2 - x;
			y1 = y2 - y;
		}
	}
	//duplicate the first triangle to the last to close the circle
	cVerts[sides * 3] = cVerts[0];
	cVerts[sides * 3 + 1] = cVerts[1];
	pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, sides * 3, cVerts, sizeof(Vert));
}


bool initFonts = true;
std::vector< ID3DXFont* > fonts = {};
void DrawTextC(const char* text, float x, float y, int size, D3DCOLOR color, LPDIRECT3DDEVICE9 pDevice) {
	RECT rect;
	size -= 1;
	if (size < 1) {
		size = 1;
	}
	if (size > 20) {
		size = 20;
	}

	if (initFonts) {
		for (int i = 0; i < 20; i++) {
			ID3DXFont* tmpFont;
			D3DXCreateFont(pDevice, i, 0, FW_NORMAL, 1, false, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas", &tmpFont);
			fonts.push_back(tmpFont);
		}
		initFonts = false;
	}

	SetRect(&rect, x, y, x, y);
	fonts[size]->DrawTextA(NULL, text, -1, &rect, DT_LEFT | DT_NOCLIP, color);
}

void drawRect(vec2 start, int width, int height, D3DCOLOR color, LPDIRECT3DDEVICE9 pDevice) {
	Vert rectVerts[5] = {
		{start.x, start.y, 0.0, 1.0, color},
		{start.x + width, start.y, 0.0, 1.0, color},
		{start.x + width, start.y + height, 0.0, 1.0, color},
		{start.x, start.y + height, 0.0, 1.0, color},
		{start.x, start.y, 0.0, 1.0, color},
	};
	pDevice->DrawPrimitiveUP(D3DPT_LINESTRIP, 5, rectVerts, sizeof(Vert));
}

void drawCursor(HWND tHWND, LPDIRECT3DDEVICE9 pDevice) {

	ShowCursor(FALSE);
	POINT cursorPos;

	RECT cRect;
	GetClientRect(tHWND, &cRect);

	GetCursorPos(&cursorPos);
	ScreenToClient(tHWND, &cursorPos);


	//float X = cursorPos.x - cRect.left;
	//float Y = cursorPos.y - cRect.top;

	float X = (float)cursorPos.x;
	float Y = (float)cursorPos.y;

	Vert cursorMain_W = { X, Y, 0.0f, 1.0f, white };
	Vert cursorBL_W = { X + 6, Y + 16, 0.0f, 1.0f, white };
	Vert cursorC_W = { X + 9, Y + 9, 0.0f, 1.0f, white };
	Vert cursorBR_W = { X + 16, Y + 6, 0.0f, 1.0f, white };

	Vert cursorMain_B = { X, Y, 0.0f, 1.0f, black };
	Vert cursorBL_B = { X + 6, Y + 16, 0.0f, 1.0f, black };
	Vert cursorC_B = { X + 9, Y + 9, 0.0f, 1.0f, black };
	Vert cursorBR_B = { X + 16, Y + 6, 0.0f, 1.0f, black };

	Vert pVertex[6] = {
		cursorMain_W,
		cursorBL_W,
		cursorC_W,
		cursorMain_W,
		cursorBR_W,
		cursorC_W,
	};

	Vert pVertexOutline[5] = {
		cursorMain_B,
		cursorBL_B,
		cursorC_B,
		cursorBR_B,
		cursorMain_B,
	};

	pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 6, pVertex, sizeof(Vert));
	pDevice->DrawPrimitiveUP(D3DPT_LINESTRIP, 5, pVertexOutline, sizeof(Vert));
}