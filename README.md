# BuildingCodeChecker_Stair

楼梯建筑规范智能检测系统 - 基于RAG的ArchiCAD插件

## 项目简介

这是一个用于检测ArchiCAD楼梯模型是否符合建筑规范的智能系统。系统由两个主要组件构成：

1. **Python RAG工具** - 使用LLM从PDF规范文档中自动提取楼梯相关数值限制
2. **ArchiCAD C++插件** - 在ArchiCAD中实时检测楼梯模型的合规性


   
[![点击播放视频](https://img.youtube.com/vi/Kpd5oaAZumI/hqdefault.jpg)](https://www.youtube.com/watch?v=Kpd5oaAZumI)

[点击播放演示视频]


  
## 核心功能

✅ **智能规范提取**
- 支持上传任意建筑规范PDF文档
- 使用大语言模型（LLM）自动提取楼梯相关规则
  - 踏步高度限制
  - 踏步宽度/深度限制
  - 2R+G公式范围
  - 平台长度要求

✅ **实时合规性检测**
- 自动读取ArchiCAD模型中的所有楼梯元素
- 与提取的规范进行对比验证
- 直观显示违规项目和详细信息

✅ **用户友好界面**
- 一键上传PDF规范
- 实时显示当前使用的规范
- 清晰的检测结果列表
- 双击定位到违规楼梯

## 快速开始

### 1. Python RAG工具设置

```bash
# 进入工具目录
cd python_rag_tool

# 安装依赖
pip install -r requirements.txt

# 配置API密钥
cp .env.example .env
# 编辑.env文件，填入您的OpenAI/OpenRouter API密钥

# 运行提取工具（可选 - 也可以直接在ArchiCAD中使用Upload PDF按钮）
python src/main.py --pdf "path/to/regulation.pdf"
```

### 2. ArchiCAD插件安装

**方法一：使用预编译插件**
```bash
# 复制插件到ArchiCAD安装目录
copy "archicad_plugin\Build\x64\Debug\BuildingCodeChecker.apx" "C:\Program Files\GRAPHISOFT\ARCHICAD 27\Add-Ons\"
```

**方法二：临时加载**
1. 打开ArchiCAD
2. 选择 `选项` → `Add-On管理器`
3. 点击 `添加` 并选择 `archicad_plugin\Build\x64\Debug\BuildingCodeChecker.apx`

### 3. 使用流程

1. 在ArchiCAD中打开包含楼梯的模型
2. 打开 `窗口` → `面板` → `BuildingCodeChecker` 面板
3. 点击 `上传PDF` 按钮上传建筑规范PDF
4. 等待Python工具自动提取规范（约1-2分钟）
5. 点击 `开始检测` 按钮
6. 查看检测结果列表，双击违规项可定位到对应楼梯

## 项目结构

```
BuildingCodeChecker_Stair/
├── README.md                      # 项目总说明（本文件）
├── docs/                          # 详细文档
│   ├── ARCHITECTURE.md            # 架构设计文档
│   ├── DEVELOPMENT.md             # 开发者指南
│   └── API_REFERENCE.md           # API参考文档
├── python_rag_tool/               # Python RAG提取工具
│   ├── README.md                  # Python工具说明
│   ├── requirements.txt           # Python依赖
│   ├── .env.example               # 环境变量模板
│   ├── src/                       # 源代码
│   │   ├── main.py                # 主程序入口
│   │   ├── pdf_parser.py          # PDF解析模块
│   │   ├── regulation_extractor.py # LLM规范提取模块
│   │   └── config_generator.py     # JSON配置生成模块
│   └── examples/                  # 示例PDF文件（如有）
├── archicad_plugin/               # ArchiCAD C++插件
│   ├── README.md                  # 插件说明
│   ├── Src/                       # C++源代码
│   │   ├── BuildingCodeChecker.cpp  # 主入口
│   │   ├── StairCompliance.cpp/hpp # 合规性检测核心
│   │   ├── RegulationConfig.cpp/hpp # 规范配置管理
│   │   └── StairCompliancePalette.cpp/hpp # UI面板
│   ├── Resources/                 # GRC资源文件
│   ├── Build/                     # 编译输出
│   │   └── x64/Debug/BuildingCodeChecker.apx # 编译好的插件
│   └── BuildingCodeChecker.sln    # Visual Studio解决方案
└── shared/                        # 共享配置
    └── current_regulation.json    # 当前使用的规范JSON（由Python工具生成）
```

## 技术栈

### Python RAG工具
- **语言**: Python 3.8+
- **核心库**:
  - `pdfplumber` - PDF文本提取
  - `openai` - LLM API调用
  - `langchain` - RAG框架
  - `pydantic` - 数据验证

### ArchiCAD插件
- **语言**: C++17
- **API**: ArchiCAD 27 API (27.6003)
- **UI框架**: DG Library (Dialog)
- **编译器**: Visual Studio 2019 (v142)
- **平台**: Windows x64

## 系统要求

- Windows 10/11 (x64)
- ArchiCAD 27
- Python 3.8 或更高版本
- Visual Studio 2019 或更高版本（仅开发者需要）
- OpenAI/OpenRouter API密钥

## 开发指南

详细的开发文档请查看：
- [架构设计](docs/ARCHITECTURE.md)
- [开发者指南](docs/DEVELOPMENT.md)

## 已知限制

1. **LLM准确性**: 提取的规范数值可能存在偏差，建议人工复核
2. **PDF格式**: 最好使用文字型PDF，扫描版PDF识别效果较差
3. **楼梯类型**: 部分异形楼梯（如螺旋梯）的treadDepth可能为0，会被跳过检测
4. **规范范围**: 当前主要支持中国建筑规范（GB系列），其他国家规范可能需要调整提示词

## 许可证

本项目仅供学习和研究使用。

