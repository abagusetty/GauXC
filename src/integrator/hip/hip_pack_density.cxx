#include "hip/hip_runtime.h"
#include "hip_pack_density.hpp"
#include "hip_device_properties.hpp"

namespace GauXC      {
namespace integrator {
namespace hip       {

using namespace GauXC::hip;

template <typename T>
__global__ void submat_set_combined_kernel( size_t           ntasks,
                                            XCTaskDevice<T>* device_tasks,
                                            T*               A,
                                            size_t           LDA ) {

  const int batch_id = blockIdx.z;

  if( batch_id < ntasks ) {

  auto& task = device_tasks[ batch_id ];

  const auto  ncut              = task.ncut;
  const auto* submat_cut_device = task.submat_cut;
  const auto  LDAS              = task.nbe;
        auto* ASmall_device     = task.nbe_scr;

  //if( LDAS == LDAB ) return;


  const int tid_x = blockDim.x * blockIdx.x + threadIdx.x;
  const int tid_y = blockDim.y * blockIdx.y + threadIdx.y;

  int64_t i(0);
  for( size_t i_cut = 0; i_cut < ncut; ++i_cut ) {
    const int64_t i_cut_first  = submat_cut_device[ 3*i_cut ];
    const int64_t delta_i      = submat_cut_device[ 3*i_cut + 1 ];

    int64_t j(0);
  for( size_t j_cut = 0; j_cut < ncut; ++j_cut ) {
    const int64_t j_cut_first  = submat_cut_device[ 3*j_cut ];
    const int64_t delta_j      = submat_cut_device[ 3*j_cut + 1 ];

    auto* ASmall_begin = ASmall_device + i           + j          *LDAS;
    auto* ABig_begin   = A             + i_cut_first + j_cut_first*LDA ;
    
    for( size_t J = tid_y; J < delta_j; J += blockDim.y )      
    for( size_t I = tid_x; I < delta_i; I += blockDim.x )
      ASmall_begin[I + J*LDAS] = ABig_begin[I + J*LDA];

    j += delta_j;
  }
    i += delta_i;
  }

  } // batch_id check
}


template <typename T>
void task_pack_density_matrix( size_t           ntasks,
                               XCTaskDevice<T>* device_tasks,
                               T*               P_device,
                               size_t           LDP,
                               hipStream_t     stream ) {

  dim3 threads(warp_size,max_warps_per_thread_block,1), blocks(1,1,ntasks);
  hipLaunchKernelGGL(submat_set_combined_kernel, dim3(blocks), dim3(threads), 0, stream , 
    ntasks, device_tasks, P_device, LDP
  );


}

template 
void task_pack_density_matrix( size_t                ntasks,
                               XCTaskDevice<double>* device_tasks,
                               double*               P_device,
                               size_t                LDP,
                               hipStream_t          stream );

}
}
}
