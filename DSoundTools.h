#pragma once

#include <dsound.h>
#include "SynthOX/SynthOX.h"

namespace DSoundTools
{
	void Init(HWND  hWnd);
	void Render(SynthOX::Synth & Synth, float Volume);
	void Release();
};
