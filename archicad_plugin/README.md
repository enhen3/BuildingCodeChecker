# BuildingCodeChecker - 楼梯规范检测插件

基于ArchiCAD 27 API的C++插件，用于实时检测楼梯模型的建筑规范合规性。

## 功能特点

- 🔍 **自动检测**: 读取模型中所有楼梯元素，提取几何参数
- 📏 **合规性验证**: 对照JSON规范配置，验证踏步高度、宽度等参数
- 🖥️ **友好界面**: 基于DG Library的面板UI，实时显示检测结果
- 📤 **PDF上传**: 集成Python RAG工具，一键上传规范PDF
- 🎯 **快速定位**: 双击违规项直接跳转到对应楼梯
- 💾 **配置持久化**: 规范配置通过JSON文件加载，无需重新编译

## 安装方法

### 方法1：直接使用预编译插件

```bash
# 复制插件到ArchiCAD安装目录
copy Build\x64\Debug\BuildingCodeChecker.apx "C:\Program Files\GRAPHISOFT\ARCHICAD 27\Add-Ons\"
```

重启ArchiCAD后插件自动加载。

### 方法2：临时加载（推荐用于测试）

1. 打开ArchiCAD 27
2. 选择菜单 `选项` → `Add-On管理器`
3. 点击 `添加` 按钮
4. 选择 `Build\x64\Debug\BuildingCodeChecker.apx`
5. 点击 `确定`

## 使用指南

### 1. 打开面板

在ArchiCAD菜单中选择：
```
窗口 → 面板 → Stair Compliance
```

### 2. 上传规范PDF

1. 点击面板中的 `Upload PDF` 按钮
2. 选择建筑规范PDF文件（如：建筑设计防火规范.pdf）
3. 等待Python工具自动提取规范（约1-2分钟）
4. 提取完成后，规范信息区会自动更新显示

### 3. 执行检测

1. 确保ArchiCAD模型中有楼梯元素
2. 点击 `开始检测` 按钮
3. 查看检测结果列表：
   - ✅ 绿色 = 符合规范
   - ❌ 红色 = 存在违规
4. 双击违规项可在模型中定位到对应楼梯

### 4. 查看详情

- **规范信息区**: 显示当前使用的规范名称和限值
- **汇总信息**: 显示总检测数、违规数、合格数
- **详细列表**: 每个楼梯的检测状态和实测参数

## 文件结构

```
archicad_plugin/
├── README.md                      # 本文件
├── BuildingCodeChecker.sln        # Visual Studio 解决方案
├── BuildingCodeChecker.vcxproj    # 项目文件
├── Src/                           # C++源代码
│   ├── BuildingCodeChecker.cpp    # 插件主入口
│   ├── StairCompliance.cpp/hpp    # 合规性检测核心逻辑
│   ├── RegulationConfig.cpp/hpp   # JSON配置管理
│   ├── StairCompliancePalette.cpp/hpp # UI面板实现
│   └── ...其他辅助文件
├── Resources/                     # GRC资源文件
│   └── BuildingCodeCheckerFix.grc # 面板UI定义
├── RFIX/, RFIX.WIN/, RINT/        # 编译生成的资源
└── Build/                         # 编译输出
    └── x64/Debug/
        └── BuildingCodeChecker.apx # 最终插件文件
```

## 核心模块说明

### 1. StairCompliance.cpp - 检测核心

**主要函数**：
- `EvaluateStairCompliance()` - 遍历所有楼梯，执行合规性检查
- `LoadRegulationConfigIfNeeded()` - 从JSON加载规范配置
- `CheckRiserHeight()` - 检查踏步高度
- `CheckTreadDepth()` - 检查踏步宽度/深度

**工作流程**：
1. 使用`ACAPI_Element_GetElemList(API_StairID)`获取所有楼梯
2. 对每个楼梯调用`ACAPI_Element_Get()`获取几何参数
3. 与全局配置`g_regulationConfig`比对
4. 返回`GS::Array<StairComplianceResult>`结果数组

### 2. RegulationConfig.cpp - 配置管理

**主要函数**：
- `LoadFromJSON()` - 从JSON文件加载规范
  - 读取文件（处理UTF-8编码，解决EOF问题）
  - 手动解析JSON（ArchiCAD 27 API限制）
  - 使用`sscanf`提取数值（需指定`CC_UTF8`编码）
- `ParseRule()` - 解析单个规则的min/max/unit/source

**关键修复**：
- ✅ UTF-8编码处理：`ToCStr(CC_UTF8).Get()`
- ✅ EOF处理：检查`bytesRead > 0`而不是`err == NoError`
- ✅ 避免Printf崩溃：使用字符串连接代替`Printf("%s")`

### 3. StairCompliancePalette.cpp - UI面板

**主要函数**：
- `PanelOpened()` - 面板打开时清空旧结果
- `ButtonClicked()` - 处理按钮点击（Upload PDF / 开始检测）
- `UpdateResults()` - 更新检测结果显示
- `UpdateRegulationInfo()` - 更新规范信息显示
- `ListBoxDoubleClicked()` - 双击定位楼梯

## 编译指南

### 系统要求

- Windows 10/11 x64
- Visual Studio 2019 或更高版本
- ArchiCAD 27 API Development Kit (27.6003)

### 编译步骤

**前提条件**：需要完整的ArchiCAD API Development Kit，包含：
- `Support/Inc/` - API头文件
- `Support/Modules/` - API模块库
- `Support/Tools/` - 资源编译工具

**注意**: 由于项目依赖ArchiCAD SDK（体积较大且有版权限制），此仓库仅包含源代码。完整编译需要：

1. 下载ArchiCAD 27 API Development Kit
2. 将本项目放置在SDK的`Examples/`目录下，或
3. 修改项目文件中的SDK路径引用

**编译命令**：

```bash
# 使用MSBuild（推荐）
"C:\Program Files\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" ^
  BuildingCodeChecker.sln /t:Rebuild /p:Configuration=Debug /p:Platform=x64

# 或使用Visual Studio
# 打开BuildingCodeChecker.sln，选择 Build → Rebuild Solution
```

**输出位置**：
```
Build\x64\Debug\BuildingCodeChecker.apx
```

## 技术要点

### ArchiCAD API使用

**获取楼梯列表**：
```cpp
GS::Array<API_Guid> stairGuids;
ACAPI_Element_GetElemList(API_StairID, &stairGuids);
```

**读取楼梯参数**：
```cpp
API_Element element = {};
element.header.guid = stairGuid;
ACAPI_Element_Get(&element);

double riserHeight = element.stair.riserHeight;  // 米
double treadDepth = element.stair.treadDepth;    // 米
```

### UTF-8字符串处理

ArchiCAD的`GS::UniString`在Windows上默认返回宽字符（UTF-16LE）：

```cpp
// ❌ 错误：返回wchar_t*，sscanf无法正确解析
sscanf(str.ToCStr().Get(), "%lf", &value);

// ✅ 正确：显式请求UTF-8编码
sscanf(str.ToCStr(CC_UTF8).Get(), "%lf", &value);
```

### 文件读取EOF处理

小文件（<4096字节）第一次`ReadBin`就会返回EOF错误码：

```cpp
// ❌ 错误：跳过了成功读取的数据
if (err == NoError && bytesRead > 0) { ... }

// ✅ 正确：只要有数据就处理
if (bytesRead > 0) { ... }
```

## 调试技巧

### 1. 查看ArchiCAD Report日志

所有`ACAPI_WriteReport()`的输出会显示在：
```
窗口 → 面板 → Report
```

### 2. Visual Studio调试

1. 在VS中设置断点
2. 按F5启动调试
3. 选择ArchiCAD.exe作为调试目标
4. ArchiCAD会加载插件，命中断点

### 3. 输出调试信息

```cpp
ACAPI_WriteReport(L"[DEBUG] 变量值: ...", false);

// 格式化输出
GS::UniString msg = GS::UniString::Printf(L"value = %.6f", value);
ACAPI_WriteReport(msg.ToCStr().Get(), false);
```

## 常见问题

### Q: 插件加载失败？

A: 检查：
1. ArchiCAD版本是否为27
2. 插件是否为x64版本
3. 查看Report窗口的错误信息

### Q: 无法找到楼梯？

A: 确保模型中的楼梯是标准的ArchiCAD楼梯工具创建的（不是普通对象）

### Q: 检测结果不准确？

A:
1. 检查JSON规范文件是否正确
2. 查看Report日志中的调试信息
3. 确认楼梯的riserHeight/treadDepth参数是否正确

### Q: 修改代码后如何更新？

A:
1. 重新编译生成.apx文件
2. 完全关闭ArchiCAD
3. 替换插件文件
4. 重新启动ArchiCAD

## 许可证

本插件基于ArchiCAD API开发，需遵守GRAPHISOFT的API使用协议。仅供学习和研究使用。
