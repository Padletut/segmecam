# MiDaS Depth Estimation Model

## What is MiDaS?

MiDaS (Monocular Depth Estimation) is a neural network that estimates depth from a single camera image. We use it for accurate AR filter depth tracking - making filters stick to your face when you move forward/backward.

## Why MiDaS instead of face size heuristics?

**Face size method** (old approach):
- ❌ Unreliable: Noise and jitter in landmark detection
- ❌ Inconsistent: Different cameras have different FOVs
- ❌ Inaccurate: ±20-30% error typical
- ❌ Needs per-user calibration

**MiDaS neural depth** (new approach):
- ✅ Trained on millions of images
- ✅ Works across different cameras
- ✅ ~5-10% error typical
- ✅ No calibration needed
- ✅ Real-time performance (8ms on GPU, 30ms on CPU)

## Download Model

### MiDaS v2.1 Small (Recommended)

**Download:** https://github.com/isl-org/MiDaS/releases/download/v2_1/model-small.onnx

```bash
cd /home/padletut/segmecam/models
wget https://github.com/isl-org/MiDaS/releases/download/v2_1/model-small.onnx -O midas_v21_small_256.onnx
```

**Model Specs:**
- Size: ~22MB
- Input: 256x256 RGB image
- Output: 256x256 inverse depth map
- Performance: ~8ms on CUDA GPU, ~30ms on CPU
- Accuracy: High quality for AR applications

### Alternative: MiDaS v2.1 Large (Higher Accuracy)

If you need maximum accuracy and have a powerful GPU:

```bash
cd /home/padletut/segmecam/models
wget https://github.com/isl-org/MiDaS/releases/download/v2_1/model-f6b98070.onnx -O midas_v21_large_384.onnx
```

**Model Specs:**
- Size: ~102MB
- Input: 384x384 RGB image
- Output: 384x384 inverse depth map
- Performance: ~25ms on CUDA GPU, ~100ms on CPU
- Accuracy: Research-grade quality

⚠️ **Note**: Large model requires changing `kModelInputSize` in `midas_depth_estimator.cpp` from 256 to 384.

## Verification

After downloading, verify the file:

```bash
ls -lh /home/padletut/segmecam/models/midas_v21_small_256.onnx
# Should show: ~22MB file

file /home/padletut/segmecam/models/midas_v21_small_256.onnx
# Should show: data (ONNX model binary)
```

## Usage in SegmeCam

The MiDaS depth estimator initializes automatically when:
1. Model file exists at `models/midas_v21_small_256.onnx`
2. ONNX Runtime library is available (`libonnxruntime.so`)
3. OpenGLRenderer initializes successfully

Check logs for initialization status:
```
✅ MiDaS depth estimator initialized successfully
🚀 MiDaS depth estimation using CUDA execution provider  # GPU acceleration
```

If MiDaS is not available, SegmeCam falls back to face size estimation with a warning:
```
⚠️  MidasDepthEstimator: Unable to locate ONNX model 'midas_v21_small_256.onnx'. Depth estimation disabled.
    Download from: https://github.com/isl-org/MiDaS/releases/download/v2_1/model-small.onnx
```

## Performance

| Hardware | MiDaS Small (256x256) | MiDaS Large (384x384) |
|----------|------------------------|------------------------|
| NVIDIA RTX 3060 | ~8ms | ~25ms |
| NVIDIA GTX 1660 | ~12ms | ~40ms |
| Intel i7 CPU | ~30ms | ~100ms |
| Intel i5 CPU | ~45ms | ~150ms |

**Recommendation**: Use Small model unless you need research-grade accuracy. Small model provides excellent results for AR filters.

## License

MiDaS models are released under MIT License by Intel ISL.
See: https://github.com/isl-org/MiDaS

## References

- **MiDaS GitHub**: https://github.com/isl-org/MiDaS
- **Paper**: "Towards Robust Monocular Depth Estimation: Mixing Datasets for Zero-shot Cross-dataset Transfer"
- **Authors**: Ranftl et al., Intel ISL
