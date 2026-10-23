"""Extract complete liquid tables from the supplied PDF; requires pdftotext."""
import subprocess,re,json,hashlib
from pathlib import Path
pdf=Path('PAKhT_Didakticheskiy_material_k_prakticheskim_zanyatiam.pdf')
s=subprocess.check_output(['pdftotext','-layout',str(pdf),'-']).decode()
def number(v):
    try:return float(v.replace(',','.').replace('−','-').replace('–','-'))
    except ValueError:return None
water=[]
section=s[s.index('Физические свойства воды'):s.index('Свойства насыщенного водяного пара',s.index('Физические свойства воды'))]
for line in section.splitlines():
    v=[number(t) for t in line.split()]
    if len(v)==9 and all(x is not None for x in v[:5]) and 0<=v[0]<=100:
        water.append([v[0],v[1],v[2]/1000,v[3],v[4]])
assert len(water)==101, len(water)
headings=['Плотность органических жидкостей','Динамическая вязкость органических жидкостей','Теплоёмкость органических жидкостей','Теплопроводность органических жидкостей']
tables=[]
for heading in headings:
    start=s.index(heading,s.index("ПРИЛОЖЕНИЕ")); end=s.index('\f',start)
    rows={}
    for line in s[start:end].splitlines():
        m=re.match(r'\s*(.*?)\s+((?:[\d—–.,]+\s+){13}[\d—–.,]+)\s*$',line)
        if m and m[1].strip() and not m[1].strip().isdigit(): rows[m[1].strip()]=[number(t) for t in m[2].split()]
    tables.append(rows)
fluids={'Вода':water}
for name in tables[0]:
    if not all(name in t for t in tables):continue
    points=[]
    for i in range(14):
        vals=[t[name][i] for t in tables]
        if all(v is not None and v>0 for v in vals):points.append([10*(i+1),vals[0],vals[1]/1000,vals[2],vals[3]])
    # Only retain contiguous complete intervals; never interpolate across a missing value.
    runs=[]
    for point in points:
        if not runs or point[0]!=runs[-1][-1][0]+10:runs.append([])
        runs[-1].append(point)
    if runs:
        run=max(runs,key=len)
        if len(run)>=2:fluids[name]=run
assert 'Толуол' in fluids and 'Этанол' in fluids
foul=[('Вода сильно загрязнённая',1400,1860),('Вода среднего качества',1860,2900),('Вода хорошего качества',2900,5800),('Вода очищенная',2900,5800),('Вода дистиллированная',11600,11600),('Нефтепродукты чистые, масла',2900,2900),('Нефтепродукты сырые',1160,1160),('Органические жидкости, рассолы',5800,5800),('Водяной пар с маслом',5800,5800),('Водяной пар чистый',11600,11600),('Органические пары',11600,11600),('Воздух',2800,2800)]
# D, do, passes, n, lengths; printed page 137. Tube flow areas may differ by one tube per pass.
geometry=[]
rows=[(159,20,[19]),(159,25,[13]),(273,20,[61]),(273,25,[37]),(325,20,[100,90]),(325,25,[62,56]),(400,20,[181,166]),(400,25,[111,100]),(600,20,[389,370,334,316]),(600,25,[257,240,206,196]),(800,20,[717,690,638,618]),(800,25,[465,442,404,384]),(1000,20,[1173,1138,1072,1044]),(1000,25,[747,718,666,642]),(1200,20,[1701,1658,1580,1544]),(1200,25,[1083,1048,986,958])]
for D,d,counts in rows:
    lengths=[1,1.5,2,3] if D<325 else [1.5,2,3,4] if D==325 else [2,3,4,6] if D<=600 else [2,3,4,6,9] if D==800 else [3,4,6,9] if D==1000 else [4,6,9]
    for passes,n in zip([1,2,4,6],counts):
        for L in lengths:geometry.append(dict(shellDiameter=D/1000,tubeOuterDiameter=d/1000,tubeWallThickness=.002,tubePasses=passes,tubeCount=n,tubeLength=L))
out={'source':pdf.name,'sha256':hashlib.sha256(pdf.read_bytes()).hexdigest(),'fluid_pages':'109–111, 123–124, 127–128','fluids':fluids,'fouling':[{'name':n,'conductanceMin':a,'conductanceMax':b,'resistance':2/(a+b)} for n,a,b in foul],'geometry':geometry}
Path('data/pakht.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
print(len(fluids),'fluids;',sum(map(len,fluids.values())),'points;',len(geometry),'standard geometries')
