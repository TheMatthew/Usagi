/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Resource/ResourceMgr.h"
#include "Engine/Maths/MathUtil.h"
#include "Bloom.h"
#include "FXAA.h"
#include "SMAA.h"
#include "LinearDepth.h"
#include "DeferredShading.h"
#include "SkyFog.h"
#include "Engine/PostFX/PostFXSys.h"
#include "Engine/Graphics/StandardVertDecl.h"
#include "Engine/Graphics/Device/GFXDevice.h"
#include "Engine/Graphics/Device/GFXContext.h"
#include API_HEADER(Engine/Graphics/Device, GFXDevice_ps.h)
#include "Engine/Core/stl/vector.h"

namespace usg {

#define GAUSS_SAMPLE_COUNT			13
#define DOWNSCALE44_SAMPLE_COUNT	4

struct GaussConstants
{
	Vector4f	vOffsets[GAUSS_SAMPLE_COUNT];
};

struct Downscale44Constants
{
	Vector4f	vOffsets[DOWNSCALE44_SAMPLE_COUNT];
};

static const ShaderConstantDecl g_gaussConstantDef[] = 
{
	SHADER_CONSTANT_ELEMENT( GaussConstants, vOffsets,	CT_VECTOR_4,	GAUSS_SAMPLE_COUNT ),
	SHADER_CONSTANT_END()
};

static const ShaderConstantDecl g_downscale44ConstantDef[] = 
{
	SHADER_CONSTANT_ELEMENT( Downscale44Constants, vOffsets,	CT_VECTOR_4,	DOWNSCALE44_SAMPLE_COUNT ),
	SHADER_CONSTANT_END()
};

static const DescriptorDeclaration g_multiSampleDescriptor[] =
{
	DESCRIPTOR_ELEMENT(0,						 DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, SHADER_FLAG_PIXEL),
	DESCRIPTOR_ELEMENT(SHADER_CONSTANT_MATERIAL, DESCRIPTOR_TYPE_CONSTANT_BUFFER, 1, SHADER_FLAG_ALL),
	DESCRIPTOR_END()
};

static const DescriptorDeclaration g_singleSampleDescriptor[] =
{
	DESCRIPTOR_ELEMENT(0,						 DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, SHADER_FLAG_PIXEL),
	DESCRIPTOR_END()
};

PostFXSys_ps::PostFXSys_ps()
	: m_pDefaultEffects{nullptr}
{
	m_uDefaultEffects = 0;
	m_pFXAA = nullptr;
	m_pSMAA = nullptr;
	m_pBloom = nullptr;
	m_pSkyFog = nullptr;
	m_pDeferredShading = nullptr;
	m_fPixelScale = 1.0f;
}

PostFXSys_ps::~PostFXSys_ps()
{
	for (uint32 i = 0; i < m_uDefaultEffects; i++)
	{
		vdelete m_pDefaultEffects[i];
	}
}

void PostFXSys_ps::Init(PostFXSys* pParent, GFXDevice* pDevice, uint32 uInitFlags, uint32 uWidth, uint32 uHeight)
{
	m_pParent = pParent;

	bool bDeferred = (uInitFlags&PostFXSys::EFFECT_DEFERRED_SHADING)!=0;
	SampleCount eSamples = SAMPLE_COUNT_1_BIT;
	uint32 uDepthFlags = 0;

	m_colorBuffer[BUFFER_HDR].Init(pDevice, uWidth, uHeight, CF_RGB_HDR, eSamples, ColorBuffer::FLAG_FAST_MEM, 0);
	m_colorBuffer[BUFFER_LIN_DEPTH].Init(pDevice, uWidth, uHeight,  CF_R_16F, eSamples, ColorBuffer::FLAG_FAST_MEM, 1);

	// We don't have enough memory for 1080p if we put this in fast memory too
	m_colorBuffer[BUFFER_LDR_0].Init(pDevice, uWidth, uHeight, CF_RGBA_8888, SAMPLE_COUNT_1_BIT, ColorBuffer::FLAG_FAST_MEM);
	m_colorBuffer[BUFFER_LDR_1].Init(pDevice, uWidth, uHeight, CF_RGBA_8888, SAMPLE_COUNT_1_BIT, ColorBuffer::FLAG_FAST_MEM);

	m_depthStencil.Init(pDevice, uWidth, uHeight, DF_DEPTH_24_S8, eSamples, uDepthFlags | DepthStencilBuffer::FLAG_FAST_MEM_LOCATION | DepthStencilBuffer::FLAG_USE_HI_Z);


	m_screenRT[TARGET_HDR].Init(pDevice, &m_colorBuffer[BUFFER_HDR], &m_depthStencil);

	if(bDeferred)
	{
		m_colorBuffer[BUFFER_DIFFUSE].Init(pDevice, uWidth, uHeight, CF_RGBA_8888, SAMPLE_COUNT_1_BIT, ColorBuffer::FLAG_FAST_MEM, 0); // 4th component is specular power
		m_colorBuffer[BUFFER_NORMAL].Init(pDevice, uWidth, uHeight, CF_NORMAL, SAMPLE_COUNT_1_BIT, ColorBuffer::FLAG_FAST_MEM, 2);
		m_colorBuffer[BUFFER_EMISSIVE].Init(pDevice, uWidth, uHeight, CF_RGBA_5551, SAMPLE_COUNT_1_BIT, ColorBuffer::FLAG_FAST_MEM, 3);
		m_colorBuffer[BUFFER_SPECULAR].Init(pDevice, uWidth, uHeight, CF_RGBA_5551, SAMPLE_COUNT_1_BIT, ColorBuffer::FLAG_FAST_MEM, 4);	
		ColorBuffer* pBuffers[] = { &m_colorBuffer[BUFFER_DIFFUSE], &m_colorBuffer[BUFFER_LIN_DEPTH], &m_colorBuffer[BUFFER_NORMAL], &m_colorBuffer[BUFFER_EMISSIVE], &m_colorBuffer[BUFFER_SPECULAR] };
		m_screenRT[TARGET_GBUFFER].InitMRT(pDevice, 5, pBuffers, &m_depthStencil);
	}
	
	ColorBuffer* pBuffers[] = { &m_colorBuffer[BUFFER_HDR], &m_colorBuffer[BUFFER_LIN_DEPTH] };
	m_screenRT[TARGET_HDR_LIN_DEPTH].InitMRT(pDevice, 2, pBuffers, &m_depthStencil);
	m_screenRT[TARGET_HDR_LIN_DEPTH].InitRenderPass(pDevice, 0, RenderTarget::RT_FLAG_COLOR_1, RenderTarget::RT_FLAG_COLOR_0 | RenderTarget::RT_FLAG_COLOR_1);
	m_screenRT[TARGET_LDR_0].Init(pDevice, &m_colorBuffer[BUFFER_LDR_0], &m_depthStencil);
	m_screenRT[TARGET_LDR_0].InitRenderPass(pDevice, RenderTarget::RT_FLAG_COLOR_0|RenderTarget::RT_FLAG_DEPTH, 0, RenderTarget::RT_FLAG_COLOR_0);
	m_screenRT[TARGET_LDR_1].Init(pDevice, &m_colorBuffer[BUFFER_LDR_1], &m_depthStencil);
	m_screenRT[TARGET_LDR_1].InitRenderPass(pDevice, RenderTarget::RT_FLAG_COLOR_0|RenderTarget::RT_FLAG_DEPTH, 0, RenderTarget::RT_FLAG_COLOR_0);
	pBuffers[0] = &m_colorBuffer[BUFFER_LDR_0];
	m_screenRT[TARGET_LDR_LIN_DEPTH].InitMRT(pDevice, 2, pBuffers, &m_depthStencil);
	// FIXME: When all hooked up properly we don't need to clear RT 0, just doing during so during the testing phase
	m_screenRT[TARGET_LDR_LIN_DEPTH].InitRenderPass(pDevice, 0, RenderTarget::RT_FLAG_COLOR_0|RenderTarget::RT_FLAG_COLOR_1, RenderTarget::RT_FLAG_COLOR_0|RenderTarget::RT_FLAG_COLOR_1);

	
	m_uLDRCount=2;

	Color clearCol(1.0f, 0.0f, 0.0f, 0.0f);
	m_screenRT[TARGET_HDR_LIN_DEPTH].SetClearColor(clearCol, 1);
	m_screenRT[TARGET_GBUFFER].SetClearColor(clearCol, 1);

	clearCol.Assign(0.0f, 0.0f, 0.0f, 0.0f);
	m_screenRT[TARGET_LDR_0].SetClearColor(clearCol, 0);
	m_screenRT[TARGET_LDR_1].SetClearColor(clearCol, 0);

	SamplerDecl pointDecl(SF_POINT, SC_CLAMP);
	SamplerDecl linearDecl(SF_LINEAR, SC_CLAMP);

	ResourceMgr* pResMgr = ResourceMgr::Inst();

#if 0
	PipelineStateDecl pipelineDecl;
	pipelineDecl.inputBindings[0].Init(GetVertexDeclaration(VT_POSITION));
	pipelineDecl.uInputBindingCount = 1;

	DescriptorSetLayoutHndl multiDesc = pDevice->GetDescriptorSetLayout(g_multiSampleDescriptor);
	DescriptorSetLayoutHndl singleDesc = pDevice->GetDescriptorSetLayout(g_singleSampleDescriptor);
	pipelineDecl.layout.descriptorSets[0] = multiDesc;
	pipelineDecl.layout.uDescriptorSetCount = 1;

	pipelineDecl.pEffect = pResMgr->GetEffect(pDevice, "PostDownscale2x2");
	m_downscale4x4Effect = pDevice->GetPipelineState(pipelineDecl);
	pipelineDecl.pEffect = pResMgr->GetEffect(pDevice, "PostGauss5x5");
	m_gaussBlur5x5Effect = pDevice->GetPipelineState(pipelineDecl);

	pipelineDecl.layout.descriptorSets[0] = singleDesc;

	pipelineDecl.pEffect = pResMgr->GetEffect(pDevice, "PostCopyScreen");
	m_downscale2x2Effect = pDevice->GetPipelineState(pipelineDecl);
#endif

	m_pointSampler	= pDevice->GetSampler(pointDecl);
	m_linearSampler = pDevice->GetSampler(linearDecl);


	// Register the default effects
	if(uInitFlags & PostFXSys::EFFECT_FXAA)
	{
		ASSERT((uInitFlags & PostFXSys::EFFECT_SMAA) == 0);
		m_pFXAA = vnew(ALLOC_OBJECT) FXAA();
		m_pFXAA->Init(pDevice, pParent, &m_screenRT[TARGET_LDR_1]);
		m_pDefaultEffects[m_uDefaultEffects++] = m_pFXAA;
	}
	if (uInitFlags & PostFXSys::EFFECT_SMAA)
	{
		ASSERT((uInitFlags & PostFXSys::EFFECT_FXAA) == 0);
		m_pSMAA = vnew(ALLOC_OBJECT) SMAA();
		m_pSMAA->Init(pDevice, pParent, &m_screenRT[TARGET_LDR_1]);
		m_pDefaultEffects[m_uDefaultEffects++] = m_pSMAA;
	}
	if(uInitFlags & PostFXSys::EFFECT_BLOOM)
	{
		m_pBloom = vnew(ALLOC_OBJECT) Bloom();
		m_pBloom->Init(pDevice, pParent, &m_screenRT[TARGET_LDR_0]);
		m_pDefaultEffects[m_uDefaultEffects++] = m_pBloom;
	}
	if (uInitFlags & PostFXSys::EFFECT_SKY_FOG)
	{
		m_pSkyFog = vnew(ALLOC_OBJECT) SkyFog();
		m_pSkyFog->Init(pDevice, pParent, &m_screenRT[TARGET_HDR]);
		m_pDefaultEffects[m_uDefaultEffects++] = m_pSkyFog;
	}
	if(uInitFlags & PostFXSys::EFFECT_DEFERRED_SHADING)
	{
		m_pDeferredShading = vnew(ALLOC_OBJECT) DeferredShading();
		m_pDeferredShading->Init(pDevice, pParent, &m_screenRT[TARGET_LDR_0]);
		m_pDefaultEffects[m_uDefaultEffects++] = m_pDeferredShading;
	}

	EnableEffects(pDevice, uInitFlags);

	
	
	for(uint32 i=0; i<m_uDefaultEffects; i++)
	{
		m_pParent->RegisterEffect(m_pDefaultEffects[i]);
	}	
}

void PostFXSys_ps::CleanUp(GFXDevice* pDevice)
{
	for (auto & colorBuffer : m_colorBuffer)
	{
		if (colorBuffer.IsValid())
		{
			colorBuffer.CleanUp(pDevice);
		}
	}

	m_depthStencil.CleanUp(pDevice);

	for (auto & screenRT : m_screenRT)
	{
		if (screenRT.IsValid())
		{
			screenRT.CleanUp(pDevice);
		}
	}

	for (auto* const pPostEffect : m_pDefaultEffects)
	{
		if (pPostEffect != nullptr)
		{
			pPostEffect->CleanUp(pDevice);
		}
	}
}

void PostFXSys_ps::EnableEffects(GFXDevice* pDevice, uint32 uEffectFlags)
{
	RenderTarget* pDst = &m_screenRT[TARGET_LDR_LIN_DEPTH];

	if (uEffectFlags & PostFXSys::EFFECT_DEFERRED_SHADING)
	{
		pDst = &m_screenRT[TARGET_GBUFFER];
	}
	else if (uEffectFlags & PostFXSys::EFFECT_SKY_FOG)
	{
		pDst = uEffectFlags & PostFXSys::EFFECT_BLOOM ? &m_screenRT[TARGET_HDR_LIN_DEPTH] : &m_screenRT[TARGET_LDR_LIN_DEPTH];
	}

	m_pInitialTarget = pDst;

	if (uEffectFlags & PostFXSys::EFFECT_DEFERRED_SHADING)
	{
		m_pDeferredShading->SetSourceTarget(pDevice, pDst);
		m_pDeferredShading->SetEnabled(true);
		pDst = uEffectFlags & PostFXSys::EFFECT_BLOOM ? &m_screenRT[TARGET_HDR] : &m_screenRT[TARGET_LDR_0];
		m_pDeferredShading->SetDestTarget(pDevice, pDst);
		m_pFinalEffect = m_pDeferredShading;
	}
	else if(m_pDeferredShading)
	{
		m_pDeferredShading->SetEnabled(false);
	}

	if (uEffectFlags & PostFXSys::EFFECT_SKY_FOG)
	{
		pDst = uEffectFlags & PostFXSys::EFFECT_BLOOM ?  &m_screenRT[TARGET_HDR] : &m_screenRT[TARGET_LDR_0];
		m_pSkyFog->SetDestTarget(pDevice, pDst);
		m_pFinalEffect = m_pSkyFog;
	}

	// If not using deferred shading the destination target will be overridden when doing the transparency pass
	// (to allow us to use linear depth for the sky, particles etc)
	if (pDst == &m_screenRT[TARGET_HDR_LIN_DEPTH])
	{
		pDst = &m_screenRT[TARGET_HDR];
	}
	if (pDst == &m_screenRT[TARGET_LDR_LIN_DEPTH])
	{
		pDst = &m_screenRT[TARGET_LDR_0];
	}

	if (uEffectFlags & PostFXSys::EFFECT_BLOOM)
	{
		m_pBloom->SetSourceTarget(pDevice, pDst);
		pDst = &m_screenRT[TARGET_LDR_0];	// Bloom goes out to LDR_0
		m_pBloom->SetEnabled(true);	
		m_pFinalEffect = m_pBloom;
	}
	else if (m_pBloom)
	{
		m_pBloom->SetEnabled(false);
	}

	if (uEffectFlags & PostFXSys::EFFECT_FXAA)
	{
		m_pFXAA->SetSourceTarget(pDevice, pDst);
		pDst = pDst == &m_screenRT[TARGET_LDR_1] ? &m_screenRT[TARGET_LDR_0] : &m_screenRT[TARGET_LDR_1];
		m_pFXAA->SetEnabled(true);
		m_pFinalEffect = m_pFXAA;
	}
	else if(m_pFXAA)
	{
		m_pFXAA->SetEnabled(false);
	}

	if (uEffectFlags & PostFXSys::EFFECT_SMAA)
	{
		m_pSMAA->SetSourceTarget(pDevice, pDst);
		pDst = pDst == &m_screenRT[TARGET_LDR_1] ? &m_screenRT[TARGET_LDR_0] : &m_screenRT[TARGET_LDR_1];
		m_pSMAA->SetEnabled(true);
		m_pFinalEffect = m_pSMAA;
	}
	else if (m_pSMAA)
	{
		m_pSMAA->SetEnabled(false);
	}


	m_pFinalTarget = pDst;

}

void PostFXSys_ps::ResizeTargetsInt(GFXDevice* pDevice, uint32 uWidth, uint32 uHeight)
{
	uint32 uScaledWidth = (uint32)((m_fPixelScale * uWidth)+0.5f);
	uint32 uScaledHeight = (uint32)((m_fPixelScale * uHeight) + 0.5f);

	for (auto & colorBuffer : m_colorBuffer)
	{
		if (colorBuffer.IsValid())
		{
			colorBuffer.Resize(pDevice, uScaledWidth, uScaledHeight);
		}
	}

	m_depthStencil.Resize(pDevice, uScaledWidth, uScaledHeight);

	for (auto & screenRT : m_screenRT)
	{
		if (screenRT.IsValid())
		{
			screenRT.Resize(pDevice, uScaledWidth, uScaledHeight);
		}
	}

	for (auto* const pPostEffect : m_pDefaultEffects)
	{
		if (pPostEffect != nullptr)
		{
			pPostEffect->Resize(pDevice, uScaledWidth, uScaledHeight);
		}
	}

	/*
	if (m_pFinalEffect)
	{
		m_pFinalEffect->SetDestTarget(pDevice)
	}*/
}

void PostFXSys_ps::Resize(GFXDevice* pDevice, uint32 uWidth, uint32 uHeight)
{
	ResizeTargetsInt(pDevice, uWidth, uHeight);
}

const TextureHndl& PostFXSys_ps::GetLinearDepthTex() const
{
	return m_colorBuffer[BUFFER_LIN_DEPTH].GetTexture();
}


void PostFXSys_ps::UpdateRTSize(GFXDevice* pDevice, Display* pDisplay)
{
	// We haven't optimised resizing on the PC yet
#ifdef PLATFORM_SWITCH
	float fFrameTime = pDevice->GetPlatform().GetGPUTime();
	uint32 uWidth, uHeight;
	pDisplay->GetActualDimensions(uWidth, uHeight, false);
	if (m_fPixelScale < 1.0f)
	{
		if (fFrameTime < 12.0f)
		{
			m_fPixelScale = 1.0f;
			ResizeTargetsInt(pDevice, uWidth, uHeight);
		}
	}
	else
	{
		if (fFrameTime > 15.5f)
		{
			m_fPixelScale = 1.f / 1.2f;
			ResizeTargetsInt(pDevice, uWidth, uHeight);
		}
	}
#endif
}


void PostFXSys_ps::SetSkyTexture(GFXDevice* pDevice, const TextureHndl& tex)
{
	if (m_pSkyFog)
	{
		m_pSkyFog->SetTexture(pDevice, tex, m_colorBuffer[BUFFER_LIN_DEPTH].GetTexture());
	}
}


void PostFXSys_ps::DepthWriteEnded(GFXContext* pContext, uint32 uActiveEffects)
{
	if (uActiveEffects & PostFXSys::EFFECT_DEFERRED_SHADING)
	{
		// Nothing to do, depth write ended
		return;
	}
	if (uActiveEffects & PostFXSys::EFFECT_BLOOM)
	{
		pContext->SetRenderTarget(&m_screenRT[TARGET_HDR]);
	}
	else
	{
		pContext->SetRenderTarget(&m_screenRT[TARGET_LDR_0]);
	}
}


PipelineStateHndl PostFXSys_ps::GetDownscale4x4Pipeline(GFXDevice* pDevice, const RenderPassHndl& renderPass) const
{
	ResourceMgr* pResMgr = ResourceMgr::Inst();

	PipelineStateDecl pipelineDecl;
	pipelineDecl.inputBindings[0].Init(GetVertexDeclaration(VT_POSITION));
	pipelineDecl.uInputBindingCount = 1;

	pipelineDecl.renderPass = renderPass;

	DescriptorSetLayoutHndl multiDesc = pDevice->GetDescriptorSetLayout(g_multiSampleDescriptor);
	pipelineDecl.layout.descriptorSets[0] = multiDesc;
	pipelineDecl.layout.uDescriptorSetCount = 1;

	pipelineDecl.pEffect = pResMgr->GetEffect(pDevice, "PostDownscale2x2");
	return pDevice->GetPipelineState(pipelineDecl);
}

PipelineStateHndl PostFXSys_ps::GetGaussBlurPipeline(GFXDevice* pDevice, const RenderPassHndl& renderPass) const
{
	ResourceMgr* pResMgr = ResourceMgr::Inst();

	PipelineStateDecl pipelineDecl;

	pipelineDecl.renderPass = renderPass;

	pipelineDecl.inputBindings[0].Init(GetVertexDeclaration(VT_POSITION));
	pipelineDecl.uInputBindingCount = 1;

	DescriptorSetLayoutHndl multiDesc = pDevice->GetDescriptorSetLayout(g_multiSampleDescriptor);
	pipelineDecl.layout.descriptorSets[0] = multiDesc;
	pipelineDecl.layout.uDescriptorSetCount = 1;

	// FIXME: Cache rather than grabbing the resource mgr
	pipelineDecl.pEffect = pResMgr->GetEffect(pDevice, "PostGauss5x5");
	return pDevice->GetPipelineState(pipelineDecl);

}

void PostFXSys_ps::SetupDownscale4x4(GFXDevice* pDevice, ConstantSet& cb, DescriptorSet& des, uint32 uWidth, uint32 uHeight) const
{
	SetupOffsets4x4(pDevice, cb, uWidth, uHeight);

	DescriptorSetLayoutHndl multiDesc = pDevice->GetDescriptorSetLayout(g_multiSampleDescriptor);

	des.Init(pDevice, multiDesc);
	des.SetConstantSet(1, &cb);

}

void PostFXSys_ps::SetupGaussBlur(GFXDevice* pDevice, ConstantSet& cb, DescriptorSet& des, uint32 uWidth, uint32 uHeight, float fMultiplier) const
{
	SetupGaussBlurBuffer(pDevice, cb, uWidth, uHeight, fMultiplier);
	DescriptorSetLayoutHndl multiDesc = pDevice->GetDescriptorSetLayout(g_multiSampleDescriptor);

	des.Init(pDevice, multiDesc);
	des.SetConstantSet(1, &cb);
}

float PostFXSys_ps::GaussianDistribution( float x, float y, float rho ) const
{
	float g = 1.0f / sqrtf( 2.0f * Math::pi * rho * rho );
    g *= expf( -( x * x + y * y ) / ( 2 * rho * rho ) );

    return g;
}



void PostFXSys_ps::SetupOffsets4x4(GFXDevice* pDevice, ConstantSet& cb, uint32 uWidth, uint32 uHeight ) const
{
	if(!cb.IsValid())
	{
		cb.Init(pDevice, g_downscale44ConstantDef);
	}

	// Divide by two as we are handling half the samples by using the linear filter
	uWidth	/=2;
	uHeight /=2;

    float tU = 1.0f / (((float)uWidth)/2.f);
    float tV = 1.0f / (((float)uHeight)/2.f);

	Downscale44Constants* pConstants = cb.Lock<Downscale44Constants>();
	Vector4f* pOffsets = pConstants->vOffsets;

    // Sample from the 16 surrounding points.
    int index = 0;
    for( int y = 0; y < 2; y++ )
    {
        for( int x = 0; x < 2; x++ )
        {
            pOffsets[ index ].x	= ( x - 0.5f ) * tU;
            pOffsets[ index ].y = ( y - 0.5f ) * tV;

            index++;
        }
    }

	cb.Unlock();
	cb.UpdateData(pDevice);
}


void PostFXSys_ps::SetupGaussBlurBuffer(GFXDevice* pDevice, ConstantSet& cb, uint32 uWidth, uint32 uHeight, float fMultiplier) const
{
	if(!cb.IsValid())
	{
		cb.Init(pDevice, g_gaussConstantDef);
	}

    float tu = 1.0f / ( float )uWidth;
    float tv = 1.0f / ( float )uHeight;

	GaussConstants* pConstants = cb.Lock<GaussConstants>();
	Vector4f* pOffsets = pConstants->vOffsets;

    float totalWeight = 0.0f;
    int index = 0;
    for( int x = -2; x <= 2; x++ )
    {
        for( int y = -2; y <= 2; y++ )
        {
            if( Math::Abs( x ) + Math::Abs( y ) > 2 )
			{
                continue;
			}

            // Get the intesnisty (unscaled)
            pOffsets[index].x	= x * tu;
			pOffsets[index].y	= y * tv;

            pOffsets[index].z = GaussianDistribution( ( float )x, ( float )y, 1.0f );
            totalWeight +=  pOffsets[index].z;

            index++;
        }
    }

    // Divide the weight by the total weight of all the samples. 
    // Add 1.0f to ensure that the intensity of the image isn't
    // changed when the blur occurs.
    for( int i = 0; i < index; i++ )
    {
        pOffsets[i].z /= totalWeight;
        pOffsets[i].z *= fMultiplier;
    }

	cb.Unlock();
	cb.UpdateData(pDevice);
}


const SceneRenderPasses& PostFXSys_ps::GetRenderPasses() const
{
	return m_renderPasses;
}

}

