#pragma once
#include "dx_local.h"
void DX_RagdollRegister();
void DX_RagdollReset(const char *reason="explicit-reset");
void DX_RagdollPaletteProof(const cl_entity_t *ent,const BoneMat34 *input,const BoneMat34 *uploaded,int count,const char *path);
void DX_RagdollSubmitted(const cl_entity_t *ent,const studio_cache_t &cache,const BoneMat34 *bones,int count,const D3DMATRIX *world,const char *path);
void DX_RagdollFrame();
void DX_RagdollDeviceReset(const char *reason);
bool DX_RagdollCandidate(const cl_entity_t *ent);
bool DX_RagdollPose(const cl_entity_t *ent, studiohdr_t *hdr,
    const D3DMATRIX &world, BoneMat34 *pose, float time);

bool R_StudioSampleRagdollReference(model_t *model,studiohdr_t *hdr,int sequence,float frame,BoneMat34 *out);
