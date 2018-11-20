#include <GT/GT_DataArray.h>
#include <SOP/SOP_Node.h>

class SOP_Isolines : public SOP_Node {
public:
  static OP_Node *myConstructor(OP_Network *, const char *, OP_Operator *);

protected:
  SOP_Isolines(OP_Network *net, const char *name, OP_Operator *op);
  virtual ~SOP_Isolines();
  virtual OP_ERROR cookMySop(OP_Context &Context);
};
