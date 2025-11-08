#include "APIEnvir.h"
#include "ACAPinc.h"

#include "ResourceIDs.h"

#include "StairCompliance.hpp"
#include "StairCompliancePalette.hpp"
#include "RegulationConfig.hpp"

// 声明全局规范配置（定义在StairCompliance.cpp）
extern RegulationConfig g_regulationConfig;

namespace {

constexpr short kMenuResId = ID_MENU_STRINGS;
constexpr short kMenuPromptResId = ID_MENU_PROMPT_STRINGS;
constexpr short kPaletteMenuResId = ID_PALETTE_MENU_STRINGS;
constexpr short kPalettePromptResId = ID_PALETTE_PROMPT_STRINGS;

static GS::UniString LoadString (short resId, short index)
{
	GS::UniString value = RSGetIndString (resId, index, ACAPI_GetOwnResModule ());
	if (!value.IsEmpty ())
		return value;

	if (resId == ID_MENU_STRINGS && index == 1)
		return GS::UniString (L"楼梯规范校验");
	if (resId == ID_MENU_PROMPT_STRINGS && index == 1)
		return GS::UniString (L"按照用户上传的规范校验楼梯是否符合规范");
	if (resId == ID_PALETTE_MENU_STRINGS && index == 1)
		return GS::UniString (L"楼梯规范校验面板");
	if (resId == ID_PALETTE_PROMPT_STRINGS && index == 1)
		return GS::UniString (L"显示或隐藏楼梯规范校验面板");

	return value;
}

static GS::UniString ExtractMenuCaption (GS::UniString raw)
{
	const UIndex pos = raw.FindFirst ('^');
	if (pos < raw.GetLength ())
		raw.Delete (pos, raw.GetLength () - pos);
	return raw;
}

static void WriteReport (const GS::UniString& text, bool addToLog = false)
{
	ACAPI_WriteReport (text.ToCStr ().Get (), addToLog);
}

static GS::UniString FormatMillimeters (double meters)
{
	GS::UniString value;
	value.Printf (L"%.0lf", meters * 1000.0);
	value.Append (L" 毫米");
	return value;
}

static GS::UniString BuildRegulationText ()
{
	// 检查是否加载了有效规范
	if (g_regulationConfig.regulationName.IsEmpty () ||
	    g_regulationConfig.regulationName == L"未加载规范") {
		return GS::UniString (L"⚠ 未加载规范配置，请先上传规范PDF文件。");
	}

	// 构建规范信息文本
	GS::UniString text = L"《";
	text += g_regulationConfig.regulationName;
	if (!g_regulationConfig.regulationCode.IsEmpty ()) {
		text += L" ";
		text += g_regulationConfig.regulationCode;
	}
	text += L"》要求：";

	// 添加各项规则
	bool hasAnyRule = false;

	if (g_regulationConfig.riserHeightRule.HasMaxValue ()) {
		if (hasAnyRule) text += L"；";
		text += L"踏步高度 ≤ ";
		text += FormatMillimeters (g_regulationConfig.riserHeightRule.maxValue.value ());
		hasAnyRule = true;
	}

	if (g_regulationConfig.treadDepthRule.HasMinValue ()) {
		if (hasAnyRule) text += L"；";
		text += L"踏步宽度 ≥ ";
		text += FormatMillimeters (g_regulationConfig.treadDepthRule.minValue.value ());
		hasAnyRule = true;
	}

	if (g_regulationConfig.landingLengthRule.HasMinValue ()) {
		if (hasAnyRule) text += L"；";
		text += L"平台长度 ≥ ";
		text += FormatMillimeters (g_regulationConfig.landingLengthRule.minValue.value ());
		hasAnyRule = true;
	}

	if (g_regulationConfig.twoRPlusGRule.HasMinValue () && g_regulationConfig.twoRPlusGRule.HasMaxValue ()) {
		if (hasAnyRule) text += L"；";
		text += L"2R+G 范围为 ";
		text += FormatMillimeters (g_regulationConfig.twoRPlusGRule.minValue.value ());
		text += L"~";
		text += FormatMillimeters (g_regulationConfig.twoRPlusGRule.maxValue.value ());
		hasAnyRule = true;
	}

	if (!hasAnyRule) {
		return GS::UniString (L"⚠ 规范文件未包含有效的楼梯规则，请重新生成JSON配置文件。");
	}

	text += L"。";
	return text;
}

static void LogDetailedResults (const GS::Array<StairComplianceResult>& results)
{
	for (const StairComplianceResult& result : results) {
		GS::UniString prefix = result.displayName;
		prefix.Append (L"：");

		if (!result.violations.IsEmpty ()) {
			for (const GS::UniString& issue : result.violations) {
				GS::UniString detail = prefix;
				detail.Append (L"违规 — ");
				detail += issue;
				WriteReport (detail);
			}
		} else if (!result.notices.IsEmpty ()) {
			for (const GS::UniString& notice : result.notices) {
				GS::UniString detail = prefix;
				detail.Append (L"提示 — ");
				detail += notice;
				WriteReport (detail);
			}
		} else {
			GS::UniString okLine = prefix;
			okLine.Append (L"符合规范。");
			WriteReport (okLine);
		}

		if (!result.metricsSummary.IsEmpty ()) {
			GS::UniString metricsLine = result.displayName;
			metricsLine.Append (L" — 实测参数：");
			metricsLine += result.metricsSummary;
			WriteReport (metricsLine);
		}
	}
}

static void RunStairComplianceCheck ()
{
	const GS::Array<StairComplianceResult> results = EvaluateStairCompliance ();

	StairCompliancePalette& palette = StairCompliancePalette::GetInstance ();
	palette.EnsureShown ();

	// 动态构建规范文本（从加载的JSON配置）
	const GS::UniString regulationText = BuildRegulationText ();

	// 检查是否加载了有效规范配置
	bool hasValidRegulation = !g_regulationConfig.regulationName.IsEmpty () &&
	                          g_regulationConfig.regulationName != L"未加载规范";

	if (!hasValidRegulation) {
		// 未加载规范，显示警告信息
		const GS::UniString warningMessage (L"⚠ 未加载规范配置\n\n请按照以下步骤操作：\n1. 准备楼梯规范PDF文件\n2. 运行Python工具生成JSON配置文件\n3. 重新启动ArchiCAD或点击刷新按钮\n\n详细说明请查看ArchiCAD报告窗口。");
		palette.UpdateResults (results, warningMessage, regulationText);
		WriteReport (L"━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
		WriteReport (warningMessage);
		WriteReport (L"━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
		return;
	}

	if (results.IsEmpty ()) {
		const GS::UniString message (L"未检测到楼梯元素，请确认模型中存在可校验的楼梯。");
		palette.UpdateResults (results, message, regulationText);
		WriteReport (message);
		WriteReport (regulationText);
		return;
	}

	GS::Array<const StairComplianceResult*> nonCompliant;
	GS::Array<const StairComplianceResult*> needsReview;

	for (const StairComplianceResult& result : results) {
		if (!result.IsCompliant ()) {
			nonCompliant.Push (&result);
		} else if (!result.notices.IsEmpty ()) {
			needsReview.Push (&result);
		}
	}

	const unsigned int totalCount = static_cast<unsigned int> (results.GetSize ());
	const unsigned int nonCompliantCount = static_cast<unsigned int> (nonCompliant.GetSize ());
	const unsigned int reviewCount = static_cast<unsigned int> (needsReview.GetSize ());
	const unsigned int compliantCount = totalCount - nonCompliantCount - reviewCount;

	GS::UniString summary;
	summary.Append (L"共检测 ");
	summary.Append (GS::UniString::Printf ("%u", totalCount));
	summary.Append (L" 个楼梯，其中 ");
	summary.Append (GS::UniString::Printf ("%u", nonCompliantCount));
	summary.Append (L" 个存在违规，");
	summary.Append (GS::UniString::Printf ("%u", reviewCount));
	summary.Append (L" 个需人工复核，");
	summary.Append (GS::UniString::Printf ("%u", compliantCount));
	summary.Append (L" 个符合规范。");

	WriteReport (summary);
	WriteReport (regulationText);
	LogDetailedResults (results);

	palette.UpdateResults (results, summary, regulationText);
}

} // namespace

static GSErrCode __ACENV_CALL MenuCommandHandler (const API_MenuParams* menuParams)
{
	if (menuParams == nullptr)
		return NoError;

	const short menuResId = static_cast<short> (menuParams->menuItemRef.menuResID);
	const short itemIndex = static_cast<short> (menuParams->menuItemRef.itemIndex);

	if (menuResId == kMenuResId && itemIndex == 1) {
		RunStairComplianceCheck ();
	} else if (menuResId == kPaletteMenuResId && itemIndex == 1) {
		StairCompliancePalette::GetInstance ().ToggleFromMenu ();
	}

	return NoError;
}

API_AddonType __ACENV_CALL CheckEnvironment (API_EnvirParams* envir)
{
	envir->addOnInfo.name = RSGetIndString (ID_ADDON_INFO, 1, ACAPI_GetOwnResModule ());
	envir->addOnInfo.description = RSGetIndString (ID_ADDON_INFO, 2, ACAPI_GetOwnResModule ());

	return APIAddon_Normal;
}

GSErrCode __ACENV_CALL RegisterInterface (void)
{
	GSErrCode err = ACAPI_MenuItem_RegisterMenu (kMenuResId, kMenuPromptResId, MenuCode_Tools, MenuFlag_Default);
	if (err != NoError)
		return err;

	err = ACAPI_MenuItem_RegisterMenu (kPaletteMenuResId, kPalettePromptResId, MenuCode_Palettes, MenuFlag_Default);
	return err;
}

GSErrCode __ACENV_CALL Initialize (void)
{
	GSErrCode err = ACAPI_MenuItem_InstallMenuHandler (kMenuResId, MenuCommandHandler);
	if (err != NoError)
		return err;

	err = ACAPI_MenuItem_InstallMenuHandler (kPaletteMenuResId, MenuCommandHandler);
	if (err != NoError)
		return err;

	API_MenuItemRef menuItemRef = {};
	menuItemRef.menuResID = kMenuResId;
	menuItemRef.itemIndex = 1;
	GSFlags flags = 0;
	ACAPI_MenuItem_SetMenuItemFlags (&menuItemRef, &flags, nullptr);
	GS::UniString menuText = ExtractMenuCaption (LoadString (kMenuResId, 1));
	ACAPI_MenuItem_SetMenuItemText (&menuItemRef, nullptr, &menuText);

	API_MenuItemRef paletteMenuRef = {};
	paletteMenuRef.menuResID = kPaletteMenuResId;
	paletteMenuRef.itemIndex = 1;
	GSFlags paletteFlags = 0;
	ACAPI_MenuItem_SetMenuItemFlags (&paletteMenuRef, &paletteFlags, nullptr);
	GS::UniString paletteText = ExtractMenuCaption (LoadString (kPaletteMenuResId, 1));
	ACAPI_MenuItem_SetMenuItemText (&paletteMenuRef, nullptr, &paletteText);

	err = StairCompliancePalette::RegisterPalette ();
	if (err != NoError)
		return err;

	ACAPI_KeepInMemory (true);
	return NoError;
}

GSErrCode __ACENV_CALL FreeData (void)
{
	StairCompliancePalette::UnregisterPalette ();
	return NoError;
}
