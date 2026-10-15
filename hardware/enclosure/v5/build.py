from pathlib import Path
import numpy as np
import manifold3d as m
import trimesh
OUT=Path(__file__).parent
W,H,D=120,70,36.4
# Flush inset plate: 0.3mm clearance per side; plate underside seats at z34.
BACK_INSET=1.7; BACK_THICKNESS=2.4
# Screen outline 93x55; owner-measured approximate screw spacing 77x44.
SCREEN_HOLES=[(12,13),(89,13),(12,57),(89,57)]
class Solid:
 def __init__(self,mesh,code):self.mesh,self.code=mesh,code
 def __add__(self,o):return Solid(self.mesh+o.mesh,'union(){'+self.code+o.code+'}')
 def __sub__(self,o):return Solid(self.mesh-o.mesh,'difference(){'+self.code+o.code+'}')
 def move(self,p):return Solid(self.mesh.translate(p),'translate('+str(list(p))+'){'+self.code+'}')
def box(p,s):return Solid(m.Manifold.cube(s),'cube('+str(list(s))+');').move(p)
def cyl(p,r,h):return Solid(m.Manifold.cylinder(h,r,circular_segments=48),f'cylinder(h={h},r={r},$fn=48);').move(p)
def rounded(w,h,d,r):
 a=np.linspace(0,2*np.pi,49)[:-1];points=[]
 for x,y,lo,hi in [(w-r,h-r,0,np.pi/2),(r,h-r,np.pi/2,np.pi),(r,r,np.pi,3*np.pi/2),(w-r,r,3*np.pi/2,2*np.pi)]:
  for t in np.linspace(lo,hi,13):points.append([x+r*np.cos(t),y+r*np.sin(t)])
 return Solid(m.CrossSection([points]).extrude(d),f'linear_extrude({d}) offset(r={r},$fn=48) translate([{r},{r}]) square([{w-2*r},{h-2*r}]);')
body=rounded(W,H,D,5)-rounded(W-4.8,H-4.8,D,2.6).move([2.4,2.4,3])
# Rear rabbet widens only last 2.4mm; 1.4mm rear rim, 2.4mm main walls.
body-=rounded(117.2,67.2,3.4,3.6).move([1.4,1.4,34])
for x in [5,115]:
 for y in [5,65]:body+=(cyl([x,y,3],3,31)-cyl([x,y,3],1.35,33))-box([3.7,7.2,3],[93.6,55.6,9])
# 1.6mm-wide continuous seating ledge, 2mm thick, below inset plate.
ledge=rounded(115.4,65.4,2,2.7).move([2.3,2.3,32])-rounded(112,62,4,1).move([4,4,31])
# Stepped 45-degree underside, 0.2mm increments, avoids unsupported ledge.
for i in range(8):
 inset=2.4+0.2*(i+1)
 ring=rounded(115.4,65.4,0.21,2.7).move([2.3,2.3,30.4+i*.2])-rounded(W-2*inset,H-2*inset,0.4,max(0.8,5-inset)).move([inset,inset,30.3+i*.2])
 body+=ring
body+=ledge
# Preserve pilot bores through ledge unions.
for x in [5,115]:
 for y in [5,65]:body-=cyl([x,y,3],1.35,34)
# Gabled cable passage: 45-degree roof instead of 14mm flat bridge.
poly=[[29,14],[43,14],[43,19],[36,26],[29,19]]
matrix=[[0,0,1,99],[1,0,0,0],[0,1,0,0]]
passage=Solid(m.CrossSection([poly]).extrude(4).transform(matrix),f'multmatrix({matrix+[[0,0,0,1]]}) linear_extrude(4) polygon({poly});')
divider=box([100,2.4,3],[2,65.2,28])-passage
body+=divider
# Dedicated screen posts; drillable 2.0-mm pilot holes. Customize before final print.
for x,y in SCREEN_HOLES:body+=cyl([x,y,3],3.5,5)-cyl([x,y,3],1,6)
body-=box([22,13.4,-1],[57.6,43.2,5])
for y in [22,29,36,43]:
 body-=box([105,y,-1],[9,3,5]);body-=box([116.6,y,9],[4,3,15])
# ESP lies at x3..68, y22..50 after cover is inverted.
body-=box([-1,27,20],[5.4,18,18])
lid=rounded(W-2*BACK_INSET,H-2*BACK_INSET,BACK_THICKNESS,3.3).move([BACK_INSET,BACK_INSET,0])
for x in [5,115]:
 for y in [5,65]:lid-=cyl([x,y,-1],1.7,5)
# Through tunnels run along y; roof spans only 2.8mm, not 26mm.
for x in [15,43]:lid+=box([x,18,2.4],[6,32,4])-box([x+1.6,17,3.4],[2.8,34,1.6])
# Board outline 65x28: 0.4..0.5mm locating clearance. Ties provide retention.
for y in [18.4,48.4]:lid+=box([30,y,2.4],[4,1.2,5.8])
for x in [1.4,68.4]:lid+=box([x,25,2.4],[1.2,3,5.8])
# AHT PCB x104..114, y28..43; bars extend under the board by 1mm.
for y in [27,42]:lid+=box([103,y,2.4],[12,2,5])-box([107.6,y-1,3.9],[2.8,4,1.6])
# Sensor stops leave 0.3mm lateral and 0.4mm end clearance.
for x in [102.7,114.3]:lid+=box([x,34,2.4],[1,2,7.1])
for y in [26.6,43.4]:lid+=box([104.5,y,2.4],[2,1,7.1])
# Check nominal PCB envelopes against plastics; does not model real components.
esp_pcb=box([3,20,6.4],[65,28,1.6])
aht_pcb=box([104,28,7.4],[10,15,1.6])
screen_pcb=box([4,7.5,8],[93,55,1.6])
assembled=lid.mesh.rotate([180,0,0]).translate([0,H,D])
for label,collision in [('body/back',body.mesh ^ assembled),('ESP/back',lid.mesh ^ esp_pcb.mesh),('AHT/back',lid.mesh ^ aht_pcb.mesh),('screen/body',body.mesh ^ screen_pcb.mesh),('ESP/body',body.mesh ^ esp_pcb.mesh.rotate([180,0,0]).translate([0,H,D])),('AHT/body',body.mesh ^ aht_pcb.mesh.rotate([180,0,0]).translate([0,H,D]))]:
 assert abs(collision.volume())<0.001,(label,collision.volume())
 print(label,'nominal clearance passed')
for name,obj in [('body',body),('back',lid)]:
 mm=obj.mesh.simplify(0.0001).to_mesh();tm=trimesh.Trimesh(vertices=np.asarray(mm.vert_properties)[:,:3],faces=np.asarray(mm.tri_verts),process=True)
 tm.update_faces(tm.nondegenerate_faces()); tm.remove_unreferenced_vertices()
 assert tm.is_watertight and tm.is_winding_consistent and tm.volume>0
 assert len(tm.split())==1
 tm.export(OUT/(name+'.stl'))
 print(name,'watertight, single solid;',len(tm.faces),'triangles; bounds',tm.bounds.tolist())
header='''// Compact v5, mm. 120 x 70 x 36.4 assembled; flush inset back.
// Back plate 116.6 x 66.6 x 2.4, with 0.3mm clearance per side.
// Sloped seat ledge starts z30.4; top z34; plate assembles z34..36.4, exterior flush.
// Assembly: translate([0,70,36.4]) rotate([180,0,0]) back();
// Screen screw spacing approximately 77 x 44, owner-measured. 2mm pilot bores.
// Screen support height 5mm, also provisional. Main bay x2.4..100.
// ESP32 rails on back: 4mm high, board 65x28, USB facing left wall.
// AHT20 10x15 on back: 5mm high bars with retaining tie passages.
// Retain ESP with two <=2.5mm ties and AHT with one <=2.5mm tie through tunnels.
// Component/header envelopes and glass clearance UNVERIFIED.
// Generated by build.py; screen holes and dimensions configurable there.
part="body";
'''
(OUT/'enclosure.scad').write_text(header+'module body(){'+body.code+'}\nmodule back(){'+lid.code+'}\nif(part=="body")body();\nif(part=="back")back();\n')
