#include "../includes/platformdefines.inc"
#include "../includes/global_3d.inc"

BUFFER_LAYOUT(1,  UBO_MATERIAL_ID) uniform Material
{
  vec4  vStartColor;        // color of the outside of the beam
  vec4  vEndColor;        // color of the outside of the beam
  float fLineWidth;         // size in world space of the beam
  float fInvLinePersist;
  float fElapsedTime;
};

ATTRIB_LOC(0) in vec3 	ao_position;
ATTRIB_LOC(1) in float 	ao_fCreateTime;
ATTRIB_LOC(2) in float 	ao_fLength;	// The length of the line at the point this point was spawned

out VertexData
{
    INT_LOC(0) vec3  vo_viewPos;
    INT_LOC(1) float vo_fCreateTime;
    INT_LOC(2) float vo_fLength;
};


void main()
{
    vo_viewPos 		= (vec4(ao_position, 1.0) * mViewMat).xyz;
   	vo_fCreateTime	= ao_fCreateTime;
   	vo_fLength		= ao_fLength;
}
