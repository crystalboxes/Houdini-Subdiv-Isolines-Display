#include "DM_Isolines.h"
#include "IsolineMaker.h"

#include <DM/DM_RenderTable.h>
#include <GUI/GUI_ViewState.h>

#include <OBJ/OBJ_Node.h>
#include <OP/OP_Director.h>
#include <RE/RE_Geometry.h>
#include <RE/RE_Render.h>
#include <SOP/SOP_Node.h>

#include <iostream>

const char *VertexShader =
    "#version 150 \n"
    "uniform mat4 glH_ProjectMatrix; \n"
    "uniform mat4 glH_ViewMatrix; \n"
    "in vec3 P; \n"
    "in vec3 Cd; \n"
    "out vec4 clr; \n"
    "void main() \n"
    "{ \n"
    "  clr = vec4(Cd, 1.0); \n"
    "  gl_Position = glH_ProjectMatrix * glH_ViewMatrix * vec4(P, 1.0); \n"
    "} \n";

const char *FragmentShader = "#version 150 \n"
                             "in vec4 clr; \n"
                             "out vec4 color; \n"
                             "void main() \n"
                             "{ \n"
                             "  color = clr; \n"
                             "} \n";

#define VIEWPORT_LOD_PARM 5

void DM_IsolinesDisplay::updateNodeState(const DM_GeoDetail &CurrentGeoDetail,
                                         GU_DetailHandle &DetailHandle,
                                         UT_DMatrix4 &LocalToWorld,
                                         bool &IsValidState,
                                         bool &ShouldRecalculate) {
  ShouldRecalculate = false;
  OP_Node *ObjNodeRef = CurrentGeoDetail.getObject();
  IsValidState = !!ObjNodeRef;

  if (!IsValidState)
    return;

  OBJ_Node *ObjNode = CAST_OBJNODE(ObjNodeRef);
  SOP_Node *SopNode = ObjNode->getDisplaySopPtr();

  int CurrentSopUid = SopNode->getUniqueId();
  OP_VERSION CurrentCookVersion = SopNode->getVersionParms();
  int CurrentSubdivDisplayState = ObjNode->evalInt("viewportlod", 0, 0);

  ShouldRecalculate = CurrentSopUid != SopUid ||
                      CurrentCookVersion != CookVersion ||
                      CurrentSubdivDisplayState != SubdivDisplayState;

  SopUid = CurrentSopUid;
  CookVersion = CurrentCookVersion;
  SubdivDisplayState = CurrentSubdivDisplayState;

  IsValidState = CurrentSubdivDisplayState == VIEWPORT_LOD_PARM;

  OP_Context Context(CHgetEvalTime());
  UT_DMatrix4 TransformMatrix;
  ObjNode->getLocalToWorldTransform(Context, LocalToWorld);

  DetailHandle = ObjNode->getDisplayGeometryHandle(Context);
}

const float PeakValue = 0.0;
const int SubdivisionLevel = 3;

bool DM_IsolinesDisplay::isGeoDetailValid(
    const DM_GeoDetail &CurrentGeoDetail) {
  return CurrentGeoDetail.isValid() && CurrentGeoDetail.getNumDetails() > 0;
}

bool DM_IsolinesDisplay::render(RE_Render *Render,
                                const DM_SceneHookData &HookData) {
  if (!HookData.disp_options->isSceneOptionEnabled("isolines_display"))
    return false;

  DM_GeoDetail CurrentGeoDetail = viewport().getCurrentDetail();

  if (!isGeoDetailValid(CurrentGeoDetail))
    return false;

  bool IsValidState, ShouldRecalculate;
  GU_DetailHandle CurrentDetailHandle;
  UT_DMatrix4 LocalToWorldMatrix;

  updateNodeState(CurrentGeoDetail, CurrentDetailHandle, LocalToWorldMatrix,
                  IsValidState, ShouldRecalculate);

  if (!IsValidState)
    return false;

  updateDrawableArrays();
  drawArrays(Render);

  if (!ShouldRecalculate)
    return false;

  IsolineMaker IsoMaker(CurrentDetailHandle, LocalToWorldMatrix, PeakValue,
                        SubdivisionLevel);

  if (!IsoMaker.calculateAttributeArrays())
    return false;

  IsoMaker.getAttributeArrays(Positions, Colors, Normals);
  CachedViewTransform = UT_DMatrix4(1);

  return false;
}

void DM_IsolinesDisplay::updateDrawableArrays() {

  const GUI_ViewState *ViewState = &viewport().getViewStateRef();
  const UT_Matrix4D ViewTransform = ViewState->getRotateMatrix();

  if (ViewTransform.isEqual(CachedViewTransform))
    return;

  DrawableColors.clear();
  DrawablePositions.clear();

  UT_Matrix4D ViewTransformInverted = ViewTransform;
  ViewTransformInverted.invertDouble();
  UT_Vector3 viewDirection = UT_Vector3(0.0, 0.0, 1.0) * ViewTransformInverted;

  for (int x = 0; x < (Normals.entries() / 2); ++x) {
    int Index = x * 2;
    if (viewDirection.dot(Normals[Index]) < 0.0)
      continue;
    DrawableColors.append(Colors[Index]);
    DrawableColors.append(Colors[Index + 1]);
    DrawablePositions.append(Positions[Index]);
    DrawablePositions.append(Positions[Index + 1]);
  }
  CachedViewTransform = ViewTransform;
}

void DM_IsolinesDisplay::drawArrays(RE_Render *Render) {
  int ItemCount = DrawableColors.entries();

  if (ItemCount == 0)
    return;

  RE_Geometry RenderGeo(ItemCount);

  RE_VertexArray *PosRenderAttr = RenderGeo.createAttribute(
      Render, "P", RE_GPU_FLOAT32, 3, DrawablePositions.array());
  RE_VertexArray *ColRenderAttr = RenderGeo.createAttribute(
      Render, "Cd", RE_GPU_FLOAT32, 3, DrawableColors.array());

  RenderGeo.connectAllPrims(Render, 0, RE_PRIM_LINES, NULL, true);

  if (!Shader) {
    Shader = RE_Shader::create("lines");
    Shader->addShader(Render, RE_SHADER_VERTEX, VertexShader, "vertex", 0);
    Shader->addShader(Render, RE_SHADER_FRAGMENT, FragmentShader, "fragment",
                      0);
    Shader->linkShaders(Render);
  }

  Render->pushDepthState();
  Render->setZFunction(RE_ZNOTEQUAL);
  Render->pushShader(Shader);
  Render->pushPointSize(3.0);
  Render->pushSmoothLines();
  Render->pushLineWidth(3.0);
  RenderGeo.draw(Render, 0);
  Render->popLineWidth();
  Render->popSmoothLines();
  Render->popPointSize();
  Render->popShader();
  Render->popDepthState();
}

void newRenderHook(DM_RenderTable *table) {
  table->registerSceneHook(new DM_IsolinesDisplayHook, DM_HOOK_POST_RENDER,
                           DM_HOOK_BEFORE_NATIVE);
  table->installSceneOption("isolines_display",
                            "Show Subdivision Surface Isolines");
}
