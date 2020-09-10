
#include "DSoundTools.h"
#include <assert.h>
#include <cmath>

namespace DSoundTools
{
	static const unsigned int SO_PLAYBACK_FREQ = 44100;
	static const unsigned int SO_PRIMARY_BUFFER_SIZE = 9000 * 2 * 2;

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
	}

	void Render(SynthOX::Synth & Synth, float MasterVolume)
	{
		DWORD PlayCursor, WriteCursor;

		static const DWORD NbChunks = 4;
		static const DWORD ChunkSize = SO_PRIMARY_BUFFER_SIZE / NbChunks;
		assert(ChunkSize*NbChunks == SO_PRIMARY_BUFFER_SIZE);

		pDSB->GetCurrentPosition(&PlayCursor, &WriteCursor);

		int CurChunk = PlayCursor / ChunkSize;
		int OldChunk = OldPlayCursor / ChunkSize;
		if(CurChunk != OldChunk)
		{
			DWORD Cursor = ((CurChunk+1) % NbChunks) * ChunkSize;
			void *P[2];
			DWORD N[2];
			pDSB->Lock(Cursor, ChunkSize, &P[0], &N[0], &P[1], &N[1], 0);

			if(N[0] + N[1] > 0)
			{
				auto Output = [&](int BufIdx) 
				{
					assert(N[BufIdx] % 4 == 0);
					N[BufIdx] /= 2;
					Synth.Render(min(N[BufIdx]/2, 44100));
					short * Buf = static_cast<short *>(P[BufIdx]);
					for(unsigned int i = 0; i < N[BufIdx];)
					{
						static float X = 0.f;
						static float t = 0.f;
						float Left, Right;
						Synth.PopOutputVal(Left, Right);
						Buf[i++] = short(Left * 32767.f * MasterVolume);
						Buf[i++] = short(Right * 32767.f * MasterVolume);
					}
				};

				Output(0);
				Output(1);
			}
		
			pDSB->Unlock(P[0], N[0], P[1], N[1]);
		}

		OldPlayCursor = PlayCursor;
	}

	void Release()
	{
		g_DS->Release();
	}
};
