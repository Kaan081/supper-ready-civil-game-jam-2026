"""Population-owned original low-poly art. Run with Unreal Python; saves only /Game/Jam/A/Population.
OBJ/TGA intermediates go to Saved/LivingPopulation/ArtSource, never into shared maps/materials.
"""
import math, struct
from pathlib import Path
import unreal

DEST='/Game/Jam/A/Population'
REIMPORT_POSES=globals().get('REIMPORT_POSES',False)
source=Path(unreal.Paths.project_saved_dir())/'LivingPopulation/ArtSource'
source.mkdir(parents=True,exist_ok=True)
palette=[(42,65,95,220),(196,139,100,25),(27,39,53,70),(15,17,22,10),(43,29,21,8),(6,8,12,0),(160,165,151,95),(200,143,105,25)]
palette += [tuple(int(c*.7) for c in p[:3])+(p[3],) for p in palette]
pixels=bytes(v for _y in range(4) for p in palette for _x in range(4) for v in (p[2],p[1],p[0],p[3]))
(source/'T_CitizenPalette.tga').write_bytes(struct.pack('<BBBHHBHHHHBB',0,0,2,0,0,0,0,0,64,4,32,40)+pixels)

class Mesh:
    def __init__(self): self.vertices=[]; self.faces=[]
    def vertex(self,p): self.vertices.append(p); return len(self.vertices)
    def face(self,indices,mat):
        for i in range(1,len(indices)-1): self.faces.append(([indices[0],indices[i],indices[i+1]],mat))
    def box(self,center,size,mat):
        x,y,z=center; a,b,c=[s*.5 for s in size]
        ids=[self.vertex((x+dx*a,y+dy*b,z+dz*c)) for dx,dy,dz in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
        for indices,shade in [([0,3,2,1],8),([4,5,6,7],0),([0,1,5,4],8),([1,2,6,5],0),([2,3,7,6],8),([3,0,4,7],8)]: self.face([ids[i] for i in indices],mat+shade)
    def tube(self,start,end,r0,r1,mat,n=6):
        d=[end[i]-start[i] for i in range(3)]; length=math.sqrt(sum(v*v for v in d)); d=[v/length for v in d]
        ref=(0,0,1) if abs(d[2])<.9 else (1,0,0)
        def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
        u=cross(d,ref); norm=math.sqrt(sum(v*v for v in u)); u=[v/norm for v in u]; v=cross(d,u)
        rings=[]
        for p,r in [(start,r0),(end,r1)]:
            rings.append([self.vertex(tuple(p[k]+r*(math.cos(2*math.pi*j/n)*u[k]+math.sin(2*math.pi*j/n)*v[k]) for k in range(3))) for j in range(n)])
        self.face(list(reversed(rings[0])),mat+8); self.face(rings[1],mat)
        for j in range(n): self.face([rings[0][j],rings[0][(j+1)%n],rings[1][(j+1)%n],rings[1][j]],mat+(8 if j%3 else 0))
    def torso(self,z0,z1,half0,half1,mat):
        rings=[]
        for z,(x,y) in [(z0,half0),(z1,half1)]: rings.append([self.vertex((x*math.cos(math.pi/8+j*math.pi/4),y*math.sin(math.pi/8+j*math.pi/4),z)) for j in range(8)])
        self.face(list(reversed(rings[0])),mat+8); self.face(rings[1],mat)
        for j in range(8): self.face([rings[0][j],rings[0][(j+1)%8],rings[1][(j+1)%8],rings[1][j]],mat+(8 if j in [1,2,3,4] else 0))
    def save(self,path):
        lines=['# Original SON HAT citizen. Centimeters, Z up, +X facing, feet Z=0.','o Citizen','s off']
        lines += ['v %.6f %.6f %.6f'%p for p in self.vertices]
        lines += ['vt %.6f 0.5'%((i+.5)/16) for i in range(16)]
        lines += ['usemtl Citizen']
        lines += ['f '+' '.join('%d/%d'%(v,mat+1) for v in ids) for ids,mat in self.faces]
        path.write_text('\n'.join(lines)+'\n',encoding='utf-8')

def citizen(pose):
    m=Mesh()
    for side in [-1,1]:
        m.box((5,side*10,5),(28,15,10),3)
        m.tube((0,side*10,10),(0,side*9,44),7,8,2)
        m.tube((0,side*9,44),(0,side*8,81),8,10,2)
    m.torso(75,88,(12,16),(12,17),2)
    m.torso(84,116,(11,16),(14,24),0)
    m.torso(116,131,(14,24),(10,21),0)
    m.tube((0,0,130),(0,0,140),7,7,1)
    m.torso(139,146,(8,9),(12,12),1)
    m.torso(146,161,(12,12),(11,11),1)
    m.torso(161,167,(11,11),(7,8),1)
    m.torso(160,166,(11.5,11.5),(10,10),4)
    m.torso(166,170,(10,10),(6,7),4)
    # Small raised facial features keep the front recognizable at close range.
    m.box((12.1,-4.6,155),(1.5,3.1,2.2),5)
    m.box((12.1,4.6,155),(1.5,3.1,2.2),5)
    m.box((13.6,0,150),(5,4.4,5),1)
    m.box((11.8,0,144.8),(1.5,5.3,1.2),4)
    for side in [-1,1]:
        shoulder=(0,side*25,125)
        elbow=(2,side*31,101); wrist=(7,side*33,81)
        if pose=='Safe' and side==1: elbow=(0,45,150); wrist=(3,39,183)
        if pose=='Threat': elbow=(0,side*37,149); wrist=(3,side*32,181)
        if pose=='Stranded':
            elbow=(15,side*35,109)
            wrist=(22,-42,124) if side<0 else (17,9,157)
        m.tube(shoulder,elbow,8,6,0)
        m.tube(elbow,wrist,6,4.5,0)
        m.tube(wrist,(wrist[0]+1,wrist[1],wrist[2]+(8 if pose in ['Safe','Threat'] else -6)),5,4,7,n=5)
    return m

def import_asset(filename,name,mesh=False):
    path=DEST+'/'+name
    if unreal.EditorAssetLibrary.does_asset_exist(path) and not (mesh and REIMPORT_POSES): return unreal.load_asset(path)
    task=unreal.AssetImportTask()
    task.filename=str(filename)
    task.destination_path=DEST
    task.destination_name=name
    task.automated=True; task.save=True; task.replace_existing=bool(mesh and REIMPORT_POSES)
    if mesh:
        options=unreal.FbxImportUI()
        options.import_mesh=True; options.import_materials=False; options.import_textures=False
        options.import_as_skeletal=False
        options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
        options.static_mesh_import_data.set_editor_property('combine_meshes',True)
        options.static_mesh_import_data.set_editor_property('auto_generate_collision',False)
        options.static_mesh_import_data.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS)
        task.options=options
        task.factory=unreal.FbxFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset=unreal.load_asset(path)
    if not asset: raise RuntimeError('Import failed: '+path)
    return asset

unreal.EditorAssetLibrary.make_directory(DEST)
texture=import_asset(source/'T_CitizenPalette.tga','T_CitizenPalette')
texture.set_editor_property('filter',unreal.TextureFilter.TF_NEAREST)
texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
unreal.EditorAssetLibrary.save_loaded_asset(texture)
material_path=DEST+'/M_CitizenState'
if not unreal.EditorAssetLibrary.does_asset_exist(material_path):
    material=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_CitizenState',DEST,unreal.Material,unreal.MaterialFactoryNew())
    material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('two_sided',True)
    material.set_editor_property('used_with_instanced_static_meshes',True)
    lib=unreal.MaterialEditingLibrary
    tex=lib.create_material_expression(material,unreal.MaterialExpressionTextureSample,-600,0)
    tex.set_editor_property('texture',texture)
    tint=lib.create_material_expression(material,unreal.MaterialExpressionVectorParameter,-600,250)
    tint.set_editor_property('parameter_name','Color')
    tint.set_editor_property('default_value',unreal.LinearColor(.8,.7,.3,1))
    mix=lib.create_material_expression(material,unreal.MaterialExpressionLinearInterpolate,-200,0)
    lib.connect_material_expressions(tex,'RGB',mix,'A')
    lib.connect_material_expressions(tint,'',mix,'B')
    lib.connect_material_expressions(tex,'A',mix,'Alpha')
    lib.connect_material_property(mix,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
else: material=unreal.load_asset(material_path)
for pose in ['Waiting','Safe','Threat','Stranded']:
    data=citizen(pose)
    obj=source/('SM_Citizen_'+pose+'.obj'); data.save(obj)
    asset=import_asset(obj,'SM_Citizen_'+pose,True)
    asset.set_material(0,material)
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(asset)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log('POPULATION_ART '+pose+' triangles='+str(len(data.faces))+' bounds='+str(asset.get_bounding_box()))
unreal.log('POPULATION_ART_COMPLETE')
