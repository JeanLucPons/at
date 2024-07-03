from at.tracking import gpu_info

gpuList = gpu_info()
nbGPU = len(gpuList)
print(f"{nbGPU} GPU detected")
for g in gpuList:
	print(g)
