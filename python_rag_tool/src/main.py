"""
楼梯规范提取工具 - 主程序
"""
import sys
import argparse
from pathlib import Path
from rich.console import Console
from rich.prompt import Prompt, Confirm
from rich.panel import Panel

from pdf_parser import PDFParser
from regulation_extractor import RegulationExtractor
from config_generator import ConfigGenerator

console = Console()


def print_banner():
    """打印程序标题"""
    banner = """
    ╔═══════════════════════════════════════════════════════════╗
    ║                                                           ║
    ║         楼梯规范提取工具 (Stair Regulation RAG)          ║
    ║                                                           ║
    ║         基于RAG技术自动提取建筑规范中的楼梯限制          ║
    ║                                                           ║
    ╚═══════════════════════════════════════════════════════════╝
    """
    console.print(banner, style="bold blue")


def process_pdf(pdf_path: str, output_path: str = None, generate_cpp: bool = False):
    """
    处理PDF文件并生成配置

    Args:
        pdf_path: PDF文件路径
        output_path: 输出JSON文件路径（可选）
        generate_cpp: 是否生成C++头文件
    """
    try:
        # 1. 解析PDF
        console.print(f"\n[bold]步骤 1/4:[/bold] 解析PDF文件\n", style="cyan")
        parser = PDFParser(pdf_path)
        pages_content = parser.parse()

        if not pages_content:
            console.print("[red]错误: 未找到楼梯相关内容[/red]")
            return False

        # 2. 提取规范
        console.print(f"\n[bold]步骤 2/4:[/bold] 使用LLM提取规范数据\n", style="cyan")
        extractor = RegulationExtractor()
        regulation = extractor.extract_with_llm(pages_content)

        # 3. 验证数据
        console.print(f"\n[bold]步骤 3/4:[/bold] 验证提取的数据\n", style="cyan")
        regulation = extractor.validate_and_fix(regulation)

        # 4. 显示结果
        ConfigGenerator.display_summary(regulation)

        # 5. 保存配置
        console.print(f"\n[bold]步骤 4/4:[/bold] 保存配置文件\n", style="cyan")

        # 确定输出路径
        if not output_path:
            pdf_name = Path(pdf_path).stem
            output_path = f"output/{pdf_name}_config.json"

        ConfigGenerator.save_json(regulation, output_path)

        # 生成C++头文件（可选）
        if generate_cpp:
            cpp_path = output_path.replace('.json', '.h')
            ConfigGenerator.generate_cpp_header(regulation, cpp_path)

        console.print(f"\n[bold green][SUCCESS] 处理完成！[/bold green]\n")

        # 提示下一步操作
        next_steps = f"""
[bold cyan]下一步操作:[/bold cyan]

1. 查看生成的配置文件: {output_path}
2. 将配置文件导入到ARCHICAD插件中
3. 如有需要，可以手动编辑配置文件修正数值

[yellow]提示: 请仔细检查提取的数值是否准确，LLM可能会出错！[/yellow]
        """
        console.print(Panel(next_steps, border_style="green"))

        return True

    except Exception as e:
        console.print(f"\n[bold red]错误: {e}[/bold red]")
        import traceback
        traceback.print_exc()
        return False


def interactive_mode():
    """交互式模式"""
    print_banner()

    console.print("\n[cyan]欢迎使用楼梯规范提取工具！[/cyan]\n")

    # 获取PDF路径
    while True:
        pdf_path = Prompt.ask("请输入PDF文件路径")

        if Path(pdf_path).exists():
            break
        else:
            console.print("[red]文件不存在，请重新输入[/red]")

    # 询问输出路径
    default_output = f"output/{Path(pdf_path).stem}_config.json"
    use_default = Confirm.ask(f"使用默认输出路径 ({default_output})?", default=True)

    if use_default:
        output_path = default_output
    else:
        output_path = Prompt.ask("请输入输出文件路径")

    # 询问是否生成C++头文件
    generate_cpp = Confirm.ask("是否同时生成C++头文件?", default=False)

    # 开始处理
    console.print()
    process_pdf(pdf_path, output_path, generate_cpp)


def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description="楼梯规范提取工具 - 从PDF规范文件中自动提取楼梯限制",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 交互式模式
  python main.py

  # 命令行模式
  python main.py --pdf "GB50368-2005.pdf" --output "config.json"

  # 同时生成C++头文件
  python main.py --pdf "规范.pdf" --cpp
        """
    )

    parser.add_argument(
        "--pdf",
        type=str,
        help="PDF规范文件路径"
    )

    parser.add_argument(
        "-o", "--output",
        type=str,
        help="输出JSON配置文件路径（默认: output/<pdf名称>_config.json）"
    )

    parser.add_argument(
        "--cpp",
        action="store_true",
        help="同时生成C++头文件"
    )

    args = parser.parse_args()

    # 命令行模式
    if args.pdf:
        print_banner()
        process_pdf(args.pdf, args.output, args.cpp)
    # 交互式模式
    else:
        interactive_mode()


if __name__ == "__main__":
    main()
