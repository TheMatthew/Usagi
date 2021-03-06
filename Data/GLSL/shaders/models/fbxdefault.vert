
#include "../includes/platformdefines.inc"
#include "../includes/global_3d.inc"

// Input attributes
ATTRIB_LOC(0) in vec3 ao_position;
ATTRIB_LOC(1) in vec3 ao_normal;
#ifdef HAS_SKELETON
ATTRIB_LOC(3) in uvec4 ao_boneIndex;
ATTRIB_LOC(4) in vec4 ao_boneWeight;
#endif
ATTRIB_LOC(5) in vec4 ao_color;
ATTRIB_LOC(6) in vec2 ao_texCoord[4];

#ifdef HAS_BUMP
ATTRIB_LOC(2) in vec3 ao_tangent;
ATTRIB_LOC(10) in vec3 ao_binormal;
#endif

#include "../includes/model_transform.inc"

// FIXME: Seperate out into seperate vertex and pixel buffers
BUFFER_LAYOUT(1,  UBO_MATERIAL_ID) uniform Material
{
	// Materials
	mat3x4	mTexMatrix[4];
	int 	iBoneCount;
	bool	bBumpMap;

} uVSMaterial;


// Output attributes
ATTRIB_LOC(0) out vec4 vo_vTexCoord01;
ATTRIB_LOC(1) out vec4 vo_vTexCoord23;
ATTRIB_LOC(2) out vec4 vo_vColor;
ATTRIB_LOC(3) out vec3 vo_vNormal;
#ifdef HAS_BUMP
ATTRIB_LOC(4) out vec3 vo_vTangent;
ATTRIB_LOC(5) out vec3 vo_vBinormal;
#endif
ATTRIB_LOC(6) out vec3 vo_vWorldPos;
ATTRIB_LOC(7) out vec3 vo_vViewDir;



void main(void)
{
	vec4 vWorldPos	= vec4(ao_position, 1.0);
	vWorldPos = ApplyWorldTransform(vWorldPos, uVSMaterial.iBoneCount);

	vec4 vTmp = vec4(0.0, 0.0, 1.0, 1.0);

	vTmp.xy = ao_texCoord[0];
	vo_vTexCoord01.x = dot(uVSMaterial.mTexMatrix[0][0].xywz, vTmp);
	vo_vTexCoord01.y = dot(uVSMaterial.mTexMatrix[0][1].xywz, vTmp);
	
	vTmp.xy = ao_texCoord[1];
	vo_vTexCoord01.z = dot(uVSMaterial.mTexMatrix[1][0].xywz, vTmp);
	vo_vTexCoord01.w = dot(uVSMaterial.mTexMatrix[1][1].xywz, vTmp);

	vTmp.xy = ao_texCoord[2];
	vo_vTexCoord23.x = dot(uVSMaterial.mTexMatrix[2][0].xywz, vTmp);
	vo_vTexCoord23.y = dot(uVSMaterial.mTexMatrix[2][1].xywz, vTmp);
	

	vTmp.xy = ao_texCoord[3];
	vo_vTexCoord23.z = dot(uVSMaterial.mTexMatrix[3][0].xywz, vTmp);
	vo_vTexCoord23.w = dot(uVSMaterial.mTexMatrix[3][1].xywz, vTmp);
	

#ifndef SHADOW_PASS
	// Transform the normal into world space
	vec3 vNormal		= ApplyWorldTransform(vec4( ao_normal, 0.0 ), uVSMaterial.iBoneCount).xyz;
	vec3 vViewNormal	= (vec4( vNormal, 0.0 ) * mViewMat).xyz;

#ifdef HAS_BUMP
	if(uVSMaterial.bBumpMap)
	{
		vec3 vTangent		= ApplyWorldTransform(vec4( ao_tangent, 0.0 ), uVSMaterial.iBoneCount).xyz;
		vec3 vViewTangent	= (vec4( vTangent, 0.0 ) * mViewMat).xyz;
		vo_vTangent = vViewTangent;
	//	if(uVSMaterial.bBinormal)
		{
			vec3 vBinormal		= ApplyWorldTransform(vec4( ao_binormal, 0.0 ), uVSMaterial.iBoneCount).xyz;
			vo_vBinormal	= (vec4( vBinormal, 0.0 ) * mViewMat).xyz;
		}
	/*	else
		{
			vo_vBinormal = cross(vTangent, vViewNormal);
		}*/
	}
#endif

	vo_vColor = ao_color;
	vo_vNormal = vViewNormal;
#endif

#ifdef OMNI_DEPTH
	gl_Position			= vWorldPos;
#else
	vec4 vViewPos	= vWorldPos * mViewMat;	// Reverse ordering as a mat3x4
	vec4 vProjPos	= vec4(vViewPos.xyz, 1.0) * mProjMat;

	vo_vViewDir			= -vViewPos.xyz;
	

	vo_vWorldPos		= vWorldPos.xyz;

	gl_Position			= vProjPos;
#endif
}
