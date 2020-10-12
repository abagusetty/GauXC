#include <iostream>
#include <cassert>

#include <gauxc/shell.hpp>

#include "collocation_angular_cartesian.hpp"
#include "collocation_angular_spherical_unnorm.hpp"
#include "collocation_radial.hpp"

namespace GauXC      {
namespace integrator {
namespace cuda       {


template <typename T>
__global__
void collocation_device_masked_kernel(
  size_t          nshells,
  size_t          nbf,
  size_t          npts,
  const Shell<T>* shells_device,
  const size_t*   mask_device,
  const size_t*   offs_device,
  const T*        pts_device,
  T*              eval_device
) {

  const int tid_x = blockIdx.x * blockDim.x + threadIdx.x;
  const int tid_y = blockIdx.y * blockDim.y + threadIdx.y;

  if( tid_x < npts and tid_y < nshells ) {

    const size_t ipt = tid_x;
    const size_t ish = tid_y;

    const size_t ibf = offs_device[ish];

    const auto& shell = shells_device[mask_device[ish]];
    const auto* pt    = pts_device + 3*ipt;
  

    const auto* O     = shell.O_data();
    const auto* alpha = shell.alpha_data();
    const auto* coeff = shell.coeff_data();

    const auto xc = pt[0] - O[0];
    const auto yc = pt[1] - O[1];
    const auto zc = pt[2] - O[2];
  
    T tmp;
    collocation_device_radial_eval( shell.nprim(), alpha, coeff, xc, yc, zc,
                                    &tmp );

    auto * bf_eval = eval_device + ibf + ipt*nbf;

    const bool do_sph = shell.pure();
    if( do_sph )
      collocation_spherical_unnorm_angular( shell.l(), tmp, xc, yc, zc, bf_eval );
    else
      collocation_cartesian_angular( shell.l(), tmp, xc, yc, zc, bf_eval );

  }

}








template <typename T>
__global__
void collocation_device_masked_kernel_deriv1(
  size_t          nshells,
  size_t          nbf,
  size_t          npts,
  const Shell<T>* shells_device,
  const size_t*   mask_device,
  const size_t*   offs_device,
  const T*        pts_device,
  T*              eval_device,
  T*              deval_device_x,
  T*              deval_device_y,
  T*              deval_device_z
) {

  const int tid_x = blockIdx.x * blockDim.x + threadIdx.x;
  const int tid_y = blockIdx.y * blockDim.y + threadIdx.y;

  if( tid_x < npts and tid_y < nshells ) {

    const size_t ipt = tid_x;
    const size_t ish = tid_y;

    const size_t ibf = offs_device[ish];

    const auto& shell = shells_device[mask_device[ish]];
    const auto* pt    = pts_device + 3*ipt;
  

    const auto* O     = shell.O_data();
    const auto* alpha = shell.alpha_data();
    const auto* coeff = shell.coeff_data();

    const auto xc = pt[0] - O[0];
    const auto yc = pt[1] - O[1];
    const auto zc = pt[2] - O[2];
  
    T tmp = 0., tmp_x = 0., tmp_y = 0., tmp_z = 0.;
    collocation_device_radial_eval_deriv1( shell.nprim(), alpha, coeff,
                                           xc, yc, zc, &tmp, &tmp_x, &tmp_y,
                                           &tmp_z );

    auto * bf_eval = eval_device    + ibf + ipt*nbf;
    auto * dx_eval = deval_device_x + ibf + ipt*nbf;
    auto * dy_eval = deval_device_y + ibf + ipt*nbf;
    auto * dz_eval = deval_device_z + ibf + ipt*nbf;

    const bool do_sph = shell.pure();
    if( do_sph ) 
      collocation_spherical_unnorm_angular_deriv1( shell.l(), tmp, tmp_x, tmp_y, tmp_z, 
                                               xc, yc, zc, bf_eval, dx_eval, 
                                               dy_eval, dz_eval );
    else
      collocation_cartesian_angular_deriv1( shell.l(), tmp, tmp_x, tmp_y, tmp_z, 
                                        xc, yc, zc, bf_eval, dx_eval, 
                                        dy_eval, dz_eval );

  }


}

} // namespace cuda
} // namespace integrator
} // namespace GauXC

