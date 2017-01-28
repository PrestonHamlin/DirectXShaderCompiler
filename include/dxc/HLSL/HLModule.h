///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLModule.h                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// HighLevel DX IR module.                                                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilMetadataHelper.h"
#include "dxc/HLSL/DxilConstants.h"
#include "dxc/HLSL/HLResource.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/HLSL/DxilSampler.h"
#include "dxc/HLSL/DxilShaderModel.h"
#include "dxc/HLSL/DxilSignature.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace llvm {
class LLVMContext;
class Module;
class Function;
class Instruction;
class MDTuple;
class MDNode;
class GlobalVariable;
class DIGlobalVariable;
class DebugInfoFinder;
};


namespace hlsl {

class ShaderModel;
class OP;
class RootSignatureHandle;

struct HLFunctionProps {
  union {
    // TODO: not every function need this union.
    // Compute shader.
    struct {
      unsigned numThreads[3];
    } CS;
    // Geometry shader.
    struct {
      DXIL::InputPrimitive inputPrimitive;
      unsigned maxVertexCount;
      unsigned instanceCount;
      DXIL::PrimitiveTopology streamPrimitiveTopologies[DXIL::kNumOutputStreams];
    } GS;
    // Hull shader.
    struct {
      llvm::Function *patchConstantFunc;
      DXIL::TessellatorDomain domain;
      DXIL::TessellatorPartitioning partition;
      DXIL::TessellatorOutputPrimitive outputPrimitive;
      unsigned inputControlPoints;
      unsigned outputControlPoints;
      float    maxTessFactor;
    } HS;
    // Domain shader.
    struct {
      DXIL::TessellatorDomain domain;
      unsigned inputControlPoints;
    } DS;
    // Vertex shader.
    struct {
      llvm::Constant *clipPlanes[DXIL::kNumClipPlanes];
    } VS;
    // Pixel shader.
    struct {
      bool EarlyDepthStencil;
    } PS;
  } ShaderProps;
  DXIL::ShaderKind shaderKind;
};

struct HLOptions {
  HLOptions()
      : bDefaultRowMajor(false), bIEEEStrict(false), bDisableOptimizations(false),
        bLegacyCBufferLoad(false), unused(0) {
  }
  uint32_t GetHLOptionsRaw() const;
  void SetHLOptionsRaw(uint32_t data);
  unsigned bDefaultRowMajor        : 1;
  unsigned bIEEEStrict             : 1;
  unsigned bAllResourcesBound      : 1;
  unsigned bDisableOptimizations   : 1;
  unsigned bLegacyCBufferLoad      : 1;
  unsigned unused                  : 27;
};

/// Use this class to manipulate HLDXIR of a shader.
class HLModule {
public:
  HLModule(llvm::Module *pModule);
  ~HLModule();
  using Domain = DXIL::TessellatorDomain;
  // Subsystems.
  llvm::LLVMContext &GetCtx() const;
  llvm::Module *GetModule() const;
  OP *GetOP() const;
  void SetShaderModel(const ShaderModel *pSM);
  const ShaderModel *GetShaderModel() const;

  // HLOptions
  void SetHLOptions(HLOptions &opts);
  const HLOptions &GetHLOptions() const;

  // Entry function.
  llvm::Function *GetEntryFunction() const;
  void SetEntryFunction(llvm::Function *pEntryFunc);
  const std::string &GetEntryFunctionName() const;
  void SetEntryFunctionName(const std::string &name);

  // Resources.
  unsigned AddCBuffer(std::unique_ptr<DxilCBuffer> pCB);
  DxilCBuffer &GetCBuffer(unsigned idx);
  const DxilCBuffer &GetCBuffer(unsigned idx) const;
  const std::vector<std::unique_ptr<DxilCBuffer> > &GetCBuffers() const;

  unsigned AddSampler(std::unique_ptr<DxilSampler> pSampler);
  DxilSampler &GetSampler(unsigned idx);
  const DxilSampler &GetSampler(unsigned idx) const;
  const std::vector<std::unique_ptr<DxilSampler> > &GetSamplers() const;

  unsigned AddSRV(std::unique_ptr<HLResource> pSRV);
  HLResource &GetSRV(unsigned idx);
  const HLResource &GetSRV(unsigned idx) const;
  const std::vector<std::unique_ptr<HLResource> > &GetSRVs() const;

  unsigned AddUAV(std::unique_ptr<HLResource> pUAV);
  HLResource &GetUAV(unsigned idx);
  const HLResource &GetUAV(unsigned idx) const;
  const std::vector<std::unique_ptr<HLResource> > &GetUAVs() const;

  void RemoveGlobal(llvm::GlobalVariable *GV);
  void RemoveFunction(llvm::Function *F);
  void RemoveResources(llvm::GlobalVariable **ppVariables, unsigned count);

  // ThreadGroupSharedMemory.
  typedef std::vector<llvm::GlobalVariable*>::iterator tgsm_iterator;
  tgsm_iterator tgsm_begin();
  tgsm_iterator tgsm_end();
  void AddGroupSharedVariable(llvm::GlobalVariable *GV);

  // Signatures.
  DxilSignature &GetInputSignature();
  DxilSignature &GetOutputSignature();
  DxilSignature &GetPatchConstantSignature();
  RootSignatureHandle &GetRootSignature();

  // HLFunctionProps.
  bool HasHLFunctionProps(llvm::Function *F);
  HLFunctionProps &GetHLFunctionProps(llvm::Function *F);
  void AddHLFunctionProps(llvm::Function *F, std::unique_ptr<HLFunctionProps> &info);

  DxilFunctionAnnotation *GetFunctionAnnotation(llvm::Function *F);
  DxilFunctionAnnotation *AddFunctionAnnotation(llvm::Function *F);

  void AddResourceTypeAnnotation(llvm::Type *Ty, DXIL::ResourceClass resClass,
                                 DXIL::ResourceKind kind);
  DXIL::ResourceClass GetResourceClass(llvm::Type *Ty);
  DXIL::ResourceKind  GetResourceKind(llvm::Type *Ty);

  // HLDXIR metadata manipulation.
  /// Serialize HLDXIR in-memory form to metadata form.
  void EmitHLMetadata();
  /// Deserialize HLDXIR metadata form into in-memory form.
  void LoadHLMetadata();
  /// Delete any HLDXIR from the specified module.
  static void ClearHLMetadata(llvm::Module &M);

  // Type related methods.
  static bool IsStreamOutputPtrType(llvm::Type *Ty);
  static bool IsStreamOutputType(llvm::Type *Ty);
  static bool IsHLSLObjectType(llvm::Type *Ty);
  static unsigned
  GetLegacyCBufferFieldElementSize(DxilFieldAnnotation &fieldAnnotation,
                                   llvm::Type *Ty, DxilTypeSystem &typeSys);

  static bool IsStaticGlobal(llvm::GlobalVariable *GV);
  static bool IsSharedMemoryGlobal(llvm::GlobalVariable *GV);
  static void GetParameterRowsAndCols(llvm::Type *Ty, unsigned &rows, unsigned &cols,
                                      DxilParameterAnnotation &paramAnnotation);
  static const char *GetLegacyDataLayoutDesc();

  // HL code gen.
  template<class BuilderTy>
  static llvm::Value *EmitHLOperationCall(BuilderTy &Builder,
                                          HLOpcodeGroup group, unsigned opcode,
                                          llvm::Type *RetType,
                                          llvm::ArrayRef<llvm::Value *> paramList,
                                          llvm::Module &M);

  static unsigned FindCastOp(bool fromUnsigned, bool toUnsigned,
                             llvm::Type *SrcTy, llvm::Type *DstTy);

  // Precise attribute.
  // Note: Precise will be marked on alloca inst with metadata in code gen.
  //       But mem2reg will remove alloca inst, so need mark precise with
  //       function call before mem2reg.
  static bool HasPreciseAttributeWithMetadata(llvm::Instruction *I);
  static void MarkPreciseAttributeWithMetadata(llvm::Instruction *I);
  static void ClearPreciseAttributeWithMetadata(llvm::Instruction *I);
  static void MarkPreciseAttributeOnPtrWithFunctionCall(llvm::Value *Ptr,
                                                        llvm::Module &M);
  static bool HasPreciseAttribute(llvm::Function *F);

  // DXIL type system.
  DxilTypeSystem &GetTypeSystem();

  /// Emit llvm.used array to make sure that optimizations do not remove unreferenced globals.
  void EmitLLVMUsed();
  std::vector<llvm::GlobalVariable* > &GetLLVMUsed();

  // Release functions used to transfer ownership.
  DxilSignature *ReleaseInputSignature();
  DxilSignature *ReleaseOutputSignature();
  DxilSignature *ReleasePatchConstantSignature();
  DxilTypeSystem *ReleaseTypeSystem();
  RootSignatureHandle *ReleaseRootSignature();

  llvm::DebugInfoFinder &GetOrCreateDebugInfoFinder();
  static llvm::DIGlobalVariable *
  FindGlobalVariableDebugInfo(llvm::GlobalVariable *GV,
                              llvm::DebugInfoFinder &DbgInfoFinder);
  // Create global variable debug info for element global variable based on the
  // whole global variable.
  static void CreateElementGlobalVariableDebugInfo(
      llvm::GlobalVariable *GV, llvm::DebugInfoFinder &DbgInfoFinder,
      llvm::GlobalVariable *EltGV, unsigned sizeInBits, unsigned alignInBits,
      unsigned offsetInBits, llvm::StringRef eltName);
  // Replace GV with NewGV in GlobalVariable debug info.
  static void
  UpdateGlobalVariableDebugInfo(llvm::GlobalVariable *GV,
                                llvm::DebugInfoFinder &DbgInfoFinder,
                                llvm::GlobalVariable *NewGV);

private:
  // Signatures.
  std::unique_ptr<DxilSignature> m_InputSignature;
  std::unique_ptr<DxilSignature> m_OutputSignature;
  std::unique_ptr<DxilSignature> m_PatchConstantSignature;
  std::unique_ptr<RootSignatureHandle> m_RootSignature;

  // Shader resources.
  std::vector<std::unique_ptr<HLResource> > m_SRVs;
  std::vector<std::unique_ptr<HLResource> > m_UAVs;
  std::vector<std::unique_ptr<DxilCBuffer> > m_CBuffers;
  std::vector<std::unique_ptr<DxilSampler> > m_Samplers;

  // ThreadGroupSharedMemory.
  std::vector<llvm::GlobalVariable*>  m_TGSMVariables;

  // High level function info.
  std::unordered_map<llvm::Function *, std::unique_ptr<HLFunctionProps>>  m_HLFunctionPropsMap;

  // Resource type annotation.
  std::unordered_map<llvm::Type *, std::pair<DXIL::ResourceClass, DXIL::ResourceKind>> m_ResTypeAnnotation;

private:
  llvm::LLVMContext &m_Ctx;
  llvm::Module *m_pModule;
  llvm::Function *m_pEntryFunc;
  std::string m_EntryName;
  std::unique_ptr<DxilMDHelper> m_pMDHelper;
  std::unique_ptr<llvm::DebugInfoFinder> m_pDebugInfoFinder;
  const ShaderModel *m_pSM;
  unsigned m_DxilMajor;
  unsigned m_DxilMinor;
  HLOptions m_Options;
  std::unique_ptr<OP> m_pOP;
  size_t m_pUnused;

  // DXIL metadata serialization/deserialization.
  llvm::MDTuple *EmitHLResources();
  void LoadHLResources(const llvm::MDOperand &MDO);
  llvm::MDTuple *EmitHLShaderProperties();
  void LoadHLShaderProperties(const llvm::MDOperand &MDO);
  llvm::MDTuple *EmitDxilShaderProperties();
  llvm::MDTuple *EmitResTyAnnotations();
  void LoadResTyAnnotations(const llvm::MDOperand &MDO);
  // LLVM used.
  std::vector<llvm::GlobalVariable*> m_LLVMUsed;

  // Type annotations.
  std::unique_ptr<DxilTypeSystem> m_pTypeSystem;

  // Helpers.
  template<typename T> unsigned AddResource(std::vector<std::unique_ptr<T> > &Vec, std::unique_ptr<T> pRes);
};


/// Use this class to manipulate metadata of extra metadata record properties that are specific to high-level DX IR.
class HLExtraPropertyHelper : public DxilExtraPropertyHelper {
public:
  HLExtraPropertyHelper(llvm::Module *pModule);

  virtual void EmitSignatureElementProperties(const DxilSignatureElement &SE, std::vector<llvm::Metadata *> &MDVals);
  virtual void LoadSignatureElementProperties(const llvm::MDOperand &MDO, DxilSignatureElement &SE);
};

} // namespace hlsl
