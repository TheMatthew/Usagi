/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Graphics/Effects/ConstantSet.h"
#include "Engine/Graphics/Device/Display.h"
#include "Engine/Graphics/Device/RenderState.h"
#include "Engine/Graphics/Device/GFXContext.h"
#include "Engine/Core/Containers/List.h"
#include "Engine/Graphics/Device/RenderStateMgr.h"
#include "Engine/Graphics/Device/IHeadMountedDisplay.h"
#include API_HEADER(Engine/Graphics/Device, GFXDevice_ps.h)
#include "GFXDevice.h"

namespace usg {

// Contains nothing but we want to limit where resources can be created
struct DeviceResource
{

};

struct GFXDevice::PIMPL
{
	RenderStateMgr	renderStates;
	GFXContext		immediateContext;
	GFXDevice_ps	platform;
};

GFXDevice::GFXDevice()
	: m_pHMD(nullptr) 
	, m_uDisplayCount(0)
	, m_uFrameCount(0)
{
	m_pImpl = vnew(ALLOC_OBJECT) PIMPL;
}

GFXDevice::~GFXDevice()
{
	if (m_pHMD)
	{
		m_pHMD->Cleanup(this);
	}
	for (uint32 i = 0; i < m_uDisplayCount; i++)
	{
		m_pDisplays[i]->CleanUp(this);
	}
	vdelete m_pImpl;
	m_pImpl = nullptr;
}

void GFXDevice::Init()
{
	m_pImpl->platform.Init(this);
	m_pImpl->immediateContext.Init(this, false);
	m_pImpl->renderStates.InitDefaults(this);
	m_pImpl->platform.PostInit();
}

uint32 GFXDevice::GetHardwareDisplayCount()
{
	return m_pImpl->platform.GetHardwareDisplayCount();
}

void GFXDevice::InitAllHardwareDisplays()
{
	if (m_uDisplayCount >= MAX_DISPLAY_COUNT)
	{
		return;
	}

	DeviceResource res;
	for (uint32 i = 0; i < GetHardwareDisplayCount(); i++)
	{
		const DisplaySettings* const pSettings = GetDisplayInfo(i);
		m_pDisplays[m_uDisplayCount] = vnew(ALLOC_OBJECT) Display;
		m_pDisplays[m_uDisplayCount]->Initialise(this, pSettings->hardwareHndl, res);
		m_uDisplayCount++;

		if (m_uDisplayCount >= MAX_DISPLAY_COUNT)
		{
			return;
		}
	}
}

void GFXDevice::InitDisplay(WindHndl hndl)
{
	if (m_uDisplayCount >= MAX_DISPLAY_COUNT)
		return;

	DeviceResource res;
	m_pDisplays[m_uDisplayCount] = vnew(ALLOC_OBJECT) Display;
	m_pDisplays[m_uDisplayCount]->Initialise(this, hndl, res);

	m_uDisplayCount++;
}

void GFXDevice::SetHeadMountedDisplay(IHeadMountedDisplay* pHMD)
{
	m_pHMD = pHMD;
}


IHeadMountedDisplay* GFXDevice::GetHMD()
{
	return m_pHMD;
}

const DisplaySettings* GFXDevice::GetDisplayInfo(uint32 uIndex)
{
	ASSERT(uIndex < GetHardwareDisplayCount());
	return m_pImpl->platform.GetDisplayInfo(uIndex);
}

PipelineStateHndl GFXDevice::GetPipelineState(const RenderPassHndl& hndl, const PipelineStateDecl& decl)
{
	return m_pImpl->renderStates.GetPipelineState(decl, this);
}

RenderPassHndl GFXDevice::GetRenderPass(const RenderPassDecl& decl)
{
	return m_pImpl->renderStates.GetRenderPass(decl, this);
}

DescriptorSetLayoutHndl GFXDevice::GetDescriptorSetLayout(const DescriptorDeclaration* pDecl)
{
	return m_pImpl->renderStates.GetDescriptorSetLayout(pDecl, this);
}

SamplerHndl GFXDevice::GetSampler(const SamplerDecl& decl)
{
	return m_pImpl->renderStates.GetSamplerState(decl, this);
}

bool GFXDevice::Is3DEnabled() const
{
	return m_pImpl->platform.Is3DEnabled();
}

void GFXDevice::GetPipelineDeclaration(const PipelineStateHndl pipeline, PipelineStateDecl& out)
{
	m_pImpl->renderStates.GetPipelineStateDeclaration(pipeline, out);
}


void GFXDevice::PostUpdate()
{

}

GFXContext* GFXDevice::CreateDeferredContext(uint32 uSizeMul)
{
	return m_pImpl->platform.CreateDeferredContext(uSizeMul);
}

void GFXDevice::Begin()
{
	m_pImpl->platform.Begin();
}


void GFXDevice::End()
{
	m_pImpl->platform.End();
	if (m_pHMD)
	{
		m_pHMD->Update();
	}
	++m_uFrameCount;
}

void GFXDevice::FinishedStaticLoad()
{
	m_pImpl->renderStates.FinishedStaticLoad();
	m_pImpl->platform.FinishedStaticLoad();
}

void GFXDevice::ClearDynamicResources()
{
	m_pImpl->renderStates.ClearDynamicResources();
	m_pImpl->platform.ClearDynamicResources();
}

float GFXDevice::GetGPUTime() const
{
	return m_pImpl->platform.GetGPUTime();
}

GFXDevice_ps& GFXDevice::GetPlatform()
{
	return m_pImpl->platform;
}

GFXContext*	GFXDevice::GetImmediateCtxt()
{ 
	return &m_pImpl->immediateContext;
}


}
