"""Generate the small, fictional, text-selectable PDF used by Phase 4C tests."""
from pathlib import Path
import zlib


def pdf_literal(text: str) -> str:
    return text.replace("\\", "\\\\").replace("(", "\\(").replace(")", "\\)")


def make_page(lines):
    commands = []
    for x, y, text in lines:
        commands.append(f"BT /F1 10 Tf {x} {y} Td ({pdf_literal(text)}) Tj ET")
    return "\n".join(commands).encode("ascii")


def build_pdf():
    # Each page is 600 x 800 pt. Top-origin table text rows map to Poppler's
    # -bbox-layout coordinates with stable 25 pt baselines.
    headers = [(25, 700, "Parameter"), (220, 700, "Unit"), (290, 700, "Min"),
               (360, 700, "Typ"), (430, 700, "Max"), (500, 700, "Condition")]
    page1 = headers + [
        (25, 675, "Resistance"), (220, 675, "kohm"), (290, 675, "9.9"),
        (360, 675, "10.0"), (430, 675, "10.1"), (500, 675, "T_A=25 degC; V_DD=5 V"),
        (25, 650, "Resistance"), (220, 650, "kohm"), (290, 650, "9.8"),
        (360, 650, "10.0"), (430, 650, "10.2"), (500, 650, "T_A=85 degC"),
        (25, 625, "Resistance"), (220, 625, "kohm"), (290, 625, "9.9"),
        (360, 625, "10.01"), (430, 625, "10.1"), (500, 625, "T_A=25 degC; V_DD=5 V"),
    ]
    page2 = headers + [
        (25, 675, "Capacitance"), (220, 675, "uF"), (290, 675, "-"),
        (360, 675, "1,000"), (430, 675, "1.1"), (500, 675, "T_A=25 degC"),
        (25, 650, "Capacitance"), (220, 650, "uF"), (290, 650, "0.9"),
        (360, 650, "1.0*"), (430, 650, "1.1"), (500, 650, "T_A=85 degC"),
        (25, 625, "Capacitance"), (220, 625, "uF"), (290, 625, "0.9"),
        (360, 625, "1.0"), (430, 625, "1.1"), (500, 625, "T_A=85 degC"),
        (25, 600, "Capacitance"), (290, 600, "0.8"),
        (360, 600, "0.9"), (430, 600, "1.0"), (500, 600, "T_A=85 degC"),
    ]
    objects = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R 6 0 R] /Count 2 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 700 800] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
        b"<< /Length " + str(len(make_page(page1))).encode() + b" >>\nstream\n" + make_page(page1) + b"\nendstream",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 700 800] /Resources << /Font << /F1 4 0 R >> >> /Contents 7 0 R >>",
        b"<< /Length " + str(len(make_page(page2))).encode() + b" >>\nstream\n" + make_page(page2) + b"\nendstream",
    ]
    result = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets = [0]
    for index, body in enumerate(objects, 1):
        offsets.append(len(result))
        result.extend(f"{index} 0 obj\n".encode())
        result.extend(body)
        result.extend(b"\nendobj\n")
    xref = len(result)
    result.extend(f"xref\n0 {len(offsets)}\n0000000000 65535 f \n".encode())
    for offset in offsets[1:]:
        result.extend(f"{offset:010d} 00000 n \n".encode())
    result.extend(f"trailer\n<< /Size {len(offsets)} /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode())
    return bytes(result)


def build_image_only_pdf():
    image = zlib.compress(bytes([180]))
    content = b"q 300 0 0 300 100 100 cm /Im0 Do Q"
    objects = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 600 800] /Resources << /XObject << /Im0 4 0 R >> >> /Contents 5 0 R >>",
        b"<< /Type /XObject /Subtype /Image /Width 1 /Height 1 /ColorSpace /DeviceGray /BitsPerComponent 8 /Filter /FlateDecode /Length "
        + str(len(image)).encode() + b" >>\nstream\n" + image + b"\nendstream",
        b"<< /Length " + str(len(content)).encode() + b" >>\nstream\n" + content + b"\nendstream",
    ]
    result = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets = [0]
    for index, body in enumerate(objects, 1):
        offsets.append(len(result))
        result.extend(f"{index} 0 obj\n".encode())
        result.extend(body)
        result.extend(b"\nendobj\n")
    xref = len(result)
    result.extend(f"xref\n0 {len(offsets)}\n0000000000 65535 f \n".encode())
    for offset in offsets[1:]:
        result.extend(f"{offset:010d} 00000 n \n".encode())
    result.extend(f"trailer\n<< /Size {len(offsets)} /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode())
    return bytes(result)


if __name__ == "__main__":
    target = Path(__file__).with_name("synthetic_datasheet.pdf")
    target.write_bytes(build_pdf())
    print(f"generated {target} ({target.stat().st_size} bytes)")
    image_target = Path(__file__).with_name("image_only_datasheet.pdf")
    image_target.write_bytes(build_image_only_pdf())
    print(f"generated {image_target} ({image_target.stat().st_size} bytes)")
