/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: The binary and associated states for a data defined effect
//	declaration (e.g. for models and particle effects)
*****************************************************************************/
#ifndef _USG_RESOURCE_CUSTOM_EFFECT_RESOURCE_H_
#define _USG_RESOURCE_CUSTOM_EFFECT_RESOURCE_H_

#include "Engine/Graphics/Device/GFXHandles.h"
#include "Engine/Resource/ResourceDecl.h"
#include "Engine/Resource/ResourceBase.h"
#include "Engine/Resource/PakDecl.h"
#include "CustomEffectDecl.h"

namespace usg
{
	namespace CustomEffectDecl
	{
		struct Sampler;
		struct Attribute;
		struct CustomEffectDecl;
		struct Constant;
		struct Header;
	};

	class CustomEffectResource : public ResourceBase
	{
	public:
		CustomEffectResource();
		~CustomEffectResource();

		virtual bool Init(GFXDevice* pDevice, const PakFileDecl::FileInfo* pFileHeader, const class FileDependencies* pDependencies, const void* pData) override;
		void Init(GFXDevice* pDevice, const char* szFileName);

		uint32 GetAttribBinding(const char* szAttrib) const;
		uint32 GetAttribCount() const;
		const CustomEffectDecl::Attribute* GetAttribute(uint32 uIndex) const;
		uint32 GetSamplerBinding(const char* szSampler) const;

		const DescriptorDeclaration* GetDescriptorDecl() const { return m_pDescriptorDecl; }
		const DescriptorSetLayoutHndl& GetDescriptorLayoutHndl() const { return m_descLayout; }
		const ShaderConstantDecl* GetConstantDecl(uint32 uIndex) const { return &m_pShaderConstDecl[m_uConstDeclOffset[uIndex]]; }

		uint32 GetConstantSetCount() const;
		uint32 GetConstantSetBinding(uint32 uSet) const;
		uint32 GetConstantCount(uint32 uSet) const;
		const CustomEffectDecl::Constant* GetConstant(uint32 uSet, uint32 uConstant) const;
		uint32 GetBindingPoint(uint32 uSet);

		void* GetDefaultData(uint32 uSet) const;
		const char* GetDefaultTexture(uint32 uSampler);

		const char* GetEffectName() const;
		const char* GetDeferredEffectName() const;
		const char* GetTransparentEffectName() const;
		const char* GetDepthEffectName() const;
		const char* GetOmniDepthEffectName() const;

		const static ResourceType StaticResType = ResourceType::CUSTOM_EFFECT;

	private:
		void FixUpPointers(GFXDevice* pDevice);
		enum
		{
			MAX_CONSTANT_SETS = 4
		};
		void*									m_pAlloc;
		ShaderConstantDecl*						m_pShaderConstDecl;
		DescriptorDeclaration*					m_pDescriptorDecl;
		uint32									m_uConstDeclOffset[MAX_CONSTANT_SETS];

		DescriptorSetLayoutHndl					m_descLayout;
		CustomEffectDecl::Header				m_header;
		const void*								m_pBinary;
		const CustomEffectDecl::ConstantSet*	m_pConstantSets;
		const CustomEffectDecl::Sampler*		m_pSamplers;
		const CustomEffectDecl::Attribute*		m_pAttributes;
	};

}

#endif	// #ifndef _USG_GRAPHICS_SCENE_MODEL_H_
