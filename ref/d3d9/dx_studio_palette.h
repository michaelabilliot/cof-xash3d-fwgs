#pragma once
// Shared by the actual renderer upload path and its CPU regression.
inline void R_StudioCopyFinalPalette(studio_cache_t &cache,const BoneMat34 *bones,int count,bool procedural) {
    const int boundedCount=std::clamp(count,0,MAXSTUDIOBONES);
    memcpy(cache.gpuSkinningBoneMatrices,bones,static_cast<size_t>(boundedCount)*sizeof(BoneMat34));
    cache.gpuSkinningBoneCount=boundedCount;
    cache.gpuSkinningPoseValid=boundedCount>0;
    cache.physicsPoseUploaded=procedural;
}
