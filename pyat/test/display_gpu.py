import at


# Retrieve GPUs
gpuList = at.tracking.gpu_info()
nbGPU = len(gpuList)
if nbGPU == 0:
	sys.exit("No GPU found")

# Display GPU info	
print(f"{nbGPU} GPU detected")
for g in gpuList:
	print(g)
