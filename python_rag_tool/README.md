# Python RAG Tool - 建筑规范智能提取工具

使用大语言模型（LLM）从PDF建筑规范文档中自动提取楼梯相关数值限制。

## 功能特点

- 📄 **PDF解析**: 使用pdfplumber智能提取PDF文本内容
- 🔍 **关键词定位**: 自动识别包含楼梯规范的页面
- 🤖 **LLM提取**: 使用OpenAI/OpenRouter API提取结构化规范数据
- ✅ **数据验证**: 自动验证提取的数值是否在合理范围内
- 📊 **JSON输出**: 生成标准化的JSON配置文件供ArchiCAD插件使用

## 安装

### 1. 安装Python依赖

```bash
pip install -r requirements.txt
```

### 2. 配置API密钥

复制环境变量模板：
```bash
copy .env.example .env
```

编辑`.env`文件，填入您的API配置：

```ini
# OpenAI API 配置（使用OpenRouter或其他兼容服务）
OPENAI_API_KEY=your-api-key-here
OPENAI_BASE_URL=https://openrouter.ai/api/v1

# LLM模型选择（可选，默认为 openai/gpt-4o-mini）
LLM_MODEL=openai/gpt-4o-mini
```

**推荐模型**：
- `openai/gpt-4o-mini` - 性价比高，速度快
- `google/gemini-2.0-flash-exp:free` - 免费，速度快
- `anthropic/claude-3.5-sonnet` - 效果最好（付费）

## 使用方法

### 方法1：命令行模式

```bash
python src/main.py --pdf "path/to/regulation.pdf" --output "output_name.json"
```

**参数说明**：
- `--pdf`: PDF文件路径
- `--output`: 输出JSON文件名（可选，默认自动生成）

### 方法2：交互模式

```bash
python src/main.py
```

根据提示输入PDF文件路径，系统会自动处理。

### 方法3：从ArchiCAD调用（推荐）

在ArchiCAD插件中点击"Upload PDF"按钮，插件会自动调用此工具。

## 输出格式

生成的JSON文件结构：

```json
{
  "regulation_name": "住宅建筑规范",
  "regulation_code": "GB 50368-2005",
  "riser_height": {
    "min_value": null,
    "max_value": 0.175,
    "unit": "m",
    "source": "第6.4.5条",
    "full_text": "踏步高度不应大于0.175m"
  },
  "tread_depth": {
    "min_value": 0.26,
    "max_value": null,
    "unit": "m",
    "source": "第6.4.5条",
    "full_text": "楼梯踏步宽度不应小于0.26m"
  },
  "two_r_plus_g": {
    "min_value": 0.54,
    "max_value": 0.62,
    "unit": "m",
    "source": "经验公式",
    "full_text": "2R+G应在540~620mm之间"
  },
  "landing_length": {
    "min_value": 1.2,
    "max_value": null,
    "unit": "m",
    "source": "平台要求",
    "full_text": "中间平台宽度不应小于1.20m"
  }
}
```

**重要说明**：
- 所有数值统一使用**米（m）**为单位
- `min_value`/`max_value` 为`null`表示该规则不存在限制
- `source` 字段记录了规范的章节号
- `full_text` 保存完整的规范条文，便于人工验证

## 工作流程

1. **PDF读取** (`pdf_parser.py`)
   - 使用pdfplumber读取PDF文本
   - 提取每一页的文本内容和表格数据

2. **关键词搜索**
   - 搜索包含"楼梯"、"踏步"等关键词的页面
   - 定位最相关的规范条文

3. **LLM提取** (`regulation_extractor.py`)
   - 将相关文本发送给LLM
   - 使用精心设计的提示词（Prompt）引导LLM提取结构化数据
   - LLM自动识别数值、单位、条文编号等信息

4. **数据验证**
   - 检查提取的数值是否在合理范围内（如踏步高度100-250mm）
   - 如有异常值会输出警告

5. **JSON生成** (`config_generator.py`)
   - 将提取的数据转换为标准JSON格式
   - 保存到指定位置（默认：`../shared/current_regulation.json`）

## 文件说明

- `src/main.py` - 主程序入口，处理命令行参数和用户交互
- `src/pdf_parser.py` - PDF解析模块，负责文本提取
- `src/regulation_extractor.py` - LLM提取模块，核心AI处理逻辑
- `src/config_generator.py` - JSON配置文件生成模块
- `requirements.txt` - Python依赖包列表
- `.env.example` - 环境变量配置模板

## 常见问题

### Q: LLM提取的数值不准确怎么办？

A: 可以手动编辑生成的JSON文件修正数值。LLM提取作为初步工作，建议人工复核。

### Q: 支持哪些PDF格式？

A: 最好使用**文字型PDF**。扫描版PDF需要先进行OCR处理（可使用Adobe Acrobat等工具）。

### Q: API调用超时怎么办？

A:
1. 检查网络连接
2. 尝试更换API Base URL
3. 选择速度更快的模型（如gemini-2.0-flash）
4. 程序已设置180秒超时和3次重试

### Q: 成本问题？

A:
- OpenRouter的`gpt-4o-mini`约$0.15/百万token，一次提取约1-2分钱
- Google的`gemini-2.0-flash-exp:free`完全免费

### Q: 提取不到某些规则？

A:
1. 检查PDF中是否确实包含该规则
2. 可能需要在`regulation_extractor.py`中调整提示词（Prompt）
3. 某些特殊格式的规范可能需要定制化处理

## 技术细节

- **PDF解析**: pdfplumber 0.11+
- **LLM调用**: OpenAI Python SDK (兼容OpenRouter等服务)
- **数据模型**: Pydantic v2.x
- **输出格式**: UTF-8编码的JSON（无BOM）

## 开发指南

如需修改提示词或添加新规则：
1. 编辑`regulation_extractor.py`中的`extract_with_llm()`方法
2. 修改Prompt模板以引导LLM提取新类型的规则
3. 在`StairRegulation`数据模型中添加新字段

## 许可证

本工具仅供学习和研究使用。
