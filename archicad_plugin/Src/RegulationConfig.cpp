#include "RegulationConfig.hpp"
#include "File.hpp"

RegulationConfig RegulationConfig::LoadFromJSON(const IO::Location& jsonPath) {
    RegulationConfig config;

    // 调试：输出正在读取的文件路径
    GS::UniString debugMsg = L"\n[LoadFromJSON] 尝试加载JSON文件:\n  路径: ";
    debugMsg += USER_REGULATION_JSON_PATH;
    debugMsg += L"\n";
    ACAPI_WriteReport(debugMsg.ToCStr().Get(), false);

    try {
        // 读取JSON文件
        IO::File jsonFile (jsonPath);
        GSErrCode err = jsonFile.Open (IO::File::ReadMode);
        if (err != NoError || !jsonFile.IsOpen ()) {
            GS::UniString errMsg;
            errMsg.Printf(L"[LoadFromJSON] ✗ 文件打开失败, GSErrCode=%d\n", (int)err);
            ACAPI_WriteReport(errMsg.ToCStr().Get(), false);
            return GetDefault();
        }

        ACAPI_WriteReport(L"[LoadFromJSON] ✓ 文件打开成功\n", false);

        // 读取文件内容（明确指定UTF-8编码）
        GS::UniString jsonContent;
        char buffer[4096];
        USize bytesRead = 0;
        USize totalBytesRead = 0;
        int readCount = 0;

        ACAPI_WriteReport(L"[LoadFromJSON] 准备开始读取文件内容...\n", false);

        GSErrCode readErr = jsonFile.ReadBin (buffer, sizeof(buffer) - 1, &bytesRead);

        GS::UniString firstReadDebug = L"[LoadFromJSON] 第一次ReadBin调用: err=" + GS::UniString::Printf(L"%d", (int)readErr) +
                                       L", bytesRead=" + GS::UniString::Printf(L"%d", (int)bytesRead) + L"\n";
        ACAPI_WriteReport(firstReadDebug.ToCStr().Get(), false);

        // 关键修复：只要读取到数据就处理，不管是否EOF（小文件第一次读取就会返回EOF）
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            GS::UniString chunk(buffer, CC_UTF8);  // 明确指定UTF-8编码
            jsonContent.Append(chunk);
            totalBytesRead += bytesRead;
            readCount++;

            ACAPI_WriteReport(L"[LoadFromJSON] ✓ 第一次读取的数据已处理\n", false);

            // 继续读取剩余内容（如果有的话）
            while (jsonFile.ReadBin (buffer, sizeof(buffer) - 1, &bytesRead) == NoError && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                GS::UniString chunk2(buffer, CC_UTF8);
                jsonContent.Append(chunk2);
                totalBytesRead += bytesRead;
                readCount++;
            }
        } else {
            ACAPI_WriteReport(L"[LoadFromJSON] ✗ 第一次ReadBin没有读取到数据\n", false);
        }

        jsonFile.Close ();

        GS::UniString sizeDebug = L"[LoadFromJSON] 文件读取完成: 总共读取" + GS::UniString::Printf(L"%d", (int)totalBytesRead) +
                                  L"字节, jsonContent长度=" + GS::UniString::Printf(L"%d", (int)jsonContent.GetLength()) + L"字符\n";
        ACAPI_WriteReport(sizeDebug.ToCStr().Get(), false);

        // 调试：输出JSON内容预览（前500字符）
        GS::UniString preview = jsonContent.GetSubstring(0, std::min(500, (int)jsonContent.GetLength()));
        GS::UniString previewMsg = L"[LoadFromJSON] JSON内容预览（前500字符）:\n";
        previewMsg += preview;
        previewMsg += L"\n...\n";
        ACAPI_WriteReport(previewMsg.ToCStr().Get(), false);

        // 简单的JSON解析（手动解析关键字段）
        // 提取regulation_name
        Int32 nameStart = jsonContent.FindFirst (L"\"regulation_name\"");
        if (nameStart >= 0) {
            Int32 valueStart = jsonContent.FindFirst (L":", nameStart) + 1;
            Int32 quoteStart = jsonContent.FindFirst (L"\"", valueStart) + 1;
            Int32 quoteEnd = jsonContent.FindFirst (L"\"", quoteStart);
            if (quoteStart > 0 && quoteEnd > quoteStart) {
                config.regulationName = jsonContent.GetSubstring (quoteStart, quoteEnd - quoteStart);
            }
        }

        // 提取regulation_code
        Int32 codeStart = jsonContent.FindFirst (L"\"regulation_code\"");
        if (codeStart >= 0) {
            Int32 valueStart = jsonContent.FindFirst (L":", codeStart) + 1;
            Int32 quoteStart = jsonContent.FindFirst (L"\"", valueStart) + 1;
            Int32 quoteEnd = jsonContent.FindFirst (L"\"", quoteStart);
            if (quoteStart > 0 && quoteEnd > quoteStart) {
                config.regulationCode = jsonContent.GetSubstring (quoteStart, quoteEnd - quoteStart);
            }
        }

        // 调试：输出解析的基本信息
        GS::UniString basicInfo = L"\n[LoadFromJSON] 解析基本信息:\n";
        basicInfo += L"  regulation_name: ";
        basicInfo += config.regulationName.IsEmpty() ? L"(空)" : config.regulationName;
        basicInfo += L"\n  regulation_code: ";
        basicInfo += config.regulationCode.IsEmpty() ? L"(空)" : config.regulationCode;
        basicInfo += L"\n";
        ACAPI_WriteReport(basicInfo.ToCStr().Get(), false);

        // 解析规则
        ACAPI_WriteReport(L"[LoadFromJSON] 准备调用ParseRule解析规则...\n", false);
        config.riserHeightRule = ParseRule (jsonContent, L"riser_height");
        ACAPI_WriteReport(L"[LoadFromJSON] riser_height解析完成\n", false);

        config.treadDepthRule = ParseRule (jsonContent, L"tread_depth");
        ACAPI_WriteReport(L"[LoadFromJSON] tread_depth解析完成\n", false);

        config.twoRPlusGRule = ParseRule (jsonContent, L"two_r_plus_g");
        ACAPI_WriteReport(L"[LoadFromJSON] two_r_plus_g解析完成\n", false);

        config.landingLengthRule = ParseRule (jsonContent, L"landing_length");
        ACAPI_WriteReport(L"[LoadFromJSON] landing_length解析完成\n", false);

        // 验证至少有一个规则被成功解析（这是关键，不强制要求regulation_name）
        bool hasAnyRule = config.riserHeightRule.HasMinValue() ||
                          config.riserHeightRule.HasMaxValue() ||
                          config.treadDepthRule.HasMinValue() ||
                          config.treadDepthRule.HasMaxValue() ||
                          config.twoRPlusGRule.HasMinValue() ||
                          config.twoRPlusGRule.HasMaxValue();

        if (!hasAnyRule) {
            GS::UniString msg = L"[RegulationConfig] JSON解析失败：没有规则被成功解析\n";
            msg += L"  riser_height maxValue: ";
            msg += config.riserHeightRule.HasMaxValue() ? L"有" : L"无";
            msg += L"\n  tread_depth minValue: ";
            msg += config.treadDepthRule.HasMinValue() ? L"有" : L"无";
            ACAPI_WriteReport(msg.ToCStr().Get(), false);
            return GetDefault();
        }

        // 如果regulation_name解析失败，使用默认名称（但仍然使用成功解析的规则值）
        if (config.regulationName.IsEmpty()) {
            config.regulationName = L"已提取规范";
            ACAPI_WriteReport(L"[RegulationConfig] ⚠ regulation_name解析失败，使用默认名称，但规则值已成功加载\n", false);
        }
        if (config.regulationCode.IsEmpty()) {
            config.regulationCode = L"从PDF提取";
        }

        return config;

    } catch (...) {
        // 解析失败,返回默认配置
        return GetDefault();
    }
}

// 辅助函数：解析单个规则
RegulationRule RegulationConfig::ParseRule(const GS::UniString& jsonContent, const GS::UniString& ruleName) {
    RegulationRule rule;

    GS::UniString enterMsg = L"\n[ParseRule] 开始解析规则: " + ruleName + L"\n";
    ACAPI_WriteReport(enterMsg.ToCStr().Get(), false);

    // 查找规则块
    GS::UniString searchKey = L"\"" + ruleName + L"\"";
    Int32 ruleStart = jsonContent.FindFirst (searchKey);
    if (ruleStart < 0) {
        GS::UniString notFoundMsg = L"[ParseRule] " + ruleName + L": 在JSON中未找到此键\n";
        ACAPI_WriteReport(notFoundMsg.ToCStr().Get(), false);
        return rule;
    }

    // 查找规则块的结束位置（下一个}）
    Int32 blockStart = jsonContent.FindFirst (L"{", ruleStart);
    Int32 blockEnd = jsonContent.FindFirst (L"}", blockStart);
    if (blockStart < 0 || blockEnd < 0) return rule;

    GS::UniString ruleBlock = jsonContent.GetSubstring (blockStart, blockEnd - blockStart);

    // 解析min_value
    Int32 minPos = ruleBlock.FindFirst (L"\"min_value\"");
    if (minPos >= 0) {
        Int32 colonPos = ruleBlock.FindFirst (L":", minPos);
        Int32 commaPos = ruleBlock.FindFirst (L",", colonPos);
        if (colonPos >= 0) {
            GS::UniString valueStr = ruleBlock.GetSubstring (colonPos + 1, commaPos - colonPos - 1);
            valueStr.Trim ();

            // 调试：显示原始提取的字符串
            GS::UniString parseDebug;
            parseDebug.Printf(L"[ParseRule] %s min_value 原始字符串: '%s'\n",
                ruleName.ToCStr().Get(), valueStr.ToCStr().Get());
            ACAPI_WriteReport(parseDebug.ToCStr().Get(), false);

            if (!valueStr.Contains (L"null")) {
                double value = 0.0;
                // 显式指定CC_UTF8编码，确保sscanf能正确解析窄字符串
                int scanResult = sscanf (valueStr.ToCStr (CC_UTF8).Get (), "%lf", &value);

                GS::UniString scanDebug;
                scanDebug.Printf(L"[ParseRule] %s min_value sscanf结果: scanResult=%d, value=%.6f\n",
                    ruleName.ToCStr().Get(), scanResult, value);
                ACAPI_WriteReport(scanDebug.ToCStr().Get(), false);

                if (scanResult == 1) {
                    rule.minValue = value;
                }
            }
        }
    }

    // 解析max_value
    Int32 maxPos = ruleBlock.FindFirst (L"\"max_value\"");
    if (maxPos >= 0) {
        Int32 colonPos = ruleBlock.FindFirst (L":", maxPos);
        Int32 commaPos = ruleBlock.FindFirst (L",", colonPos);
        if (colonPos >= 0) {
            GS::UniString valueStr = ruleBlock.GetSubstring (colonPos + 1, commaPos - colonPos - 1);
            valueStr.Trim ();

            // 调试：显示原始提取的字符串
            GS::UniString parseDebug;
            parseDebug.Printf(L"[ParseRule] %s max_value 原始字符串: '%s'\n",
                ruleName.ToCStr().Get(), valueStr.ToCStr().Get());
            ACAPI_WriteReport(parseDebug.ToCStr().Get(), false);

            if (!valueStr.Contains (L"null")) {
                double value = 0.0;
                // 显式指定CC_UTF8编码，确保sscanf能正确解析窄字符串
                int scanResult = sscanf (valueStr.ToCStr (CC_UTF8).Get (), "%lf", &value);

                GS::UniString scanDebug;
                scanDebug.Printf(L"[ParseRule] %s max_value sscanf结果: scanResult=%d, value=%.6f\n",
                    ruleName.ToCStr().Get(), scanResult, value);
                ACAPI_WriteReport(scanDebug.ToCStr().Get(), false);

                if (scanResult == 1) {
                    rule.maxValue = value;
                }
            }
        }
    }

    // 解析unit
    Int32 unitPos = ruleBlock.FindFirst (L"\"unit\"");
    if (unitPos >= 0) {
        Int32 colonPos = ruleBlock.FindFirst (L":", unitPos);
        Int32 quoteStart = ruleBlock.FindFirst (L"\"", colonPos + 1) + 1;
        Int32 quoteEnd = ruleBlock.FindFirst (L"\"", quoteStart);
        if (quoteStart > 0 && quoteEnd > quoteStart) {
            rule.unit = ruleBlock.GetSubstring (quoteStart, quoteEnd - quoteStart);
        }
    }

    // 解析source
    Int32 sourcePos = ruleBlock.FindFirst (L"\"source\"");
    if (sourcePos >= 0) {
        Int32 colonPos = ruleBlock.FindFirst (L":", sourcePos);
        Int32 quoteStart = ruleBlock.FindFirst (L"\"", colonPos + 1) + 1;
        Int32 quoteEnd = ruleBlock.FindFirst (L"\"", quoteStart);
        if (quoteStart > 0 && quoteEnd > quoteStart) {
            rule.source = ruleBlock.GetSubstring (quoteStart, quoteEnd - quoteStart);
        }
    }

    // 解析full_text
    Int32 textPos = ruleBlock.FindFirst (L"\"full_text\"");
    if (textPos >= 0) {
        Int32 colonPos = ruleBlock.FindFirst (L":", textPos);
        Int32 quoteStart = ruleBlock.FindFirst (L"\"", colonPos + 1) + 1;
        Int32 quoteEnd = ruleBlock.FindFirst (L"\"", quoteStart);
        if (quoteStart > 0 && quoteEnd > quoteStart) {
            rule.fullText = ruleBlock.GetSubstring (quoteStart, quoteEnd - quoteStart);
        }
    }

    // 添加调试日志
    GS::UniString debugMsg = L"[ParseRule] ";
    debugMsg += ruleName;
    debugMsg += L": min=";
    if (rule.HasMinValue()) {
        debugMsg += GS::UniString::Printf(L"%.3f", rule.minValue.value());
    } else {
        debugMsg += L"null";
    }
    debugMsg += L", max=";
    if (rule.HasMaxValue()) {
        debugMsg += GS::UniString::Printf(L"%.3f", rule.maxValue.value());
    } else {
        debugMsg += L"null";
    }
    debugMsg += L", source=";
    debugMsg += rule.source;
    ACAPI_WriteReport(debugMsg.ToCStr().Get(), false);

    return rule;
}

RegulationConfig RegulationConfig::GetDefault() {
    RegulationConfig config;

    // 返回空配置，提示用户上传规范
    config.regulationName = L"未加载规范";
    config.regulationCode = L"请上传规范PDF文件";

    return config;
}

bool RegulationConfig::SaveToJSON(const IO::Location& /*jsonPath*/) const {
    // TODO: 实现保存功能（可选）
    return false;
}
