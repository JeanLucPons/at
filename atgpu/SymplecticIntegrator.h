#ifndef AT_GPU_SYMPLECTICINTEGRATOR_H
#define AT_GPU_SYMPLECTICINTEGRATOR_H
#include <string>
#include <vector>
#include "Element.h"

class SymplecticIntegrator {

public:

  // Construct a symplectic integrator of the given type
  SymplecticIntegrator(int type);
  ~SymplecticIntegrator();

  // Coefficients
  int nbCoefficients;
  AT_FLOAT *c; // Drift
  AT_FLOAT *d; // Kick

  // Generate integrator code
  void generateCode(std::string& code);

  // Kick/Drift method for all subtype
  void resetMethods();
  void addDriftMethod(const std::string& driftMethod);
  void addKickMethod(const std::string& kickMethod);

private:

  void allocate(int nb);
  void generateLoopCode(std::string& code,size_t subType);
  bool replace(std::string& str, const std::string& from, const std::string& to);
  std::string formatFloat(AT_FLOAT *f);

  std::vector<std::string> driftMethods;
  std::vector<std::string> kickMethods;

};

#endif //AT_GPU_SYMPLECTICINTEGRATOR_H
