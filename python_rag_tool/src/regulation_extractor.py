"""
规范提取模块
使用LLM从文本中提取结构化的楼梯规范数据
"""
import os
import json
import re
from typing import List, Dict, Optional
from pydantic import BaseModel, Field
from rich.console import Console
from openai import OpenAI
from dotenv import load_dotenv

load_dotenv()
console = Console()


# 定义规范数据结构
class RegulationRule(BaseModel):
    """单个规范规则"""
    min_value: Optional[float] = Field(None, description="最小值")
    max_value: Optional[float] = Field(None, description="最大值")
    unit: str = Field("m", description="单位")
    source: Optional[str] = Field("", description="规范来源（章节号）")
    full_text: Optional[str] = Field("", description="完整条文")


class StairRegulation(BaseModel):
    """楼梯规范配置"""
    regulation_name: str = Field("", description="规范名称")
    regulation_code: str = Field("", description="规范编号")
    riser_height: Optional[RegulationRule] = Field(None, description="踏步高度规则")
    tread_depth: Optional[RegulationRule] = Field(None, description="踏步宽度规则")
    two_r_plus_g: Optional[RegulationRule] = Field(None, description="2R+G公式规则")
    landing_length: Optional[RegulationRule] = Field(None, description="平台长度规则")


class RegulationExtractor:
    """规范提取器"""

    def __init__(self, api_key: Optional[str] = None, base_url: Optional[str] = None):
        """
        初始化提取器

        Args:
            api_key: OpenAI API密钥（如果不提供，从环境变量读取）
            base_url: API基础URL（可用于国内API）
        """
        self.api_key = api_key or os.getenv("OPENAI_API_KEY")
        self.base_url = base_url or os.getenv("OPENAI_BASE_URL", "https://api.openai.com/v1")

        if not self.api_key:
            console.print("[red]警告: 未找到OPENAI_API_KEY，请在.env文件中配置[/red]")

        # 增加超时时间到180秒
        self.client = OpenAI(
            api_key=self.api_key,
            base_url=self.base_url,
            timeout=180.0,  # 3分钟超时
            max_retries=3   # 自动重试3次
        )

    def extract_regulation_info(self, text: str) -> Dict:
        """
        从文本中提取规范名称和编号

        Args:
            text: 规范文本

        Returns:
            Dict: 规范信息
        """
        # 使用正则表达式提取规范编号
        patterns = [
            r'GB\s*\d+[-\s]*\d+',  # GB 50368-2005
            r'JGJ\s*\d+[-\s]*\d+',  # JGJ 64-2017
            r'DB\s*\d+/T\s*\d+[-\s]*\d+',  # 地方标准
        ]

        regulation_code = ""
        for pattern in patterns:
            match = re.search(pattern, text, re.IGNORECASE)
            if match:
                regulation_code = match.group()
                break

        # 提取规范名称（通常在标题或首页）
        lines = text.split('\n')
        regulation_name = ""
        for line in lines[:20]:  # 只检查前20行
            if any(word in line for word in ["规范", "标准", "规程", "建筑"]):
                if len(line.strip()) < 50:  # 标题通常不会太长
                    regulation_name = line.strip()
                    break

        return {
            "regulation_name": regulation_name,
            "regulation_code": regulation_code
        }

    def extract_with_llm(self, pages_content: List[Dict]) -> StairRegulation:
        """
        使用LLM提取楼梯规范

        Args:
            pages_content: PDF页面内容列表

        Returns:
            StairRegulation: 提取的规范数据
        """
        # 合并所有楼梯相关文本
        combined_text = "\n\n".join([
            f"=== 第{page['page_number']}页 ===\n{page['text']}"
            for page in pages_content
        ])

        # 限制文本长度（避免超过token限制）
        max_chars = 15000
        if len(combined_text) > max_chars:
            console.print(f"[yellow]文本过长，截取前{max_chars}字符[/yellow]")
            combined_text = combined_text[:max_chars]

        # 提取规范基本信息
        reg_info = self.extract_regulation_info(combined_text)

        console.print("[cyan]正在调用LLM提取规范数据...[/cyan]")

        # 构造提示词
        prompt = f"""
你是一个专业的建筑规范分析专家。请从以下建筑规范文本中提取楼梯相关的数值限制。

规范文本：
{combined_text}

请提取以下信息，并以JSON格式返回：

**基本信息**：
1. **regulation_name**: 规范的完整标题名称（如"住宅建筑规范"、"建筑设计防火规范"等，只提取主标题，不要提取条文内容）
2. **regulation_code**: 规范编号（如"GB 50368-2005"、"JGJ 242-2011"等）

**楼梯规则**：
1. **踏步高度 (riser_height)**：最大值限制
2. **踏步宽度/深度 (tread_depth)**：最小值限制
3. **2R+G公式 (two_r_plus_g)**：最小值和最大值范围
4. **平台长度 (landing_length)**：最小值限制

对于每个规则，请提供：
- min_value: 最小值（如果有）
- max_value: 最大值（如果有）
- unit: 单位（统一转换为米 "m"）
- source: 规范来源（章节号，如"第6.3.2条"）
- full_text: 完整的规范条文

**重要提示**：
- regulation_name必须是规范文档的主标题，不要提取条文的一部分
- 所有数值统一转换为米（m）为单位。例如：170mm = 0.17m, 260mm = 0.26m
- 如果某个规则在文本中没有找到，设置为null
- 确保提取的数值准确无误
- source字段只写章节号，不要包含规范名称

返回JSON格式：
{{
  "regulation_name": "住宅建筑规范",
  "regulation_code": "GB 50368-2005",
  "riser_height": {{
    "max_value": 0.175,
    "unit": "m",
    "source": "第6.3.2条",
    "full_text": "楼梯踏步高度不应大于0.175m"
  }},
  "tread_depth": {{
    "min_value": 0.26,
    "unit": "m",
    "source": "第6.3.2条",
    "full_text": "踏步宽度不应小于0.26m"
  }},
  "two_r_plus_g": {{
    "min_value": 0.54,
    "max_value": 0.62,
    "unit": "m",
    "source": "经验公式",
    "full_text": "2R+G应在540~620mm之间"
  }},
  "landing_length": {{
    "min_value": 1.2,
    "unit": "m",
    "source": "平台要求",
    "full_text": "中间平台宽度不应小于1.20m"
  }}
}}

只返回JSON，不要添加其他说明文字。
"""

        try:
            # 调用LLM
            # OpenRouter支持的模型，按推荐顺序：
            # 1. google/gemini-2.0-flash-exp:free (免费，速度快)
            # 2. openai/gpt-4o-mini (性价比高)
            # 3. anthropic/claude-3.5-sonnet (效果最好)
            model = os.getenv("LLM_MODEL", "openai/gpt-4o-mini")

            console.print(f"[cyan]使用模型: {model}[/cyan]")
            console.print("[yellow]正在调用LLM API...（这可能需要1-2分钟，请耐心等待）[/yellow]")

            response = self.client.chat.completions.create(
                model=model,
                messages=[
                    {"role": "system", "content": "你是建筑规范分析专家，擅长从规范文本中提取结构化数据。"},
                    {"role": "user", "content": prompt}
                ],
                temperature=0.1,  # 降低温度以提高准确性
                response_format={"type": "json_object"}
            )

            # 解析响应
            result_text = response.choices[0].message.content
            console.print("[green][OK] LLM提取完成[/green]")

            # 解析JSON
            extracted_data = json.loads(result_text)
            console.print("[green][OK] JSON解析成功[/green]")

            # 辅助函数：清理None值为空字符串
            def clean_rule_data(rule_dict):
                if not rule_dict:
                    return {}
                cleaned = {}
                for key, value in rule_dict.items():
                    if value is None and key in ['source', 'full_text', 'unit']:
                        cleaned[key] = ""
                    else:
                        cleaned[key] = value
                return cleaned

            # 构建StairRegulation对象（优先使用LLM提取的regulation_name/code，如果没有则使用正则提取的）
            regulation = StairRegulation(
                regulation_name=extracted_data.get("regulation_name") or reg_info["regulation_name"] or "未知规范",
                regulation_code=extracted_data.get("regulation_code") or reg_info["regulation_code"] or "",
                riser_height=RegulationRule(**clean_rule_data(extracted_data.get("riser_height"))) if extracted_data.get("riser_height") else None,
                tread_depth=RegulationRule(**clean_rule_data(extracted_data.get("tread_depth"))) if extracted_data.get("tread_depth") else None,
                two_r_plus_g=RegulationRule(**clean_rule_data(extracted_data.get("two_r_plus_g"))) if extracted_data.get("two_r_plus_g") else None,
                landing_length=RegulationRule(**clean_rule_data(extracted_data.get("landing_length"))) if extracted_data.get("landing_length") else None
            )

            return regulation

        except json.JSONDecodeError as e:
            console.print(f"[red][ERROR] JSON解析失败: {e}[/red]")
            console.print(f"[yellow]LLM返回的原始内容:\n{result_text[:500]}...[/yellow]")
            raise
        except Exception as e:
            error_type = type(e).__name__
            console.print(f"[red][ERROR] LLM提取失败 ({error_type}): {e}[/red]")
            if "timeout" in str(e).lower():
                console.print("[yellow]提示: 网络超时，请检查网络连接或稍后重试[/yellow]")
            elif "rate_limit" in str(e).lower():
                console.print("[yellow]提示: API调用频率限制，请稍后重试[/yellow]")
            elif "authentication" in str(e).lower():
                console.print("[yellow]提示: API密钥无效，请检查.env文件中的OPENAI_API_KEY[/yellow]")
            raise

    def validate_and_fix(self, regulation: StairRegulation) -> StairRegulation:
        """
        验证并修正提取的数据

        Args:
            regulation: 提取的规范数据

        Returns:
            StairRegulation: 修正后的数据
        """
        console.print("[cyan]正在验证数据...[/cyan]")

        warnings = []

        # 检查踏步高度
        if regulation.riser_height:
            if regulation.riser_height.max_value:
                if not (0.10 <= regulation.riser_height.max_value <= 0.25):
                    warnings.append(f"踏步高度异常: {regulation.riser_height.max_value}m")

        # 检查踏步宽度
        if regulation.tread_depth:
            if regulation.tread_depth.min_value:
                if not (0.20 <= regulation.tread_depth.min_value <= 0.40):
                    warnings.append(f"踏步宽度异常: {regulation.tread_depth.min_value}m")

        # 检查2R+G
        if regulation.two_r_plus_g:
            if regulation.two_r_plus_g.min_value and regulation.two_r_plus_g.max_value:
                if not (0.50 <= regulation.two_r_plus_g.min_value <= 0.70):
                    warnings.append(f"2R+G最小值异常: {regulation.two_r_plus_g.min_value}m")
                if not (0.50 <= regulation.two_r_plus_g.max_value <= 0.70):
                    warnings.append(f"2R+G最大值异常: {regulation.two_r_plus_g.max_value}m")

        if warnings:
            console.print("[yellow][WARNING] 发现异常数据:[/yellow]")
            for warning in warnings:
                console.print(f"  - {warning}")
        else:
            console.print("[green][OK] 数据验证通过[/green]")

        return regulation


if __name__ == "__main__":
    # 测试代码
    test_text = """
    住宅建筑规范 GB 50368-2005

    6.3 楼梯
    6.3.2 楼梯梯段净宽不应小于1.10m，踏步宽度不应小于0.26m，踏步高度不应大于0.175m。
    """

    extractor = RegulationExtractor()
    # extractor.extract_with_llm([{"page_number": 1, "text": test_text}])
