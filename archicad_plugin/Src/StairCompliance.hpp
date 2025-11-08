#ifndef STAIR_COMPLIANCE_HPP
#define STAIR_COMPLIANCE_HPP

#include "APIEnvir.h"
#include "ACAPinc.h"

#include "UniString.hpp"

struct StairComplianceResult {
    API_Guid                    guid;
    GS::UniString               displayName;
    GS::UniString               storyName;
    short                       floorIndex;
    double                      riserHeight;
    double                      treadDepth;
    double                      minLandingLength;
    double                      twoRPlusGoing;
    bool                        landingEvaluated;
    GS::UniString               metricsSummary;
    GS::Array<GS::UniString>    violations;
    GS::Array<GS::UniString>    notices;

    bool IsCompliant () const { return violations.IsEmpty (); }
};

GS::Array<StairComplianceResult> EvaluateStairCompliance ();

// 强制重新加载规范配置（供"开始检测"按钮使用）
void ForceReloadRegulationConfig ();

#endif

