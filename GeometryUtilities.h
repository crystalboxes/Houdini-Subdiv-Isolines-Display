#pragma once
#include <GU/GU_Detail.h>

namespace {
namespace GeometryUtilities {
#define _EPS 0.00001f
float almostEqual(float A, float B) { return fabs(A - B) < _EPS; }

void doPeak(GU_Detail *Gdp, float Peak) {
  Gdp->normal();
  GA_RWAttributeRef NormalAttribRef =
      Gdp->findFloatTuple(GA_ATTRIB_POINT, "N", 3);
  GA_Attribute *Attrib = NormalAttribRef.getAttribute();
  const GA_AIFTuple *Tuple = Attrib->getAIFTuple();

  for (GA_Iterator It(Gdp->getPointRange()); !It.atEnd(); ++It) {
    GA_Offset Offset = *It;
    UT_Vector3 N;
    Tuple->get(Attrib, Offset, N.data(), 3);
    N.normalize();
    Gdp->setPos3(Offset, Gdp->getPos3(Offset) + N * Peak);
  }
  Gdp->destroyAttribute(GA_ATTRIB_POINT, "N");
}

void pointPrims(const GU_Detail *Gdp, GA_OffsetArray &PointPrims,
                GA_Offset PointOffset) {
  UT_Array<GA_Offset> PointPrimitives;
  GA_OffsetArray Vertices;
  Gdp->getVerticesReferencingPoint(Vertices, PointOffset);
  for (int VertexNumber = 0; VertexNumber < Vertices.entries();
       VertexNumber++) {
    GA_Offset PrimitiveOffset = Gdp->vertexPrimitive(Vertices[VertexNumber]);
    PointPrimitives.append(PrimitiveOffset);
  }
  PointPrims = GA_OffsetArray(PointPrimitives);
}

void primPoints(const GU_Detail *gdp, GA_OffsetArray &PrimPoints,
                GA_Offset PrimitiveOffset) {
  UT_Array<GA_Offset> PointArray;
  GA_OffsetListRef vertexList = gdp->getPrimitiveVertexList(PrimitiveOffset);
  for (int VertexNumber = 0;
       VertexNumber < gdp->getPrimitiveVertexCount(PrimitiveOffset);
       VertexNumber++) {
    GA_Offset VertexOffset = vertexList.get(VertexNumber);
    GA_Offset PointOffset = gdp->vertexPoint(VertexOffset);
    PointArray.append(PointOffset);
  }
  PrimPoints = GA_OffsetArray(PointArray);
}

void adjacentPrimitivesToEdge(const GU_Detail *Gdp, GA_OffsetArray &Adjacent,
                              GA_Offset EdgePoint0, GA_Offset EdgePoint1) {
  UT_Array<GA_Offset> AdjacentArray;

  GA_OffsetArray PointPrimitives;
  pointPrims(Gdp, PointPrimitives, EdgePoint0);

  for (int PrimitiveNumber = 0; PrimitiveNumber < PointPrimitives.entries();
       PrimitiveNumber++) {

    GA_Offset PrimitiveOffset = PointPrimitives[PrimitiveNumber];
    GA_OffsetArray PrimitivePoints;
    primPoints(Gdp, PrimitivePoints, PrimitiveOffset);

    if (PrimitivePoints.find(EdgePoint1) != -1) {
      AdjacentArray.append(PrimitiveOffset);
    }
  }
  Adjacent = GA_OffsetArray(AdjacentArray);
}

int vertexPrimNumber(const GU_Detail *Gdp, GA_Offset Vertex,
                     GA_Offset PrimitiveOffset) {
  return Gdp->getPrimitiveVertexList(PrimitiveOffset).find(Vertex);
}

void orderedVertexOffsetsForPrim(const GU_Detail *Gdp, GA_OffsetArray &Vertices,
                                 GA_Offset PrimitiveOffset,
                                 GA_Offset EdgePoint0, GA_Offset EdgePoint1) {
  UT_Array<GA_Offset> VertexOffsets;
  GA_Offset Vtx0 = Gdp->getVertexReferencingPoint(EdgePoint0, PrimitiveOffset);
  GA_Offset Vtx1 = Gdp->getVertexReferencingPoint(EdgePoint1, PrimitiveOffset);

  VertexOffsets.append(Vtx0);
  VertexOffsets.append(Vtx1);
  int VertexPrimIndex0 =
      vertexPrimNumber(Gdp, VertexOffsets[0], PrimitiveOffset);
  int VertexPrimIndex1 =
      vertexPrimNumber(Gdp, VertexOffsets[1], PrimitiveOffset);

  if (VertexPrimIndex0 > VertexPrimIndex1) {
    if ((VertexPrimIndex0 - VertexPrimIndex1) == 1) {
      VertexOffsets.reverse();
      VertexPrimIndex0 =
          vertexPrimNumber(Gdp, VertexOffsets[0], PrimitiveOffset);
      VertexPrimIndex1 =
          vertexPrimNumber(Gdp, VertexOffsets[1], PrimitiveOffset);
    }
  } else if ((VertexPrimIndex1 - VertexPrimIndex0) > 1) {
    VertexOffsets.reverse();
    VertexPrimIndex0 = vertexPrimNumber(Gdp, VertexOffsets[0], PrimitiveOffset);
    VertexPrimIndex1 = vertexPrimNumber(Gdp, VertexOffsets[1], PrimitiveOffset);
  }
  Vertices = GA_OffsetArray(VertexOffsets);
}

float getCreaseValue(const GU_Detail *Gdp, GA_Offset PrimitiveOffset,
                     GA_Offset AdjacentOffset, GA_Offset EdgePoint0,
                     GA_Offset EdgePoint1) {

  float CreaseValue = 0.0f;
  const GA_Attribute *CreaseAttr = Gdp->findVertexAttribute("creaseweight");

  GA_OffsetArray VertexOffsets0, VertexOffsets1;
  orderedVertexOffsetsForPrim(Gdp, VertexOffsets0, PrimitiveOffset, EdgePoint0,
                              EdgePoint1);
  orderedVertexOffsetsForPrim(Gdp, VertexOffsets1, AdjacentOffset, EdgePoint0,
                              EdgePoint1);

  const GA_AIFTuple *Tuple = CreaseAttr->getAIFTuple();
  fpreal32 Weight0;
  fpreal32 Weight1;
  Tuple->get(CreaseAttr, VertexOffsets0[0], Weight0);
  Tuple->get(CreaseAttr, VertexOffsets1[0], Weight1);

  if (almostEqual(Weight0, Weight1))
    return Weight0;

  return CreaseValue;
}

const UT_Vector3 OsdTriCoords[3] = {
    UT_Vector3(0.0f, 0.0f, 0.0f),
    UT_Vector3(1.0f, 0.0f, 0.0f),
    UT_Vector3(0.0f, 1.0f, 0.0f),
};

const UT_Vector3 OsdQuadCoords[4] = {
    UT_Vector3(0.0f, 0.0f, 0.0f), UT_Vector3(0.0f, 1.0f, 0.0f),
    UT_Vector3(1.0f, 1.0f, 0.0f), UT_Vector3(1.0f, 0.0f, 0.0f)};

void getOsdParametricValues(const GU_Detail *Gdp, GA_Offset PrimitiveOffset,
                            GA_Offset EdgePointOffset0,
                            GA_Offset EdgePointOffset1, UT_Vector3 &Uv0,
                            UT_Vector3 &Uv1) {
  const int PrimitivePointCount = Gdp->getPrimitiveVertexCount(PrimitiveOffset);

  const int VertexNumber0 = vertexPrimNumber(
      Gdp, Gdp->getVertexReferencingPoint(EdgePointOffset0, PrimitiveOffset),
      PrimitiveOffset);
  const int VertexNumber1 = vertexPrimNumber(
      Gdp, Gdp->getVertexReferencingPoint(EdgePointOffset1, PrimitiveOffset),
      PrimitiveOffset);

  if (PrimitivePointCount == 3) {
    Uv0 = OsdTriCoords[VertexNumber0];
    Uv1 = OsdTriCoords[VertexNumber1];
    return;
  } else if (PrimitivePointCount == 4) {
    Uv0 = OsdQuadCoords[VertexNumber0];
    Uv1 = OsdQuadCoords[VertexNumber1];
    return;
  } else if (PrimitivePointCount > 4) {
    if (VertexNumber0 == 0 || VertexNumber1 == 0) {
      int x0 = VertexNumber0 < VertexNumber1 ? VertexNumber0 : VertexNumber1;
      int x1 = VertexNumber0 > VertexNumber1 ? VertexNumber0 : VertexNumber1;
      if (x1 != 1) {
        Uv0 = UT_Vector3(1.0f, 0.0f, 0.0f);
        Uv1 = UT_Vector3(x1 / (float)PrimitivePointCount, _EPS, 0.0f);
        return;
      }
    }
    Uv0 = UT_Vector3(VertexNumber0 / (float)PrimitivePointCount, _EPS, 0.0f);
    Uv1 = UT_Vector3(VertexNumber1 / (float)PrimitivePointCount, _EPS, 0.0f);
    return;
  }
  Uv0 = Uv1 = UT_Vector3(0.0f, 0.0f, 0.0f);
  return;
}
} // namespace GeometryUtilities
} // namespace
