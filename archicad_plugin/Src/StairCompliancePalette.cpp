#include "StairCompliancePalette.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "APICommon.h"
#include "ResourceIDs.h"
#include "RegulationConfig.hpp"
#include "File.hpp"

// 外部函数声明
extern RegulationConfig g_regulationConfig;
extern GS::Array<StairComplianceResult> EvaluateStairCompliance ();

namespace {

// 列宽配置的键名
constexpr const char* kColumnWidthKey_Name = "StairCompliance_ColumnWidth_Name";
constexpr const char* kColumnWidthKey_Status = "StairCompliance_ColumnWidth_Status";
constexpr const char* kColumnWidthKey_Detail = "StairCompliance_ColumnWidth_Detail";

static GS::UniString LoadString (short resId, short index)
{
    GS::UniString value = RSGetIndString (resId, index, ACAPI_GetOwnResModule ());
    if (!value.IsEmpty ())
        return value;

    if (resId == ID_COMPLIANCE_STRINGS) {
        switch (index) {
            case 1: return GS::UniString (L"楼梯/检查项");
            case 2: return GS::UniString (L"规范条例");
            case 3: return GS::UniString (L"实测值");
            case 4: return GS::UniString (L"✓ 符合");
            case 5: return GS::UniString (L"⚠ 需复核");
            case 6: return GS::UniString (L"✗ 违规");
            default: break;
        }
    }

    return value;
}

constexpr short kPaletteMenuResId = ID_PALETTE_MENU_STRINGS;
constexpr short kPaletteMenuItemIndex = 1;

// 注意：规范条文现在从JSON配置文件加载，不再使用硬编码

static GS::UniString GetStatusText (const StairComplianceResult& result)
{
    if (!result.violations.IsEmpty ())
        return LoadString (ID_COMPLIANCE_STRINGS, 6);
    return LoadString (ID_COMPLIANCE_STRINGS, 4);
}

} // namespace

StairCompliancePalette* StairCompliancePalette::instance = nullptr;
bool StairCompliancePalette::paletteRegistered = false;

const GS::Guid& StairCompliancePalette::PaletteGuid ()
{
    static const GS::Guid guid ("{4A4A65EA-4049-4B9B-93C1-9F8E9FA55B14}");
    return guid;
}

Int32 StairCompliancePalette::PaletteReferenceId ()
{
    static const Int32 referenceId = GS::CalculateHashValue (PaletteGuid ());
    return referenceId;
}

StairCompliancePalette::StairCompliancePalette () :
    DG::Palette (ACAPI_GetOwnResModule (), ID_COMPLIANCE_PALETTE, ACAPI_GetOwnResModule (), PaletteGuid ()),
    summaryText (GetReference (), ID_COMPLIANCE_SUMMARY),
    uploadPdfButton (GetReference (), ID_UPLOAD_PDF_BUTTON),
    checkNowButton (GetReference (), ID_CHECK_NOW_BUTTON),
    regulationInfoText (GetReference (), ID_REGULATION_INFO_TEXT),
    listBox (GetReference (), ID_COMPLIANCE_LISTBOX)
{
    Attach (*this);
    listBox.Attach (*this);
    uploadPdfButton.Attach (*this);  // 附加上传按钮观察者
    checkNowButton.Attach (*this);    // 附加检测按钮观察者

    summaryText.SetText (GS::UniString (L"汇总信息将在检查后显示"));

    // 初始化规范信息显示
    UpdateRegulationInfo ();

    InitializeListBox ();
    LoadColumnWidths ();  // 加载保存的列宽
    BeginEventProcessing ();
}

StairCompliancePalette::~StairCompliancePalette ()
{
    SaveColumnWidths ();  // 保存列宽
    EndEventProcessing ();
    checkNowButton.Detach (*this);
    uploadPdfButton.Detach (*this);
    listBox.Detach (*this);
    Detach (*this);
}

StairCompliancePalette& StairCompliancePalette::GetInstance ()
{
    if (instance == nullptr)
        instance = new StairCompliancePalette ();
    return *instance;
}

GSErrCode StairCompliancePalette::RegisterPalette ()
{
    if (paletteRegistered)
        return NoError;

    const GSErrCode err = ACAPI_RegisterModelessWindow (
        PaletteReferenceId (),
        PaletteCallback,
        API_PalEnabled_FloorPlan | API_PalEnabled_Section | API_PalEnabled_Elevation |
        API_PalEnabled_InteriorElevation | API_PalEnabled_3D | API_PalEnabled_Detail |
        API_PalEnabled_Worksheet | API_PalEnabled_Layout | API_PalEnabled_DocumentFrom3D,
        GSGuid2APIGuid (PaletteGuid ()));

    if (err == NoError)
        paletteRegistered = true;

    return err;
}

void StairCompliancePalette::UnregisterPalette ()
{
    if (paletteRegistered) {
        ACAPI_UnregisterModelessWindow (PaletteReferenceId ());
        paletteRegistered = false;
    }

    SetMenuItemCheckedState (false);

    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }
}

void StairCompliancePalette::UpdateResults (const GS::Array<StairComplianceResult>& results,
                                            const GS::UniString& summary,
                                            const GS::UniString& /*regulation*/)
{
    storedResults = results;

    UpdateSummary (summary);
    FillListBox (results);
}

void StairCompliancePalette::UpdateSummary (const GS::UniString& summary)
{
    GS::UniString summaryDisplay = L"📊 汇总：";
    summaryDisplay += summary;
    summaryText.SetText (summaryDisplay);
}

void StairCompliancePalette::UpdateRegulationInfo ()
{
    extern RegulationConfig g_regulationConfig;

    GS::UniString info;

    if (g_regulationConfig.regulationName.IsEmpty() || g_regulationConfig.regulationName == L"未加载规范") {
        info = L"【未加载规范】\n";
        info += L"请点击 'Upload PDF' 按钮上传规范PDF文件";
    } else {
        info = L"【当前规范】";
        info += g_regulationConfig.regulationName;
        info += L" (";
        info += g_regulationConfig.regulationCode;
        info += L")\n";

        // 只显示踏步高度和宽度的规范
        if (g_regulationConfig.riserHeightRule.HasMaxValue()) {
            info += L"踏步高度限制: ≤ ";
            double valueMeters = g_regulationConfig.riserHeightRule.maxValue.value();
            info += GS::UniString::Printf(L"%.0f mm", valueMeters * 1000.0);
        } else {
            info += L"踏步高度限制: 未设置";
        }

        if (g_regulationConfig.treadDepthRule.HasMinValue()) {
            info += L"  |  踏步宽度限制: ≥ ";
            double valueMeters = g_regulationConfig.treadDepthRule.minValue.value();
            info += GS::UniString::Printf(L"%.0f mm", valueMeters * 1000.0);
        } else {
            info += L"  |  踏步宽度限制: 未设置";
        }
    }

    regulationInfoText.SetText (info);
}

void StairCompliancePalette::EnsureShown ()
{
    if (!IsVisible ())
        Show ();
    BringToFront ();
    SetMenuItemCheckedState (true);
}

void StairCompliancePalette::HidePalette ()
{
    if (IsVisible ())
        DG::Palette::Hide ();
    SetMenuItemCheckedState (false);
}

void StairCompliancePalette::ToggleFromMenu ()
{
    if (IsVisible ())
        HidePalette ();
    else
        EnsureShown ();
}

void StairCompliancePalette::PanelOpened (const DG::PanelOpenEvent&)
{
    SetMenuItemCheckedState (true);

    // 设置按钮中文文字
    uploadPdfButton.SetText(L"上传PDF");
    checkNowButton.SetText(L"开始检测");

    // 清空之前的检测结果，让用户重新上传PDF和执行检测
    storedResults.Clear();
    listBox.DeleteItem(DG::ListBox::AllItems);
    summaryText.SetText(L"请先上传PDF规范，然后点击'开始检测'按钮");

    // 更新规范信息显示（如果已有规范则显示，否则显示提示）
    UpdateRegulationInfo();
}

void StairCompliancePalette::PanelClosed (const DG::PanelCloseEvent&)
{
    SetMenuItemCheckedState (false);
}

void StairCompliancePalette::ListBoxDoubleClicked (const DG::ListBoxDoubleClickEvent& ev)
{
    if (ev.GetSource () != &listBox)
        return;

    const short row = listBox.GetSelectedItem ();
    SelectResult (row);
}

void StairCompliancePalette::ItemToolTipRequested (const DG::ItemHelpEvent& ev, GS::UniString* toolTipText)
{
    if (ev.GetSource () != &listBox || toolTipText == nullptr)
        return;

    // 获取当前选中的行，显示其tooltip
    const short row = listBox.GetSelectedItem ();
    if (row > 0) {
        GS::UniString tooltip;
        if (rowTooltips.Get (row, &tooltip)) {
            *toolTipText = tooltip;
        }
    }
}

void StairCompliancePalette::ButtonClicked (const DG::ButtonClickEvent& ev)
{
    if (ev.GetSource () == &uploadPdfButton) {
        OnUploadPdfClicked ();
    } else if (ev.GetSource () == &checkNowButton) {
        OnCheckNowClicked ();
    }
}

GSErrCode __ACENV_CALL StairCompliancePalette::PaletteCallback (Int32 referenceID, API_PaletteMessageID messageID, GS::IntPtr param)
{
    if (referenceID != PaletteReferenceId ())
        return NoError;

    switch (messageID) {
        case APIPalMsg_OpenPalette:
            GetInstance ().EnsureShown ();
            break;

        case APIPalMsg_ClosePalette:
            if (instance != nullptr)
                instance->HidePalette ();
            break;

        case APIPalMsg_HidePalette_Begin:
            if (instance != nullptr)
                instance->HidePalette ();
            break;

        case APIPalMsg_HidePalette_End:
            if (instance != nullptr && !instance->IsVisible ())
                instance->EnsureShown ();
            break;

        case APIPalMsg_IsPaletteVisible:
            if (param != 0) {
                bool* isVisible = reinterpret_cast<bool*> (param);
                *isVisible = (instance != nullptr && instance->IsVisible ());
            }
            break;

        default:
            break;
    }

    return NoError;
}

void StairCompliancePalette::InitializeListBox ()
{
    listBox.SetTabFieldCount (3);

    const short totalWidth = listBox.GetItemWidth ();
    const short nameWidth = std::max<short> (200, totalWidth / 3);      // 增加默认宽度 160→200
    const short statusWidth = 280;                                       // 增加默认宽度 110→280 (规范条文较长)
    const short detailWidth = std::max<short> (totalWidth - nameWidth - statusWidth, 150);

    short pos = 0;
    // 启用表头同步,允许用户拖动调整列宽
    listBox.SetHeaderSynchronState (true);
    listBox.SetHeaderItemSize (NameColumn, nameWidth);
    listBox.SetHeaderItemSize (StatusColumn, statusWidth);
    listBox.SetHeaderItemSize (DetailColumn, detailWidth);

    // 设置列属性,使用EndTruncate在文本过长时显示省略号
    listBox.SetTabFieldProperties (NameColumn, pos, pos + nameWidth, DG::ListBox::Left, DG::ListBox::EndTruncate, false);
    pos += nameWidth;
    listBox.SetTabFieldProperties (StatusColumn, pos, pos + statusWidth, DG::ListBox::Left, DG::ListBox::EndTruncate, false);
    pos += statusWidth;
    listBox.SetTabFieldProperties (DetailColumn, pos, pos + detailWidth, DG::ListBox::Left, DG::ListBox::EndTruncate, false);

    listBox.SetHeaderItemText (NameColumn, LoadString (ID_COMPLIANCE_STRINGS, 1));
    listBox.SetHeaderItemText (StatusColumn, LoadString (ID_COMPLIANCE_STRINGS, 2));
    listBox.SetHeaderItemText (DetailColumn, LoadString (ID_COMPLIANCE_STRINGS, 3));

    // 设置列可以调整大小
    listBox.SetHeaderItemSizeableFlag (NameColumn, true);
    listBox.SetHeaderItemSizeableFlag (StatusColumn, true);
    listBox.SetHeaderItemSizeableFlag (DetailColumn, true);
}

void StairCompliancePalette::FillListBox (const GS::Array<StairComplianceResult>& results)
{
    ClearListBox ();
    displayedRowToResult.SetCapacity (results.GetSize () * 5);
    rowTooltips.Clear ();  // 清空tooltip映射

    const auto appendRow = [this](const GS::UniString& name,
                                  const GS::UniString& regulation,
                                  const GS::UniString& measured,
                                  UIndex resultIndex,
                                  const GS::UniString& tooltip = GS::UniString())
    {
        listBox.AppendItem ();
        const short row = listBox.GetItemCount ();
        listBox.SetTabItemText (row, NameColumn, name);
        listBox.SetTabItemText (row, StatusColumn, regulation);
        listBox.SetTabItemText (row, DetailColumn, measured);
        displayedRowToResult.Push (resultIndex);

        // 保存tooltip文本（如果提供）
        if (!tooltip.IsEmpty ()) {
            rowTooltips.Add (row, tooltip);
        }
    };

    const auto formatMM = [](double meters) -> GS::UniString {
        return GS::UniString::Printf (L"%.0lf mm", meters * 1000.0);
    };

    for (UIndex i = 0; i < results.GetSize (); ++i) {
        const StairComplianceResult& result = results[i];

        // 统计违规项数量
        UIndex violationCount = result.violations.GetSize ();

        // 显示所有楼梯（包括符合规范的）
        if (violationCount == 0) {
            // 符合规范的楼梯
            GS::UniString statusText = L"✓ 符合规范";

            // 显示实测参数
            GS::UniString debugInfo;
            debugInfo.Printf (L" [实测: 踏步高度%.0fmm 踏步宽度%.0fmm 2R+G%.0fmm]",
                             result.riserHeight * 1000.0,
                             result.treadDepth * 1000.0,
                             result.twoRPlusGoing * 1000.0);
            statusText.Append (debugInfo);

            appendRow (result.displayName, GS::UniString (), statusText, i);
            continue;  // 跳过下面的违规项显示逻辑
        }

        // 违规的楼梯
        GS::UniString statusText = GetStatusText (result);
        statusText.Append (GS::UniString::Printf (L"（%d项违规）", (int)violationCount));

        // 调试信息：显示楼梯的实测数据
        GS::UniString debugInfo;
        debugInfo.Printf (L"[调试] 踏步高度:%.0fmm 踏步深度:%.0fmm",
                         result.riserHeight * 1000.0,
                         result.treadDepth * 1000.0);
        statusText.Append (L" ");
        statusText.Append (debugInfo);

        appendRow (result.displayName, GS::UniString (), statusText, i);

        // 显示所有违规项（violations中已包含完整的规范条文）
        UIndex violationIndex = 0;
        for (const GS::UniString& violation : result.violations) {
            GS::UniString itemName;
            GS::UniString measuredValue;

            // 根据违规内容判断是哪个参数
            if (violation.Contains (L"踏步高度")) {
                itemName = L"  ├─ 踏步高度";
                measuredValue = formatMM (result.riserHeight) + L" ✗ 超标";
            } else if (violation.Contains (L"踏步宽度") || violation.Contains (L"踏步深度")) {
                itemName = L"  ├─ 踏步宽度/深度";
                measuredValue = formatMM (result.treadDepth) + L" ✗ 不足";
            } else if (violation.Contains (L"2R+G") || violation.Contains (L"步行舒适度") || violation.Contains (L"舒适度")) {
                itemName = L"  ├─ 步行舒适度";
                // 判断是低于下限还是超过上限（简化判断）
                if (result.twoRPlusGoing < 0.57) {
                    measuredValue = formatMM (result.twoRPlusGoing) + L" ✗ 过于陡峭";
                } else {
                    measuredValue = formatMM (result.twoRPlusGoing) + L" ✗ 过于平缓";
                }
            } else if (violation.Contains (L"平台")) {
                itemName = L"  ├─ 平台长度";
                measuredValue = formatMM (result.minLandingLength) + L" ✗ 不足";
            } else if (violation.Contains (L"楼梯") && violation.Contains (L"净宽度")) {
                itemName = L"  ├─ 楼梯净宽度";
                measuredValue = L"需在ARCHICAD中手动测量";
            } else if (violation.Contains (L"栏杆") || violation.Contains (L"扶手")) {
                itemName = L"  ├─ 栏杆扶手高度";
                measuredValue = L"需在ARCHICAD中手动测量";
            } else if (violation.Contains (L"倾斜") || violation.Contains (L"角度")) {
                itemName = L"  ├─ 倾斜角度";
                measuredValue = L"需在ARCHICAD中手动测量";
            } else if (violation.Contains (L"梯段") && violation.Contains (L"间距")) {
                itemName = L"  ├─ 两梯段间距";
                measuredValue = L"需在ARCHICAD中手动测量";
            } else {
                // 未识别的违规项，使用通用显示
                itemName = GS::UniString::Printf (L"  ├─ 违规项 %d", (int)(violationIndex + 1));
                measuredValue = L"详见规范条文";
            }

            // 最后一项使用└─而不是├─
            if (violationIndex == result.violations.GetSize() - 1) {
                itemName.ReplaceAll (L"├", L"└");
            }

            // violation本身就是完整的规范条文
            // 添加tooltip，当规范条文被截断时鼠标悬停显示完整文本
            appendRow (itemName, violation, measuredValue, i, violation);
            violationIndex++;
        }
    }
}

void StairCompliancePalette::ClearListBox ()
{
    listBox.DeleteItem (DG::ListBox::AllItems);
    displayedRowToResult.Clear ();
}

void StairCompliancePalette::SelectResult (short listIndex) const
{
    if (listIndex < 1 || listIndex > static_cast<short> (displayedRowToResult.GetSize ()))
        return;

    const UIndex resultIndex = displayedRowToResult[listIndex - 1];
    if (resultIndex == InvalidResultIndex || resultIndex >= storedResults.GetSize ())
        return;

    const StairComplianceResult& result = storedResults[resultIndex];

    ACAPI_Selection_Select ({ API_Neig (result.guid) }, false);
}

void StairCompliancePalette::SetMenuItemCheckedState (bool isChecked)
{
    API_MenuItemRef itemRef = {};
    GSFlags itemFlags = {};

    itemRef.menuResID = kPaletteMenuResId;
    itemRef.itemIndex = kPaletteMenuItemIndex;

    if (ACAPI_MenuItem_GetMenuItemFlags (&itemRef, &itemFlags) != NoError)
        return;

    if (isChecked)
        itemFlags |= API_MenuItemChecked;
    else
        itemFlags &= ~API_MenuItemChecked;

    ACAPI_MenuItem_SetMenuItemFlags (&itemRef, &itemFlags);
}

void StairCompliancePalette::SaveColumnWidths ()
{
    // 获取当前列宽
    short nameWidth = listBox.GetHeaderItemSize (NameColumn);
    short statusWidth = listBox.GetHeaderItemSize (StatusColumn);
    short detailWidth = listBox.GetHeaderItemSize (DetailColumn);

    // 保存到ARCHICAD偏好设置
    struct PrefsData {
        short nameWidth;
        short statusWidth;
        short detailWidth;
    };

    PrefsData data;
    data.nameWidth = nameWidth;
    data.statusWidth = statusWidth;
    data.detailWidth = detailWidth;

    ACAPI_SetPreferences (1, sizeof(PrefsData), &data);
}

void StairCompliancePalette::LoadColumnWidths ()
{
    // 从ARCHICAD偏好设置加载
    struct PrefsData {
        short nameWidth;
        short statusWidth;
        short detailWidth;
    };

    Int32 version = 0;
    GSSize size = 0;

    // 先获取大小
    if (ACAPI_GetPreferences (&version, &size, nullptr) == NoError && size == sizeof(PrefsData)) {
        PrefsData data;
        // 再获取实际数据
        if (ACAPI_GetPreferences (&version, &size, &data) == NoError && version == 1) {
            // 验证宽度值合理性（避免加载到损坏的数据）
            if (data.nameWidth >= 100 && data.nameWidth <= 500 &&
                data.statusWidth >= 100 && data.statusWidth <= 600 &&
                data.detailWidth >= 100 && data.detailWidth <= 500) {

                listBox.SetHeaderItemSize (NameColumn, data.nameWidth);
                listBox.SetHeaderItemSize (StatusColumn, data.statusWidth);
                listBox.SetHeaderItemSize (DetailColumn, data.detailWidth);
            }
        }
    }
}

void StairCompliancePalette::SetupTooltips ()
{
    // tooltip功能已通过ItemToolTipRequested事件实现
    // 鼠标悬停在规范条例列时会显示完整的规范条文
}

void StairCompliancePalette::OnUploadPdfClicked ()
{
    // 创建文件选择对话框
    DG::FileDialog dialog (DG::FileDialog::OpenFile);
    dialog.SetTitle (GS::UniString (L"选择建筑规范PDF文件"));

    // 设置文件过滤器
    FTM::FileTypeManager fileTypeManager ("PdfFileType");
    FTM::FileType pdfType (nullptr, "pdf", 0, 0, 0);
    FTM::TypeID pdfTypeID = fileTypeManager.AddType (pdfType);
    dialog.AddFilter (pdfTypeID);

    // 显示对话框
    if (dialog.Invoke ()) {
        // GetSelectedFile返回const引用，索引默认为0
        const IO::Location& selectedFile = dialog.GetSelectedFile (0);
        ProcessPdfFile (selectedFile);
    }
}

void StairCompliancePalette::ProcessPdfFile (const IO::Location& pdfLocation)
{
    // 获取PDF文件路径
    IO::Name fileName;
    pdfLocation.GetLastLocalName (&fileName);

    GS::UniString pdfPath = pdfLocation.ToDisplayText ();
    GS::UniString fileNameStr = fileName.ToString ();

    // 更新状态：正在处理
    GS::UniString statusMsg = GS::UniString::Printf (L"📄 正在处理: %s ...", fileNameStr.ToCStr ().Get ());
    summaryText.SetText (statusMsg);

    // 使用统一的输出路径（与LoadRegulationConfigIfNeeded一致）
    GS::UniString jsonPath = USER_REGULATION_JSON_PATH;
    IO::Location jsonLocation (jsonPath);

    // 构建Python命令
    GS::UniString pythonCmd;
    pythonCmd = L"python \"E:\\ArchiCAD_Development\\StairRegulationRAG\\src\\main.py\"";
    pythonCmd += L" --pdf \"" + pdfPath + L"\"";
    pythonCmd += L" --output \"" + jsonPath + L"\"";

    // 更新状态：调用AI分析
    statusMsg = GS::UniString::Printf (L"🤖 正在使用AI分析PDF: %s ...", fileNameStr.ToCStr ().Get ());
    summaryText.SetText (statusMsg);

    // 执行Python命令 - 使用Unicode
    // 将输出重定向到日志文件以便调试
    GS::UniString logPath = L"E:\\ArchiCAD_Development\\python_output.log";
    GS::UniString cmdWithLog = pythonCmd + L" > \"" + logPath + L"\" 2>&1";

    // 在Windows上使用_wsystem来支持Unicode路径
    #ifdef WINDOWS
        int result = _wsystem (cmdWithLog.ToUStr ().Get ());
    #else
        char cmdBuffer[2048];
        strcpy (cmdBuffer, cmdWithLog.ToCStr ().Get ());
        int result = system (cmdBuffer);
    #endif

    if (result != 0) {
        // Python执行失败 - 显示详细错误信息
        statusMsg = GS::UniString::Printf (L"❌ Python执行失败 (代码: %d)\n请查看日志: %s",
                                           result, logPath.ToCStr ().Get ());
        summaryText.SetText (statusMsg);
        return;
    }

    // 检查JSON文件是否生成 - 使用IO::File检查
    IO::File jsonFile (jsonLocation);
    GSErrCode openErr = jsonFile.Open (IO::File::ReadMode);
    if (openErr != NoError) {
        // 文件未生成 - 提供详细的错误信息
        statusMsg = GS::UniString::Printf (
            L"❌ 处理失败: 未找到输出文件\n"
            L"预期路径: %s\n"
            L"请查看Python日志: %s",
            jsonPath.ToCStr ().Get (),
            logPath.ToCStr ().Get ()
        );
        summaryText.SetText (statusMsg);
        return;
    }
    jsonFile.Close ();

    // 更新状态：正在加载配置
    statusMsg = GS::UniString::Printf (L"📥 正在加载新规范配置...");
    summaryText.SetText (statusMsg);

    // 加载JSON配置并更新全局规范
    RegulationConfig newConfig = RegulationConfig::LoadFromJSON (jsonLocation);
    g_regulationConfig = newConfig;

    // 更新规范信息显示
    UpdateRegulationInfo ();

    // 显示加载的配置信息（调试用）
    GS::UniString debugInfo;
    debugInfo.Append (L"📥 已加载规范:\n");
    debugInfo.Append (L"名称: " + newConfig.regulationName + L"\n");
    debugInfo.Append (L"编号: " + newConfig.regulationCode + L"\n");

    if (newConfig.riserHeightRule.HasMaxValue ()) {
        debugInfo.Append (GS::UniString::Printf (L"踏步高度≤%.3fm ", newConfig.riserHeightRule.maxValue.value ()));
    }
    if (newConfig.treadDepthRule.HasMinValue ()) {
        debugInfo.Append (GS::UniString::Printf (L"踏步宽度≥%.3fm ", newConfig.treadDepthRule.minValue.value ()));
    }

    summaryText.SetText (debugInfo);

    // 等待2秒让用户看到配置信息
    // （实际应用中可以移除这个等待）

    // 更新状态：正在重新检查
    statusMsg = L"🔍 正在重新检查所有楼梯...";
    summaryText.SetText (statusMsg);

    // 重新执行检查
    const GS::Array<StairComplianceResult> newResults = EvaluateStairCompliance ();

    if (newResults.IsEmpty ()) {
        statusMsg = L"❌ 未检测到楼梯元素";
        summaryText.SetText (statusMsg);
        return;
    }

    // 生成汇总
    GS::Array<const StairComplianceResult*> nonCompliant;
    for (const StairComplianceResult& result : newResults) {
        if (!result.IsCompliant ()) {
            nonCompliant.Push (&result);
        }
    }

    const unsigned int totalCount = static_cast<unsigned int> (newResults.GetSize ());
    const unsigned int nonCompliantCount = static_cast<unsigned int> (nonCompliant.GetSize ());
    const unsigned int compliantCount = totalCount - nonCompliantCount;

    GS::UniString newSummary;
    newSummary.Append (L"共检测 ");
    newSummary.Append (GS::UniString::Printf ("%u", totalCount));
    newSummary.Append (L" 个楼梯，其中 ");
    newSummary.Append (GS::UniString::Printf ("%u", nonCompliantCount));
    newSummary.Append (L" 个存在违规，");
    newSummary.Append (GS::UniString::Printf ("%u", compliantCount));
    newSummary.Append (L" 个符合规范。");

    // 更新显示
    UpdateResults (newResults, newSummary, newConfig.regulationName);

    // 完成
    statusMsg = GS::UniString::Printf (L"✅ 完成! 已使用新规范 [%s] 重新检查",
                                       newConfig.regulationName.ToCStr ().Get ());
    summaryText.SetText (statusMsg);
}

void StairCompliancePalette::OnCheckNowClicked ()
{
    // 输出日志
    ACAPI_WriteReport(L"━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━", false);
    ACAPI_WriteReport(L"[用户操作] 点击'开始检测'按钮", false);

    // 更新状态
    summaryText.SetText (GS::UniString (L"正在重新加载规范并检测..."));

    // 强制重新加载JSON配置
    ForceReloadRegulationConfig ();

    // 更新规范信息显示
    UpdateRegulationInfo ();

    // 重新检测所有楼梯
    ACAPI_WriteReport(L"[Stair Compliance] 开始检测楼梯...", false);
    const GS::Array<StairComplianceResult> results = EvaluateStairCompliance ();

    if (results.IsEmpty ()) {
        summaryText.SetText (GS::UniString (L"未检测到楼梯元素，请确认模型中存在可校验的楼梯。"));
        ACAPI_WriteReport(L"[Stair Compliance] 未检测到楼梯元素", false);
        return;
    }

    // 统计结果
    unsigned int totalCount = static_cast<unsigned int> (results.GetSize ());
    unsigned int nonCompliantCount = 0;
    unsigned int compliantCount = 0;

    for (const StairComplianceResult& result : results) {
        if (!result.IsCompliant ())
            nonCompliantCount++;
        else
            compliantCount++;
    }

    // 构建汇总信息
    GS::UniString summary;
    summary.Append (GS::UniString (L"【检测结果】共检测 "));
    summary.Append (GS::UniString::Printf ("%u", totalCount));
    summary.Append (GS::UniString (L" 个楼梯，其中 "));
    summary.Append (GS::UniString::Printf ("%u", nonCompliantCount));
    summary.Append (GS::UniString (L" 个存在违规，"));
    summary.Append (GS::UniString::Printf ("%u", compliantCount));
    summary.Append (GS::UniString (L" 个符合规范"));

    // 获取当前规范名称
    extern RegulationConfig g_regulationConfig;
    GS::UniString regulationText;
    if (!g_regulationConfig.regulationName.IsEmpty () &&
        g_regulationConfig.regulationName != L"未加载规范") {
        regulationText = g_regulationConfig.regulationName;
    } else {
        regulationText = L"未加载规范";
    }

    // 更新面板显示
    UpdateResults (results, summary, regulationText);

    // 输出完成日志
    ACAPI_WriteReport(summary.ToCStr ().Get (), false);
    ACAPI_WriteReport(L"[Stair Compliance] ✅ 检测完成!", false);
    ACAPI_WriteReport(L"━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━", false);
}
