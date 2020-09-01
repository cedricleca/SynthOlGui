
#include "DSoundTools.h"

namespace DSoundTools
{
	static const unsigned int SO_PLAYBACK_FREQ = 44100;
	static const unsigned int SO_PRIMARY_BUFFER_SIZE = 4 * SO_PLAYBACK_FREQ;

	IDirectSound*			g_DS = nullptr;
	LPDIRECTSOUNDBUFFER		pDSB = nullptr;
	DWORD					OldPlayCursor;
	char *					JKevOutBuf = nullptr;

	void Init(HWND  hWnd)
	{
		// Init DSound_____________
		DirectSoundCreate( nullptr, &g_DS, nullptr );
		g_DS->SetCooperativeLevel( hWnd, DSSCL_PRIORITY );

		LPDIRECTSOUNDBUFFER pDSBPrimary = nullptr;
		DSBUFFERDESC dsbd;
		ZeroMemory( &dsbd, sizeof(DSBUFFERDESC) );
		dsbd.dwSize        = sizeof(DSBUFFERDESC);
		dsbd.dwFlags       = DSBCAPS_PRIMARYBUFFER;
		dsbd.dwBufferBytes = 0;
		dsbd.lpwfxFormat   = nullptr;

		g_DS->CreateSoundBuffer( &dsbd, &pDSBPrimary, nullptr ); 

		WAVEFORMATEX wfx;
		ZeroMemory( &wfx, sizeof(WAVEFORMATEX) );
		wfx.wFormatTag      = (WORD) WAVE_FORMAT_PCM;
		wfx.nChannels       = (WORD) 2;
		wfx.nSamplesPerSec  = (DWORD) SO_PLAYBACK_FREQ;
		wfx.wBitsPerSample  = (WORD) 16;
		wfx.nBlockAlign     = (WORD) (wfx.wBitsPerSample / 8 * wfx.nChannels);
		wfx.nAvgBytesPerSec = (DWORD) (wfx.nSamplesPerSec * wfx.nBlockAlign);

		pDSBPrimary->SetFormat(&wfx);

		ZeroMemory( &dsbd, sizeof(DSBUFFERDESC) );
		dsbd.dwSize          = sizeof(DSBUFFERDESC);
		dsbd.dwFlags         = DSBCAPS_GETCURRENTPOSITION2;
		dsbd.dwBufferBytes   = SO_PRIMARY_BUFFER_SIZE;
		dsbd.lpwfxFormat     = &wfx;

		// Init Secondary Buffer_____________    
		g_DS->CreateSoundBuffer( &dsbd, &pDSB, nullptr );

		void *P1, *P2;
		DWORD N1, N2;
		pDSB->Lock(0, SO_PRIMARY_BUFFER_SIZE, &P1, &N1, &P2, &N2, 0);
		N1 /= 2;
		memset(P1, 0, N1);
		pDSB->Unlock(P1, N1, P2, N2);
	
		pDSB->Play( 0, 0, DSBPLAY_LOOPING );	
		OldPlayCursor = 0;

//		Machine.JKev.SetOutputSurface(JKevOutBuf, SO_PRIMARY_BUFFER_SIZE);
	}

	void Render(float Volume)
	{
		DWORD PlayCursor, WriteCursor;
		DWORD BytesToLock;

		pDSB->GetCurrentPosition(&PlayCursor, &WriteCursor);

		if(PlayCursor > OldPlayCursor)
			BytesToLock = PlayCursor - OldPlayCursor;
		else
			BytesToLock = SO_PRIMARY_BUFFER_SIZE - (OldPlayCursor - PlayCursor);

		void *P[2];
		DWORD N[2];
		pDSB->Lock(OldPlayCursor, BytesToLock, &P[0], &N[0], &P[1], &N[1], 0);
		OldPlayCursor = PlayCursor;

		N[0] /= 2;
		N[1] /= 2;

/*
		for(int a = 0; a < 2; a++)
		{
			short * Buf = (short *)P[a];
			char Val;
			for(DWORD i = 0; i < N[a] && Machine.JKev.Pop(Val); i++)
				Buf[i] = short(float(Val<<8) * Volume);
		}
*/

		pDSB->Unlock(P[0], N[0], P[1], N[1]);
	}

	void Release()
	{
		g_DS->Release();
	}
};
