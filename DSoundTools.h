#pragma once

#include <dsound.h>
#include "SynthOl/SynthOl.h"

namespace DSoundTools
{
	void Init(HWND  hWnd);
	void Render(SynthOl::Synth & Synth, float Volume);
	void Release();
};
