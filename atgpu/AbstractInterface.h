#ifndef AT_GPU_ABSTRACTINTERFACE_H
#define AT_GPU_ABSTRACTINTERFACE_H
#include <string>
#include <vector>
#include "Lattice.h"

// Abstract interface class
// Abstract Python or Matlab layer
class AbstractInterface {

public:

  virtual std::string getString(const std::string& name) = 0;

  virtual AT_FLOAT getDouble(const std::string& name) = 0;
  virtual AT_FLOAT getOptionalDouble(const std::string& name,AT_FLOAT defaultValue) = 0;

  virtual AT_FLOAT *getDoubleArray(const std::string& name,std::vector<int64_t> expectedShape) = 0;
  virtual AT_FLOAT *getOptionalDoubleArray(const std::string& name,std::vector<int64_t> expectedShape) = 0;

  // Return handle to singleton class
  static AbstractInterface *getInstance();
  static void setHandler(AbstractInterface *obj);

private:
  static AbstractInterface *handler;

};


#endif //AT_GPU_ABSTRACTINTERFACE_H
