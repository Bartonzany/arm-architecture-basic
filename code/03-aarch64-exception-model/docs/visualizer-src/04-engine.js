const SYSGROUPS = [
  {t:"EL3（固件）", regs:[["scr_el3","SCR_EL3"],["vbar_el3","VBAR_EL3"],["spsr_el3","SPSR_EL3"],["elr_el3","ELR_EL3"],["esr_el3","ESR_EL3"]]},
  {t:"EL2（Hypervisor）", regs:[["hcr_el2","HCR_EL2"],["vbar_el2","VBAR_EL2"],["spsr_el2","SPSR_EL2"],["elr_el2","ELR_EL2"],["esr_el2","ESR_EL2"]]},
  {t:"EL1（内核）", regs:[["sctlr_el1","SCTLR_EL1"],["vbar_el1","VBAR_EL1"],["cntkctl_el1","CNTKCTL_EL1"],["spsr_el1","SPSR_EL1"],["elr_el1","ELR_EL1"],["esr_el1","ESR_EL1"]]},
  {t:"定时器 / GIC", regs:[["cntp_tval","CNTP_TVAL_EL0"],["cntp_ctl","CNTP_CTL_EL0"],["gicd_igroupr","GICD_IGROUPR"],["gicd_ctlr","GICD_CTLR"],["gicd_isenabler","GICD_ISENABLER0"],["gicc_ctlr","GICC_CTLR"],["gicc_pmr","GICC_PMR"],["gicc_iar","GICC_IAR"],["gicc_eoir","GICC_EOIR"]]},
  {t:"内存", regs:[["g_irq_ok","[g_irq_ok] 0x40003070"]]}
];

const NAMEMAP = {pc:"PC", el:"EL", mode:"PSTATE.M", daif:"PSTATE.DAIF", sp:"SP", spEl0:"SP_EL0", spEl1:"SP_EL1", spEl2:"SP_EL2", spEl3:"SP_EL3", secure:"安全状态"};
for(const g of SYSGROUPS) for(const r of g.regs) NAMEMAP[r[0]] = r[1];
for(let i=0;i<=30;i++) NAMEMAP["x"+i] = "X"+i;

const MODE_D = {0:"EL0t（用户态，用 SP_EL0）",4:"EL1t",5:"EL1h（内核，用 SP_ELx）",8:"EL2t",9:"EL2h（Hypervisor，用 SP_ELx）",12:"EL3t",13:"EL3h（Secure Monitor，用 SP_ELx）"};
const EC_D = {0x15:"SVC（AArch64 系统调用）",0x16:"HVC（虚拟化调用）",0x17:"SMC（安全监控调用）",0x3C:"BRK（软件断点）",0x00:"UNDEFINED（未定义指令）"};

function F(h,l,n,o){ return Object.assign({h:h,l:l,n:n},o||{}); }
function fldv(v,h,l){
  if(v===null||v===undefined) return null;
  return Math.floor(v/Math.pow(2,l))%Math.pow(2,h-l+1);
}
function fvstr(fv,w){
  if(fv===null) return "·";
  return w<=4 ? fv.toString(2).padStart(w,"0") : "0x"+fv.toString(16);
}
function bitrange(f){ return f.h===f.l ? String(f.h) : f.h+":"+f.l; }

const SPSR_UPPER = [
  F(63,36,"RES0",{res:1}),
  F(35,32,"UINJ",{res:1,d:"架构扩展字段（FEAT_NMI 引入，本示例恒为 0）"}),
  F(31,31,"N",{d:"条件标志：负（Negative）"}),
  F(30,30,"Z",{d:"条件标志：零（Zero）"}),
  F(29,29,"C",{d:"条件标志：进位/借位（Carry）"}),
  F(28,28,"V",{d:"条件标志：有符号溢出（oVerflow）"})
];
const SPSR_LOW = [
  F(27,25,"RES0",{res:1,d:"AArch64 下保留（Q / IT[1:0] 仅 AArch32 有效）"}),
  F(24,24,"DIT",{d:"数据无关时间（FEAT_DIT）"}),
  F(23,23,"UAO",{d:"未对齐访问覆盖（FEAT_UAO）"}),
  F(22,22,"PAN",{d:"特权访问永不（FEAT_PAN）"}),
  F(21,21,"SS",{d:"软件单步状态位（配合 MDSCR_EL1.SS）"}),
  F(20,20,"IL",{d:"非法执行状态位（位宽组合非法时置位）"}),
  F(19,13,"RES0",{res:1,d:"保留"}),
  F(12,12,"SSBS",{d:"推测屏障抑制提示（FEAT_SSBS）"}),
  F(11,10,"RES0",{res:1,d:"保留"}),
  F(9,9,"D",{d:v=>v?"1：Debug 异常屏蔽":"0：Debug 异常开放"}),
  F(8,8,"A",{d:v=>v?"1：SError 中止屏蔽":"0：SError 开放"}),
  F(7,7,"I",{d:v=>v?"1：IRQ 屏蔽（异常入口自动置 1）":"0：IRQ 开放（允许中断）"}),
  F(6,6,"F",{d:v=>v?"1：FIQ 屏蔽":"0：FIQ 开放"}),
  F(5,4,"RES0",{res:1,d:"保留（T / M[4] 仅 AArch32 有效）"}),
  F(3,0,"M",{d:v=>MODE_D[v]||"保留编码"})
];
const SPSR_ALT = [
  F(27,27,"Q"),F(26,25,"IT[1:0]"),F(24,24,"DIT"),F(23,23,"SSBS"),F(22,22,"PAN"),F(21,21,"SS"),F(20,20,"IL"),
  F(19,16,"GE"),F(15,10,"IT[7:2]"),F(9,9,"E"),F(8,8,"A"),F(7,7,"I"),F(6,6,"F"),F(5,5,"T"),F(4,4,"M[4]"),F(3,0,"M")
];
const ESR_ROW = [
  F(63,32,"ISS2",{res:1,d:"ISS 扩展位（本示例恒为 0）"}),
  F(31,26,"EC",{d:v=>EC_D[v]||"其他/未知类别"}),
  F(25,25,"IL",{d:v=>v?"1：32 位指令触发（IL=1）":"0：（保留）"}),
  F(24,0,"ISS",{d:(v,full)=>{
    const ec = full===null?null:fldv(full,31,26);
    if(ec===0x15||ec===0x16||ec===0x17||ec===0x3C) return "imm16 = 0x"+(v&0xFFFF).toString(16)+"（触发指令的立即数）";
    if(ec===0x00) return "未定义指令编码相关位（本例 0）";
    return "指令相关状态";
  }})
];
const SCR_ROW = [
  F(31,25,"RES0",{res:1}),
  F(24,24,"NSE",{d:"RME 路由域选择（本例 0）"}),
  F(23,15,"RES0",{res:1}),
  F(14,14,"TWE",{d:"Trap WFE 到 EL3"}),
  F(13,13,"TWI",{d:"Trap WFI 到 EL3"}),
  F(12,12,"RES0",{res:1}),
  F(11,11,"ST",{d:"Secure 自托管 trace 使能"}),
  F(10,10,"RW",{d:v=>v?"1：下一级（EL2）为 AArch64":"0：下一级为 AArch32"}),
  F(9,9,"RES0",{res:1}),
  F(8,8,"HCE",{d:v=>v?"1：HVC 使能（EL0/EL1 可发 HVC）":"0：HVC 视为未定义"}),
  F(7,7,"SMD",{d:v=>v?"1：SMC 禁用":"0：SMC 可用"}),
  F(6,5,"RES0",{res:1,d:"保留（QEMU 复位默认置 1）"}),
  F(4,4,"SIF",{d:v=>v?"1：Secure 态禁止非安全取指":"0：不限制取指"}),
  F(3,3,"EA",{d:v=>v?"1：外部中止/SError 路由到 EL3":"0：按当前 EL 路由"}),
  F(2,2,"FIQ",{d:v=>v?"1：FIQ 路由到 EL3":"0：按路由规则（本例送 EL1）"}),
  F(1,1,"IRQ",{d:v=>v?"1：IRQ 路由到 EL3":"0：IRQ 送往 EL1（本例）"}),
  F(0,0,"NS",{d:v=>v?"1：ERET 后进入 Non-secure 世界":"0：保持 Secure"})
];
const HCR_ROW = [
  F(31,31,"RW",{d:v=>v?"1：EL1 为 AArch64":"0：EL1 为 AArch32"}),
  F(30,30,"RES0",{res:1}),
  F(29,29,"HCD",{d:v=>v?"1：HVC 禁用（视为未定义）":"0：HVC 可用（本例）"}),
  F(28,28,"RES0",{res:1}),
  F(27,27,"TGE",{d:v=>v?"1：一般异常 trap 到 EL2":"0：不 trap（本例）"}),
  F(26,26,"TVM",{d:"Trap 虚拟内存控制寄存器到 EL2"}),
  F(25,20,"RES0",{res:1}),
  F(19,19,"TSC",{d:v=>v?"1：SMC trap 到 EL2":"0：SMC 直通 EL3（本例）"}),
  F(18,15,"RES0",{res:1}),
  F(14,14,"TWE",{d:"Trap WFE 到 EL2"}),
  F(13,13,"TWI",{d:"Trap WFI 到 EL2"}),
  F(12,5,"RES0",{res:1}),
  F(4,4,"IMO",{d:v=>v?"1：IRQ 虚拟化/路由到 EL2":"0：物理路由（本例）"}),
  F(3,3,"FMO",{d:v=>v?"1：FIQ 路由到 EL2":"0：物理路由"}),
  F(2,1,"RES0",{res:1}),
  F(0,0,"VM",{d:v=>v?"1：启用 EL1 虚拟化（stage-2）":"0：关闭（本例）"})
];
const SCTLR_ROW = [
  F(31,26,"RES0",{res:1}),
  F(25,25,"EE",{d:v=>v?"1：整体大端":"0：整体小端（本例）"}),
  F(24,24,"E0E",{d:"EL0 数据端序（0=小端）"}),
  F(23,20,"RES0",{res:1}),
  F(19,19,"WXN",{d:v=>v?"1：可写区域不可执行":"0：不限制"}),
  F(18,18,"nTWE",{d:v=>v?"1：WFE 不 trap（复位默认）":"0：WFE trap"}),
  F(17,17,"RES0",{res:1}),
  F(16,16,"nTWI",{d:v=>v?"1：WFI 不 trap（复位默认）":"0：WFI trap"}),
  F(15,13,"RES0",{res:1}),
  F(12,12,"I",{d:v=>v?"1：指令 cache 使能":"0：指令 cache 关闭"}),
  F(11,6,"RES0",{res:1}),
  F(5,5,"RES0",{res:1,d:"QEMU 复位默认置 1"}),
  F(4,4,"SA0",{d:"EL0 SP 对齐检查"}),
  F(3,3,"SA",{d:"SP 对齐检查（复位默认 1）"}),
  F(2,2,"C",{d:"数据 cache 使能（本例 0）"}),
  F(1,1,"A",{d:"对齐检查（本例 0）"}),
  F(0,0,"M",{d:v=>v?"1：MMU 开启":"0：MMU 关闭（本例裸机直映射）"})
];
const CNTKCTL_ROW = [
  F(31,10,"RES0",{res:1}),
  F(9,9,"EL0PTEN",{d:v=>v?"1：允许 EL0 访问 EL1 物理定时器（cntp_*_el0）← 本示例关键":"0：EL0 访问定时器将 trap 到 EL1"}),
  F(8,8,"EL0VTEN",{d:"允许 EL0 访问虚拟定时器"}),
  F(7,4,"EVNTI",{d:"事件流触发计数值"}),
  F(3,3,"EVNTDIR",{d:"事件流触发方向"}),
  F(2,2,"EVNTEN",{d:"事件流使能"}),
  F(1,1,"EL0VCTEN",{d:"允许 EL0 读虚拟计数器"}),
  F(0,0,"EL0PCTEN",{d:"允许 EL0 读物理计数器 CNTPCT"})
];
const CNTPCTL_ROW = [
  F(31,3,"RES0",{res:1}),
  F(2,2,"ISTATUS",{d:v=>v?"1：定时器条件已满足，输出已断言（即将触发 IRQ）":"0：输出未断言"}),
  F(1,1,"IMASK",{d:v=>v?"1：屏蔽定时器中断输出":"0：不屏蔽（本例）"}),
  F(0,0,"ENABLE",{d:v=>v?"1：定时器使能，TVAL 开始递减":"0：定时器关闭"})
];

const BF = {};
for(let el=1;el<=3;el++){
  BF["spsr_el"+el] = {name:"SPSR_EL"+el, upper:SPSR_UPPER, low:SPSR_LOW, alt:SPSR_ALT, grid:true,
    note:"异常进入时由硬件自动写入 PSTATE 快照；ERET 时整体恢复。M 决定返回后的 EL 与栈选择，DAIF 决定中断开关。"};
  BF["esr_el"+el] = {name:"ESR_EL"+el, row:ESR_ROW,
    note:"同步异常入口由硬件写入：EC=异常类别，IL=指令长度，ISS=具体信息（SVC/HVC/SMC/BRK 为 imm16）。"};
}
BF.scr_el3 = {name:"SCR_EL3", row:SCR_ROW, note:"bits[63:32] 恒为 0。QEMU 上 bit4/5 为复位默认 1，写入 0x501 读回 0x531。"};
BF.hcr_el2 = {name:"HCR_EL2", row:HCR_ROW, note:"bits[63:32] 恒为 0。本例只设 RW=1，SMC/HVC 直通。"};
BF.sctlr_el1 = {name:"SCTLR_EL1", row:SCTLR_ROW, note:"QEMU cortex-a53 复位值 0x00C50838，本示例只读（不用 MMU/cache）。"};
BF.cntkctl_el1 = {name:"CNTKCTL_EL1", row:CNTKCTL_ROW, note:"bits[63:32] 恒为 0。"};
BF.cntp_ctl = {name:"CNTP_CTL_EL0", row:CNTPCTL_ROW, note:"bits[63:3] 恒为 0。"};
const BFPRIORITY = ["spsr_el3","spsr_el2","spsr_el1","esr_el3","esr_el2","esr_el1","scr_el3","hcr_el2","cntkctl_el1","cntp_ctl","sctlr_el1"];

let bfCur = "spsr_el1", bfPre = null, bfChg = [];

function cellCore(f, val, old, alt){
  const w = f.h-f.l+1;
  const fv = fldv(val, f.h, f.l);
  const ov = (old===null||old===undefined) ? 0 : fldv(old, f.h, f.l);
  const set = fv!==null && fv!==0;
  const flip = fv!==null && fv!==ov;
  const cls = "bcell"+(f.res?" res":"")+(set?" set":"")+(alt?" alt":"")+(flip?" flip":"");
  return {cls:cls, fv:fv, w:w, set:set, flip:flip};
}
function fcell(f, val, old, alt){
  const c = cellCore(f, val, old, alt);
  const mw = Math.max(28, f.n.length*7+9);
  return '<div class="'+c.cls+'" style="flex-grow:'+c.w+";min-width:"+mw+'px"><div class="bn">'+bitrange(f)+'</div><div class="bfn">'+f.n+'</div><div class="bv">'+fvstr(c.fv,c.w)+"</div></div>";
}
function gcell(f, val, old, alt){
  const c = cellCore(f, val, old, alt);
  return '<div class="'+c.cls+'" style="grid-column:span '+c.w+'"><div class="bn">'+bitrange(f)+'</div><div class="bfn">'+f.n+'</div><div class="bv">'+fvstr(c.fv,c.w)+"</div></div>";
}
function bfFields(def){ return def.grid ? def.upper.concat(def.low) : def.row; }
function bitOwner(def, b){
  for(const f of bfFields(def)){ if(b<=f.h && b>=f.l) return f.n; }
  return "";
}
function renderBitfield(pre, changed){
  bfPre = pre; bfChg = changed || [];
  if($("bfauto").checked && bfChg.length){
    for(const k of BFPRIORITY){ if(bfChg.includes(k)){ bfCur = k; break; } }
    $("bfselect").value = bfCur;
  }
  const def = BF[bfCur];
  if(!def) return;
  const val = state[bfCur];
  const old = (pre===null||pre===undefined) ? null : pre[bfCur];

  $("bfvalue").innerHTML = '<span class="bfc">'+def.name+"</span> = "+(val===null?"—（尚未写入，继续单步观察）":hx16(val));

  let chgTxt = "";
  if(val!==null){
    const ov = (old===null||old===undefined)?0:old;
    const xs = [];
    for(let b=0;b<32;b++){
      if(((val>>>b)&1)!==((ov>>>b)&1)) xs.push("bit"+b+(bitOwner(def,b)?"("+bitOwner(def,b)+")":"")+": "+((ov>>>b)&1)+"→"+((val>>>b)&1));
    }
    chgTxt = xs.length ? "本步位变化："+xs.join(" · ") : (bfChg.includes(bfCur)?"本步重写了该寄存器（值未变）":"本步该寄存器无变化");
  }
  $("bfchg").textContent = chgTxt;

  let h = "";
  if(def.grid){
    h += '<div class="bflbl">bits 63 … 28</div>';
    h += '<div class="bfrow">'+def.upper.map(f=>fcell(f,val,old,false)).join("")+"</div>";
    h += '<div class="bflbl">bits 27 … 0 —— AArch64 视图（本示例实际状态）</div>';
    h += '<div class="bfgrid">'+def.low.map(f=>gcell(f,val,old,false)).join("")+"</div>";
    h += '<div class="bflbl">bits 27 … 0 —— AArch32 视图（对照参考，本示例为 AArch64）</div>';
    h += '<div class="bfgrid">'+def.alt.map(f=>gcell(f,val,old,true)).join("")+"</div>";
  } else {
    h += '<div class="bflbl">bits '+(def.row[0].h)+" … 0</div>";
    h += '<div class="bfrow">'+def.row.map(f=>fcell(f,val,old,false)).join("")+"</div>";
  }
  $("bfwrap").innerHTML = h;

  let dh = "";
  if(val===null){
    dh = '<div class="bde"><span class="m">该寄存器尚未被写入；单步到相关步骤时位图会自动出现。</span></div>';
  } else {
    for(const f of bfFields(def)){
      const w = f.h-f.l+1;
      const fv = fldv(val, f.h, f.l);
      const ov = (old===null||old===undefined)?0:fldv(old, f.h, f.l);
      if(f.res && fv===0) continue;
      const chg = fv!==ov;
      let m = "";
      if(f.d){ m = (typeof f.d==="function") ? f.d(fv, val) : f.d; }
      dh += '<div class="bde'+(chg?" chg":"")+'"><span class="n">'+f.n+'</span><span class="b">['+bitrange(f)+']</span><span class="v">= '+fvstr(fv,w)+'</span><span class="m">'+m+"</span></div>";
    }
    if(def.note) dh += '<div class="bde"><span class="m">'+def.note+"</span></div>";
    if(def.alt) dh += '<div class="bde"><span class="m">第二行 AArch32 视图字段（Q、IT、GE、E、T、M[4]）仅 AArch32 状态有效，本示例恒为 0/无效，仅供对照。</span></div>';
  }
  $("bfdecode").innerHTML = dh;
}

function freshState(){
  const st = {pc:A.start, el:3, mode:"EL3h", daif:15, secure:true, sp:0,
    spEl0:null, spEl1:null, spEl2:null, spEl3:null,
    scr_el3:0x30, vbar_el3:null, spsr_el3:null, elr_el3:null, esr_el3:null,
    hcr_el2:null, vbar_el2:null, spsr_el2:null, elr_el2:null, esr_el2:null,
    sctlr_el1:0xC50838, vbar_el1:null, cntkctl_el1:null, spsr_el1:null, elr_el1:null, esr_el1:null,
    cntp_tval:null, cntp_ctl:null,
    gicd_igroupr:null, gicd_ctlr:null, gicd_isenabler:null, gicc_ctlr:null, gicc_pmr:null, gicc_iar:null, gicc_eoir:null,
    g_irq_ok:0};
  for(let i=0;i<=30;i++) st["x"+i] = null;
  st.x19 = 0;
  return st;
}

function applyStep(st, s){
  const changed = [];
  for(const k in s.w){ if(st[k]!==s.w[k]){ st[k]=s.w[k]; changed.push(k); } }
  if(s.to){
    for(const k in s.to){ if(st[k]!==s.to[k]){ st[k]=s.to[k]; changed.push(k); } }
  } else if(st.pc !== s.pc+4){
    st.pc = s.pc+4; changed.push("pc");
  }
  return changed;
}

function hx16(v){ return v===null||v===undefined ? "—" : "0x"+(v>>>0).toString(16).padStart(8,"0").padStart(16,"0"); }
function hx8(v){ return v===null||v===undefined ? "—" : "0x"+v.toString(16).padStart(8,"0"); }
function modeOfSPSR(v){ const m=v&0xF; return {0:"EL0t",4:"EL0t",5:"EL1h",9:"EL2h",13:"EL3h"}[m]||"?"; }
function esrDecode(v){
  if(v===null||v===undefined) return "";
  const ec=(v>>>26)&0x3F, il=(v>>>25)&1, imm=v&0xFFFF;
  const names={0x15:"SVC",0x16:"HVC",0x17:"SMC",0x3C:"BRK",0x00:"UNDEFINED"};
  return "EC=0x"+ec.toString(16)+(names[ec]?" ("+names[ec]+")":"")+"  IL="+il+"  imm=0x"+imm.toString(16);
}
function spsrDecode(v){
  if(v===null||v===undefined) return "";
  const nzcv=(v>>>28)&0xF, m=v&0xF, daif=(v>>>6)&0xF;
  return modeOfSPSR(v)+" · NZCV=0b"+nzcv.toString(2).padStart(4,"0")+" · DAIF=0b"+daif.toString(2).padStart(4,"0");
}

const $ = id=>document.getElementById(id);
let idx = 0, state = freshState(), playing = false, timer = null;

function buildStatic(){
  const xr = $("xregs");
  let h = "";
  for(let i=0;i<=30;i++){
    if(i===19) continue;
    h += '<div class="rrow" id="r-x'+i+'"><span class="n">X'+i+'</span><span class="v na">—</span></div>';
  }
  xr.innerHTML = h;

  const sr = $("sysregs");
  h = "";
  for(const g of SYSGROUPS){
    h += '<h3>'+g.t+'</h3><div class="reggrid">';
    for(const r of g.regs){
      if(BF[r[0]]){
        h += '<div class="rrow bf" id="r-'+r[0]+'" data-bf="'+r[0]+'" title="点击查看位域图"><span class="n">'+r[1]+'</span><span class="v na">—</span></div>';
      } else {
        h += '<div class="rrow" id="r-'+r[0]+'" title=""><span class="n">'+r[1]+'</span><span class="v na">—</span></div>';
      }
    }
    h += "</div>";
  }
  sr.innerHTML = h;
  sr.addEventListener("click", e=>{
    const r = e.target.closest(".rrow");
    if(!r || !r.dataset.bf) return;
    bfCur = r.dataset.bf;
    $("bfselect").value = bfCur;
    $("bfauto").checked = false;
    renderBitfield(bfPre, bfChg);
    $("bfpanel").scrollIntoView({behavior:"smooth", block:"nearest"});
  });

  const bs = $("bfselect");
  h = "";
  for(const k of BFPRIORITY) h += '<option value="'+k+'">'+BF[k].name+"</option>";
  bs.innerHTML = h;
  bs.value = bfCur;
  bs.addEventListener("change", ()=>{
    bfCur = bs.value;
    $("bfauto").checked = false;
    renderBitfield(bfPre, bfChg);
  });

  const vt = $("vt");
  const groups = [
    {t:"当前EL SP_EL0", base:0x000},
    {t:"当前EL SP_ELx", base:0x200},
    {t:"低EL AArch64", base:0x400},
    {t:"低EL AArch32", base:0x600}
  ];
  const slots = [["0x000","同步","unexpected"],["0x080","IRQ","unexpected"],["0x100","FIQ","unexpected"],["0x180","SError","unexpected"],
    ["0x200","同步","unexpected"],["0x280","IRQ","unexpected"],["0x300","FIQ","unexpected"],["0x380","SError","unexpected"],
    ["0x400","同步","sync"],["0x480","IRQ","irq"],["0x500","FIQ","unexpected"],["0x580","SError","unexpected"],
    ["0x600","同步","unexpected"],["0x680","IRQ","unexpected"],["0x700","FIQ","unexpected"],["0x780","SError","unexpected"]];
  h = "";
  let gi = 0;
  for(const g of groups){
    h += '<div class="grouph">'+g.t+"</div>";
    for(let s=0;s<4;s++){
      const cell = slots[gi];
      const cls = "vcell"+(g.base===0x400?" hot":"");
      h += '<div class="'+cls+'" data-vec="'+cell[0]+'"><span class="off">+'+cell[0]+'</span> <span class="ty">'+cell[1]+'</span><br><span class="hd">'+(cell[2]==="sync"?"lower_el_sync_handler":cell[2]==="irq"?"lower_el_irq_handler":"unexpected_handler")+"</span></div>";
      gi++;
    }
  }
  vt.innerHTML = h;

  const ph = $("phases");
  h = "";
  PHASES.forEach((p,i)=>{ h += '<span class="chip" data-ph="'+i+'">'+p.name+"</span>"; });
  ph.innerHTML = h;
  ph.addEventListener("click", e=>{
    const c = e.target.closest(".chip");
    if(!c) return;
    const want = +c.dataset.ph;
    let t = 0;
    for(let i=0;i<STEPS.length;i++) if(STEPS[i].ph===want){ t=i; break; }
    pause(); gotoIdx(t);
  });

  const tr = $("trace");
  h = "";
  let lastPh = -1;
  STEPS.forEach((s,i)=>{
    if(s.ph!==lastPh){
      lastPh = s.ph;
      h += '<div class="phdr">'+PHASES[s.ph].name+' <small>'+PHASES[s.ph].hint+"</small></div>";
    }
    h += '<div class="trow'+(s.hw?" hw":"")+'" data-i="'+i+'"><span class="a">'+hx8(s.pc)+'</span><span class="c">'+s.code+"</span></div>";
  });
  tr.innerHTML = h;
  tr.addEventListener("click", e=>{
    const r = e.target.closest(".trow");
    if(!r) return;
    pause(); gotoIdx(+r.dataset.i);
  });
}

function setVal(id, txt, chg, title){
  const el = $(id);
  if(!el) return;
  const v = el.querySelector(".v");
  v.textContent = txt;
  v.classList.toggle("na", txt==="—");
  el.classList.toggle("chg", !!chg);
  if(title!==undefined) el.title = title;
}

function renderRegs(changed){
  const ch = new Set(changed);
  for(let i=0;i<=30;i++) setVal("r-x"+i, state["x"+i]===null?"—":hx16(state["x"+i]), ch.has("x"+i));
  setVal("r-x19", hx16(state.x19), ch.has("x19"));
  for(const g of SYSGROUPS){
    for(const r of g.regs){
      const k = r[0];
      let title = "";
      if(k.startsWith("esr")) title = esrDecode(state[k]);
      if(k.startsWith("spsr")) title = spsrDecode(state[k]);
      if(k==="scr_el3" && state[k]!==null) title = "NS(bit0)="+((state[k]>>0)&1)+" HCE(bit8)="+((state[k]>>8)&1)+" RW(bit10)="+((state[k]>>10)&1);
      if(k==="hcr_el2" && state[k]!==null) title = "RW(bit31)="+((state[k]>>>31)&1);
      if(k==="cntkctl_el1" && state[k]!==null) title = "EL0PTEN(bit9)="+((state[k]>>9)&1);
      setVal("r-"+k, state[k]===null?"—":hx16(state[k]), ch.has(k), title);
    }
  }
  setVal("r-sp", state.sp===null?"—":hx16(state.sp), ch.has("sp"));
  setVal("r-spel0", state.spEl0===null?"—":hx16(state.spEl0), ch.has("spEl0"));
  setVal("r-spel1", state.spEl1===null?"—":hx16(state.spEl1), ch.has("spEl1"));
  setVal("r-spel2", state.spEl2===null?"—":hx16(state.spEl2), ch.has("spEl2"));
  setVal("r-spel3", state.spEl3===null?"—":hx16(state.spEl3), ch.has("spEl3"));

  $("v-pc").textContent = hx8(STEPS[idx].pc);
  $("ts-pc").classList.toggle("chg", ch.has("pc"));
  $("v-el").textContent = "EL"+state.el;
  $("ts-el").classList.toggle("chg", ch.has("el"));
  const d = state.daif;
  $("daifl").innerHTML = ["D","A","I","F"].map((c,i)=>'<span class="'+(((d>>(3-i))&1)?"m":"")+'">'+c+"</span>").join(" ");
  $("ts-daif").classList.toggle("chg", ch.has("daif"));
}

function renderEL(){
  document.querySelectorAll(".elrow").forEach(r=>{
    r.classList.toggle("on", +r.dataset.el===state.el);
  });
  $("sp0").textContent = "SP_EL0 "+(state.spEl0===null?"—":hx8(state.spEl0));
  $("sp1").textContent = "SP_EL1 "+(state.spEl1===null?"—":hx8(state.spEl1));
  $("sp2").textContent = "SP_EL2 "+(state.spEl2===null?"—":hx8(state.spEl2));
  $("sp3").textContent = "SP_EL3 "+(state.spEl3===null?"—":hx8(state.spEl3));
  const sec = $("secv");
  sec.textContent = state.secure ? "Secure" : "Non-secure";
  sec.classList.toggle("ns", !state.secure);
  $("modev").textContent = state.mode;
}

function elBadge(ev){
  if(!ev || (ev.kind!=="exc" && ev.kind!=="eret")) return;
  const col = $("elcol");
  const rows = col.querySelectorAll(".elrow");
  const up = ev.kind==="exc";
  const from = up?ev.from:ev.to, to = up?ev.to:ev.from;
  const src = rows[3-from];
  const dst = rows[3-to];
  if(!src||!dst) return;
  const b = document.createElement("div");
  b.className = "tbadge "+(up?"up":"down");
  b.textContent = (up?"▲ "+ev.cause+"：EL"+from+" → EL"+to : "▼ "+ev.cause+"：EL"+from+" → EL"+to);
  const y = (src.offsetTop + dst.offsetTop)/2;
  b.style.top = y+"px";
  col.appendChild(b);
  setTimeout(()=>b.remove(), 1400);
}

function flashVec(ev){
  if(!ev || ev.kind!=="exc" || ev.vec===undefined) return;
  const off = "0x"+ev.vec.toString(16);
  const cell = document.querySelector('.vcell[data-vec="'+off+'"]');
  if(!cell) return;
  cell.classList.remove("hit");
  void cell.offsetWidth;
  cell.classList.add("hit");
}

const TAGCLS = {"[EL0]":"c-el0","[EL1]":"c-el1","[EL2]":"c-el2","[EL3]":"c-el3","[SYNC]":"c-sync","[ASYNC]":"c-async","[IRQ]":"c-irq","[OK]":"c-ok","[REG]":"c-reg","[RET]":"c-ret","[WARN]":"c-warn","[INFO]":"c-info"};

function renderConsole(){
  const c = $("console");
  let h = "";
  for(let i=0;i<=idx;i++){
    for(const lg of STEPS[i].logs){
      if(lg.k==="blank"){ h += '<div class="cl"></div>'; }
      else if(lg.k==="banner"){ h += '<div class="cl banner">'+lg.t+"</div>"; }
      else if(lg.k==="kv"){ h += '<div class="cl kv"><span class="kk">'+lg.key+":</span>"+lg.val+"</div>"; }
      else if(lg.k==="step"){ h += '<div class="cl step">'+lg.t+"</div>"; }
      else if(lg.k==="test"){ h += '<div class="cl test">'+lg.t+"</div>"; }
      else if(lg.k==="ln"){ h += '<div class="cl"><span class="tag '+(TAGCLS[lg.tag]||"")+'">'+lg.tag+"</span>"+lg.t+"</div>"; }
    }
  }
  c.innerHTML = h;
  c.scrollTop = c.scrollHeight;
}

function renderDesc(changed){
  const s = STEPS[idx];
  $("dloc").textContent = s.loc + (s.hw?"  ·  硬件/隐含动作":"");
  $("dcode").textContent = s.code;
  $("descbox").classList.toggle("hwstep", !!s.hw);
  $("ddesc").textContent = s.desc;
  const parts = [];
  for(const k in s.w){
    if(k==="x30") continue;
    parts.push(NAMEMAP[k]+" ← "+(k==="g_irq_ok"?String(s.w[k]):hx16(s.w[k])));
  }
  if(s.w.x30!==undefined) parts.push("X30(LR) ← "+hx8(s.w.x30));
  if(s.to){
    if(s.to.pc!==undefined) parts.push("PC ← "+hx8(s.to.pc));
    if(s.to.el!==undefined) parts.push("PSTATE ← "+s.to.mode+"（EL"+s.to.el+"，DAIF="+(s.to.daif?"屏蔽":"开放")+"）");
    if(s.to.sp!==undefined) parts.push("SP ← "+hx16(s.to.sp));
    if(s.to.secure!==undefined) parts.push("安全状态 ← "+(s.to.secure?"Secure":"Non-secure"));
  }
  const w = $("writes");
  if(parts.length){
    w.innerHTML = "<div>本步效果：</div>"+parts.map(p=>"<div>  <b>"+p+"</b></div>").join("");
    w.style.display = "block";
  } else {
    w.style.display = "none";
  }
}

function renderTrace(){
  const cur = document.querySelector('.trow[data-i="'+idx+'"]');
  document.querySelectorAll(".trow.cur").forEach(r=>r.classList.remove("cur"));
  if(cur){ cur.classList.add("cur"); cur.scrollIntoView({block:"nearest"}); }
  document.querySelectorAll(".chip.cur").forEach(c=>c.classList.remove("cur"));
  const chip = document.querySelector('.chip[data-ph="'+STEPS[idx].ph+'"]');
  if(chip) chip.classList.add("cur");
}

function renderProg(){
  $("prog").textContent = "步骤 "+(idx+1)+"/"+STEPS.length+" · "+PHASES[STEPS[idx].ph].name;
  $("bprev").disabled = idx===0;
  $("bnext").disabled = idx===STEPS.length-1;
}

function gotoIdx(i, animate){
  idx = Math.max(0, Math.min(STEPS.length-1, i));
  const st = freshState();
  let changed = [], pre = null;
  for(let j=0;j<=idx;j++){
    if(j===idx){
      pre = {};
      for(const k in BF) pre[k] = st[k];
      changed = applyStep(st, STEPS[j]);
    } else {
      applyStep(st, STEPS[j]);
    }
  }
  state = st;
  renderTrace();
  renderRegs(changed);
  renderEL();
  renderDesc();
  renderConsole();
  renderProg();
  renderBitfield(pre, changed);
  if(animate!==false){
    const ev = STEPS[idx].ev;
    elBadge(ev);
    flashVec(ev);
  }
}

function step(){ if(idx<STEPS.length-1) gotoIdx(idx+1); else pause(); }
function play(){
  if(playing) return;
  playing = true;
  $("bplay").textContent = "暂停";
  const tick = ()=>{
    if(!playing) return;
    step();
    if(idx>=STEPS.length-1){ pause(); return; }
    timer = setTimeout(tick, +$("speed").value);
  };
  tick();
}
function pause(){
  playing = false;
  clearTimeout(timer);
  $("bplay").textContent = "自动运行";
}

$("bnext").addEventListener("click", ()=>{ pause(); step(); });
$("bprev").addEventListener("click", ()=>{ pause(); gotoIdx(idx-1); });
$("bplay").addEventListener("click", ()=>{ playing?pause():play(); });
$("breset").addEventListener("click", ()=>{ pause(); gotoIdx(0); });
document.addEventListener("keydown", e=>{
  if(e.target.tagName==="SELECT") return;
  if(e.code==="Space"){ e.preventDefault(); playing?pause():play(); }
  else if(e.code==="ArrowRight"){ e.preventDefault(); pause(); step(); }
  else if(e.code==="ArrowLeft"){ e.preventDefault(); pause(); gotoIdx(idx-1); }
  else if(e.code==="KeyR"){ pause(); gotoIdx(0); }
});

buildStatic();
gotoIdx(0);
</script>
