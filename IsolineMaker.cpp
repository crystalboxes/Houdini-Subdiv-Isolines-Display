#include "IsolineMaker.h"
#include "GeometryUtilities.h"
#include <GEO/GEO_PrimPoly.h>
#include <GT/GT_GEODetail.h>
#include <GT/GT_PrimSubdivisionMesh.h>
#include <GT/GT_Util.h>
#include <GT/GT_UtilOpenSubdiv.h>

IsolineMaker::IsolineMaker(GU_DetailHandle GdpHandle, float Peak,
                           int SubdivisionLevel)
    : Transform(UT_Matrix4D(1.f)), Peak(Peak),
      SubdivisionLevel(SubdivisionLevel), UsesTransform(false) {
  this->GdpHandle = GdpHandle;
}

IsolineMaker::IsolineMaker(GU_DetailHandle GdpHandle, UT_DMatrix4 Transform,
                           float Peak, int SubdivisionLevel)
    : Transform(Transform), Peak(Peak), SubdivisionLevel(SubdivisionLevel),
      UsesTransform(true) {
  this->GdpHandle = GdpHandle;
}

bool IsolineMaker::calculateAttributeArrays() {
  if (!isValidGeo())
    return false;

  Positions.clear();
  Colors.clear();
  Normals.clear();

  FaceIndices.clear();
  ReferenceIndices.clear();
  U.clear();
  V.clear();

  fillAttributeArrays();
  getLimitSurfacePositions();
  applyLimitSurfacePositions();
  return true;
}

void IsolineMaker::getLimitSurfacePositions() {
  GU_ConstDetailHandle ConstDetailHandle = GU_ConstDetailHandle(GdpHandle);
  GT_PrimitiveHandle Mesh = GT_GEODetail::makePolygonMesh(ConstDetailHandle);
  const GT_PrimPolygonMesh &PrimPolyMesh =
      *(const GT_PrimPolygonMesh *)(Mesh.get());
  GT_PrimSubdivisionMesh PrimSubdivMesh(PrimPolyMesh,
                                        GT_Scheme::GT_CATMULL_CLARK);
  if (HasCrease) {
    GT_DataArrayHandle EdgeIndices;
    GT_DataArrayHandle EdgeSharpness;
    GT_DataArrayHandle CornerIndices;
    GT_DataArrayHandle CornerSharpness;
    GT_DataArrayHandle HoleIndices;

    GT_Util::computeSubdivisionCreases(PrimPolyMesh, EdgeIndices, EdgeSharpness,
                                       CornerIndices, CornerSharpness,
                                       HoleIndices);

    GT_PrimSubdivisionMesh::Tag CreaseTag("crease");
    GT_PrimSubdivisionMesh::Tag CornerTag("corner");

    CreaseTag.appendInt(EdgeIndices);
    CreaseTag.appendReal(EdgeSharpness);
    PrimSubdivMesh.appendTag(CreaseTag);

    CornerTag.appendInt(CornerIndices);
    CornerTag.appendReal(CornerSharpness);
    PrimSubdivMesh.appendTag(CornerTag);
  }

  GT_UtilOpenSubdiv Osd;
  Osd.setupLimitEval(PrimSubdivMesh.createPointNormalsIfMissing());

  GT_UtilOpenSubdiv::AttribId Attribute = Osd.limitFindAttribute("uv");

  // find a corresponding subd patch of the given parametric value
  for (int x = 0; x < FaceIndices.entries(); x++) {
    GT_Size OsdFace;
    fpreal OsdU, OsdV;

    Osd.limitLookupPatch(FaceIndices[x], U[x], V[x], OsdFace, OsdU, OsdV,
                         Attribute);

    FaceIndices[x] = OsdFace;
    U[x] = OsdU;
    V[x] = OsdV;
  }

  Surface =
      Osd.limitSurface("P", false, FaceIndices.entries(),
                       FaceIndices.getArray(), U.getArray(), V.getArray());

  LimitNormals =
      Osd.limitSurface("N", false, FaceIndices.entries(),
                       FaceIndices.getArray(), U.getArray(), V.getArray());
}

void IsolineMaker::applyLimitSurfacePositions() {
  for (int x = 0; x < ReferenceIndices.entries(); ++x) {
    int PointIndex = ReferenceIndices[x];

    UT_Vector3 Position = UT_Vector3(Surface->getF32(PointIndex, 0),
                                     Surface->getF32(PointIndex, 1),
                                     Surface->getF32(PointIndex, 2));
    if (UsesTransform)
      Position *= Transform;

    UT_Vector3 NormalDirection =
        UT_Vector3(LimitNormals->getF32(PointIndex, 0),
                   LimitNormals->getF32(PointIndex, 1),
                   LimitNormals->getF32(PointIndex, 2));
    NormalDirection.normalize();
    UT_Vector3 LimitSurfacePosition =
        Position + NormalDirection * getPeakProportional();

    Normals.append(NormalDirection);
    Positions[x] = LimitSurfacePosition;
  }
}

void IsolineMaker::fillAttributeArrays() {
  UT_Array<GA_OffsetArray> AllPointNeighbours;
  gdp()->buildRingZeroPoints(AllPointNeighbours, NULL);

  const GA_Attribute *CreaseAttribute =
      gdp()->findVertexAttribute("creaseweight");
  HasCrease = GA_ROHandleF(CreaseAttribute).isValid();

  int CurrentPoint = 0;
  float OverallCreaseValue = 0.0f;

  for (GA_Iterator It(gdp()->getPointRange()); !It.atEnd(); ++It) {
    const GA_Offset PointOffset = *It;
    const GA_Index PointIndex = gdp()->pointIndex(PointOffset);

    GA_OffsetArray Neighbours;
    Neighbours = AllPointNeighbours[PointIndex];

    for (int x = 0; x < Neighbours.entries(); x++) {
      GA_Index NeighbourIndex = gdp()->pointIndex(Neighbours[x]);
      if (PointIndex > NeighbourIndex) {
        // keep point edge as indices
        GA_Offset PointEdge[2] = {Neighbours[x], PointOffset};

        GA_OffsetArray AdjacentPrimitives;
        GeometryUtilities::adjacentPrimitivesToEdge(gdp(), AdjacentPrimitives,
                                                    PointEdge[0], PointEdge[1]);
        float CreaseValue = 0.0f;

        GA_Offset StartPrimitive, AdjacentPrimitive;
        StartPrimitive = AdjacentPrimitives[0];
        if (AdjacentPrimitives.entries() > 1) {
          AdjacentPrimitive = AdjacentPrimitives[1];

          if (HasCrease)
            CreaseValue = GeometryUtilities::getCreaseValue(
                gdp(), StartPrimitive, AdjacentPrimitive, PointEdge[0],
                PointEdge[1]);
          OverallCreaseValue += CreaseValue;
        }

        // patametric coordinates
        int FaceIndex = gdp()->primitiveIndex(StartPrimitive);
        UT_Vector3 Uv0, Uv1;
        GeometryUtilities::getOsdParametricValues(
            gdp(), StartPrimitive, PointEdge[0], PointEdge[1], Uv0, Uv1);

        CreaseValue = SYSfit(CreaseValue, 0.0f, 4.0f, 0.0f, 1.0f);
        UT_Vector3 CdValue = SYSlerp(UT_Vector3(0.0, 0.9, 0.9),
                                     UT_Vector3(1.0, 0.0, 0.0), CreaseValue);

        ReferenceIndices.append(CurrentPoint);
        FaceIndices.append(FaceIndex);
        U.append(Uv0.x());
        V.append(Uv0.y());
        CurrentPoint++;

        // points inbetween
        int InEdgePointsCount = int(pow(2, SubdivisionLevel)) - 1;
        for (int PointId = 0; PointId < InEdgePointsCount; PointId++) {
          float Factor = float(PointId + 1) / float(InEdgePointsCount + 1);
          UT_Vector3 Uvi = Uv0 + (Uv1 - Uv0) * Factor;

          ReferenceIndices.append(CurrentPoint);
          ReferenceIndices.append(CurrentPoint);
          FaceIndices.append(FaceIndex);
          U.append(Uvi.x());
          V.append(Uvi.y());
          CurrentPoint++;
        }

        ReferenceIndices.append(CurrentPoint);

        FaceIndices.append(FaceIndex);
        U.append(Uv1.x());
        V.append(Uv1.y());
        CurrentPoint++;

        // append point block of inEdgePointsCount + 2 points
        // create polyline out of them
        // set color attr to points remapped from creasevalue
        int NumberOfPoints = InEdgePointsCount * 2 + 2;
        // int num_points = lineCount;
        Positions.appendMultiple(CdValue, NumberOfPoints);
        Colors.appendMultiple(CdValue, NumberOfPoints);
      }
    }
  }

  if (OverallCreaseValue < 0.01f)
    HasCrease = false;
}

void IsolineMaker::createGeometry(GU_Detail *TargetGdp) {
  TargetGdp->addFloatTuple(GA_ATTRIB_POINT, GA_SCOPE_PUBLIC, "Cd", 3);
  // MAGIC
  int PointsPerPolyline = (int(pow(2, SubdivisionLevel)) - 1) * 2 + 2;
  int PolylineCount = Positions.size() / PointsPerPolyline;

  for (int x = 0; x < PolylineCount; x++) {
    GA_Offset appendOffset = TargetGdp->appendPointBlock(PointsPerPolyline);

    GA_Attribute *Cd = TargetGdp->findPointAttribute("Cd");
    const GA_AIFTuple *tuple = Cd->getAIFTuple();

    for (exint PointId = 0; PointId < PointsPerPolyline; ++PointId) {
      int Index = x * PointsPerPolyline + PointId;

      GA_Offset ptoff = appendOffset + PointId;
      UT_Vector3 CdValue = Colors[Index];
      tuple->set(Cd, ptoff, CdValue.data(), 3);
      TargetGdp->setPos3(ptoff, Positions[Index]);
    }

    GEO_PrimPoly *PrimPolyPtr =
        (GEO_PrimPoly *)TargetGdp->appendPrimitive(GA_PRIMPOLY);
    PrimPolyPtr->setSize(0);
    for (exint PointId = 0; PointId < PointsPerPolyline; ++PointId)
      PrimPolyPtr->appendVertex(appendOffset + PointId);
  }
}

void IsolineMaker::getAttributeArrays(UT_Vector3FArray &OutPositions,
                                      UT_Vector3FArray &OutColors,
                                      UT_Vector3FArray &OutNormals) {
  OutPositions.clear();
  OutColors.clear();
  OutNormals.clear();

  OutPositions = Positions;
  OutColors = Colors;
  OutNormals = Normals;
}

bool IsolineMaker::isValidGeo() {
  const GA_AttributeOwner SearchOrder[4] = {
      GA_ATTRIB_VERTEX, GA_ATTRIB_POINT, GA_ATTRIB_PRIMITIVE, GA_ATTRIB_GLOBAL};
  const GA_Attribute *NormalAttribute =
      gdp()->findAttribute("N", SearchOrder, 4);
  const GA_ROHandleF NormalRoHandle(NormalAttribute);

  if (NormalRoHandle.isValid())
    return false;

  for (GA_Iterator it(gdp()->getPrimitiveRange()); !it.atEnd(); ++it) {
    auto PrimType = GA_PRIMPOLY;
    const GA_Size MinVertexCount = 3;

    const GA_Offset primitiveOffset = *it;
    const GA_Primitive *CurrentPrimitive = gdp()->getPrimitive(primitiveOffset);

    // Skip unusual prim types like volumes
    if (CurrentPrimitive->getTypeId() != PrimType)
      return false;

    // Won't work with polylines
    if (CurrentPrimitive->getVertexCount() < MinVertexCount)
      return false;
  }
  return true;
}

const GU_Detail *IsolineMaker::gdp() { return GdpHandle.gdp(); }

float IsolineMaker::getPeakProportional() {
  UT_BoundingBox Bbox;
  gdp()->getBBox(&Bbox);
  return Peak * cbrt(Bbox.volume());
}
