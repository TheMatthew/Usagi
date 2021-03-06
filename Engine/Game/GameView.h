#include "Engine/Common/Common.h"
#include "Engine/Graphics/RenderConsts.h"
#include "Engine/Scene/Camera/StandardCamera.h"
#include "Engine/Scene/Camera/HMDCamera.h"

namespace usg
{
	class Scene;
	class PostFXSys;
	class ViewContext;
	class DebugRender;
	class Display;
	enum ViewType;

	class GameView
	{
	public:
		GameView(usg::GFXDevice* pDevice, usg::Scene& scene, usg::PostFXSys& postFXSys, const usg::GFXBounds& bounds, float32 fFov, float32 fNear, float32 fFar);
		~GameView();

		void CleanUp(usg::GFXDevice* pDevice, usg::Scene& scene);
		void AddDebugRender(usg::DebugRender* pDebugRender);

		void Draw(usg::PostFXSys* pPostFXSys, usg::Display* pDisplay, usg::GFXContext* pImmContext, usg::GFXContext* pDeferredContext, usg::ViewType eType);

		const usg::GFXBounds& GetBounds() const;
		void SetBounds(usg::GFXBounds& bounds);
		usg::ViewContext* GetViewContext() { return m_pViewContext; }
		usg::StandardCamera& GetCamera() { return m_camera; }
		usg::HMDCamera& GetHMDCamera() { return m_hmdCamera; }

	private:
		void SetViewContext(usg::ViewContext& context);

		usg::ViewContext* m_pViewContext = nullptr;
		usg::GFXBounds m_bounds;
		usg::StandardCamera	m_camera;
		usg::HMDCamera	m_hmdCamera;
		usg::DebugRender* m_pDebugRender = nullptr;

	};

}