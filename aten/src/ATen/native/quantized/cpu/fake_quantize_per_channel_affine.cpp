#include <ATen/ATen.h>
#include <ATen/NativeFunctions.h>
#include <ATen/native/TensorIterator.h>
#include <ATen/native/cpu/Loops.h>
#include "fake_quantize_core.h"

/* FakeQuantize Op for PerChannelAffine quantization scheme */
namespace at {
namespace native {

/* Per channel fake-quantizes the 'inputs' tensor.
Args:
  X: Forward input tensor.
  dY: Backward input tensor (_backward op only).
  scale: scale of per channel affine quantization
  zero_point: zero_point of per channel affine quantization
  axis: int specifying the axis to be quantized
  quant_min: minimum quantized value
  quant_max: maximum quantized value
Returns:
  Fake quantized tensor (double dtype).

*/

Tensor fake_quantize_per_channel_affine_cpu(
    const Tensor& self,
    const Tensor& scale,
    const Tensor& zero_point,
    int64_t axis,
    int64_t quant_min,
    int64_t quant_max) {

  TORCH_CHECK(self.scalar_type() == ScalarType::Float);

  TORCH_CHECK(scale.size(0) == zero_point.size(0),
  "scale and zero-point need to have the same dimensions");
  TORCH_CHECK(scale.size(0) == self.size(axis),
  "dimensions of scale and zero-point are not consistent with input tensor")


  TORCH_CHECK(
      quant_min <= quant_max,
      "`quant_min` should be less than or \
        equal to `quant_max`.");


  TORCH_CHECK(at::min(zero_point).item().toLong() >= quant_min &&
              at::max(zero_point).item().toLong() <= quant_max,
      "`zero_point` must be between `quant_min` and `quant_max`.");

  TORCH_CHECK(axis >= 0 &&
              axis <= self.dim(),
      "`axis` must be between 0 and number of dimensions of input");

  auto Y = at::empty_like(self, self.options(), MemoryFormat::Preserve);
  for (int i = 0; i < self.size(axis); i++)
  {
    auto input_slice = self.slice(axis,i,i+1);
    auto output_slice = Y.slice(axis,i,i+1);

    float sc = scale[i].item().toFloat();
    float inv_scale = 1.0f / sc;
    int64_t z_point = zero_point[i].item().toLong();
    fake_quantize_slice(output_slice, input_slice,
                           sc, z_point, quant_min, quant_max);

  }

  return Y;
}

/* Backward path for per-channel fake-quantization of the 'inputs' tensor.

Args:
  X: Forward input tensor.
  dY: Backward input tensor.
  scale: scale of per tensor affine quantization
  zero_point: zero_point of per tensor affine quantization
  axis: int ,the axis over which quantization parameters vary
  quant_min: int, minimum quantized value
  quant_max: int, maximum quantized value

Returns:
  Gradient for per channel fake quant (double dtype).

*/
Tensor fake_quantize_per_channel_affine_backward_cpu(
    const Tensor& dY,
    const Tensor& X,
    const Tensor& scale,
    const Tensor& zero_point,
    int64_t axis,
    int64_t quant_min,
    int64_t quant_max) {


  TORCH_CHECK(dY.scalar_type() == ScalarType::Float);
  TORCH_CHECK(X.scalar_type() == ScalarType::Float);

  TORCH_CHECK(X.numel() == dY.numel(), "`X` and `dY` are not the same size");
  TORCH_CHECK(
      quant_min <= quant_max,
      "`quant_min` should be less than or \
        equal to `quant_max`.");

  TORCH_CHECK(scale.size(0) == zero_point.size(0),
  "scale and zero-point need to have the same dimensions")
  TORCH_CHECK(scale.size(0) == X.size(axis),
  "dimensions of scale and zero-point are not consistent with input tensor")


  TORCH_CHECK(
      quant_min <= quant_max,
      "`quant_min` should be less than or \
        equal to `quant_max`.");


  TORCH_CHECK(at::min(zero_point).item().toLong() >= quant_min &&
              at::max(zero_point).item().toLong() <= quant_max,
      "`zero_point` must be between `quant_min` and `quant_max`.");

  TORCH_CHECK(axis >= 0 &&
              axis <= X.dim(),
      "`axis` must be between 0 and number of dimensions of input");

  if (X.numel() <= 0) {
    return X;
  }

  Tensor dX = at::zeros_like(X);

  for (int i = 0; i < X.size(axis); i++)
  {
    auto ZX = X.slice(axis,i,i+1);
    auto ZdY = dY.slice(axis,i,i+1);
    auto ZdX = dX.slice(axis,i,i+1);

    float sc = scale[i].item().toFloat();
    float inv_scale = 1.0f / sc;
    int64_t z_point = zero_point[i].item().toLong();
    fake_quantize_grad_slice(ZdX, ZX, ZdY, sc, z_point, quant_min, quant_max);
  }

  return dX;

}
} // namespace native
} // namespace at
