
#include "SOP_Isolines.h"

#include <OP/OP_AutoLockInputs.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_Include.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Interrupt.h>

#include <GU/GU_DetailHandle.h>
#include <PRM/PRM_TemplateBuilder.h>
#include <SYS/SYS_Math.h>

#include "GeometryUtilities.h"
#include "IsolineMaker.h"

const char *DsFile = R"THEDSFILE(
{
  name parameters
  parm {
    name "subdlevel"
    label "Subdivision Level"
    type integer
    default { "3" }
    range { 1! 7 }
  }
  parm {
    name "peak"
    label "Peak"
    type float
    default { "0.005" }
    range { "0.0001"! "0.1" }
  }
}
)THEDSFILE";

PRM_Template *getTemplate() {
  return PRM_TemplateBuilder("SOP_Isolines.C", DsFile).templates();
}

void newSopOperator(OP_OperatorTable *table) {
  table->addOperator(new OP_Operator("cb_isolines", "Isolines",
                                     SOP_Isolines::myConstructor, getTemplate(),
                                     1, 1));
}

OP_Node *SOP_Isolines::myConstructor(OP_Network *net, const char *name,
                                     OP_Operator *op) {
  return new SOP_Isolines(net, name, op);
}

SOP_Isolines::SOP_Isolines(OP_Network *net, const char *name,
                           OP_Operator *entry)
    : SOP_Node(net, name, entry) {
  mySopFlags.setManagesDataIDs(true);
}

SOP_Isolines::~SOP_Isolines() {}

OP_ERROR
SOP_Isolines::cookMySop(OP_Context &Context) {

  OP_AutoLockInputs inputs(this);

  if (inputs.lock(Context) >= UT_ERROR_ABORT)
    return error();

  GU_Detail *TargetGdp = gdp;

  if (error() < UT_ERROR_ABORT) {
    UT_AutoInterrupt Progress("Drawing Isolines");
    GU_DetailHandle InputGdpHandle = inputGeoHandle(0);

    fpreal Now = Context.getTime();
    IsolineMaker IsoMaker(InputGdpHandle, evalFloat("peak", 0, Now),
                          evalInt("subdlevel", 0, Now));

    if (IsoMaker.calculateAttributeArrays()) {
      TargetGdp->clearAndDestroy();
      IsoMaker.createGeometry(TargetGdp);
    }
  }

  resetLocalVarRefs();
  return error();
}
