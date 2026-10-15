from pathlib import Path
import numpy as np
import trimesh
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
p=Path(__file__).parent
fig=plt.figure(figsize=(14,7),facecolor='#eef2f5')
for i,(name,title) in enumerate([('body','Shell · sloped seat and screen mounts'),('back','Inset back · locators and tie mounts')],1):
 ax=fig.add_subplot(1,2,i,projection='3d',facecolor='#eef2f5')
 mesh=trimesh.load(p/(name+'.stl'))
 normals=mesh.face_normals; light=np.array([-.3,-.4,.86]);light/=np.linalg.norm(light)
 shade=.60+.35*np.clip(normals@light,0,1)
 colors=shade[:,None]*np.array([.68,.79,.85])[None,:]
 ax.add_collection3d(Poly3DCollection(mesh.triangles,facecolors=colors,edgecolors='none'))
 ax.set_xlim(0,120);ax.set_ylim(0,70);ax.set_zlim(0,34);ax.set_box_aspect((120,70,34));ax.view_init(55,-125);ax.set_axis_off();ax.set_title(title,fontsize=15)
fig.suptitle('Enclosure v5 · actual exported STL geometry',fontsize=21)
fig.text(.5,.06,'120 × 70 × 36.4 mm assembled · screen hole spacing and hardware clearances provisional',ha='center',fontsize=11)
fig.subplots_adjust(left=0,right=1,bottom=.1,top=.86,wspace=0)
fig.savefig(p/'preview.png',dpi=150)
