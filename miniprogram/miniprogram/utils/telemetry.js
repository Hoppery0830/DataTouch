// row: seq,ms,flags,light,kseq,kms, then 22 float values. Units preserved end-to-end.
const GROUPS=[
 {name:'三轴力',unit:'N',fields:[['Fx',0,1],['Fy',1,1],['Fz',2,1]]},
 {name:'三轴力矩',unit:'N·m',fields:[['Mx',3,1],['My',4,1],['Mz',5,1]]},
 {name:'切向力与法向力',unit:'N',fields:[['Ft',6,1],['|Fz|',7,1],['窗口 Ft 均值',17,4],['窗口法向力均值',16,4]]},
 {name:'摩擦比',unit:'无量纲',fields:[['100 ms 摩擦比',8,8],['2 s 均值比',9,4],['2 s 峰值',15,4]]},
 {name:'切向力波动',unit:'N',fields:[['标准差',10,4],['峰峰值',11,4]]},
 {name:'振动',unit:'mg',fields:[['100 ms RMS',12,2],['2 s 块 RMS 峰值',13,64],['2 s 块 RMS 均值',20,64]]},
 {name:'振动包络主频',unit:'Hz',fields:[['包络主频',19,64]]},
 {name:'切向力阈值上穿次数',unit:'次 / 2 s',fields:[['上穿次数',18,4]]},
 // Visual results are shown per experiment on pages/vision/vision.
];
function validRows(rows){return (rows||[]).filter(r=>Array.isArray(r)&&r.length===28&&r.every(Number.isFinite));}
function value(row,index,flag){return row&&(row[2]&flag)?row[6+index]:null;}
function series(rows,group){
 rows=validRows(rows);if(!rows.length)return [];
 const end=rows[rows.length-1][1];
 return group.fields.map(([name,index,flag])=>{
  let previous=null,kseq=null;
  const points=[];
  for(const r of rows){
   if(index===14){if(!(r[2]&16)||r[4]===kseq)continue;kseq=r[4];
    if(r[5]>end||end-r[5]>60000)continue;
    points.push({x:(r[5]-end)/1000,y:r[20],break:true});continue;}
   const y=value(r,index,flag),gap=!previous || r[0]!==previous[0]+1 || r[1]-previous[1]>250;
   points.push({x:(r[1]-end)/1000,y,break:gap});previous=r;
  }
  return {name,points};
 });
}
module.exports={GROUPS,validRows,value,series};
