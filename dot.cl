char *dot_product = "__kernel void dot_product( __global float *inputA, __global float *inputB, __local float *p, __global float *sum) {" \
	"uint lid = get_local_id(0);" \
	"uint n = get_local_size(0);" \
	"p[lid] = inputA[get_global_id(0)] * inputB[get_global_id(0)];" \
	"barrier(CLK_LOCAL_MEM_FENCE);" \
	"for(int i = n/2; i>0; i >>= 1) {" \
		"if(lid < i) {" \
			"p[lid] += p[lid + i];" \
	  	"}" \
		"barrier(CLK_LOCAL_MEM_FENCE);" \
	"}" \
	"if(lid == 0)" \
		"sum[get_group_id(0)] = p[0];" \
"}";
