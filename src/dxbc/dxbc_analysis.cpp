#include "dxbc_analysis.h"

namespace dxvk {
  
  DxbcAnalyzer::DxbcAnalyzer(
    const DxbcModuleInfo&     moduleInfo,
    const DxbcProgramInfo&    programInfo,
    const Rc<DxbcIsgn>&       isgn,
    const Rc<DxbcIsgn>&       osgn,
    const Rc<DxbcIsgn>&       psgn,
          DxbcAnalysisInfo&   analysis)
  : m_isgn    (isgn),
    m_osgn    (osgn),
    m_psgn    (psgn),
    m_analysis(&analysis) {
    // Get number of clipping and culling planes from the
    // input and output signatures. We will need this to
    // declare the shader input and output interfaces.
    m_analysis->clipCullIn  = getClipCullInfo(m_isgn);
    m_analysis->clipCullOut = getClipCullInfo(m_osgn);

    // Default sampler<->texture assignments mean
    // that there is no 1:1 mapping between thw two
    for (auto& i : m_analysis->textureSamplerIds) i = -1;
    for (auto& i : m_analysis->samplerTextureIds) i = -1;
  }
  
  
  DxbcAnalyzer::~DxbcAnalyzer() {
    
  }
  
  
  void DxbcAnalyzer::processInstruction(const DxbcShaderInstruction& ins) {
    switch (ins.opClass) {
      case DxbcInstClass::Atomic: {
        const uint32_t operandId = ins.dstCount - 1;
        
        if (ins.dst[operandId].type == DxbcOperandType::UnorderedAccessView) {
          const uint32_t registerId = ins.dst[operandId].idx[0].offset;
          m_analysis->uavInfos[registerId].accessAtomicOp = true;
        }
      } break;
      
      case DxbcInstClass::TextureSample:
      case DxbcInstClass::TextureQueryLod:
        m_analysis->usesDerivatives = true;
        /* fall through */
      case DxbcInstClass::TextureGather: {
        const bool isExtendedGather = ins.op == DxbcOpcode::Gather4Po
                                   || ins.op == DxbcOpcode::Gather4PoC;
        const DxbcRegister& textureReg = ins.src[1 + isExtendedGather];
        const DxbcRegister& samplerReg = ins.src[2 + isExtendedGather];

        this->handleTextureSamplerPair(textureReg, samplerReg);
      } break;

      case DxbcInstClass::VectorDeriv: {
        m_analysis->usesDerivatives = true;
      } break;
      
      case DxbcInstClass::ControlFlow: {
        if (ins.op == DxbcOpcode::Discard)
          m_analysis->usesKill = true;
      } break;
      
      case DxbcInstClass::TypedUavLoad: {
        const uint32_t registerId = ins.src[1].idx[0].offset;
        m_analysis->uavInfos[registerId].accessTypedLoad = true;
      } break;
      
      default:
        return;
    }
  }
  
  
  void DxbcAnalyzer::handleTextureSamplerPair(
    const DxbcRegister&       texture,
    const DxbcRegister&       sampler) {
    int32_t textureId = int32_t(texture.idx[0].offset);
    int32_t samplerId = int32_t(sampler.idx[0].offset);

    int32_t& textureSamplerId = m_analysis->textureSamplerIds[textureId];
    int32_t& samplerTextureId = m_analysis->samplerTextureIds[samplerId];

    if (textureSamplerId == -1 && samplerTextureId == -1) {
      textureSamplerId = samplerId;
      samplerTextureId = textureId;
    } else if (textureSamplerId != samplerId || samplerTextureId != textureId) {
      if (textureSamplerId >= 0)
        m_analysis->samplerTextureIds[textureSamplerId] = -2;
      if (samplerTextureId >= 0)
        m_analysis->textureSamplerIds[samplerTextureId] = -2;
      textureSamplerId = -2;
      samplerTextureId = -2;
    }
  }


  DxbcClipCullInfo DxbcAnalyzer::getClipCullInfo(const Rc<DxbcIsgn>& sgn) const {
    DxbcClipCullInfo result;
    
    if (sgn != nullptr) {
      for (auto e = sgn->begin(); e != sgn->end(); e++) {
        const uint32_t componentCount = e->componentMask.popCount();
        
        if (e->systemValue == DxbcSystemValue::ClipDistance)
          result.numClipPlanes += componentCount;
        if (e->systemValue == DxbcSystemValue::CullDistance)
          result.numCullPlanes += componentCount;
      }
    }
    
    return result;
  }
  
}
