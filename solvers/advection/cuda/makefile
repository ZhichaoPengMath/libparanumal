# -cg 42%
cudaFlags = -Dcomp_Nq=$(comp_Nq) -Dcomp_cubNq=$(comp_cubNq) -Dcomp_MAX_ITERATIONS=$(comp_MAX_ITERATIONS) -arch=sm_70   --use_fast_math -Xptxas -v,-dlcm=ca,-O3 -Xcompiler -O3
#cudaFlags = -Dcomp_Nq=$(comp_Nq) -Dcomp_cubNq=$(comp_cubNq) -Dcomp_MAX_ITERATIONS=$(comp_MAX_ITERATIONS) -arch=sm_70   --use_fast_math -Xptxas -v,-dlcm=cg,-O3 -Xcompiler -O3 
#hipFlags = -Dcomp_Nq=$(comp_Nq) -Dcomp_cubNq=$(comp_cubNq) -Dcomp_MAX_ITERATIONS=$(comp_MAX_ITERATIONS) --amdgpu-target=gfx906 -O3 -ffast-math
nonoptFlags = -g -G -Xcompiler -O0 -Xptxas -v,-O0 -lineinfo -O0 -arch=sm_70
hipFlags = -Dcomp_Nq=$(comp_Nq) -Dcomp_cubNq=$(comp_cubNq) -Dcomp_MAX_ITERATIONS=$(comp_MAX_ITERATIONS) --amdgpu-target=gfx906 -O3 -ffast-math 

#-g -G

advectionInvertMassMatrix: advectionInvertMassMatrixV2.cu
	nvcc -o advectionInvertMassMatrix $(cudaFlags) advectionInvertMassMatrixV2.cu

advectionMassMatrixMultiply: advectionMassMatrixMultiply.cu
	nvcc -o advectionMassMatrixMultiply $(cudaFlags) advectionMassMatrixMultiply.cu

massMatrixMultiplyVT: massMatrixMultiplyVTV3.cu
	nvcc -o massMatrixMultiplyVT $(cudaFlags) massMatrixMultiplyVTV3.cu
#	nvcc -o massMatrixMultiplyVT $(nonoptFlags) massMatrixMultiplyVT.cu

stiffnessMatrixMultiplyVT: stiffnessMatrixMultiplyVT.cu
	nvcc -o stiffnessMatrixMultiplyVT $(cudaFlags) stiffnessMatrixMultiplyVT.cu

advectionMassMatrixMultiplyHip: advectionMassMatrixMultiplyHip.c
	hipcc -o advectionMassMatrixMultiplyHip $(hipFlags) advectionMassMatrixMultiplyHip.c

massMatrixMultiplyHipVT: massMatrixMultiplyHipVTV3.c
	hipcc -o massMatrixMultiplyHipVT $(hipFlags) massMatrixMultiplyHipVTV3.c

stiffnessMatrixMultiplyHipVT: stiffnessMatrixMultiplyHipVT.c
	hipcc -o stiffnessMatrixMultiplyHipVT $(hipFlags) stiffnessMatrixMultiplyHipVT.c


stiffnessMatrixMultiplyHipVTV2: stiffnessMatrixMultiplyHipVTV2.c
	hipcc -o stiffnessMatrixMultiplyHipVTV2 $(hipFlags) stiffnessMatrixMultiplyHipVTV2.c



roofline: roofline.cu
	nvcc -o roofline $(cudaFlags) roofline.cu


clean:
	rm -f advectionInvertMassMatrix roofline
