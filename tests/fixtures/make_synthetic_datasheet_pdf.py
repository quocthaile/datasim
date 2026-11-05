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


def build_header_binding_pdf():
    # Page 1: Band 1 (Cases A, B, E, F, G) at y_pdf=700..650; Band 2 (Case C conflict) at y_pdf=500..475
    page1_lines = [
        # Band 1 header (y_pdf = 700 -> Poppler y ~ 90.7..103.0)
        (25, 700, "Parameter"),
        (220, 700, "Unit"),
        (285, 700, "Min"),
        (345, 700, "Capacitance (uF)"),
        (430, 700, "Leakage (uA)"),
        (515, 700, "VOLTAGE (V)"),
        # Band 1 Row 1: blank row unit, two value columns (uF and uA), bare numeric condition 100
        (25, 675, "CAPACITANCE_A"),
        (360, 675, "0.22"),
        (445, 675, "0.5"),
        (530, 675, "100"),
        # Band 1 Row 2: explicit consistent row unit "uF" (only typ populated), non-numeric condition code "A"
        (25, 650, "CAPACITANCE_B"),
        (225, 650, "uF"),
        (360, 650, "0.33"),
        (530, 650, "A"),
        # Band 2 header (y_pdf = 500 -> Poppler y ~ 290.7..303.0)
        (25, 500, "Parameter"),
        (220, 500, "Unit"),
        (285, 500, "Min"),
        (345, 500, "Capacitance (uF)"),
        (430, 500, "Max"),
        (515, 500, "VOLTAGE (V)"),
        # Band 2 Row 1: explicit conflicting row unit "nF" vs header "uF"
        (25, 475, "CAPACITANCE_C"),
        (225, 475, "nF"),
        (360, 475, "10.0"),
        (530, 475, "50"),
    ]
    # Page 2: Unicode Greek mu header "(μF)" using Type1 Symbol font (/F2 'm' -> U+03BC)
    page2_cmds = [
        "BT /F1 10 Tf 25 700 Td (Parameter) Tj ET",
        "BT /F1 10 Tf 220 700 Td (Unit) Tj ET",
        "BT /F1 10 Tf 285 700 Td (Min) Tj ET",
        "BT /F1 10 Tf 345 700 Td (CAPACITANCE ) Tj (\\() Tj /F2 10 Tf (m) Tj /F1 10 Tf (F\\)) Tj ET",
        "BT /F1 10 Tf 455 700 Td (Max) Tj ET",
        "BT /F1 10 Tf 515 700 Td (Condition) Tj ET",
        "BT /F1 10 Tf 25 675 Td (0.22) Tj ET",
        "BT /F1 10 Tf 360 675 Td (0.22) Tj ET",
        "BT /F1 10 Tf 530 675 Td (A) Tj ET",
    ]
    p1_stream = make_page(page1_lines)
    p2_stream = "\n".join(page2_cmds).encode("ascii")
    objects = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R 7 0 R] /Count 2 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 700 800] /Resources << /Font << /F1 4 0 R /F2 5 0 R >> >> /Contents 6 0 R >>",
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Symbol >>",
        b"<< /Length " + str(len(p1_stream)).encode() + b" >>\nstream\n" + p1_stream + b"\nendstream",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 700 800] /Resources << /Font << /F1 4 0 R /F2 5 0 R >> >> /Contents 8 0 R >>",
        b"<< /Length " + str(len(p2_stream)).encode() + b" >>\nstream\n" + p2_stream + b"\nendstream",
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


def build_inline_unit_pdf():
    # Page 1:
    # - Band 1 (y_pdf = 725..550 -> Poppler y ~ 65..255): Cases 1, 2, 3, 4, 5, 7, 8, 10, 11
    # - Band 2 (y_pdf = 475..450 -> Poppler y ~ 315..355): Case 9 (range 0.22..0.47 uF)
    # - Band 3 (y_pdf = 400..375 -> Poppler y ~ 390..430): Case 6b (row unit mV vs inline 200 V)
    # - Band 4 (y_pdf = 325..300 -> Poppler y ~ 465..505): Case 12 (overlapping word boxes in cell)
    # - Band 5 (y_pdf = 250..225 -> Poppler y ~ 540..580): Case 6a (header unit mV vs inline 200 V)
    page1_lines = [
        # Band 1 header (y_pdf = 725 -> Poppler y ~ 65.7..78.0)
        (25, 725, "Parameter"),
        (220, 725, "Unit"),
        (270, 725, "Min"),
        (345, 725, "Typ"),
        (425, 725, "Max (V)"),
        (510, 725, "Condition"),
        # Row 1 (y_pdf = 700 -> Poppler y ~ 90.7..103.0): Case 1 (200 V) & Case 5 (consistent header V)
        (25, 700, "Operating voltage"),
        (430, 700, "200 V"),
        (510, 700, "T_A=25 degC"),
        # Row 2 (y_pdf = 675 -> Poppler y ~ 115.7..128.0): Case 2 (0.4 W)
        (25, 675, "Rated dissipation"),
        (430, 675, "0.4 W"),
        (510, 675, "T_A=70 degC"),
        # Row 3 (y_pdf = 650 -> Poppler y ~ 140.7..153.0): Case 3 (220 pF) & Case 11 (adjacent narrow cols)
        (25, 650, "Capacitance C0G"),
        (275, 650, "1.0 pF"),
        (350, 650, "220 pF"),
        (510, 650, "T_A=25 degC"),
        # Row 4 (y_pdf = 625 -> Poppler y ~ 165.7..178.0): Case 4 (10 nF)
        (25, 625, "Capacitance X7R"),
        (275, 625, "56 pF"),
        (350, 625, "10 nF"),
        (510, 625, "T_A=25 degC"),
        # Row 5 (y_pdf = 600 -> Poppler y ~ 190.7..203.0): Case 7 (missing unit: bare 200)
        (25, 600, "Unbound rating"),
        (430, 600, "200"),
        (510, 600, "T_A=25 degC"),
        # Row 6 (y_pdf = 575 -> Poppler y ~ 215.7..228.0): Case 8 (ambiguous number 1,000 V)
        (25, 575, "Isolation voltage"),
        (430, 575, "1,000 V"),
        (510, 575, "T_A=25 degC"),
        # Row 7 (y_pdf = 550 -> Poppler y ~ 240.7..253.0): Case 10 (footnote 200* V and 200 V*)
        (25, 550, "Peak pulse voltage"),
        (275, 550, "200* V"),
        (430, 550, "200 V*"),
        (510, 550, "T_A=25 degC"),
        # Band 2 (y_pdf = 475..450 -> Poppler y ~ 315.7..353.0): Case 9 (range 0.22..0.47 uF)
        (25, 475, "Parameter"),
        (220, 475, "Unit"),
        (270, 475, "Min"),
        (345, 475, "Typ"),
        (425, 475, "Max"),
        (510, 475, "Condition"),
        (25, 450, "Capacitance range"),
        (350, 450, "0.22..0.47 uF"),
        (510, 450, "T_A=25 degC"),
        # Band 3 (y_pdf = 400..375 -> Poppler y ~ 390.7..428.0): Case 6b (conflicting row unit mV vs 200 V)
        (25, 400, "Parameter"),
        (220, 400, "Unit"),
        (270, 400, "Min"),
        (345, 400, "Typ"),
        (425, 400, "Max"),
        (510, 400, "Condition"),
        (25, 375, "Operating voltage"),
        (225, 375, "mV"),
        (430, 375, "200 V"),
        (510, 375, "T_A=25 degC"),
        # Band 4 (y_pdf = 325..300 -> Poppler y ~ 465.7..503.0): Case 12 (overlapping words in cell)
        (25, 325, "Parameter"),
        (220, 325, "Unit"),
        (270, 325, "Min"),
        (345, 325, "Typ"),
        (425, 325, "Max"),
        (510, 325, "Condition"),
        (25, 300, "Overlapping cell"),
        (430, 300, "200"),
        (435, 300, "V"),
        (510, 300, "T_A=25 degC"),
        # Band 5 (y_pdf = 250..225 -> Poppler y ~ 540.7..578.0): Case 6a (header unit mV vs inline 200 V)
        (25, 250, "Parameter"),
        (220, 250, "Unit"),
        (270, 250, "Min"),
        (345, 250, "Typ"),
        (425, 250, "Max (mV)"),
        (510, 250, "Condition"),
        (25, 225, "Operating voltage"),
        (430, 225, "200 V"),
        (510, 225, "T_A=25 degC"),
    ]
    # Page 2: Case 13 — Unicode inline unit "1.0 µF" using Type1 Symbol font (/F2 'm' -> U+00B5)
    page2_cmds = [
        "BT /F1 10 Tf 25 700 Td (Parameter) Tj ET",
        "BT /F1 10 Tf 220 700 Td (Unit) Tj ET",
        "BT /F1 10 Tf 270 700 Td (Min) Tj ET",
        "BT /F1 10 Tf 345 700 Td (Typ) Tj ET",
        "BT /F1 10 Tf 425 700 Td (Max) Tj ET",
        "BT /F1 10 Tf 510 700 Td (Condition) Tj ET",
        "BT /F1 10 Tf 25 675 Td (Nominal capacitance) Tj ET",
        "BT /F1 10 Tf 350 675 Td (1.0 ) Tj /F2 10 Tf (m) Tj /F1 10 Tf (F) Tj ET",
        "BT /F1 10 Tf 510 675 Td (T_A=25 degC) Tj ET",
    ]
    p1_stream = make_page(page1_lines)
    p2_stream = "\n".join(page2_cmds).encode("ascii")
    objects = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R 7 0 R] /Count 2 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 700 800] /Resources << /Font << /F1 4 0 R /F2 5 0 R >> >> /Contents 6 0 R >>",
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Symbol >>",
        b"<< /Length " + str(len(p1_stream)).encode() + b" >>\nstream\n" + p1_stream + b"\nendstream",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 700 800] /Resources << /Font << /F1 4 0 R /F2 5 0 R >> >> /Contents 8 0 R >>",
        b"<< /Length " + str(len(p2_stream)).encode() + b" >>\nstream\n" + p2_stream + b"\nendstream",
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
    hb_target = Path(__file__).with_name("synthetic_header_binding_datasheet.pdf")
    hb_target.write_bytes(build_header_binding_pdf())
    print(f"generated {hb_target} ({hb_target.stat().st_size} bytes)")
    iu_target = Path(__file__).with_name("synthetic_inline_unit_datasheet.pdf")
    iu_target.write_bytes(build_inline_unit_pdf())
    print(f"generated {iu_target} ({iu_target.stat().st_size} bytes)")
