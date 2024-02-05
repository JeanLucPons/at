#include "Lattice.h"
#include <iostream>
#include <cstring>
//#define _PROFILE

using namespace std;

// Get the shared ELEMENT header file
// Generated by setup.py
const string header = {
#include "element.gpuh"
};

Lattice::Lattice(int32_t nbElements,SymplecticIntegrator& integrator,double energy,int gpuId) : factory(PassMethodFactory(integrator)) {
  elements.reserve(nbElements);
  gpuRing = nullptr;
  memset(&ringParams,0,sizeof(ringParams));
  ringParams.Energy = energy;
  gpu = AbstractGPU::getInstance()->createContext(gpuId);
}

Lattice::~Lattice() {
  for(auto & element : elements)
    delete element;
  if(gpuRing) gpu->freeDevice(gpuRing);
  delete gpu;
}

// Resize lattice (debugging purpose)
void Lattice::resize(int32_t nbElements) {
  for(size_t i=nbElements;i<elements.size();i++)
    delete elements[i];
  elements.resize(nbElements);
}

void Lattice::addElement() {

  try {

    string passMethod = AbstractInterface::getInstance()->getString("PassMethod");
    AbstractElement *elem = factory.createElement(passMethod);
    elements.push_back(elem);

  } catch (string& err) {
    // Try to retrieve name of element (if any)
    string idxStr = "#" + to_string(elements.size()) + " (from #0)";
    string name = "";
    try {
      name = " (" + AbstractInterface::getInstance()->getString("Name") + ")";
    } catch (string&) {}
    string errStr = err + " in element " + idxStr + name;
    throw errStr;
  }

}

AT_FLOAT Lattice::getLength() {
  AT_FLOAT L = 0.0;
  for(auto & element : elements)
    L += element->getLength();
  return L;
}

void Lattice::setTurnCounter(uint64_t count) {
  ringParams.turnCounter = count;
}

uint32_t Lattice::getNbElement() {
  return (uint32_t)elements.size();
}

GPUContext *Lattice::getGPUContext() {
  return gpu;
}

void Lattice::mapBuffers(std::string &code) {

  string kType;
  string mType;
  AbstractGPU::getInstance()->getKernelFunctionQualifier(kType);
  AbstractGPU::getInstance()->getGlobalQualifier(mType);

  // Map buffer address to device space (see fillGPUMemory)
  code.append(kType + "void mapbuffer(" + mType + "ELEMENT* gpuRing,uint32_t nbTotalElement) {\n");
  code.append("  for(int elem=0; elem < nbTotalElement; elem++) {\n");
  code.append("    MAP_FLOAT_BUFFER(gpuRing[elem].T1,gpuRing);\n");
  code.append("    MAP_FLOAT_BUFFER(gpuRing[elem].T2,gpuRing);\n");
  code.append("    MAP_FLOAT_BUFFER(gpuRing[elem].R1,gpuRing);\n");
  code.append("    MAP_FLOAT_BUFFER(gpuRing[elem].R2,gpuRing);\n");
  code.append("    MAP_FLOAT_BUFFER(gpuRing[elem].EApertures,gpuRing);\n");
  code.append("    MAP_FLOAT_BUFFER(gpuRing[elem].RApertures,gpuRing);\n");
  code.append("    MAP_FLOAT_BUFFER(gpuRing[elem].mpole.PolynomA,gpuRing);\n");
  code.append("    MAP_FLOAT_BUFFER(gpuRing[elem].mpole.PolynomB,gpuRing);\n");
  code.append("  }\n");
  code.append("}\n");

}

void Lattice::checkLostParticle(std::string &code) {

  // Check if particle is lost
  if( sizeof(AT_FLOAT)==8 ) {
    code.append("    pLost = !isfinite(r6[4]) || !isfinite(r6[5]) ||\n");
    code.append("            (fabs(r6[0]) > 1.0 || fabs(r6[1]) > 1.0) ||\n");
    code.append("            (fabs(r6[2]) > 1.0 || fabs(r6[3]) > 1.0);\n");
  } else {
    code.append("    pLost = !isfinite(r6[4]) || !isfinite(r6[5]) ||\n");
    code.append("            (fabsf(r6[0]) > 1.0f || fabsf(r6[1]) > 1.0f) ||\n");
    code.append("            (fabsf(r6[2]) > 1.0f || fabsf(r6[3]) > 1.0f);\n");
  }

  code.append("    if(!lost[threadId] && pLost) {\n");
  // Store in which element particle is lost
  code.append("      if( lostAtElem ) {\n");
  code.append("        lostAtElem[threadId] = elem;\n");
  code.append("      }\n");
  // Store where particle is lost
  code.append("      if( lostAtCoord ) {\n");
  code.append("        lostAtCoord[0 + 6*threadId] = r6[0];\n");
  code.append("        lostAtCoord[1 + 6*threadId] = r6[1];\n");
  code.append("        lostAtCoord[2 + 6*threadId] = r6[2];\n");
  code.append("        lostAtCoord[3 + 6*threadId] = r6[3];\n");
  code.append("        lostAtCoord[4 + 6*threadId] = r6[4];\n");
  code.append("        lostAtCoord[5 + 6*threadId] = r6[5];\n");
  code.append("      }\n");
  code.append("      lost[threadId] = turn + 1;\n");
  // Mark as lost
  code.append("      r6[0] = NAN;\n");
  code.append("      r6[1] = 0;\n");
  code.append("      r6[2] = 0;\n");
  code.append("      r6[3] = 0;\n");
  code.append("      r6[4] = 0;\n");
  code.append("      r6[5] = 0;\n");
  code.append("    }\n");

}

void Lattice::storeParticleCoord(std::string &code) {

  // Store particle coordinates in rout
  code.append("    refIdx = refpts[elem];\n");
  code.append("    if(refIdx>=0) {\n");
  code.append("      _rout[6 * (refIdx*nbPart) +  0] = r6[0];\n");
  code.append("      _rout[6 * (refIdx*nbPart) +  1] = r6[1];\n");
  code.append("      _rout[6 * (refIdx*nbPart) +  2] = r6[2];\n");
  code.append("      _rout[6 * (refIdx*nbPart) +  3] = r6[3];\n");
  code.append("      _rout[6 * (refIdx*nbPart) +  4] = r6[4];\n");
  code.append("      _rout[6 * (refIdx*nbPart) +  5] = r6[5];\n");
  code.append("    }\n");

}

void Lattice::generateGPUKernel() {

#ifdef _PROFILE
  double t0,t1;
  t0=AbstractGPU::get_ticks();
#endif

  // Perform post element initialisation
  ringParams.Length = getLength();
  for(auto & element : elements)
    element->postInit(&ringParams);

  // Generate code
  string kType;
  string mType;
  string threadIdCmd;
  AbstractGPU::getInstance()->getKernelFunctionQualifier(kType);
  AbstractGPU::getInstance()->getGlobalQualifier(mType);
  AbstractGPU::getInstance()->getThreadId(threadIdCmd);
  std::string code;

  AbstractGPU::getInstance()->addUtilsFunctions(code);
  code.append(header);
  factory.generateUtilsFunctions(code);
  factory.generatePassMethods(code);
  mapBuffers(code);

  code.append("#define NB_TOTAL_ELEMENT " + to_string(getNbElement()) + "\n");

  // GPU main track function
  code.append(kType + "void track(" + mType + "RING_PARAM *ringParam," + mType + "ELEMENT* gpuRing,\n"
              "                   uint32_t startElem,uint32_t nbElementToProcess,\n"
              "                   " + mType + "int32_t *elemOffsets, uint32_t offsetStride,\n"
              "                   uint32_t nbPart," + mType + "AT_FLOAT* rin," + mType + "AT_FLOAT* rout,\n"
              "                   " + mType + "uint32_t* lost, uint32_t turn,\n"
              "                   " + mType + "int32_t *refpts, uint32_t nbRef,\n"
              "                   " + mType + "uint32_t *lostAtElem," + mType + "AT_FLOAT *lostAtCoord\n"
              "                   ) {\n");

  code.append(threadIdCmd);
  // Exit immediately if no particle to handle
  // Done for performance to have a constant group size multiple of particle number
  code.append("  if(threadId>=nbPart) return;\n");
  //code.append("  printf(\"Entering kernel %d part=%d turn=%d\\n\",threadId,nbPart,turn);\n");
  code.append("  AT_FLOAT sr6[6];\n");
  code.append("  AT_FLOAT* r6 = sr6;\n");
  code.append("  "+mType+" AT_FLOAT* _rout = rout + 6 * ((uint64_t)turn * (uint64_t)nbPart * (uint64_t)nbRef + (uint64_t)(threadId));\n");
  code.append("  AT_FLOAT fTurn = (AT_FLOAT)(ringParam->turnCounter + turn);\n");
  code.append("  bool pLost = false;\n");
  code.append("  int32_t refIdx = 0;\n");
  code.append("  int elem = startElem;\n");

  // Copy particle coordinates into registers
  code.append("  sr6[0] = rin[0 + 6*threadId];\n"); // x
  code.append("  sr6[1] = rin[1 + 6*threadId];\n"); // px/p0 = x'(1+d)
  code.append("  sr6[2] = rin[2 + 6*threadId];\n"); // y
  code.append("  sr6[3] = rin[3 + 6*threadId];\n"); // py/p0 = y'(1+d)
  code.append("  sr6[4] = rin[4 + 6*threadId];\n"); // d = (pz-p0)/p0
  code.append("  sr6[5] = rin[5 + 6*threadId];\n"); // c.tau (time lag)

  // Exit if particle lost and fill rout
  code.append("  if(nbRef>0 && lost[threadId]) {\n");
  code.append("    while(nbRef > 0 && elem <= startElem+nbElementToProcess) {\n"); // don't forget last ref
  storeParticleCoord(code);
  code.append("      elem++;\n");
  code.append("    }\n");
  code.append("    return;\n");
  code.append("  }\n");

  // The tracking starts at elem elemOffset (equivalent to lattice.rotate(elemOffset))
  code.append("  int32_t elemOffset = elemOffsets?elemOffsets[threadId/offsetStride]:0;\n");

  // Loop over elements
  code.append("  while(!pLost && elem<startElem+nbElementToProcess) {\n");
  code.append("    "+mType+" ELEMENT* elemPtr = gpuRing + ((elem+elemOffset)%NB_TOTAL_ELEMENT);\n");
  storeParticleCoord(code);
  code.append("    switch(elemPtr->Type) {\n");
  factory.generatePassMethodsCalls(code);
  code.append("    }\n");
  checkLostParticle(code);
  //code.append("    printf(\"%d: %f %f %f %f %f %f\\n\",elem,r6[0],r6[1],r6[2],r6[3],r6[4],r6[5]);\n");
  code.append("    elem++;\n");
  code.append("  }\n");

  // Last ref after the last element
  code.append("  if( pLost ) elem=startElem+nbElementToProcess;\n");
  code.append("  if( elem==NB_TOTAL_ELEMENT ) {\n");
  storeParticleCoord(code);
  code.append("  }\n");

  // Copy back particle coordinates to global mem
  code.append("  rin[0 + 6*threadId] = sr6[0];\n");
  code.append("  rin[1 + 6*threadId] = sr6[1];\n");
  code.append("  rin[2 + 6*threadId] = sr6[2];\n");
  code.append("  rin[3 + 6*threadId] = sr6[3];\n");
  code.append("  rin[4 + 6*threadId] = sr6[4];\n");
  code.append("  rin[5 + 6*threadId] = sr6[5];\n");
  code.append("}\n");

#ifdef _PROFILE
  t1=AbstractGPU::get_ticks();
  cout << "Code generation: " << (t1-t0)*1000.0 << "ms" << endl;
  t0=AbstractGPU::get_ticks();
#endif

  gpu->compile(code);

#ifdef _PROFILE
  t1=AbstractGPU::get_ticks();
  cout << "Code compilation: " << (t1-t0)*1000.0 << "ms" << endl;
#endif

}

void Lattice::fillGPUMemory() {

  // Size of all element (no buffers)
  uint64_t elemSize = elements.size() * sizeof( ELEMENT );

  // Buffers size
  uint64_t privSize = 0;
  for(auto & element : elements)
    privSize += element->getMemorySize();

  // Total memory size including buffers
  uint64_t size = elemSize;
  size += privSize;

  // Allocate GPU Memory
  if(!gpuRing) gpu->allocDevice(&gpuRing, size);

  // Create and fill memory cache
  ELEMENT *memPtr = (ELEMENT *)malloc(size);
  ELEMENT *ptr = memPtr;
  uint64_t memOffset = elemSize;
  for(auto & element : elements)
    element->fillGPUMemory((uint8_t *)memPtr,ptr++, &memOffset);

  // Update GPU memory and map buffers
  gpu->hostToDevice(gpuRing,memPtr,size);
  gpu->mapBuffer(&gpuRing,(uint32_t)elements.size());
  free(memPtr);

}

void Lattice::run(uint64_t nbTurn,uint32_t nbParticles,AT_FLOAT *rin,AT_FLOAT *rout,uint32_t nbRef,
                  uint32_t *refPts,uint32_t nbElemOffset,uint32_t *elemOffsets,
                  uint32_t *lostAtTurn,uint32_t *lostAtElem,AT_FLOAT *lostAtCoord,
                  bool updateRin) {

#ifdef _PROFILE
  double t0 = AbstractGPU::get_ticks();
#endif

  // Copy rin to gpu mem
  void *gpuRin;
  gpu->allocDevice(&gpuRin, nbParticles * 6 * sizeof(AT_FLOAT));
  gpu->hostToDevice(gpuRin, rin, nbParticles * 6 * sizeof(AT_FLOAT));

  // Expand ref indexes
  int32_t *expandedRefPts = new int32_t[elements.size()+1];
  for(size_t i=0;i<=elements.size();i++)
    expandedRefPts[i] = -1;
  for(size_t i=0;i<nbRef;i++)
    expandedRefPts[refPts[i]] = (int32_t)i;
  void *gpuRefs;
  gpu->allocDevice(&gpuRefs, (elements.size() + 1) * sizeof(int32_t));
  gpu->hostToDevice(gpuRefs, expandedRefPts, (elements.size() + 1) * sizeof(int32_t));

  // Lost flags
  uint32_t lostSize = nbParticles * sizeof(uint32_t);
  uint32_t *lost = new uint32_t[nbParticles];
  for(uint32_t i=0;i<nbParticles;i++) lost[i] = 0;
  for(uint32_t i=nbParticles;i<nbParticles;i++) lost[i] = 1;
  void *gpuLost;
  gpu->allocDevice(&gpuLost, lostSize);
  gpu->hostToDevice(gpuLost, lost, lostSize);

  // rout
  void *gpuRout = nullptr;
  uint64_t routSize = nbParticles * nbRef * nbTurn * 6 * sizeof(AT_FLOAT);
  if( routSize ) gpu->allocDevice(&gpuRout, routSize);

  // Global ring param
  void *gpuRingParams;
  gpu->allocDevice(&gpuRingParams, sizeof(ringParams));
  gpu->hostToDevice(gpuRingParams,&ringParams,sizeof(ringParams));

  // Lost infos
  void *gpuLostAtElem = nullptr;
  void *gpuLostAtCoord = nullptr;
  if(lostAtElem) gpu->allocDevice(&gpuLostAtElem, nbParticles * sizeof(uint32_t));
  if(lostAtCoord) gpu->allocDevice(&gpuLostAtCoord, nbParticles * 6 * sizeof(AT_FLOAT));

  // Starting element (lattice rotation)
  void *gpuOffsets = nullptr;
  uint32_t offsetStride = 0;
  if(nbElemOffset) {
    gpu->allocDevice(&gpuOffsets, nbElemOffset * sizeof(int32_t));
    gpu->hostToDevice(gpuOffsets, elemOffsets, nbElemOffset * sizeof(int32_t));
    // nbParticles % nbElemOffset must be 0
    // offsetStride (nbParticles/nbElemOffset) should be a multiple of 64
    offsetStride = nbParticles / nbElemOffset;
  }

  // Call GPU
  gpu->resetArg();
  uint32_t startElem;
  uint32_t nbElemToProcess = (uint32_t)elements.size();
  uint32_t turn;
  gpu->addArg(sizeof(void *),&gpuRingParams);       // Global ring params
  gpu->addArg(sizeof(void *),&gpuRing);             // The lattice
  gpu->addArg(sizeof(uint32_t),&startElem);         // Process from
  gpu->addArg(sizeof(uint32_t),&nbElemToProcess);   // Number of element to process
  gpu->addArg(sizeof(void *),&gpuOffsets);          // Tracking start offsets
  gpu->addArg(sizeof(uint32_t),&offsetStride);      // Number of particle which starts at elem specified in gpuOffsets
  gpu->addArg(sizeof(uint32_t),&nbParticles);       // Total number of particle to track
  gpu->addArg(sizeof(void *),&gpuRin);              // Input 6D coordinates
  gpu->addArg(sizeof(void *),&gpuRout);             // Output 6D coordinates
  gpu->addArg(sizeof(void *),&gpuLost);             // Lost flags
  gpu->addArg(sizeof(uint32_t),&turn);              // Current turn
  gpu->addArg(sizeof(void *),&gpuRefs);             // Observation points at specified elements
  gpu->addArg(sizeof(uint32_t),&nbRef);             // Number of observation points
  gpu->addArg(sizeof(void *),&gpuLostAtElem);       // Element indices where particles are lost
  gpu->addArg(sizeof(void *),&gpuLostAtCoord);      // Output coordinates where particles are lost

  // Turn loop
  for(turn=0;turn<nbTurn;turn++) {
    startElem = 0;
    gpu->run(nbParticles); // 1 particle per thread
  }

  // Get back data
  gpu->deviceToHost(lost,gpuLost,lostSize);
  if(updateRin) gpu->deviceToHost(rin, gpuRin, nbParticles * 6 * sizeof(AT_FLOAT));
  if( routSize ) gpu->deviceToHost(rout,gpuRout,routSize);
  if( lostAtElem ) gpu->deviceToHost(lostAtElem,gpuLostAtElem, nbParticles * sizeof(uint32_t));
  if( lostAtCoord ) gpu->deviceToHost(lostAtCoord,gpuLostAtCoord, nbParticles * 6 * sizeof(AT_FLOAT));

  // format data to AT format
  for(uint32_t i=0;i<nbParticles;i++) {
    if( !lost[i] ) {
      // Alive particle
      if( lostAtCoord )
        memset(lostAtCoord+6*i,0,6 * sizeof(AT_FLOAT));
      if( lostAtElem )
        lostAtElem[i] = 0;
      if( lostAtTurn )
        lostAtTurn[i] = (uint32_t)nbTurn;
    } else {
      // Dead particle
      if( lostAtTurn )
        lostAtTurn[i] = lost[i] - 1;
    }
  }

  // Free
  gpu->freeDevice(gpuRin);
  if( gpuRout ) gpu->freeDevice(gpuRout);
  gpu->freeDevice(gpuRefs);
  gpu->freeDevice(gpuLost);
  gpu->freeDevice(gpuRingParams);
  if( gpuLostAtElem ) gpu->freeDevice(gpuLostAtElem);
  if( gpuLostAtCoord ) gpu->freeDevice(gpuLostAtCoord);
  if( gpuOffsets ) gpu->freeDevice(gpuOffsets);
  delete[] expandedRefPts;
  delete[] lost;

#ifdef _PROFILE
  double t1 = AbstractGPU::get_ticks();
  cout << "GPU tracking: " << (t1-t0)*1000.0 << "ms" << endl;
#endif

}