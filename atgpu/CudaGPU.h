#ifndef AT_GPU_CUDAGPU_H
#define AT_GPU_CUDAGPU_H
#include "AbstractGPU.h"
#include <cuda.h>
#include <nvrtc.h>

typedef struct {
  int devId;
  std::string arch;
  GPU_INFO info;
} CUDA_GPU_INFO;

class CudaContext final: public GPUContext {

public:

  explicit CudaContext(CUDA_GPU_INFO* gpu);
  ~CudaContext() final;

  // Empty kernel parameter
  void resetArg() final;

  // Set kernel parameter
  void addArg(size_t argSize,void *value) final;

  // Run the kernel
  void run(uint32_t blockSize, uint64_t nbThread) final;

  // Compile and load the kernel
  void compile(std::string& code) final;

  // Map memory buffer
  void mapBuffer(void **ring,uint32_t nbElement);

  // Copy from host to device
  void hostToDevice(void *dest,void *src,size_t size) final;

  // Copy from device to host
  void deviceToHost(void *dest,void *src,size_t size) final;

  // Allocate device memory
  void allocDevice(void **dest,size_t size) final;

  // Free device memory
  void freeDevice(void *dest) final;

  // Return device name
  std::string& name();

  // Return number fo core
  int coreNumber();

private:

  void cudaCheckCall(const char *funcName,CUresult r);
  void nvrtcCheckCall(const char *funcName,nvrtcResult r);

  CUcontext context;
  CUmodule module;
  CUfunction kernel;
  CUfunction mapkernel;
  std::vector<void *> args;
  GPU_INFO info;
  std::string arch;
  CUdevice cuDevice;

};

class CudaGPU: public AbstractGPU {

public:
  CudaGPU();

  // Return list of GPU device present on the system
  std::vector<GPU_INFO> getDeviceList() override;

  // Create a context for the given GPU
  GPUContext *createContext(int devId) override;

  // Return device function qualifier
  void getDeviceFunctionQualifier(std::string& ftype) override;

  // Return kernel function qualifier
  void getKernelFunctionQualifier(std::string& ftype);

  // Return global memory qualifier
  void getGlobalQualifier(std::string& ftype);

  // Return command to compute the thread id
  void getThreadId(std::string& command);

  // Add implementation specific function to the code
  void addSpecificFunctions(std::string& code);

  // Format a double
  std::string formatFloat(double *f);

private:

  // Check error and throw exception in case of failure
  static void cudaCheckCall(const char *funcName,CUresult r);
  // Compute number of stream processor (SIMT width)
  static int _ConvertSMVer2Cores(int major,int minor);

  std::vector<CUDA_GPU_INFO> cudaGPUs;

};


#endif //AT_GPU_CUDAGPU_H
