#include <gauxc/xc_integrator/xc_cuda_util.hpp>

//#include "cuda_weights.hpp"
#include "collocation_device.hpp"
//#include "cuda_zmat.hpp"
#include "integrator_common.hpp"
//#include "blas.hpp"
//#include "util.hpp"

namespace GauXC  {
namespace integrator::cuda {



template <typename F, size_t n_deriv>
void process_batches_cuda_replicated_p(
  XCWeightAlg            weight_alg,
  const functional_type& func,
  const BasisSet<F>&     basis,
  const Molecule   &     mol,
  const MolMeta    &     meta,
  XCCudaData<F>    &     cuda_data,
  std::vector< XCTask >& tasks,
  const F*               P,
  F*                     VXC,
  F*                     exc,
  F*                     n_el
) {

#if 0
  partition_weights_cuda( weight_alg, mol, meta, tasks );
  const auto nbf = basis.nbf();

  std::fill( VXC, VXC + size_t(nbf)*nbf, F(0.) );

  size_t ntasks = tasks.size();
  for( size_t iT = 0; iT < ntasks; ++iT ) {

    auto& task = tasks[iT];

    const size_t  npts    = task.points.size();
    const size_t  nbe     = task.nbe;
    const size_t  nshells = task.shell_list.size();

    const F* points      = task.points.data()->data();
    const F* weights     = task.weights.data();
    const int32_t* shell_list = task.shell_list.data();

    F* basis_eval = cuda_data.basis_eval.data();
    F* den_eval   = cuda_data.den_scr.data();
    F* nbe_scr    = cuda_data.nbe_scr.data();
    F* zmat       = cuda_data.zmat.data();

    F* eps        = cuda_data.eps.data();
    F* gamma      = cuda_data.gamma.data();
    F* vrho       = cuda_data.vrho.data();
    F* vgamma     = cuda_data.vgamma.data();

    F* dbasis_x_eval = nullptr;
    F* dbasis_y_eval = nullptr;
    F* dbasis_z_eval = nullptr;
    F* dden_x_eval = nullptr;
    F* dden_y_eval = nullptr;
    F* dden_z_eval = nullptr;

    if( n_deriv > 0 ) {
      dbasis_x_eval = basis_eval    + npts * nbe;
      dbasis_y_eval = dbasis_x_eval + npts * nbe;
      dbasis_z_eval = dbasis_y_eval + npts * nbe;
      dden_x_eval   = den_eval    + npts;
      dden_y_eval   = dden_x_eval + npts;
      dden_z_eval   = dden_y_eval + npts;
    }


    // Get the submatrix map for batch
    auto submat_map = gen_compressed_submat_map( basis, task.shell_list );


    // Evaluate Collocation Matrix 
    if( n_deriv == 1 )
      eval_collocation_deriv1( npts, nshells, nbe, points, basis, shell_list, 
                               basis_eval, dbasis_x_eval, dbasis_y_eval, 
                               dbasis_z_eval );
    else
      eval_collocation( npts, nshells, nbe, points, basis, shell_list, basis_eval );


    // Extrat Submatrix
    const F* den_ptr_use = P;
    if( nbe != nbf ) {
      detail::submat_set( nbf, nbf, nbe, nbe, P, nbf, nbe_scr, nbe, submat_map );
      den_ptr_use = nbe_scr;
    } 

    // Z = P * BF
    GauXC::blas::gemm( 'N', 'N', nbe, npts, nbe, 1., den_ptr_use, nbe,
                       basis_eval, nbe, 0., zmat, nbe );
    

    // Evaluate the density 
    for( int32_t i = 0; i < npts; ++i ) {

      const size_t ioff = size_t(i) * nbe;
      const F*     zmat_i = zmat + ioff;

      den_eval[i] = 
        2. * GauXC::blas::dot( nbe, basis_eval + ioff, 1, zmat_i, 1 );

      if( n_deriv > 0 ) {
        const F dx = 
          4. * GauXC::blas::dot( nbe, dbasis_x_eval + ioff, 1, zmat_i, 1 );
        const F dy = 
          4. * GauXC::blas::dot( nbe, dbasis_y_eval + ioff, 1, zmat_i, 1 );
        const F dz = 
          4. * GauXC::blas::dot( nbe, dbasis_z_eval + ioff, 1, zmat_i, 1 );

        dden_x_eval[i] = dx;
        dden_y_eval[i] = dy;
        dden_z_eval[i] = dz;

        gamma[i] = dx*dx + dy*dy + dz*dz;
      }

    }


    // Evaluate XC functional
    if( func.is_gga() )
      func.eval_exc_vxc( npts, den_eval, gamma, eps, vrho, vgamma );
    else
      func.eval_exc_vxc( npts, den_eval, eps, vrho );

    // Scalar integrations
    if( n_el )
      for( int32_t i = 0; i < npts; ++i ) *n_el += weights[i] * den_eval[i];

    for( int32_t i = 0; i < npts; ++i ) *exc += weights[i] * den_eval[i] * eps[i];
    

    // Assemble Z
    if( func.is_gga() )
      zmat_gga_cuda( npts, nbe, weights, vrho, vgamma, basis_eval, dbasis_x_eval,
                     dbasis_y_eval, dbasis_z_eval, dden_x_eval, dden_y_eval,
                     dden_z_eval, zmat ); 
    else
      zmat_lda_cuda( npts, nbe, weights, vrho, basis_eval, zmat ); 


    // Update VXC
    GauXC::blas::syr2k( 'L', 'N', nbe, npts, F(1.), basis_eval,
                        nbe, zmat, nbe, F(0.), nbe_scr, nbe );

    detail::inc_by_submat( nbf, nbf, nbe, nbe, VXC, nbf, nbe_scr, nbe,
                           submat_map );
  }

  // Symmetrize
  for( int32_t j = 0;   j < nbf; ++j )
  for( int32_t i = j+1; i < nbf; ++i )
    VXC[ j + i*nbf ] = VXC[ i + j*nbf ];

  //std::cout << std::scientific << std::setprecision(12);
  //std::cout << "NEL = " << *n_el << std::endl;

#endif
}


#define CUDA_IMPL( F, ND ) \
template \
void process_batches_cuda_replicated_p<F, ND>(\
  XCWeightAlg            weight_alg,\
  const functional_type& func,\
  const BasisSet<F>&     basis,\
  const Molecule   &     mol,\
  const MolMeta    &     meta,\
  XCCudaData<F>    &     cuda_data,\
  std::vector< XCTask >& local_work,\
  const F*               P,\
  F*                     VXC,\
  F*                     exc,\
  F*                     n_el\
) 

CUDA_IMPL( double, 0 );
CUDA_IMPL( double, 1 );

}
}

