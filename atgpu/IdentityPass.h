#ifndef AT_GPU_IDENTITYPASS_H
#define AT_GPU_IDENTITYPASS_H
#include "AbstractElement.h"

class IdentityPass: public AbstractElement {

public:

  // Construct an identity pass
  IdentityPass() noexcept;
  ~IdentityPass() noexcept override;

  // Retrieve parameters from upper layer (Python, Matlab)
  void getParameters(AbstractInterface *param, PASSMETHOD_INFO *info) override;
  uint64_t getMemorySize() override;
  void fillGPUMemory(GPUContext *gpu,void *elemMem,void *privateMem) override;

  // GPU code generation
  static void generateGPUKernel(std::string& code, PASSMETHOD_INFO *info) noexcept;
  static void generateEnter(std::string& code, PASSMETHOD_INFO *info) noexcept;
  static void generateExit(std::string& code, PASSMETHOD_INFO *info) noexcept;
  static void generateApertures(std::string& code, PASSMETHOD_INFO *info) noexcept;

  static void generateEAperture(std::string& code) noexcept;
  static void generateRAperture(std::string& code) noexcept;
  static void generateR(std::string& code,const std::string& pname) noexcept;
  static void generateT(std::string& code,const std::string& pname) noexcept;
  static void generateCall(std::string& code) noexcept;

protected:

  ELEMENT elemData;

private:

  AT_FLOAT *R1;    // Enter 6x6 transformation matrix
  AT_FLOAT *R2;    // Exit 6x6 transformation matrix
  AT_FLOAT *T1;    // Enter 6D vector translation
  AT_FLOAT *T2;    // Exit 6D vector translation
  AT_FLOAT *EApertures; // Elliptical transverse aperture check (xradius,yradius)
  AT_FLOAT *RApertures; // Rectangular transverse aperture check (xmin,xmax,ymin,ymax)

};


#endif //AT_GPU_IDENTITYPASS_H
