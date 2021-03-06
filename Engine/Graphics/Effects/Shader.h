/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//  A shader for a specific pipeline stage - it is perfectly valid to not 
//  implement this on platforms which use combined effects
****************************************************************************/
#ifndef _USG_GRAPHICS_SHADER_
#define _USG_GRAPHICS_SHADER_
#include "Engine/Common/Common.h"
#include "Engine/Core/String/U8String.h"
#include API_HEADER(Engine/Graphics/Effects, Shader_ps.h)
#include "Engine/Resource/ResourceBase.h"

namespace usg {

class Texture;
class GFXDevice;

class Shader : public ResourceBase
{
public:
	Shader() { m_resourceType = ResourceType::SHADER; }
	virtual ~Shader() {}

	bool Init(GFXDevice* pDevice, PakFile* pFile, const PakFileDecl::FileInfo* pFileHeader, const void* pData);
	void CleanUp(GFXDevice* pDevice) { m_platform.CleanUp(pDevice); }

	Shader_ps& GetPlatform() { return m_platform; }
	const Shader_ps& GetPlatform() const { return m_platform; }
	const U8String& GetName() const { return m_name; }

private:
	PRIVATIZE_COPY(Shader)

	U8String	m_name;
	Shader_ps	m_platform;
};


inline bool Shader::Init(GFXDevice* pDevice, PakFile* pFile, const PakFileDecl::FileInfo* pFileHeader, const void* pData)
{
	m_name = pFileHeader->szName;
	SetupHash(m_name.CStr());
	bool bLoaded = m_platform.Init(pDevice, pFile, pFileHeader, pData, pFileHeader->uDataSize);
	// FIXME: This should be done internally
	SetReady(true);
	return bLoaded;
}

}
 
#endif
