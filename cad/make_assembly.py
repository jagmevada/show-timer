"""remote_v2 full 3D assembly for case design: EasyEDA board export + HC-12 (J2, bottom) + 0.96" I2C OLED (on J3).

Input : cad/remote_v2_easyeda.step   (EasyEDA Pro 3D export: all 66 parts incl. DNP, silkscreen)
Output: cad/remote_v2_assembly.step
Run   : python make_assembly.py   (needs cadquery-ocp: pip install cadquery-ocp)

Frame = EasyEDA PCB frame: x_mm = x_mil * 0.0254, y up = toward the top tab, board top face z = 1.6, bottom z = 0.
"""
import os
from OCP.STEPCAFControl import STEPCAFControl_Reader, STEPCAFControl_Writer
from OCP.STEPControl import STEPControl_AsIs
from OCP.TDocStd import TDocStd_Document
from OCP.TCollection import TCollection_ExtendedString
from OCP.XCAFDoc import XCAFDoc_DocumentTool, XCAFDoc_Editor, XCAFDoc_ColorSurf
from OCP.OCP.collections import Sequence_TDF_Label, DataMap_TDF_Label_TDF_Label
from OCP.TDataStd import TDataStd_Name
from OCP.TDF import TDF_Label
from OCP.TopLoc import TopLoc_Location
from OCP.gp import gp_Trsf, gp_Ax3, gp_Ax2, gp_Pnt, gp_Dir
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder
from OCP.BRepAlgoAPI import BRepAlgoAPI_Cut
from OCP.Quantity import Quantity_Color, Quantity_TOC_RGB

HERE = os.path.dirname(os.path.abspath(__file__))
HC12_STEP = os.path.join(HERE, "HC-12_module.step")   # detailed model (from dmx_node/docs/3d)
MIL = 0.0254

# ---- calibration knobs (measure your parts) ----
OLED_LIFT = 1.2      # remote PCB top -> OLED module PCB bottom (user's build, 2026-11-07); no J3 header plastic
PIN_ABOVE = 1.0      # J3 pin tip above the OLED PCB top (trimmed)
OLED_PCB_T = 1.0     # OLED module PCB thickness
GLASS_T = 1.4        # glass panel 1.26 + tape
GLASS_W, GLASS_H = 26.7, 19.26   # SSD1306 0.96" panel
GLASS_FROM_TOP = 4.3             # module top edge (pin side) -> glass top edge
AA_W, AA_H, AA_FROM_GLASS_TOP = 21.74, 10.86, 2.5   # active (lit) area
MOUNT_HOLE_D, MOUNT_HOLE_IN = 2.0, 2.0              # M2 holes, centre 2 mm in from each edge

def load(path):
    doc = TDocStd_Document(TCollection_ExtendedString("XmlOcaf"))
    r = STEPCAFControl_Reader(); r.SetColorMode(True); r.SetNameMode(True)
    assert r.ReadFile(path) == 1, path
    r.Transfer(doc)
    st = XCAFDoc_DocumentTool.ShapeTool_s(doc.Main())
    free = Sequence_TDF_Label(); st.GetFreeShapes(free)
    return doc, st, free.Value(1)

def name(lab):
    n = TDataStd_Name()
    return n.Get().ToExtString() if lab.FindAttribute(TDataStd_Name.GetID_s(), n) else ""

def box(x0, x1, y0, y1, z0, z1):
    return BRepPrimAPI_MakeBox(gp_Pnt(x0, y0, z0), gp_Pnt(x1, y1, z1)).Shape()

def drill(shape, holes, z0, z1):
    for x, y, d in holes:
        cyl = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(x, y, z0 - 1), gp_Dir(0, 0, 1)), d / 2, z1 - z0 + 2).Shape()
        shape = BRepAlgoAPI_Cut(shape, cyl).Shape()
    return shape

doc, st, top = load(os.path.join(HERE, "remote_v2_easyeda.step"))
ct = XCAFDoc_DocumentTool.ColorTool_s(doc.Main())

def add_part(shape, label, rgb):
    lab = st.AddShape(shape, False)
    TDataStd_Name.Set_s(lab, TCollection_ExtendedString(label))
    ct.SetColor(lab, Quantity_Color(*rgb, Quantity_TOC_RGB), XCAFDoc_ColorSurf)
    comp = st.AddComponent(top, lab, TopLoc_Location())
    TDataStd_Name.Set_s(comp, TCollection_ExtendedString(label))

# ---- HC-12 (J2) on the bottom, castellated pin edge on the J2 pads, antenna end at the board's bottom edge ----
hdoc, hst, htop = load(HC12_STEP)
comps = Sequence_TDF_Label(); hst.GetComponents_s(htop, comps)
for i in range(1, comps.Length() + 1):
    if name(comps.Value(i)).startswith("PinHeader"):   # model ships with a 1x5 header; ours is soldered flat
        hst.RemoveComponent(comps.Value(i))
hst.UpdateAssemblies()
hc = XCAFDoc_Editor.CloneShapeLabel_s(htop, hst, st, DataMap_TDF_Label_TDF_Label())
# model frame: pins along y at x~1.3 (pin 1 VCC at y 11.68), antenna end x = 27.4, PCB z 0..1, parts +z.
# board: pin edge at y = 1008.9 + 10 mil, body toward -y, pin 1 at x = 509.8 mil, component side facing down.
pads_x, pin_edge_y = 709.8 * MIL, (1008.9 + 10) * MIL
t = gp_Trsf()
t.SetTransformation(gp_Ax3(gp_Pnt(pads_x + 6.6, pin_edge_y, 0), gp_Dir(0, 0, -1), gp_Dir(0, -1, 0)), gp_Ax3())
c = st.AddComponent(top, hc, TopLoc_Location(t))
TDataStd_Name.Set_s(c, TCollection_ExtendedString("J2 HC-12 module (bottom)"))

# ---- 0.96" I2C OLED module on J3 (outline = top-silk rectangle 27.3 x 27.8 mm) ----
x0, x1, y0, y1 = 172.3984 * MIL, 1247.2016 * MIL, 1531.9118 * MIL, 2626.4 * MIL
zb = 1.6 + OLED_LIFT; zt = zb + OLED_PCB_T
pins = [(x * MIL, 2576.4 * MIL, 1.0) for x in (559.8, 659.8, 759.8, 859.8)]
mounts = [(x, y, MOUNT_HOLE_D) for x in (x0 + MOUNT_HOLE_IN, x1 - MOUNT_HOLE_IN) for y in (y0 + MOUNT_HOLE_IN, y1 - MOUNT_HOLE_IN)]
add_part(drill(box(x0, x1, y0, y1, zb, zt), pins + mounts, zb, zt), "OLED module PCB", (0.05, 0.20, 0.60))
cx = (x0 + x1) / 2; gy1 = y1 - GLASS_FROM_TOP; gy0 = gy1 - GLASS_H
add_part(box(cx - GLASS_W / 2, cx + GLASS_W / 2, gy0, gy1, zt, zt + GLASS_T), "OLED glass", (0.08, 0.08, 0.08))
ay1 = gy1 - AA_FROM_GLASS_TOP
add_part(box(cx - AA_W / 2, cx + AA_W / 2, ay1 - AA_H, ay1, zt + GLASS_T, zt + GLASS_T + 0.02), "OLED active area", (0.10, 0.35, 0.75))
add_part(box(cx - 6, cx + 6, y0 - 0.3, gy0, zt, zt + 0.15), "OLED FPC (top)", (0.80, 0.45, 0.10))   # flex wraps the bottom edge
add_part(box(cx - 6, cx + 6, y0 - 0.3, y0, zb - 0.3, zt + 0.15), "OLED FPC (wrap)", (0.80, 0.45, 0.10))
# J3: the EasyEDA 2.5 mm male-header model doesn't fit a 1.2 mm gap -> replace it with 4 bare 0.64 mm pins
comps = Sequence_TDF_Label(); st.GetComponents_s(top, comps)
for i in range(1, comps.Length() + 1):
    if name(comps.Value(i)) == "J3":
        proto = TDF_Label(); st.GetReferredShape_s(comps.Value(i), proto)
        st.RemoveComponent(comps.Value(i)); st.RemoveShape(proto)   # else it stays as a stray free shape
for x, y, _ in pins:
    add_part(box(x - 0.32, x + 0.32, y - 0.32, y + 0.32, -1.4, zt + PIN_ABOVE), "J3 pin", (0.85, 0.70, 0.25))

st.UpdateAssemblies()
w = STEPCAFControl_Writer(); w.SetColorMode(True); w.SetNameMode(True)
w.Transfer(doc, STEPControl_AsIs)
out = os.path.join(HERE, "remote_v2_assembly.step")
assert w.Write(out) == 1
print(out, os.path.getsize(out) // 1024, "KiB")
