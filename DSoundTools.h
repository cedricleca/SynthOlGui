#pragma once

#include <dsound.h>

namespace DSoundTools
{
	void Init(HWND  hWnd);
	void Render(float Volume);
	void Release();
};
