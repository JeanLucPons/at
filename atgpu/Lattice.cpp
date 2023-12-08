#include "Lattice.h"
#include "AbstractGPU.h"
#include "PassMethodFactory.h"

using namespace std;

// Get the header file
// Generated by setup.py
const string header = {
#include "element.gpuh"
};

Lattice::Lattice(size_t nElements) {
  // Reserve memory
  elements.reserve(nElements);
  PassMethodFactory::getInstance()->reset();
}

void Lattice::addElement() {

  PassMethodFactory *factory = PassMethodFactory::getInstance();
  string passMethod = AbstractInterface::getInstance()->getString("PassMethod");
  AbstractElement *elem = factory->createElement(passMethod);
  elements.push_back(elem);

}


void Lattice::generateGPUKernel(std::string& code) {

  code.append(header);
  AbstractGPU::getInstance()->addMathFunctions(code);
  PassMethodFactory::getInstance()->generatePassMethods(code);

  // GPU entering method
  code.append("__global__ void track(ELEMENT* gpuRing,uint32_t nbElement,AT_FLOAT* rin,AT_FLOAT* rout,"
              "                      uint32_t* lost,uint32_t turn,uint32_t nbPart,int32_t takeTurn) {\n");
  code.append("  int threadId = blockIdx.x * blockDim.x + threadIdx.x;\n");
  code.append("  AT_FLOAT* _r6 = rin + (6 * threadId);\n");
  code.append("  AT_FLOAT* _rout = rout + ((uint64_t)turn * (uint64_t)nbPart * 2) + (uint64_t)(2 * threadId);\n");

  // Copy particle coordinates into fast shared mem
  code.append("  __shared__ AT_FLOAT sr6[GPU_GRID * 6];\n");
  code.append("  sr6[0 + threadIdx.x * 6] = _r6[0];\n"); // x
  code.append("  sr6[1 + threadIdx.x * 6] = _r6[1];\n"); // px/p0 = x'(1+d)
  code.append("  sr6[2 + threadIdx.x * 6] = _r6[2];\n"); // y
  code.append("  sr6[3 + threadIdx.x * 6] = _r6[3];\n"); // py/p0 = y'(1+d)
  code.append("  sr6[4 + threadIdx.x * 6] = _r6[4];\n"); // d = (pz-p0)/p0
  code.append("  sr6[5 + threadIdx.x * 6] = _r6[5];\n"); // c.tau (time lag)
  code.append("  AT_FLOAT* r6 = &sr6[0 + threadIdx.x * 6];\n");

  // Loop over turns
  code.append("  ELEMENT* elemPtr = gpuRing;\n");
  code.append("  for(uint32_t elem = 0; elem < nbElement; elem++) {\n");
  code.append("    switch(elemPtr->Type) {\n");
  PassMethodFactory::getInstance()->generatePassMethodsCalls(code);
  code.append("    }\n");
  code.append("    elemPtr++;\n");
  code.append("  }\n");

  // Particle lost check
  code.append("  bool pLost = !isfinite(_r6[0]) ||\n");
  code.append("               !isfinite(_r6[1]) ||\n");
  code.append("               !isfinite(_r6[2]) ||\n");
  code.append("               !isfinite(_r6[3]) ||\n");
  code.append("               !isfinite(_r6[4]) ||\n");
  code.append("               !isfinite(_r6[5]) ||\n");
  code.append("               (fabs(_r6[0]) > 1.0 || fabs(_r6[1]) > 1.0) ||\n");
  code.append("               (fabs(_r6[2]) > 1.0 || fabs(_r6[3]) > 1.0);\n");

  code.append("  if(!lost[threadId] & pLost) {\n");
  code.append("    _r6[0] = NAN;\n");
  code.append("    _r6[1] = 0;\n");
  code.append("    _r6[2] = 0;\n");
  code.append("    _r6[3] = 0;\n");
  code.append("    _r6[4] = 0;\n");
  code.append("    _r6[5] = 0;\n");
  code.append("    lost[threadId] = turn + 1;\n");
  code.append("  }\n");

  code.append("}\n");

}