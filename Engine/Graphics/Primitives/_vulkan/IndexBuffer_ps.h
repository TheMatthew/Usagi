/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#ifndef _USG_GRAPHICS_PC_INDEXBUFFER_H
#define _USG_GRAPHICS_PC_INDEXBUFFER_H

#include "Engine/Common/Common.h"
#include "Engine/Graphics/RenderConsts.h"
#include OS_HEADER(Engine/Graphics/Device, VulkanIncludes.h)

namespace usg {

class GFXDevice;

class IndexBuffer_ps
{
	public:
		IndexBuffer_ps();
		~IndexBuffer_ps();

		void Init(GFXDevice* pDevice, const void* pIndices, uint32 uCount, uint32 uIndexSize, bool bStatic, GPULocation eLocation);
		void CleanUp(GFXDevice* pDevice);
		void SetContents(GFXDevice* pDevice, const void* pData, uint32 uIndexCount);

		const VkBuffer& GetBuffer() const { return m_buffer[m_uActiveIBO]; }
		VkIndexType GetType() const { return m_eIndexType; }

	private:
		VkIndexType		m_eIndexType;
		uint32			m_uIndexSize;
		VkBuffer 		m_buffer[GFX_NUM_DYN_BUFF];
    	VkDeviceMemory	m_mem[GFX_NUM_DYN_BUFF];
		uint32			m_uActiveIBO;
		uint32			m_uBufferCount;
};

}

#endif
