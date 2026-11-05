"""Fictional selectable-text hierarchy cases; no vendor material."""
from pathlib import Path
from make_synthetic_datasheet_pdf import make_page


def build_pdf():
    lines = [(25,700,"Parameter"),(220,700,"Unit"),(360,700,"Typ")]
    lines += [(25,675,"Pulse rating:"),(25,650,"short"),(220,650,"V"),(360,650,"12"),
              (25,625,"long"),(220,625,"V"),(360,625,"8"),
              (25,600,"Hold rating:"),(25,575,"short"),(220,575,"V"),(360,575,"6"),
              (25,550,"Complete resistance"),(220,550,"ohm"),(360,550,"10"),
              (25,500,"Shared rating"),(220,500,"V"),(360,500,"4"),
              (220,475,"V"),(360,475,"3"),
              (25,400,"SYN-A"),(360,400,"2"),(500,400,"SYN-B 7 V"),
              (25,350,"Overlap"),(26,350,"Text"),(360,325,"1"),(220,325,"V"),
              (25,250,"Group"),(140,250,"Own label"),(360,250,"2"),(220,250,"V")]
    stream=make_page(lines)
    objects=[b"<< /Type /Catalog /Pages 2 0 R >>",
             b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
             b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 700 800] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
             b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
             b"<< /Length "+str(len(stream)).encode()+b" >>\nstream\n"+stream+b"\nendstream"]
    result=bytearray(b"%PDF-1.4\n")
    offsets=[0]
    for i,body in enumerate(objects,1):
        offsets.append(len(result))
        result.extend(f"{i} 0 obj\n".encode()+body+b"\nendobj\n")
    xref=len(result)
    result.extend(f"xref\n0 {len(offsets)}\n0000000000 65535 f \n".encode())
    for offset in offsets[1:]:
        result.extend(f"{offset:010d} 00000 n \n".encode())
    result.extend(f"trailer\n<< /Size {len(offsets)} /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode())
    return bytes(result)

if __name__ == "__main__":
    Path(__file__).with_name("synthetic_hierarchy_datasheet.pdf").write_bytes(build_pdf())
