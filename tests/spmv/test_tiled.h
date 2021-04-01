#pragma once

#include <cooperative_groups.h>
#include <cuda_runtime_api.h>

#include "test_utils.h"

namespace cg = cooperative_groups;

template <typename index_t = int, typename value_t = float>
class TileIterator {
 public:
  __device__ TileIterator(index_t _num_rows, index_t _num_cols,
                          index_t _num_nonzeros, index_t *_row_offsets,
                          index_t *_col_idx, value_t *_nonzeros,
                          value_t *_input, value_t *_output,
                          index_t _tile_row_size, index_t _tile_col_size,
                          index_t *_local_row_offsets) {
    num_rows = _num_rows;
    num_cols = _num_cols;
    num_nonzeros = _num_nonzeros;
    row_offsets = _row_offsets;
    col_idx = _col_idx;
    nonzeros = _nonzeros;
    input = _input;
    output = _output;
    tile_row_size = _tile_row_size;
    tile_col_size = _tile_col_size;

    cur_row_tile_idx = 0;
    cur_col_tile_idx = 0;

    local_row_offsets = _local_row_offsets;
  }

  __device__ __forceinline__ bool all_tiles_finished() {
    // How many evenly-sized tiles are there?
    index_t number_of_tiles_in_matrix = (num_rows / tile_row_size);

    // Remainder tile
    if (num_rows % tile_row_size) number_of_tiles_in_matrix++;

    // cur_row_tile_idx incremented only after the primary tile is finished
    if (cur_row_tile_idx >= number_of_tiles_in_matrix) {
      return true;
    } else {
      return false;
    }
  }

  __device__ __forceinline__ void load_primary_tile() {
    // Depending on the implementation, maybe allocate space for the output, or
    // the metadata, or both

    // Need to simultaneously keep track of the current row in the tile as well
    // as the row index in the global coordinates

    // Initialize to beginning of tile

    // Row in the tile is based solely on the thread & block index
    int cur_row_in_tile = blockIdx.x * blockDim.x + threadIdx.x;

    // Index relative to the entire matrix is based on the offset within the
    // tile added to the total number of rows before it
    int cur_row_in_matrix =
        cur_row_in_tile + (cur_row_tile_idx * tile_row_size);

    int stride = blockDim.x * gridDim.x;

    for (cur_row_in_tile, cur_row_in_matrix; cur_row_in_matrix < num_rows;
         cur_row_in_tile += stride, cur_row_in_matrix += stride) {
      printf("Loading row %d tile idx %d\n", cur_row_in_matrix,
             cur_row_in_tile);
    }

    cg::grid_group grid = cg::this_grid();
    grid.sync();

    cur_col_tile_idx++;
  }

  __device__ __forceinline__ void load_secondary_tile(){};

  __device__ __forceinline__ void evict_primary_tile() {
    // In the src-first context, this means writing the output back to global
    // memory
  }

  __device__ __forceinline__ void evict_secondary_tile() {
    // In the src-first implementation, there is nothing to do for this function
    // except maybe resetting the L2 cach
  }

  __device__ __forceinline__ bool primary_tile_finished() { return false; };

  __device__ __forceinline__ void process_all_tiles() {
    while (!all_tiles_finished()) {
      load_primary_tile();
      // process_primary_tile();
      // evict_primary_tile();
    }
  }

  __device__ __forceinline__ void process_primary_tile() {
    while (!primary_tile_finished()) {
      load_secondary_tile();
      process_secondary_tile();
      evict_secondary_tile();
    }

    cur_row_tile_idx++;
    cur_col_tile_idx = 0;
  }

  __device__ __forceinline__ void process_secondary_tile() {
    // Initialize to beginning of tile
    int row_in_tile = blockIdx.x * blockDim.x + threadIdx.x;

    int stride = blockDim.x * gridDim.x;

    for (int global_row = row_in_tile + (cur_row_tile_idx * tile_row_size);
         global_row < min((cur_row_tile_idx + 1) * tile_row_size, num_rows);
         global_row += stride) {
      printf("Processing row %d\n", global_row);
    }

    cg::grid_group grid = cg::this_grid();
    grid.sync();

    cur_col_tile_idx++;
  }

 private:
  index_t num_rows;
  index_t num_cols;
  index_t num_nonzeros;
  index_t *row_offsets;
  index_t *col_idx;
  value_t *nonzeros;
  value_t *input;
  value_t *output;
  index_t tile_row_size;
  index_t tile_col_size;

  index_t cur_row_tile_idx;
  index_t cur_col_tile_idx;

  index_t *local_row_offsets;
};

template <typename index_t = int, typename value_t = float, typename cudaProp_t>
__global__ void spmv_tiled_kernel(index_t num_rows, index_t num_cols,
                                  index_t num_nonzeros, index_t *row_offsets,
                                  index_t *col_idx, value_t *nonzeros,
                                  value_t *input, value_t *output,
                                  index_t tile_row_size, index_t tile_col_size,
                                  cudaProp_t deviceProp) {
  // Store the output in shared memory
  extern __shared__ index_t shmem[];

  // TileIterator<int, float> iterator(
  //     num_rows, num_cols, num_nonzeros, row_offsets, col_idx, nonzeros, input,
  //     output, tile_row_size, tile_col_size, shmem);

  // iterator.process_all_tiles();

  int cur_row_in_tile = blockIdx.x * blockDim.x + threadIdx.x;

  // Index relative to the entire matrix is based on the offset within the
  // tile added to the total number of rows before it
      int cur_row_tile_idx = 0;

  int cur_row_in_matrix =
      cur_row_in_tile + (cur_row_tile_idx * tile_row_size);

  int stride = blockDim.x * gridDim.x;

  for (cur_row_in_tile, cur_row_in_matrix; cur_row_in_matrix < num_rows;
       cur_row_in_tile += stride, cur_row_in_matrix += stride) {
    printf("Loading row %d tile idx %d\n", cur_row_in_matrix,
           cur_row_in_tile);
  }

  cg::grid_group grid = cg::this_grid();
  grid.sync();
  // Each block sets the shared memory region as the output, initialized to 0
  // Iterate up to the tile boundary

  // Use Ampere's CUDAMemCpyAsynch
  // Need to use cuda cooperative groups

  // Simple, single-threaded implementation
  // if (blockIdx.x == 0 && threadIdx.x == 0) {
  //   for (int i = 0; i < num_rows; i++) {
  //     value_t y = 0;
  //     for (int k = row_offsets[i]; k < row_offsets[i + 1]; k++) {
  //       y = y + (nonzeros[k] * input[col_idx[k]]);
  //     }
  //     output[i] = y;
  //   }
  // }

  // Grid iterates over rows
  // the initial row that this thread will work on

  // Aamer's approach:
  // 1. Load tile src values into shmem
  // 2.

  // Within a src tile, blocks iterate up to the tile boundary, and then perform
  // a grid sync
}

template <typename index_t = int, typename value_t = float, typename dinput_t,
          typename doutput_t>
double spmv_tiled(csr_t<index_t, value_t> &A, dinput_t &input,
                  doutput_t &output) {
  /* ========== Setup Device Properties ========== */
  int device = 0;
  cudaDeviceProp deviceProp;
  CHECK_CUDA(cudaGetDeviceProperties(&deviceProp, device))

  // Setup grid and block properties
  int numBlocksPerSm = 0;
  int numThreadsPerBlock = 0;
  int shmemPerBlock = 0;  // bytes

  int target_occupancy = 1;

  size_t tile_size = 100;  // Coordinates

  // Use the max number of threads per block to maximize parallelism over shmem
  numThreadsPerBlock = deviceProp.maxThreadsPerBlock / target_occupancy;
  shmemPerBlock = deviceProp.sharedMemPerBlockOptin / target_occupancy;

  cudaFuncSetAttribute(spmv_tiled_kernel<int, float, cudaDeviceProp>,
                       cudaFuncAttributeMaxDynamicSharedMemorySize,
                       shmemPerBlock);

  int rows_per_block = (shmemPerBlock / sizeof(value_t)) / 3;
  printf("Threads Per Block: %d\n", numThreadsPerBlock);
  printf("Rows Per Block: %d\n", rows_per_block);
  printf("Shmem Per Block (bytes): %d\n", shmemPerBlock);

  // Need to know the max occupancy to determine how many blocks to launch for
  // the cooperative kernel. All blocks must be resident on SMs
  CHECK_CUDA(cudaOccupancyMaxActiveBlocksPerMultiprocessor(
      &numBlocksPerSm, spmv_tiled_kernel<int, float, cudaDeviceProp>,
      numThreadsPerBlock, shmemPerBlock))

  printf("Blocks per SM: %d\n", numBlocksPerSm);

  /* ========== Setup Kernel Call ========== */
  void *row_offsets = thrust::raw_pointer_cast(A.d_row_offsets.data());
  void *col_idx = thrust::raw_pointer_cast(A.d_col_idx.data());
  void *nonzeros = thrust::raw_pointer_cast(A.d_nonzero_vals.data());
  void *input_ptr = thrust::raw_pointer_cast(input.data());
  void *output_ptr = thrust::raw_pointer_cast(output.data());
  void *kernelArgs[] = {&A.num_rows,  &A.num_columns, &A.num_nonzeros,
                        &row_offsets, &col_idx,       &nonzeros,
                        &input_ptr,   &output_ptr,    &tile_size,
                        &tile_size,   &deviceProp};
  dim3 dimBlock(numThreadsPerBlock, 1, 1);
  dim3 dimGrid(deviceProp.multiProcessorCount * numBlocksPerSm, 1, 1);

  /* ========== Execute SPMV ========== */
  Timer t;
  t.start();
  CHECK_CUDA(cudaLaunchCooperativeKernel(
      (void *)spmv_tiled_kernel<int, float, cudaDeviceProp>, dimGrid, dimBlock,
      kernelArgs, shmemPerBlock, 0))

  CHECK_CUDA(cudaDeviceSynchronize())
  t.stop();

  return t.elapsed();
}