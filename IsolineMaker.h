#pragma once

#include <GT/GT_DataArray.h>
#include <GU/GU_Detail.h>
#include <GU/GU_DetailHandle.h>
#include <SYS/SYS_Math.h>

class IsolineMaker {
public:
  IsolineMaker(GU_DetailHandle GdpHandle, float Peak, int SubdivisionLevel);
  IsolineMaker(GU_DetailHandle GdpHandle, UT_DMatrix4 Transform, float Peak,
               int SubdivisionLevel);
  // function which calculates isoline positions
  bool calculateAttributeArrays();
  // arrays for gl rendering
  void getAttributeArrays(UT_Vector3FArray &OutPositions,
                          UT_Vector3FArray &OutColors,
                          UT_Vector3FArray &OutNormals);
  // constructs polyline geo in the target gdp
  void createGeometry(GU_Detail *TargetGdp);

private:
  // Checks if incoming geo only has primitives with n-vertices > 2
  bool isValidGeo();
  // Adds n = output geometry elements into attribute arrays
  void fillAttributeArrays();
  // evaluates opensubdiv functions to find the limit surface
  void getLimitSurfacePositions();
  // writes positions into attribute array
  void applyLimitSurfacePositions();

  const GU_Detail *gdp();
  float getPeakProportional();

  GU_DetailHandle GdpHandle;

  const bool UsesTransform;
  const UT_DMatrix4 Transform;
  const float Peak;
  const int SubdivisionLevel;

  UT_Vector3FArray Positions, Colors, Normals;
  UT_Array<int> FaceIndices, ReferenceIndices;
  UT_Array<float> U, V;
  bool HasCrease = false;

  GT_DataArrayHandle Surface, LimitNormals;
};
