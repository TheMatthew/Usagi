/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Audio/Audio.h"
#include "SoundObject_ps.h"

namespace usg{



SoundObject_ps::SoundObject_ps()
{
	m_pSourceVoice = NULL;
	m_uChannels = 0;
	m_bValid = false;
	m_bPaused = false;
}

SoundObject_ps::~SoundObject_ps()
{
	Reset();
}

void SoundObject_ps::Init(Audio* pAudio)
{

}

void SoundObject_ps::Reset()
{
	if(m_pSourceVoice)
	{
		m_pSourceVoice->DestroyVoice();
		m_pSourceVoice = NULL;
		m_bValid = false;
		m_bPaused = false;
	}
}

void SoundObject_ps::BindWaveFile(WaveFile &waveFile, uint32 uPriority)
{
	if( FAILED( Audio::Inst()->GetPlatform().GetEngine()->CreateSourceVoice( &m_pSourceVoice, &waveFile.GetFormat() ) ) )
	{
		ASSERT(false);
		return;
	}

	ZeroMemory(&m_buffer, sizeof(m_buffer));
	m_buffer.pAudioData = waveFile.GetData();
	m_buffer.Flags = XAUDIO2_END_OF_STREAM;  // tell the source voice not to expect any data after this buffer
	m_buffer.AudioBytes = waveFile.GetSize();
	m_uChannels = waveFile.GetFormat().nChannels;

	if(m_bLooping)
	{
		m_buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
		m_buffer.LoopBegin = waveFile.GetLoopStart();
		m_buffer.LoopLength = waveFile.GetLoopLength();
	}

	if (FAILED(m_pSourceVoice->SubmitSourceBuffer(&m_buffer)))
	{
		m_pSourceVoice->DestroyVoice();
		m_pSourceVoice = NULL;
		ASSERT(false);
		return;
	}

	m_bValid = true;
}

void SoundObject_ps::SetSoundFile(WaveFile* pWaveFile, bool bPositional, bool bLoop)
{
	m_pSoundFile = pWaveFile;
	m_bPositional = bPositional;
	m_bLooping = bLoop;
}

void SoundObject_ps::Start()
{
	if (m_bValid)
	{
		m_pSourceVoice->Start();
		m_bPaused = false;
	}
}

void SoundObject_ps::Stop()
{
	if (m_bValid)
	{
		m_pSourceVoice->Stop();
		m_pSourceVoice->FlushSourceBuffers();
		m_bPaused = false;
		m_bValid = false;
		// FIXME: Reset the internal data
	}
}

void SoundObject_ps::Pause()
{
	if (m_bValid)
	{
		m_pSourceVoice->Stop();
		m_bPaused = true;
	}
}

void SoundObject_ps::Update(const SoundObject* pParent)
{
	if(pParent->GetRequestedPlayState() == PLAY_STATE_PLAYING && !m_bValid && m_pSoundFile != NULL)
	{
		m_pSoundFile->BindToSound(this, pParent->GetPriority());
	}

	if(!m_bValid)
		return;

	static float32 matrixCoefficients[2 * 8];
	ZeroMemory(matrixCoefficients, sizeof(float)*2*8);
	// Merging left and right channels of the sound
	const int destChannels = 2;
	uint32 uIndex = 0;
	for (uint32 i = 0; i < destChannels; i++)
	{
		for (uint32 j = 0; j < m_uChannels; j++)
		{
			matrixCoefficients[uIndex] = pParent->GetPanningData().fMatrix[SOUND_CHANNEL_LEFT + i];
			uIndex++;
		}
	}
	
	float fVolume = pParent->GetAdjVolume() * pParent->GetAttenMul();
	//ASSERT(fVolume >= 0.0f && fVolume <= 1.0f);
	m_pSourceVoice->SetVolume(pParent->GetAdjVolume() * pParent->GetAttenMul());
	m_pSourceVoice->SetFrequencyRatio(pParent->GetPitch() * pParent->GetDopplerFactor());
	m_pSourceVoice->SetOutputMatrix(NULL, m_uChannels, destChannels, matrixCoefficients);
	// TODO: Would need interal handling
	//m_pSourceVoice->SetPriority( pParent->GetPriority() );

	switch (pParent->GetRequestedPlayState())
	{
	case PLAY_STATE_PLAYING:
		Start();
		break;
	case PLAY_STATE_STOPPED:
		Stop();
		break;
	case PLAY_STATE_PAUSED:
		Pause();
		break;
	case PLAY_STATE_NONE:
	default:
		break;
	}
}


bool SoundObject_ps::IsPlaying() const
{
	if(!m_bValid)
		return false;

	XAUDIO2_VOICE_STATE voiceState;
	m_pSourceVoice->GetState( &voiceState );
	return voiceState.BuffersQueued > 0;
}


}
