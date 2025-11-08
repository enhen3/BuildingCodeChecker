# 快速开始指南

本文档帮助您快速搭建并运行BuildingCodeChecker_Stair系统。

## 系统要求

### 必需软件
- ✅ Windows 10/11 (x64)
- ✅ ArchiCAD 27
- ✅ Python 3.8+

### 可选软件（仅开发者需要）
- Visual Studio 2019+ （用于编译C++插件）
- ArchiCAD 27 API Development Kit

## 第一步：Python环境设置

### 1.1 安装Python依赖

```bash
# 进入Python工具目录
cd E:\ArchiCAD_Development_File\BuildingCodeChecker_Stair\python_rag_tool

# 安装依赖包
pip install -r requirements.txt
```

### 1.2 配置API密钥

复制环境变量模板并编辑：

```bash
copy .env.example .env
notepad .env
```

在`.env`文件中填入您的API配置：

```ini
# OpenAI API配置
OPENAI_API_KEY=your-api-key-here
OPENAI_BASE_URL=https://openrouter.ai/api/v1

# 模型选择（可选）
LLM_MODEL=openai/gpt-4o-mini
```

**获取API密钥**：
- OpenRouter: https://openrouter.ai/keys （推荐，支持多种模型）
- OpenAI: https://platform.openai.com/api-keys

### 1.3 测试Python工具（可选）

```bash
python src/main.py --help
```

如看到帮助信息，说明环境配置成功。

## 第二步：安装ArchiCAD插件

### 方法1：直接复制（推荐）

```bash
# 复制插件到ArchiCAD安装目录
copy "archicad_plugin\Build\x64\Debug\ARCHICADAddOn1.apx" ^
     "C:\Program Files\GRAPHISOFT\ARCHICAD 27\Add-Ons\"
```

重启ArchiCAD后插件自动加载。

### 方法2：临时加载

1. 打开ArchiCAD 27
2. 选择 `选项` → `Add-On管理器`
3. 点击 `添加`
4. 选择 `E:\ArchiCAD_Development_File\BuildingCodeChecker_Stair\archicad_plugin\Build\x64\Debug\ARCHICADAddOn1.apx`
5. 点击 `确定`

## 第三步：使用系统

### 3.1 准备工作

1. 在ArchiCAD中创建或打开包含楼梯的项目
2. 准备建筑规范PDF文件（如：建筑设计防火规范.pdf）

### 3.2 打开检测面板

在ArchiCAD菜单中选择：
```
窗口 → 面板 → Stair Compliance
```

### 3.3 上传规范PDF

1. 点击面板中的 `Upload PDF` 按钮
2. 选择您的建筑规范PDF文件
3. 等待Python工具自动提取规范（约1-2分钟）
4. 提取完成后，面板会显示提取的规范信息

**提取过程说明**：
- 命令行窗口会短暂出现（正常现象）
- 可查看 `python_rag_tool\python_output.log` 了解详细进度
- 生成的JSON文件位于 `shared\current_regulation.json`

### 3.4 执行检测

1. 确认面板显示了正确的规范信息
2. 点击 `开始检测` 按钮
3. 系统会自动检测模型中所有楼梯
4. 查看检测结果：
   - ✅ 绿色 = 符合规范
   - ❌ 红色 = 违规
   - 双击违规项可定位到对应楼梯

### 3.5 查看详情

- **规范信息区**：当前使用的规范及限值
- **汇总信息**：总检测数、违规数、合格数
- **详细列表**：每个楼梯的状态和参数

## 常见问题排查

### Q1: Python命令不可用

**症状**：提示 `'python' 不是内部或外部命令`

**解决**：
1. 确认Python已安装：`python --version`
2. 检查Python是否在PATH环境变量中
3. 尝试使用 `python3` 或 `py` 命令

### Q2: API密钥错误

**症状**：Python输出日志显示 `authentication` 错误

**解决**：
1. 检查`.env`文件中的`OPENAI_API_KEY`是否正确
2. 确认API密钥有足够的额度
3. 检查网络连接是否正常

### Q3: 插件加载失败

**症状**：ArchiCAD中找不到Stair Compliance面板

**解决**：
1. 确认ArchiCAD版本为27
2. 检查插件文件是否完整（约600KB）
3. 查看ArchiCAD的Report窗口查找错误信息
4. 尝试完全关闭并重启ArchiCAD

### Q4: 提取的规范不准确

**症状**：JSON中的数值与PDF不符

**解决**：
1. 手动编辑 `shared\current_regulation.json` 修正数值
2. 使用文字型PDF而非扫描版PDF
3. 尝试更换更强大的LLM模型（如claude-3.5-sonnet）

### Q5: 找不到楼梯

**症状**：检测结果显示"未检测到楼梯元素"

**解决**：
1. 确认模型中使用的是ArchiCAD标准楼梯工具
2. 检查楼梯是否在当前楼层
3. 确认楼梯未被隐藏或锁定

## 项目文件说明

### 关键文件位置

```
项目根目录/
├── python_rag_tool/
│   ├── src/main.py              # Python工具主程序
│   ├── .env                     # API密钥配置（需手动创建）
│   └── python_output.log        # Python执行日志
├── archicad_plugin/
│   ├── Build/x64/Debug/
│   │   └── ARCHICADAddOn1.apx   # 编译好的插件
│   └── Src/                     # C++源代码
└── shared/
    └── current_regulation.json   # 当前规范（由Python生成）
```

### 日志文件

- **Python日志**: `python_rag_tool\python_output.log`
  - Python工具执行过程的详细输出
  - PDF解析、LLM调用、JSON生成等信息

- **ArchiCAD Report**: ArchiCAD菜单 → `窗口` → `面板` → `Report`
  - 插件加载状态
  - 规范JSON加载信息
  - 楼梯检测详细过程
  - 错误和警告信息

## 下一步

- 📖 阅读 [README.md](README.md) 了解完整功能
- 🔧 阅读 [archicad_plugin/README.md](archicad_plugin/README.md) 了解插件开发
- 🐍 阅读 [python_rag_tool/README.md](python_rag_tool/README.md) 了解Python工具

## 获取帮助

- 查看ArchiCAD Report窗口的详细日志
- 查看 `python_rag_tool\python_output.log`
- 检查 `shared\current_regulation.json` 是否正确生成
- 参考项目文档中的常见问题部分

---

**祝您使用愉快！** 如有问题，请查阅详细文档或联系开发者。
