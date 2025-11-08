#include "StairCompliance.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "APICommon.h"
#include "HashTable.hpp"
#include "RegulationConfig.hpp"
#include "File.hpp"
#include "Location.hpp"

// 全局规范配置（运行时从JSON加载）- 可被其他文件访问
RegulationConfig g_regulationConfig;

namespace {

static bool g_configLoaded = false;

constexpr double kEpsilon = 1e-4;  // Changed from 1e-6 for more robust floating-point comparison

static GS::UniString FormatMillimeters (double meters)
{
	// 转换为毫米
	int mm = static_cast<int>(meters * 1000.0 + 0.5);  // 四舍五入

	// 使用字符串连接避免Printf问题
	GS::UniString value = GS::UniString::Printf(L"%d", mm);
	value.Append (L" 毫米");
	return value;
}

// 加载规范配置
static void LoadRegulationConfigIfNeeded()
{
	if (g_configLoaded)
		return;

	// 尝试从JSON文件加载配置
	// 使用统一路径（与上传功能一致）
	GS::UniString jsonPathStr = USER_REGULATION_JSON_PATH;
	IO::Location jsonPath (jsonPathStr);

	// 尝试加载JSON配置
	RegulationConfig loadedConfig = RegulationConfig::LoadFromJSON(jsonPath);

	// 增强验证：检查规范名称有效 AND 至少有一个规则被成功解析
	if (!loadedConfig.regulationName.IsEmpty() &&
	    loadedConfig.regulationName != L"未加载规范") {

		// 验证至少有一个规则被成功解析
		bool hasAnyRule = loadedConfig.riserHeightRule.HasMaxValue() ||
		                  loadedConfig.riserHeightRule.HasMinValue() ||
		                  loadedConfig.treadDepthRule.HasMinValue() ||
		                  loadedConfig.treadDepthRule.HasMaxValue() ||
		                  loadedConfig.twoRPlusGRule.HasMinValue() ||
		                  loadedConfig.twoRPlusGRule.HasMaxValue() ||
		                  loadedConfig.landingLengthRule.HasMinValue() ||
		                  loadedConfig.landingLengthRule.HasMaxValue();

			if (hasAnyRule) {
				g_regulationConfig = loadedConfig;
				g_configLoaded = true;

				// 输出详细的加载成功信息到ArchiCAD日志
				GS::UniString logMsg = L"[Stair Compliance] ✓ 成功从JSON加载规范:\n";
				logMsg += L"  规范名称: ";
				logMsg += g_regulationConfig.regulationName;
				logMsg += L" (";
				logMsg += g_regulationConfig.regulationCode;
				logMsg += L")\n";

				// 显示每个规则的详细信息
				int ruleCount = 0;
				if (loadedConfig.riserHeightRule.HasMaxValue()) {
					logMsg += L"  - 踏步高度: ≤ ";
					logMsg += FormatMillimeters(loadedConfig.riserHeightRule.maxValue.value());
					logMsg += L"\n";
					ruleCount++;
				}
				if (loadedConfig.treadDepthRule.HasMinValue()) {
					logMsg += L"  - 踏步宽度: ≥ ";
					logMsg += FormatMillimeters(loadedConfig.treadDepthRule.minValue.value());
					logMsg += L"\n";
					ruleCount++;
				}
				if (loadedConfig.twoRPlusGRule.HasMinValue() && loadedConfig.twoRPlusGRule.HasMaxValue()) {
					logMsg += L"  - 2R+G公式: ";
					logMsg += FormatMillimeters(loadedConfig.twoRPlusGRule.minValue.value());
					logMsg += L" ~ ";
					logMsg += FormatMillimeters(loadedConfig.twoRPlusGRule.maxValue.value());
					logMsg += L"\n";
					ruleCount++;
				}
				if (loadedConfig.landingLengthRule.HasMinValue()) {
					logMsg += L"  - 平台长度: ≥ ";
					logMsg += FormatMillimeters(loadedConfig.landingLengthRule.minValue.value());
					logMsg += L"\n";
					ruleCount++;
				}

				logMsg += L"  共加载 ";
				logMsg += GS::UniString::Printf(L"%d", ruleCount);
				logMsg += L" 条规则";

				ACAPI_WriteReport(logMsg.ToCStr().Get(), false);

				// 详细调试：输出加载的规则原始数值（以米为单位）
				GS::UniString debugMsg = L"\n[DEBUG] 规则数值详情 (单位:米):\n";
				if (loadedConfig.riserHeightRule.HasMaxValue()) {
					debugMsg += GS::UniString::Printf(L"  riserHeightRule.maxValue = %.6f\n",
						loadedConfig.riserHeightRule.maxValue.value());
				}
				if (loadedConfig.treadDepthRule.HasMinValue()) {
					debugMsg += GS::UniString::Printf(L"  treadDepthRule.minValue = %.6f\n",
						loadedConfig.treadDepthRule.minValue.value());
				}
				if (loadedConfig.twoRPlusGRule.HasMinValue()) {
					debugMsg += GS::UniString::Printf(L"  twoRPlusGRule.minValue = %.6f\n",
						loadedConfig.twoRPlusGRule.minValue.value());
				}
				if (loadedConfig.twoRPlusGRule.HasMaxValue()) {
					debugMsg += GS::UniString::Printf(L"  twoRPlusGRule.maxValue = %.6f\n",
						loadedConfig.twoRPlusGRule.maxValue.value());
				}
				if (loadedConfig.landingLengthRule.HasMinValue()) {
					debugMsg += GS::UniString::Printf(L"  landingLengthRule.minValue = %.6f\n",
						loadedConfig.landingLengthRule.minValue.value());
				}
				debugMsg += GS::UniString::Printf(L"  kEpsilon = %.9f\n", kEpsilon);
				ACAPI_WriteReport(debugMsg.ToCStr().Get(), false);

				return;
			}
		}

	// JSON加载失败或解析失败，使用空配置并提示用户
	g_regulationConfig = RegulationConfig::GetDefault();
	g_configLoaded = true;

	// 输出详细的失败警告，指导用户如何操作
	GS::UniString warningMsg = L"[Stair Compliance] ⚠ 未加载有效规范配置\n";
	warningMsg += L"━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
	warningMsg += L"原因: JSON文件不存在或解析失败\n";
	warningMsg += L"\n";
	warningMsg += L"请按照以下步骤操作：\n";
	warningMsg += L"1. 准备楼梯规范PDF文件（如：建筑设计防火规范.pdf）\n";
	warningMsg += L"2. 运行Python工具提取规范：\n";
	warningMsg += L"   cd E:\\ArchiCAD_Development\\StairRegulationRAG\n";
	warningMsg += L"   python src/main.py <你的PDF文件路径>\n";
	warningMsg += L"3. 确认生成JSON文件：\n";
	warningMsg += L"   E:\\ArchiCAD_Development\\StairRegulationRAG\\fire_regulation_6.4.5.json\n";
	warningMsg += L"4. 重新加载ArchiCAD或点击面板刷新按钮\n";
	warningMsg += L"\n";
	warningMsg += L"当前状态: 所有规范检查已禁用\n";
	warningMsg += L"━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";

	ACAPI_WriteReport(warningMsg.ToCStr().Get(), false);
}

static void AppendMetric (GS::UniString& target, const wchar_t* label, double valueMeters)
{
	if (!target.IsEmpty ())
		target.Append (L"；");

	target.Append (label);
	target.Append (L" ");
	target += FormatMillimeters (valueMeters);
}

static double ComputeTwoRPlusGoing (double riserHeight, double treadDepth)
{
	return (2.0 * riserHeight) + treadDepth;
}

static double ComputeSegmentLength (const API_StairPolylineData& polyline, Int32 edgeIndex)
{
	if (polyline.coords == nullptr || *polyline.coords == nullptr)
		return 0.0;

	const API_Coord* coords = *polyline.coords;
	const API_Coord& start = coords[edgeIndex - 1];
	const API_Coord& end = coords[edgeIndex];

	const double dx = end.x - start.x;
	const double dy = end.y - start.y;
	const double chordLength = std::sqrt (dx * dx + dy * dy);

	if (polyline.parcs != nullptr && *polyline.parcs != nullptr && polyline.polygon.nArcs > 0) {
		const API_PolyArc* arcs = *polyline.parcs;
		for (Int32 arcIndex = 0; arcIndex < polyline.polygon.nArcs; ++arcIndex) {
			const API_PolyArc& arc = arcs[arcIndex];
			if (arc.begIndex == edgeIndex - 1 && arc.endIndex == edgeIndex) {
				const double angle = std::fabs (arc.arcAngle);
				if (angle > kEpsilon) {
					const double halfChord = chordLength * 0.5;
					const double radius = halfChord / std::sin (angle * 0.5);
					return radius * angle;
				}
				break;
			}
		}
	}

	return chordLength;
}

static double ComputeMinimumLandingLength (const API_StairPolylineData& polyline, bool* landingEvaluated)
{
	if (landingEvaluated != nullptr)
		*landingEvaluated = false;

	if (polyline.coords == nullptr || *polyline.coords == nullptr)
		return 0.0;

	if (polyline.edgeData == nullptr || polyline.polygon.nCoords <= 1)
		return 0.0;

	const Int32 segmentCount = polyline.polygon.nCoords - 1;
	if (segmentCount <= 0)
		return 0.0;

	double minLanding = DBL_MAX;
	double currentLanding = 0.0;
	bool inLanding = false;
	bool foundLandingSegment = false;

	for (Int32 edgeIdx = 1; edgeIdx <= segmentCount; ++edgeIdx) {
		const API_StairPolylineEdgeData& edge = polyline.edgeData[edgeIdx];
		const bool isLandingSegment = (edge.segmentType == APIST_LandingSegment || edge.segmentType == APIST_DividedLandingSegment);
		const double segmentLength = ComputeSegmentLength (polyline, edgeIdx);

		if (isLandingSegment) {
			foundLandingSegment = true;
			if (!inLanding) {
				inLanding = true;
				currentLanding = 0.0;
			}
			currentLanding += segmentLength;
		} else if (inLanding) {
			minLanding = std::min (minLanding, currentLanding);
			currentLanding = 0.0;
			inLanding = false;
		}
	}

	if (inLanding)
		minLanding = std::min (minLanding, currentLanding);

	if (landingEvaluated != nullptr && foundLandingSegment)
		*landingEvaluated = true;

	if (minLanding == DBL_MAX)
		return 0.0;

	return minLanding;
}

static void CollectStoryNames (GS::HashTable<short, GS::UniString>& storyNames)
{
	storyNames.Clear ();

	API_StoryInfo storyInfo;
	BNZeroMemory (&storyInfo, sizeof (API_StoryInfo));

	if (ACAPI_ProjectSetting_GetStorySettings (&storyInfo) == NoError && storyInfo.data != nullptr) {
		const short firstStory = storyInfo.firstStory;
		const short lastStory = storyInfo.lastStory;
		const short count = static_cast<short> (lastStory - firstStory + 1);

		for (short offset = 0; offset < count; ++offset) {
			const API_StoryType& info = (*storyInfo.data)[offset];
			const short floorIndex = static_cast<short> (firstStory + offset);
			storyNames.Add (floorIndex, GS::UniString (info.uName));
		}
	}

	if (storyInfo.data != nullptr)
		BMKillHandle (reinterpret_cast<GSHandle*> (&storyInfo.data));
}

static GS::UniString BuildDisplayName (const API_Element& element, const GS::UniString* storyName)
{
	GS::UniString name;
	if (storyName != nullptr && !storyName->IsEmpty ()) {
		name = *storyName;
		name.Append (L" 楼梯");
	} else {
		name = GS::UniString::Printf (L"楼层索引 %d 楼梯", element.header.floorInd);
	}

	return name;
}

} // namespace

GS::Array<StairComplianceResult> EvaluateStairCompliance ()
{
	// 确保配置已加载
	LoadRegulationConfigIfNeeded();

	GS::Array<StairComplianceResult> results;

	GS::HashTable<short, GS::UniString> storyNames;
	CollectStoryNames (storyNames);

	GS::Array<API_Guid> stairGuids;
	if (ACAPI_Element_GetElemList (API_StairID, &stairGuids) != NoError || stairGuids.IsEmpty ())
		return results;

	results.SetCapacity (stairGuids.GetSize ());

	for (UIndex i = 0; i < stairGuids.GetSize (); ++i) {
		const API_Guid& stairGuid = stairGuids[i];

		API_Element element;
		BNZeroMemory (&element, sizeof (API_Element));
		element.header.guid = stairGuid;

		if (ACAPI_Element_Get (&element) != NoError)
			continue;

		API_ElementMemo memo;
		BNZeroMemory (&memo, sizeof (API_ElementMemo));
		const bool memoLoaded = (ACAPI_Element_GetMemo (stairGuid, &memo, 0) == NoError);

		StairComplianceResult result;
		result.guid = stairGuid;
		result.floorIndex = element.header.floorInd;
		result.riserHeight = element.stair.riserHeight;
		result.treadDepth = element.stair.treadDepth;
		result.twoRPlusGoing = ComputeTwoRPlusGoing (result.riserHeight, result.treadDepth);
		result.minLandingLength = 0.0;
		result.landingEvaluated = false;

		const GS::UniString* storyNamePtr = nullptr;
		if (storyNames.Get (result.floorIndex, &storyNamePtr) && storyNamePtr != nullptr)
			result.storyName = *storyNamePtr;

		result.displayName = BuildDisplayName (element, storyNamePtr);

		if (memoLoaded) {
			result.minLandingLength = ComputeMinimumLandingLength (memo.stairWalkingLine, &result.landingEvaluated);
		}

		// 调试：输出当前楼梯的实测数据
		GS::UniString stairDebug;
		stairDebug.Printf(L"\n[DEBUG] 楼梯 #%u (%s) 实测数据:\n", (unsigned int)i + 1, result.displayName.ToCStr().Get());
		stairDebug += GS::UniString::Printf(L"  riserHeight = %.6f 米 (%.0f 毫米)\n",
			result.riserHeight, result.riserHeight * 1000.0);
		stairDebug += GS::UniString::Printf(L"  treadDepth = %.6f 米 (%.0f 毫米)\n",
			result.treadDepth, result.treadDepth * 1000.0);
		stairDebug += GS::UniString::Printf(L"  twoRPlusGoing = %.6f 米 (%.0f 毫米)\n",
			result.twoRPlusGoing, result.twoRPlusGoing * 1000.0);
		if (result.landingEvaluated) {
			stairDebug += GS::UniString::Printf(L"  minLandingLength = %.6f 米 (%.0f 毫米)\n",
				result.minLandingLength, result.minLandingLength * 1000.0);
		} else {
			stairDebug += L"  minLandingLength = 未评估\n";
		}
		ACAPI_WriteReport(stairDebug.ToCStr().Get(), false);

		// 使用动态规范配置进行检查

		// 检查踏步高度
		if (g_regulationConfig.riserHeightRule.HasMaxValue()) {
			const double maxHeight = g_regulationConfig.riserHeightRule.maxValue.value();
			const double difference = result.riserHeight - maxHeight;

			GS::UniString comparisonMsg;
			comparisonMsg.Printf(L"[DEBUG] 踏步高度检查: 实测%.6f vs 限制≤%.6f, 差值=%.9f, kEpsilon=%.9f\n",
				result.riserHeight, maxHeight, difference, kEpsilon);

			if (difference > kEpsilon) {
				comparisonMsg += L"  → 结果: ✗ 违规! 超出限制\n";
				ACAPI_WriteReport(comparisonMsg.ToCStr().Get(), false);
				result.violations.Push (g_regulationConfig.riserHeightRule.fullText);
			} else {
				comparisonMsg += L"  → 结果: ✓ 符合规范\n";
				ACAPI_WriteReport(comparisonMsg.ToCStr().Get(), false);
			}
		} else {
			ACAPI_WriteReport(L"[DEBUG] 踏步高度检查: 跳过（规则未设置maxValue）\n", false);
		}

		// 检查踏步宽度/深度
		// 注意：只有当treadDepth有效时才检查（大于0）
		// 某些楼梯类型可能无法通过API获取treadDepth，跳过检查
		if (g_regulationConfig.treadDepthRule.HasMinValue()) {
			if (result.treadDepth > kEpsilon) {
				const double minDepth = g_regulationConfig.treadDepthRule.minValue.value();
				const double difference = minDepth - result.treadDepth;

				GS::UniString comparisonMsg;
				comparisonMsg.Printf(L"[DEBUG] 踏步宽度检查: 实测%.6f vs 限制≥%.6f, 差值=%.9f, kEpsilon=%.9f\n",
					result.treadDepth, minDepth, difference, kEpsilon);

				if (difference > kEpsilon) {
					comparisonMsg += L"  → 结果: ✗ 违规! 低于限制\n";
					ACAPI_WriteReport(comparisonMsg.ToCStr().Get(), false);
					result.violations.Push (g_regulationConfig.treadDepthRule.fullText);
				} else {
					comparisonMsg += L"  → 结果: ✓ 符合规范\n";
					ACAPI_WriteReport(comparisonMsg.ToCStr().Get(), false);
				}
			} else {
				ACAPI_WriteReport(L"[DEBUG] 踏步宽度检查: 跳过（treadDepth无效或为0）\n", false);
			}
		} else {
			ACAPI_WriteReport(L"[DEBUG] 踏步宽度检查: 跳过（规则未设置minValue）\n", false);
		}

		// 【已禁用】检查平台长度 - 用户要求只检查踏步高度和宽度
		/*
		if (result.landingEvaluated && result.minLandingLength > 0.0) {
			if (g_regulationConfig.landingLengthRule.HasMinValue()) {
				const double minLanding = g_regulationConfig.landingLengthRule.minValue.value();
				const double difference = minLanding - result.minLandingLength;

				GS::UniString comparisonMsg;
				comparisonMsg.Printf(L"[DEBUG] 平台长度检查: 实测%.6f vs 限制≥%.6f, 差值=%.9f, kEpsilon=%.9f\n",
					result.minLandingLength, minLanding, difference, kEpsilon);

				if (difference > kEpsilon) {
					comparisonMsg += L"  → 结果: ✗ 违规! 低于限制\n";
					ACAPI_WriteReport(comparisonMsg.ToCStr().Get(), false);
					result.violations.Push (g_regulationConfig.landingLengthRule.fullText);
				} else {
					comparisonMsg += L"  → 结果: ✓ 符合规范\n";
					ACAPI_WriteReport(comparisonMsg.ToCStr().Get(), false);
				}
			} else {
				ACAPI_WriteReport(L"[DEBUG] 平台长度检查: 跳过（规则未设置minValue）\n", false);
			}
		} else {
			ACAPI_WriteReport(L"[DEBUG] 平台长度检查: 跳过（未评估或长度为0）\n", false);
		}
		*/
		ACAPI_WriteReport(L"[DEBUG] 平台长度检查: 已禁用（只检查踏步高度和宽度）\n", false);

		// 【已禁用】检查2R+G公式 - 用户要求只检查踏步高度和宽度
		/*
		if (g_regulationConfig.twoRPlusGRule.HasMinValue() && g_regulationConfig.twoRPlusGRule.HasMaxValue()) {
			const double minValue = g_regulationConfig.twoRPlusGRule.minValue.value();
			const double maxValue = g_regulationConfig.twoRPlusGRule.maxValue.value();

			GS::UniString comparisonMsg;
			comparisonMsg.Printf(L"[DEBUG] 2R+G检查: 实测%.6f vs 限制范围[%.6f, %.6f], kEpsilon=%.9f\n",
				result.twoRPlusGoing, minValue, maxValue, kEpsilon);

			if (result.twoRPlusGoing + kEpsilon < minValue) {
				const double difference = minValue - result.twoRPlusGoing;
				comparisonMsg += GS::UniString::Printf(L"  差值=%.9f (低于下限)\n", difference);
				comparisonMsg += L"  → 结果: ✗ 违规! 低于下限\n";
				ACAPI_WriteReport(comparisonMsg.ToCStr().Get(), false);
				result.violations.Push (g_regulationConfig.twoRPlusGRule.fullText);
			} else if (result.twoRPlusGoing - maxValue > kEpsilon) {
				const double difference = result.twoRPlusGoing - maxValue;
				comparisonMsg += GS::UniString::Printf(L"  差值=%.9f (超出上限)\n", difference);
				comparisonMsg += L"  → 结果: ✗ 违规! 超出上限\n";
				ACAPI_WriteReport(comparisonMsg.ToCStr().Get(), false);
				result.violations.Push (g_regulationConfig.twoRPlusGRule.fullText);
			} else {
				comparisonMsg += L"  → 结果: ✓ 符合规范\n";
				ACAPI_WriteReport(comparisonMsg.ToCStr().Get(), false);
			}
		} else {
			ACAPI_WriteReport(L"[DEBUG] 2R+G检查: 跳过（规则未设置min或max值）\n", false);
		}
		*/
		ACAPI_WriteReport(L"[DEBUG] 2R+G检查: 已禁用（只检查踏步高度和宽度）\n", false);

		// 只显示高度和宽度（用户要求简化检测范围）
		GS::UniString metrics;
		AppendMetric (metrics, L"踏步高度", result.riserHeight);
		AppendMetric (metrics, L"踏步宽度", result.treadDepth);
		result.metricsSummary = metrics;

		results.Push (result);

		if (memoLoaded)
			ACAPI_DisposeElemMemoHdls (&memo);
	}

	return results;
}

void ForceReloadRegulationConfig ()
{
	// 重置加载标志，强制重新加载JSON
	g_configLoaded = false;

	// 输出日志
	ACAPI_WriteReport(L"[Stair Compliance] 强制重新加载规范配置...", false);

	// 重新加载配置
	LoadRegulationConfigIfNeeded();
}
