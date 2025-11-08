#ifndef REGULATION_CONFIG_HPP
#define REGULATION_CONFIG_HPP

#include "APIEnvir.h"
#include "ACAPinc.h"
#include "UniString.hpp"
#include <optional>

// 统一的JSON配置文件路径（上传和加载都使用此路径）
#define USER_REGULATION_JSON_PATH L"E:\\ArchiCAD_Development_File\\BuildingCodeChecker_Stair\\shared\\current_regulation.json"

/**
 * 规范规则结构
 */
struct RegulationRule {
    std::optional<double>  minValue;
    std::optional<double>  maxValue;
    GS::UniString          unit;
    GS::UniString          source;
    GS::UniString          fullText;

    RegulationRule() : unit(L"m") {}

    bool HasMinValue() const { return minValue.has_value(); }
    bool HasMaxValue() const { return maxValue.has_value(); }
};

/**
 * 楼梯规范配置类
 * 支持从JSON文件加载动态配置
 */
class RegulationConfig {
public:
    GS::UniString      regulationName;
    GS::UniString      regulationCode;

    // 设计规范参数 (Design Code Parameters)
    RegulationRule     riserHeightRule;        // 踏步高度
    RegulationRule     treadDepthRule;         // 踏步宽度/深度
    RegulationRule     twoRPlusGRule;          // 2R+G 公式
    RegulationRule     landingLengthRule;      // 平台长度

    // 防火规范参数 (Fire Code Parameters)
    RegulationRule     stairWidthRule;         // 楼梯净宽度
    RegulationRule     handrailHeightRule;     // 栏杆扶手高度
    RegulationRule     slopeAngleRule;         // 倾斜角度
    RegulationRule     betweenFlightsRule;     // 两梯段间距

    /**
     * 从JSON文件加载配置
     */
    static RegulationConfig LoadFromJSON(const IO::Location& jsonPath);

    /**
     * 获取默认配置（硬编码的规范）
     */
    static RegulationConfig GetDefault();

    /**
     * 保存为JSON文件
     */
    bool SaveToJSON(const IO::Location& jsonPath) const;

private:
    static RegulationRule ParseRule(const GS::UniString& jsonText, const GS::UniString& ruleName);
};

#endif // REGULATION_CONFIG_HPP
