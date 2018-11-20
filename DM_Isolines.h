#pragma once

#include <DM/DM_SceneHook.h>
#include <DM/DM_VPortAgent.h>

class DM_IsolinesDisplay : public DM_SceneRenderHook {
public:
  DM_IsolinesDisplay(DM_VPortAgent &ViewPort)
      : DM_SceneRenderHook(ViewPort, DM_VIEWPORT_ALL) {}
  virtual ~DM_IsolinesDisplay() { delete Shader; }

  virtual bool render(RE_Render *r, const DM_SceneHookData &HookData);

private:
  void updateDrawableArrays();
  void drawArrays(RE_Render *Render);
  void updateNodeState(const DM_GeoDetail &CurrentGeoDetail,
                       GU_DetailHandle &DetailHandle, UT_DMatrix4 &LocalToWorld,
                       bool &IsValidState, bool &ShouldRecalculate);
  bool isGeoDetailValid(const DM_GeoDetail &CurrentGeoDetail);

  RE_Shader *Shader = NULL;

  UT_Vector3FArray Positions;
  UT_Vector3FArray Colors;
  UT_Vector3FArray Normals;

  UT_Vector3FArray DrawablePositions;
  UT_Vector3FArray DrawableColors;

  UT_DMatrix4 CachedViewTransform;

  int SopUid = -999;
  int SubdivDisplayState = -1;
  OP_VERSION CookVersion = -999;
};

class DM_IsolinesDisplayHook : public DM_SceneHook {
public:
  DM_IsolinesDisplayHook() : DM_SceneHook("", 0) {}

  virtual DM_SceneRenderHook *newSceneRender(DM_VPortAgent &vport,
                                             DM_SceneHookType type,
                                             DM_SceneHookPolicy policy) {
    return new DM_IsolinesDisplay(vport);
  }

  virtual void retireSceneRender(DM_VPortAgent &vport,
                                 DM_SceneRenderHook *hook) {
    delete hook;
  }
};
