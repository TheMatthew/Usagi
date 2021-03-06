/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Scene/Frustum.h"
#include "Engine/Core/ProtocolBuffers/ProtocolBufferFile.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/ViewContext.h"
#include "Engine/Resource/ResourceMgr.h"
#include "Engine/Graphics/Shadows/ShadowCascade.h"
#include "Engine/Graphics/Lights/LightSpec.pb.h"
#include "Engine/Graphics/Lights/LightMgr.h"
#include "Engine/Graphics/Lights/PointLight.h"
#include "Engine/Graphics/Lights/DirLight.h"
#include "Engine/Graphics/Lights/SpotLight.h"
#include "Engine/Graphics/Lights/ProjectionLight.h"

namespace usg {

LightMgr::LightMgr(void):
m_pParent(nullptr)
{
	m_skyColor.Assign(1.0f, 1.0f, 1.0f, 1.0f);
	m_groundColor.Assign(1.0f, 1.0f, 1.0f, 1.0f);
	m_hemisphereDir.Assign(1.0f, 1.0f, 1.0f);
	m_hemipshereLerp = 0.5f;
	m_uShadowedDirLights = 0;
	m_uShadowedDirLightIndex = UINT_MAX;
	m_uActiveFrame = UINT_MAX;
	m_shadowMapRes = 2048;
}

LightMgr::~LightMgr(void)
{
}

void LightMgr::Init(GFXDevice* pDevice, Scene* pParent)
{
	m_pParent = pParent;

	// Set up an initial dummy array for binding purposes
	m_cascadeBuffer.InitArray(pDevice, 32, 32, 2, DF_DEPTH_32F);//DF_DEPTH_32F); //DF_DEPTH_24
	m_cascadeTarget.Init(pDevice, NULL, &m_cascadeBuffer);
	usg::RenderTarget::RenderPassFlags flags;
	flags.uClearFlags = RenderTarget::RT_FLAG_DEPTH;
	flags.uStoreFlags = RenderTarget::RT_FLAG_DEPTH;
	flags.uShaderReadFlags = RenderTarget::RT_FLAG_DEPTH;
	m_cascadeTarget.InitRenderPass(pDevice, flags);
}


void LightMgr::SetShadowCascadeResolution(GFXDevice* pDevice, uint32 uResolution)
{
	// TODO: Handle resizing after layers have been created (could be a useful performance optimization)
	m_shadowMapRes = uResolution;
	if (m_cascadeBuffer.GetSlices() > 1)
	{
		m_cascadeBuffer.Resize(pDevice, uResolution, uResolution);
		m_cascadeTarget.Resize(pDevice);
	}
}


void LightMgr::InitShadowCascade(GFXDevice* pDevice, uint32 uLayers)
{
	if (m_cascadeBuffer.GetWidth() != m_shadowMapRes || uLayers != m_cascadeBuffer.GetSlices())
	{
		m_cascadeBuffer.CleanUp(pDevice);
		m_cascadeTarget.CleanUp(pDevice);
		m_cascadeBuffer.InitArray(pDevice, m_shadowMapRes, m_shadowMapRes, uLayers, DF_DEPTH_32F);
		m_cascadeTarget.Init(pDevice, NULL, &m_cascadeBuffer);
		usg::RenderTarget::RenderPassFlags flags;
		flags.uClearFlags = RenderTarget::RT_FLAG_DEPTH;
		flags.uStoreFlags = RenderTarget::RT_FLAG_DEPTH;
		flags.uShaderReadFlags = RenderTarget::RT_FLAG_DEPTH;
		m_cascadeTarget.InitRenderPass(pDevice, flags);
	}
}


void LightMgr::CleanUp(GFXDevice* pDevice)
{
	m_dirLights.CleanUp(pDevice, m_pParent);
	m_spotLights.CleanUp(pDevice, m_pParent);
	m_pointLights.CleanUp(pDevice, m_pParent);
	m_projLights.CleanUp(pDevice, m_pParent);
	m_cascadeTarget.CleanUp(pDevice);
	m_cascadeBuffer.CleanUp(pDevice);
}


void LightMgr::Update(float fDelta, uint32 uFrame)
{
	// TODO: Handle multiple viewcontexts for the shodows
	ViewContext* pContext= m_pParent->GetViewContext(0);
	m_uActiveFrame = uFrame;

	m_uShadowedDirLights = 0;
	m_uShadowedDirLightIndex = UINT_MAX;

	List<DirLight> dirLights;
	GetActiveDirLights(dirLights);

	// Now find the most influential shadowed directional light and make it the first
	uint32 uCascadeIndex = 0;
	for (uint32 i = 0; i < m_dirLights.GetActiveLights().size(); i++)
	{
		if (m_dirLights.GetActiveLights()[i]->GetShadowEnabled())
		{
			m_dirLights.GetActiveLights()[i]->GetCascade()->AssignRenderTarget(&m_cascadeTarget, uCascadeIndex);
			uCascadeIndex += ShadowCascade::CASCADE_COUNT;
			if (m_uShadowedDirLightIndex == UINT_MAX)
			{
				m_uShadowedDirLightIndex = i;
			}
			m_uShadowedDirLights++;
		}
	}

	if (m_uShadowedDirLightIndex == UINT_MAX)
	{
		// No shadowed lights, set the index to be beyond the list
		m_uShadowedDirLightIndex = (uint32)m_dirLights.GetActiveLights().size();
	}

	for (auto itr : m_dirLights.GetActiveLights())
	{
		itr->UpdateCascade(*pContext->GetCamera(), 0);
	}


}


TextureHndl	LightMgr::GetShadowCascadeImage() const
{
	return m_cascadeBuffer.GetTexture();
}

void LightMgr::GPUUpdate(GFXDevice* pDevice)
{
	for (auto itr : m_dirLights.GetActiveLights())
	{
		itr->GPUUpdate(pDevice);
	}

	for (auto itr : m_pointLights.GetActiveLights())
	{
		itr->GPUUpdate(pDevice);
	}

	for (auto itr : m_spotLights.GetActiveLights())
	{
		itr->GPUUpdate(pDevice);
	}

	for (auto itr : m_projLights.GetActiveLights())
	{
		itr->GPUUpdate(pDevice);
	}
}


void LightMgr::GlobalShadowRender(GFXContext* pContext, Scene* pScene)
{
	for (auto itr : m_pointLights.GetActiveLights())
	{
		if (itr->GetVisibleFrame() == pScene->GetFrame())
		{
			itr->ShadowRender(pContext);
		}
	}

	for (auto itr : m_spotLights.GetActiveLights())
	{
		if (itr->GetVisibleFrame() == pScene->GetFrame())
		{
			itr->ShadowRender(pContext);
		}
	}

	for (auto itr : m_projLights.GetActiveLights())
	{
		if (itr->GetVisibleFrame() == pScene->GetFrame())
		{
			itr->ShadowRender(pContext);
		}
	}
}

void LightMgr::ViewShadowRender(GFXContext* pContext, Scene* pScene, ViewContext* pView)
{
	for (auto itr : m_dirLights.GetActiveLights())
	{
		itr->ShadowRender(pContext);
	}
}


DirLight* LightMgr::AddDirectionalLight(GFXDevice* pDevice, bool bSupportsShadow, const char* szName)
{
	DirLight* pLight = m_dirLights.GetLight(pDevice, m_pParent, bSupportsShadow);	
	if(szName)
		pLight->SetName(szName);
	ASSERT(pLight);

	if (bSupportsShadow)
		m_uShadowedDirLights++;

	if (m_cascadeBuffer.GetSlices() < ShadowCascade::CASCADE_COUNT * m_uShadowedDirLights)
	{
		InitShadowCascade(pDevice, ShadowCascade::CASCADE_COUNT * m_uShadowedDirLights);
	}

	return pLight;
}

void LightMgr::RemoveDirLight(DirLight* pLight)
{
	return m_dirLights.Free(pLight);
}

PointLight* LightMgr::AddPointLight(GFXDevice* pDevice, bool bSupportsShadow, const char* szName)
{
	PointLight* pLight = m_pointLights.GetLight(pDevice, m_pParent, bSupportsShadow);
	if(szName)
		pLight->SetName(szName);
	return pLight;
}

void LightMgr::RemovePointLight(PointLight* pLight)
{
	return m_pointLights.Free(pLight);
}


SpotLight* LightMgr::AddSpotLight(GFXDevice* pDevice, bool bSupportsShadow, const char* szName)
{
	SpotLight* pLight = m_spotLights.GetLight(pDevice, m_pParent, bSupportsShadow);
	if(szName)
		pLight->SetName(szName);
	return pLight;
}

void LightMgr::RemoveSpotLight(SpotLight* pLight)
{
	return m_spotLights.Free(pLight);
}


ProjectionLight* LightMgr::AddProjectionLight(GFXDevice* pDevice, bool bSupportsShadow, const char* szName)
{
	ProjectionLight* pLight = m_projLights.GetLight(pDevice, m_pParent, bSupportsShadow);
	if(szName)
		pLight->SetName(szName);
	return pLight;	
}

void LightMgr::RemoveProjectionLight(ProjectionLight* pLight)
{
	return m_projLights.Free(pLight);
}


void LightMgr::GetActiveDirLights(List<DirLight>& lightsOut) const
{
	lightsOut.Clear();
	for(auto it = m_dirLights.GetActiveLights().begin(); it!=m_dirLights.GetActiveLights().end(); ++it)
	{
		if( (*it)->IsActive() )
		{
			(*it)->SetVisibleFrame(m_uActiveFrame);
			if ((*it)->GetShadowEnabled())
			{
				lightsOut.AddToEnd(*it);
			}
			else
			{
				lightsOut.AddToFront(*it);
			}
		}
	}

	lightsOut.Sort();
}



void LightMgr::GetPointLightsInView(const Camera* pCamera, List<PointLight>& lightsOut) const
{
	lightsOut.Clear();
	// TODO: Give the point lights transforms and bounding volumes
	for (auto it = m_pointLights.GetActiveLights().begin(); it!=m_pointLights.GetActiveLights().end(); ++it)
	{
		if( (*it)->IsActive() && ((*it)->GetFar() > 0.0f || !(*it)->HasAttenuation()) )
		{
			// TODO: These lights should be culled like every object in the game using a quad tree
			if( !(*it)->HasAttenuation() || pCamera->GetFrustum().IsSphereInFrustum( (*it)->GetColSphere() ) )
			{
				(*it)->SetVisibleFrame(m_uActiveFrame);
				lightsOut.AddToEnd(*it);
			}
		}
	}
}


void LightMgr::GetSpotLightsInView(const Camera* pCamera, List<SpotLight>& lightsOut) const
{
	lightsOut.Clear();
	// TODO: Give the point lights transforms and bounding volumes
	for (auto it = m_spotLights.GetActiveLights().begin(); it != m_spotLights.GetActiveLights().end(); ++it)
	{
		if( (*it)->IsActive() )
		{
			(*it)->SetVisibleFrame(m_uActiveFrame);
			// TODO: These lights should be culled like every object in the game using a quad tree
			if( !(*it)->HasAttenuation() || pCamera->GetFrustum().IsSphereInFrustum( (*it)->GetColSphere() ) )
			{
				lightsOut.AddToEnd(*it);
			}
		}
	}
}


void LightMgr::GetProjectionLightsInView(const Camera* pCamera, List<ProjectionLight>& lightsOut) const
{
	lightsOut.Clear();
	// TODO: Give the point lights transforms and bounding volumes
	for (auto it = m_projLights.GetActiveLights().begin(); it != m_projLights.GetActiveLights().end(); ++it)
	{
		if( (*it)->IsActive() )
		{
			(*it)->SetVisibleFrame(m_uActiveFrame);
			if( (*it)->GetFrustum().ArePointsInFrustum( (*it)->GetCorners(), 8 ) )
			{
				lightsOut.AddToEnd(*it);
			}
		}
	}
}


Light* LightMgr::FindLight(const char* szName)
{
	U8String name(szName);
	for (auto it : m_pointLights.GetActiveLights() )
	{
		if( it->GetName() == name )
		{
			return it;
		}
	}

	for (auto it : m_dirLights.GetActiveLights())
	{
		if (it->GetName() == name)
		{
			return it;
		}
	}

	for (auto it : m_spotLights.GetActiveLights())
	{
		if (it->GetName() == name)
		{
			return it;
		}
	}

	return NULL;
}



Light* LightMgr::CreateLight(GFXDevice* pDevice, ResourceMgr* pResMgr, const LightSpec &light)
{
	Light* newLight = NULL;
	switch (light.base.kind)
	{
	case LightKind_DIRECTIONAL:
	{
		newLight = AddDirectionalLight(pDevice, light.base.bShadow);
		Vector4f vDir(light.direction, 0.0);
		vDir.Normalise();
		newLight->SetDirection(vDir);
	}
	break;
	case LightKind_POINT:
	{
		PointLight* pPoint = AddPointLight(pDevice, light.base.bShadow);
		if (light.atten.bEnabled)
		{
			pPoint->SetRange(light.atten.fNear, light.atten.fFar);
		}
		else
		{
			pPoint->EnableAttenuation(false);
		}
		newLight = pPoint;
	}

	break;
	case LightKind_SPOT:
	{
		SpotLight* pSpot = AddSpotLight(pDevice, light.base.bShadow);
		Vector4f vDir(light.direction, 0.0);
		vDir.Normalise();
		pSpot->SetDirection(vDir);

		pSpot->SetInnerCutoff(Math::DegToRad(light.spot.fInnerAngle));
		pSpot->SetOuterCutoff(Math::DegToRad(light.spot.fOuterAngle));

		if (light.atten.bEnabled)
		{
			pSpot->SetRange(light.atten.fNear, light.atten.fFar);
		}
		else
		{
			pSpot->EnableAttenuation(false);
		}

		newLight = pSpot;
	}
	break;
	case LightKind_PROJECTION:
	{
		ProjectionLight* pProj = AddProjectionLight(pDevice, light.base.bShadow);
		Matrix4x4 mProj;
		mProj.Perspective(Math::DegToRad(light.proj.fFov), light.proj.fAspect, 0.1f, light.atten.fFar);
		pProj->SetProjectionMtx(mProj);
		pProj->SetTexture(pDevice, pResMgr->GetTexture(pDevice, light.proj.texName));
		pProj->SetRange(light.atten.fNear, light.atten.fFar);

		// TODO: Set the texture

		newLight = pProj;
	}
	break;
	}

	if (newLight)
	{
		newLight->SetDiffuse(light.base.diffuse);
		newLight->SetAmbient(light.base.ambient);
		newLight->SetSpecularColor(light.base.specular);
		newLight->SwitchOn(true);
	}

	return newLight;
}


void LightMgr::RemoveLight(const Light* pLight)
{
	ASSERT(pLight != NULL);
	switch (pLight->GetType())
	{
	case LIGHT_TYPE_DIR: 
		RemoveDirLight((DirLight*)pLight);
		break;
	case LIGHT_TYPE_POS:
		RemovePointLight((PointLight*)pLight);
		break;
	case LIGHT_TYPE_PROJ:
		RemoveProjectionLight((ProjectionLight*)pLight);
		break;
	case LIGHT_TYPE_SPOT:
		RemoveSpotLight((SpotLight*)pLight);
		break;
	default:
		ASSERT(false);
	}
}


}
