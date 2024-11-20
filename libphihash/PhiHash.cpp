#include "PhiHash.h"
#include <limits>
#include <sstream>
#define pcg_rnd() (pcg32(pcg_rnd_state))
// #define pcg_rnd() (kiss99(rnd_state))
// #define pcg_rnd() (pcg32(pcg_rnd_state))
#define mix_src()   ("mix[" + std::to_string(pcg_rnd() % PHIHASH_REGS) + "]")
#define mix_dst()   ("mix[" + std::to_string(mix_seq_dst[(mix_seq_dst_cnt++)%PHIHASH_REGS]) + "]")
#define mix_cache() ("mix[" + std::to_string(mix_seq_cache[(mix_seq_cache_cnt++)%PHIHASH_REGS]) + "]")

void swap(int &a, int &b)
{
    int t = a;
    a = b;
    b = t;
}

std::string PhiHash::getKern(uint64_t phi_seed, kernel_t kern)
{
    std::stringstream ret;

    uint32_t seed0 = (uint32_t)phi_seed;
    uint32_t seed1 = phi_seed >> 32;
    uint32_t fnv_hash = 0x811c9dc5;

    pcg32_t pcg_rnd_state;
    pcg_rnd_state.state = fnv1a(fnv_hash, seed0);
    pcg_rnd_state.inc = fnv1a(fnv_hash, seed1) | 1;  

    int mix_seq_dst[PHIHASH_REGS];
    int mix_seq_cache[PHIHASH_REGS];
    int mix_seq_dst_cnt = 0;
    int mix_seq_cache_cnt = 0;
    for (int i = 0; i < PHIHASH_REGS; i++)
    {
        mix_seq_dst[i] = i;
        mix_seq_cache[i] = i;
    }
    for (int i = PHIHASH_REGS - 1; i > 0; i--)
    {
        int j;
        j = pcg_rnd() % (i + 1);
        swap(mix_seq_dst[i], mix_seq_dst[j]);
        j = pcg_rnd() % (i + 1);
        swap(mix_seq_cache[i], mix_seq_cache[j]);
    }

    if (kern == KERNEL_CUDA)
    {
        ret << "typedef unsigned int       uint32_t;\n";
        ret << "typedef unsigned long long uint64_t;\n";
        ret << "#if __CUDA_ARCH__ < 350\n";
        ret << "#define ROTL32(x,n) (((x) << (n % 32)) | ((x) >> (32 - (n % 32))))\n";
        ret << "#define ROTR32(x,n) (((x) >> (n % 32)) | ((x) << (32 - (n % 32))))\n";
        ret << "#else\n";
        ret << "#define ROTL32(x,n) __funnelshift_l((x), (x), (n))\n";
        ret << "#define ROTR32(x,n) __funnelshift_r((x), (x), (n))\n";
        ret << "#endif\n";
        ret << "#define min(a,b) ((a<b) ? a : b)\n";
        ret << "#define mul_hi(a, b) __umulhi(a, b)\n";
        ret << "#define clz(a) __clz(a)\n";
        ret << "#define popcount(a) __popc(a)\n\n";

        ret << "#define DEV_INLINE __device__ __forceinline__\n";
        ret << "#if (__CUDACC_VER_MAJOR__ > 8)\n";
        ret << "#define SHFL(x, y, z) __shfl_sync(0xFFFFFFFF, (x), (y), (z))\n";
        ret << "#else\n";
        ret << "#define SHFL(x, y, z) __shfl((x), (y), (z))\n";
        ret << "#endif\n\n";

        ret << "\n";
        #define _IS_CUDA true;
    }
    else
    {
        ret << "#ifndef GROUP_SIZE\n";
        ret << "#define GROUP_SIZE 128\n";
        ret << "#endif\n";
        ret << "#define GROUP_SHARE (GROUP_SIZE / " << PHIHASH_LANES << ")\n";
        ret << "\n";
        ret << "typedef unsigned int       uint32_t;\n";
        ret << "typedef unsigned long      uint64_t;\n";
        ret << "#define ROTL32(x, n) rotate((x), (uint32_t)(n))\n";
        ret << "#define ROTR32(x, n) rotate((x), (uint32_t)(32-n))\n";
        ret << "\n";
    }
    ret << "#define max(a, b) ((a) > (b) ? (a) : (b))\n"; 
    ret << "#define PHIHASH_LANES           " << PHIHASH_LANES << "\n";
    ret << "#define PHIHASH_REGS            " << PHIHASH_REGS << "\n";
    ret << "#define PHIHASH_DAG_LOADS       " << PHIHASH_DAG_LOADS << "\n";
    ret << "#define PHIHASH_CACHE_WORDS     " << PHIHASH_CACHE_BYTES / sizeof(uint32_t) << "\n";
    ret << "#define PHIHASH_CNT_DAG         " << PHIHASH_CNT_DAG << "\n";
    ret << "#define PHIHASH_CNT_MATH        " << PHIHASH_CNT_MATH << "\n";
    ret << "\n";


    if (kern == KERNEL_CUDA)
    {
        ret << "typedef struct __align__(16) {uint32_t s[PHIHASH_DAG_LOADS];} dag_t;\n";
        ret << "\n";
        ret << "// Inner loop for phi_seed " << phi_seed << "\n";
        ret << "__device__ __forceinline__ void PhihashLoop(const uint32_t loop,\n";
        ret << "        uint32_t mix[PHIHASH_REGS],\n";
        ret << "        const dag_t *g_dag,\n";
        ret << "        const uint32_t c_dag[PHIHASH_CACHE_WORDS],\n";
        ret << "        const bool hack_false)\n";
    }
    else
    {
        ret << "typedef struct __attribute__ ((aligned (16))) {uint32_t s[PHIHASH_DAG_LOADS];} dag_t;\n";
        ret << "\n";
        ret << "// Inner loop for phi_seed " << phi_seed << "\n";
        ret << "inline void PhihashLoop(const uint32_t loop,\n";
        ret << "        volatile uint32_t mix_arg[PHIHASH_REGS],\n";
        ret << "        __global const dag_t *g_dag,\n";
        ret << "        __local const uint32_t c_dag[PHIHASH_CACHE_WORDS],\n";
        ret << "        __local uint64_t share[GROUP_SHARE],\n";
        ret << "        const bool hack_false)\n";
    }
    ret << "{\n";

    ret << "dag_t data_dag;\n";
    ret << "uint32_t offset, data;\n";
    // Work around AMD OpenCL compiler bug
    // See https://github.com/gangnamtestnet/phihashminer/issues/16
    if (kern == KERNEL_CL)
    {
        ret << "uint32_t mix[PHIHASH_REGS];\n";
        ret << "for(int i=0; i<PHIHASH_REGS; i++)\n";
        ret << "    mix[i] = mix_arg[i];\n";
    }

    if (kern == KERNEL_CUDA)
        ret << "const uint32_t lane_id = threadIdx.x & (PHIHASH_LANES-1);\n";
    else
    {
        ret << "const uint32_t lane_id = get_local_id(0) & (PHIHASH_LANES-1);\n";
        ret << "const uint32_t group_id = get_local_id(0) / PHIHASH_LANES;\n";
    }

    // Global memory access
    // lanes access sequential locations
    // Hard code mix[0] to guarantee the address for the global load depends on the result of the
    // load
    ret << "// global load\n";
    if (kern == KERNEL_CUDA)
        ret << "offset = SHFL(mix[0], loop%PHIHASH_LANES, PHIHASH_LANES);\n";
    else
    {
        ret << "if(lane_id == (loop % PHIHASH_LANES))\n";
        ret << "    share[group_id] = mix[0];\n";
        ret << "barrier(CLK_LOCAL_MEM_FENCE);\n";
        ret << "offset = share[group_id];\n";
    }
    ret << "offset %= PHIHASH_DAG_ELEMENTS;\n";
    ret << "offset = offset * PHIHASH_LANES + (lane_id ^ loop) % PHIHASH_LANES;\n";
    ret << "data_dag = g_dag[offset];\n";
    ret << "// hack to prevent compiler from reordering LD and usage\n";
    if (kern == KERNEL_CUDA)
        ret << "if (hack_false) __threadfence_block();\n";
    else
        ret << "if (hack_false) barrier(CLK_LOCAL_MEM_FENCE);\n";

    for (int i = 0; (i < PHIHASH_CNT_CACHE) || (i < PHIHASH_CNT_MATH); i++)
    {
        if (i < PHIHASH_CNT_CACHE)
        {
            // Cached memory access
            // lanes access random locations
            std::string src = mix_cache();
            std::string dest = mix_dst();
            uint32_t r = pcg_rnd();
            ret << "// cache load " << i << "\n";
            ret << "offset = " << src << " % PHIHASH_CACHE_WORDS;\n";
            ret << "data = c_dag[offset];\n";
            ret << merge(dest, "data", r);
        }
        if (i < PHIHASH_CNT_MATH)
        {
            // Random Math
            // Generate 2 unique sources
            int src_rnd = pcg_rnd() % ((PHIHASH_REGS - 1) * PHIHASH_REGS);
            int src1 = src_rnd % PHIHASH_REGS; // 0 <= src1 < PHIHASH_REGS
            int src2 = src_rnd / PHIHASH_REGS; // 0 <= src2 < PHIHASH_REGS - 1
            if (src2 >= src1) ++src2; // src2 is now any reg other than src1
            std::string src1_str = "mix[" + std::to_string(src1) + "]";
            std::string src2_str = "mix[" + std::to_string(src2) + "]";
            uint32_t r1 = pcg_rnd();
            std::string dest = mix_dst();
            uint32_t r2 = pcg_rnd();
            ret << "// random math " << i << "\n";
            ret << math("data", src1_str, src2_str, r1);
            ret << merge(dest, "data", r2);
        }
    }
    // Consume the global load data at the very end of the loop, to allow fully latency hiding
    ret << "// consume global load data\n";
    ret << "// hack to prevent compiler from reordering LD and usage\n";
    if (kern == KERNEL_CUDA)
        ret << "if (hack_false) __threadfence_block();\n";
    else
        ret << "if (hack_false) barrier(CLK_LOCAL_MEM_FENCE);\n";
    ret << merge("mix[0]", "data_dag.s[0]", pcg_rnd());
    for (int i = 1; i < PHIHASH_DAG_LOADS; i++)
    {
        std::string dest = mix_dst();
        uint32_t    r = pcg_rnd();
        ret << merge(dest, "data_dag.s["+std::to_string(i)+"]", r);
    }
    // Work around AMD OpenCL compiler bug
    if (kern == KERNEL_CL)
    {
        ret << "for(int i=0; i<PHIHASH_REGS; i++)\n";
        ret << "    mix_arg[i] = mix[i];\n";
    }
    ret << "}\n";
    ret << "\n";

    return ret.str();
}

// Merge new data from b into the value in a
// Assuming A has high entropy only do ops that retain entropy, even if B is low entropy
// (IE don't do A&B)
std::string PhiHash::merge(std::string a, std::string b, uint32_t r)
{
    switch (r % 4)
    {
    case 0:
        return a + " = (" + a + " * 33) + " + b + ";\n";
    case 1:
        return a + " = (" + a + " ^ " + b + ") * 33;\n";
    case 2:
        return a + " = ROTL32(" + a + ", " + std::to_string(((r >> 16) % 31) + 1) + ") ^ " + b +
               ";\n";
    case 3:
        return a + " = ROTR32(" + a + ", " + std::to_string(((r >> 16) % 31) + 1) + ") ^ " + b +
               ";\n";
    }
    return "#error\n";
}

// Random math between two input values
std::string PhiHash::math(std::string d, std::string a, std::string b, uint32_t r)
{
    std::ostringstream ret;
    // printf("%d\n", r % 11);  

    static const char* operations[] = {
        " + ",  // case 0
        " * ",  // case 1
        " - ",  // case 2
        "min(", // case 3
        "max(", // case 4
        "ROTL32(", // case 5
        "ROTR32(", // case 6
        " & ",  // case 7
        " | ",  // case 8
        " ^ "   // case 9
    };

    int op_index = r % 11;  
    if (op_index < 3) {
        ret << d << " = " << a << operations[op_index] << b << ";\n";
    } else if (op_index < 5) {
        ret << d << " = " << operations[op_index] << a << ", " << b << ");\n";
    } else if (op_index < 7) {
        ret << d << " = " << operations[op_index] << a << ", " << b << " & 31);\n";
    } else if (op_index == 10) {  
        #ifdef _IS_CUDA  
            ret << d << " = [&]() { "
                << "constexpr float PHI = 0.61803398875f; "
                << "float sum = static_cast<float>(" << a << ") + static_cast<float>(" << b << "); "
                << "float result = tanhf(sum * PHI); "
                << "float frac_part = result - floor(result); "
                << "return static_cast<uint32_t>(frac_part * (1ULL << 32)); "
                << "}();\n";
        #else
            ret << d << " = [&]() { "
                << "constexpr float PHI = 0.61803398875f; "
                << "float sum = static_cast<float>(" << a << ") + static_cast<float>(" << b << "); "
                << "float result = tanh(sum * PHI); "
                << "float frac_part = result - floor(result); "
                << "return static_cast<uint32_t>(frac_part * (1ULL << 32)); "
                << "}();\n";
        #endif
    } else {
        ret << d << " = " << a << operations[op_index] << b << ";\n";
    }

    return ret.str();
}

uint32_t PhiHash::fnv1a(uint32_t &h, uint32_t d)
{
    return h = (h ^ d) * 0x1000193;
}


uint32_t PhiHash::pcg32(pcg32_t &st)
{
    uint64_t oldstate = st.state;
    st.state = oldstate * 6364136223846793005ULL + (st.inc | 1);
    // uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t xorshifted = static_cast<uint32_t>(((oldstate >> 18u) ^ oldstate) >> 27u);

    uint32_t rot = oldstate >> 59u;
    // return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));

    return (xorshifted >> rot) | (xorshifted << ((32 - rot) & 31));
}
