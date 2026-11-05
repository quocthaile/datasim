"""Fictional PDF for explicit baseline-shift association; no vendor content."""
from pathlib import Path
from make_synthetic_datasheet_pdf import pdf_literal


def build_pdf():
    words = [(25,700,12,"H"),(35,694,7,"1"),(220,700,12,"Unit"),(360,700,12,"Typ"),
             (25,675,12,"P"),(34,669,7,"70"),(220,675,12,"W"),(360,675,12,"2"),
             (25,625,12,"Q"),(36,631,7,"2"),(220,625,12,"V"),(360,625,12,"3"),
             (25,575,12,"Factor"),(220,575,12,"V"),(360,575,12,"10"),(375,581,7,"-4"),
             (25,525,12,"P"),(34,519,7,"1"),(100,525,12,"U"),(110,519,7,"2"),(220,525,12,"V"),(360,525,12,"5"),
             (25,475,12,"X"),(34,469,7,"a"),(40.5,469,7,"b"),(220,475,12,"V"),(360,475,12,"6"),
             (25,425,12,"P"),(25,421,12,"R"),(34,416,7,"1"),(220,425,12,"V"),(360,425,12,"7"),
             (25,375,12,"P"),(34,369,7,"1"),(32,372,7,"!"),(220,375,12,"V"),(360,375,12,"8"),
             (25,325,12,"P"),(34,319,7,"1"),(100,319,7,"(1)"),(220,325,12,"V"),(360,325,12,"9"),
             (25,300,12,"Fixture group:"),(25,275,12,"U"),(35,269,7,"ins"),(220,275,12,"V"),(360,275,12,"10"),
             (25,225,12,"Ordinary"),(220,225,12,"V"),(360,225,12,"11")]
    stream="\n".join(f"BT /F1 {size} Tf {x} {y} Td ({pdf_literal(text)}) Tj ET" for x,y,size,text in words).encode("ascii")
    objects=[b"<< /Type /Catalog /Pages 2 0 R >>",b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
             b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 700 800] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
             b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
             b"<< /Length "+str(len(stream)).encode()+b" >>\nstream\n"+stream+b"\nendstream"]
    result=bytearray(b"%PDF-1.4\n"); offsets=[0]
    for i,body in enumerate(objects,1):
        offsets.append(len(result)); result.extend(f"{i} 0 obj\n".encode()+body+b"\nendobj\n")
    xref=len(result); result.extend(f"xref\n0 {len(offsets)}\n0000000000 65535 f \n".encode())
    for offset in offsets[1:]: result.extend(f"{offset:010d} 00000 n \n".encode())
    result.extend(f"trailer\n<< /Size {len(offsets)} /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode())
    return bytes(result)

if __name__ == "__main__":
    Path(__file__).with_name("synthetic_baseline_datasheet.pdf").write_bytes(build_pdf())
