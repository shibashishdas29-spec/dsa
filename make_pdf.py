"""Create a simple black-on-white PDF from report.txt without external libs."""

from pathlib import Path

INPUT = Path("report.txt")
OUTPUT = Path("parking_report.pdf")

PAGE_WIDTH = 612  # 8.5in * 72
PAGE_HEIGHT = 792  # 11in * 72
LEFT = 54
TOP = 60
BOTTOM = 54
FONT_SIZE = 11
LEADING = 14
MAX_CHARS = 95


def escape_pdf_text(s: str) -> str:
    return s.replace("\\", "\\\\").replace("(", "\\(").replace(")", "\\)")


def wrap_line(line: str, width: int):
    if not line:
        return [""]
    words = line.split(" ")
    out = []
    cur = words[0]
    for w in words[1:]:
        if len(cur) + 1 + len(w) <= width:
            cur += " " + w
        else:
            out.append(cur)
            cur = w
    out.append(cur)
    return out


def build_pages(text: str):
    logical_lines = []
    for raw in text.splitlines():
        logical_lines.extend(wrap_line(raw.rstrip(), MAX_CHARS))

    lines_per_page = (PAGE_HEIGHT - TOP - BOTTOM) // LEADING
    pages = []
    for i in range(0, len(logical_lines), lines_per_page):
        pages.append(logical_lines[i : i + lines_per_page])
    return pages


def build_pdf(pages):
    objects = []

    def add_object(body: str) -> int:
        objects.append(body)
        return len(objects)

    font_id = add_object("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>")

    content_ids = []
    page_ids = []

    for page_lines in pages:
        y0 = PAGE_HEIGHT - TOP
        content_parts = ["BT", f"/F1 {FONT_SIZE} Tf", f"1 0 0 1 {LEFT} {y0} Tm", f"{LEADING} TL"]
        first = True
        for line in page_lines:
            escaped = escape_pdf_text(line)
            if first:
                content_parts.append(f"({escaped}) Tj")
                first = False
            else:
                content_parts.append(f"T* ({escaped}) Tj")
        content_parts.append("ET")
        stream = "\n".join(content_parts).encode("latin-1", errors="replace")
        content_obj = f"<< /Length {len(stream)} >>\nstream\n".encode("latin-1") + stream + b"\nendstream"
        content_id = add_object(content_obj.decode("latin-1"))
        content_ids.append(content_id)

        page_obj = (
            "<< /Type /Page /Parent PAGES_ID 0 R "
            f"/MediaBox [0 0 {PAGE_WIDTH} {PAGE_HEIGHT}] "
            f"/Resources << /Font << /F1 {font_id} 0 R >> >> "
            f"/Contents {content_id} 0 R >>"
        )
        page_id = add_object(page_obj)
        page_ids.append(page_id)

    kids = " ".join(f"{pid} 0 R" for pid in page_ids)
    pages_id = add_object(f"<< /Type /Pages /Kids [ {kids} ] /Count {len(page_ids)} >>")

    # Patch parent references
    for pid in page_ids:
        objects[pid - 1] = objects[pid - 1].replace("PAGES_ID", str(pages_id))

    catalog_id = add_object(f"<< /Type /Catalog /Pages {pages_id} 0 R >>")

    pdf = b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n"
    offsets = [0]
    for i, obj in enumerate(objects, start=1):
        offsets.append(len(pdf))
        pdf += f"{i} 0 obj\n".encode("latin-1")
        pdf += obj.encode("latin-1")
        pdf += b"\nendobj\n"

    xref_pos = len(pdf)
    pdf += f"xref\n0 {len(objects)+1}\n".encode("latin-1")
    pdf += b"0000000000 65535 f \n"
    for off in offsets[1:]:
        pdf += f"{off:010d} 00000 n \n".encode("latin-1")

    pdf += (
        "trailer\n"
        f"<< /Size {len(objects)+1} /Root {catalog_id} 0 R >>\n"
        "startxref\n"
        f"{xref_pos}\n"
        "%%EOF\n"
    ).encode("latin-1")

    return pdf


def main():
    text = INPUT.read_text(encoding="utf-8")
    pages = build_pages(text)
    pdf_bytes = build_pdf(pages)
    OUTPUT.write_bytes(pdf_bytes)
    print(f"Created {OUTPUT} with {len(pages)} page(s).")


if __name__ == "__main__":
    main()
