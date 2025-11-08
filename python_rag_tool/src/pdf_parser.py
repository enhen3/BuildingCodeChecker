"""
PDF解析模块
用于从PDF文件中提取文本内容
"""
import fitz  # PyMuPDF
import pdfplumber
from typing import List, Dict
from pathlib import Path
from rich.console import Console
from rich.progress import track

console = Console()


class PDFParser:
    """PDF文档解析器"""

    def __init__(self, pdf_path: str):
        self.pdf_path = Path(pdf_path)
        if not self.pdf_path.exists():
            raise FileNotFoundError(f"PDF文件不存在: {pdf_path}")

    def extract_text_with_pymupdf(self) -> List[Dict]:
        """
        使用PyMuPDF提取文本（速度快，适合纯文本PDF）

        Returns:
            List[Dict]: 每页的文本内容和元数据
        """
        console.print(f"[cyan]正在使用PyMuPDF解析: {self.pdf_path.name}[/cyan]")

        pages_data = []
        doc = fitz.open(self.pdf_path)

        for page_num in track(range(len(doc)), description="提取文本"):
            page = doc[page_num]
            text = page.get_text()

            pages_data.append({
                "page_number": page_num + 1,
                "text": text,
                "method": "pymupdf"
            })

        doc.close()
        console.print(f"[green]成功提取 {len(pages_data)} 页[/green]")
        return pages_data

    def extract_text_with_pdfplumber(self) -> List[Dict]:
        """
        使用pdfplumber提取文本（适合表格和复杂布局）

        Returns:
            List[Dict]: 每页的文本、表格内容
        """
        console.print(f"[cyan]正在使用pdfplumber解析: {self.pdf_path.name}[/cyan]")

        pages_data = []

        with pdfplumber.open(self.pdf_path) as pdf:
            for page_num, page in enumerate(track(pdf.pages, description="提取文本和表格")):
                text = page.extract_text() or ""
                tables = page.extract_tables()

                pages_data.append({
                    "page_number": page_num + 1,
                    "text": text,
                    "tables": tables,
                    "method": "pdfplumber"
                })

        console.print(f"[green]成功提取 {len(pages_data)} 页[/green]")
        return pages_data

    def extract_stair_related_content(self, pages_data: List[Dict]) -> List[Dict]:
        """
        筛选出与楼梯相关的内容

        Args:
            pages_data: 所有页面的数据

        Returns:
            List[Dict]: 楼梯相关的页面内容
        """
        keywords = [
            "楼梯", "踏步", "平台", "栏杆", "扶手",
            "梯段", "踢面", "踏面", "梯井",
            "stair", "step", "riser", "tread", "landing"
        ]

        stair_pages = []

        for page_data in pages_data:
            text = page_data["text"].lower()

            # 检查是否包含楼梯相关关键词
            if any(keyword in text for keyword in keywords):
                stair_pages.append(page_data)

        console.print(f"[yellow]找到 {len(stair_pages)} 页包含楼梯相关内容[/yellow]")
        return stair_pages

    def parse(self, method: str = "auto") -> List[Dict]:
        """
        解析PDF并返回楼梯相关内容

        Args:
            method: 解析方法 ("pymupdf", "pdfplumber", "auto")

        Returns:
            List[Dict]: 楼梯相关的页面内容
        """
        if method == "pymupdf":
            pages_data = self.extract_text_with_pymupdf()
        elif method == "pdfplumber":
            pages_data = self.extract_text_with_pdfplumber()
        else:  # auto
            # 先尝试pdfplumber（更准确），失败则用pymupdf
            try:
                pages_data = self.extract_text_with_pdfplumber()
            except Exception as e:
                console.print(f"[yellow]pdfplumber失败，切换到pymupdf: {e}[/yellow]")
                pages_data = self.extract_text_with_pymupdf()

        # 筛选楼梯相关内容
        stair_content = self.extract_stair_related_content(pages_data)

        return stair_content


if __name__ == "__main__":
    # 测试代码
    import sys

    if len(sys.argv) > 1:
        pdf_path = sys.argv[1]
        parser = PDFParser(pdf_path)
        content = parser.parse()

        # 打印前100个字符
        for page in content[:3]:
            print(f"\n=== 第 {page['page_number']} 页 ===")
            print(page['text'][:200])
    else:
        print("用法: python pdf_parser.py <pdf文件路径>")
