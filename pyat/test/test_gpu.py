from at.tracking import gpu_info

gpuList = gpu_info()
for g in gpuList:
	print(g)
