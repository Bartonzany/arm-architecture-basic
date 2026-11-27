#!/usr/bin/env node
"use strict";
const fs = require("fs");
const path = require("path");
const root = path.resolve(__dirname, "..");
const html = fs.readFileSync(path.join(root, "docs/exception-visualizer.html"), "utf8");
const js = html.split("<script>")[1].split("</script>")[0];

let fails = 0;
function ok(cond, msg){ if(!cond){ fails++; console.log("FAIL: " + msg); } }

ok(html.includes("</html>"), "html complete");
ok(js.length > 5000, "script extracted");

const idRefs = new Set();
for(const m of js.matchAll(/\$\("([a-zA-Z0-9-]+)"\)/g)) idRefs.add(m[1]);
for(const id of idRefs){
  ok(new RegExp('id="' + id + '"').test(html), 'html 缺少 id="' + id + '"');
}

const VOID = new Set(["br","hr","input","meta","link"]);
class Elem {
  constructor(tag){ this.tagName=(tag||"div").toUpperCase(); this.children=[]; this.dataset={}; this._cls=new Set(); this._text=""; this.parentNode=null; this.style={}; this.scrollTop=0; this.scrollHeight=0; this.offsetTop=0; this.offsetWidth=0; this.disabled=false; this.value=""; this.attrs={}; }
  set className(v){ this._cls=new Set(String(v).split(/\s+/).filter(Boolean)); }
  get className(){ return [...this._cls].join(" "); }
  get id(){ return this.attrs.id||""; }
  set id(v){ this.attrs.id=v; }
  get classList(){ const self=this; return {
    add:(...c)=>c.forEach(x=>self._cls.add(x)),
    remove:(...c)=>c.forEach(x=>self._cls.delete(x)),
    toggle:(c,f)=>{ if(f===undefined){ f=!self._cls.has(c);} f?self._cls.add(c):self._cls.delete(c); return f; },
    contains:c=>self._cls.has(c) }; }
  get title(){ return this.attrs.title||""; }
  set title(v){ this.attrs.title=v; }
  set innerHTML(v){ this.children=[]; this._text=""; this._raw=String(v); parseHTML(this, String(v)); }
  get innerHTML(){ return this._raw!==undefined ? this._raw : (this._text + this.children.map(c=>c.innerHTML).join("")); }
  set textContent(v){ this.children=[]; this._text=String(v); this._raw=undefined; }
  get textContent(){ return this._text + this.children.map(c=>c.textContent).join(""); }
  appendChild(ch){ ch.parentNode=this; this.children.push(ch); return ch; }
  remove(){ if(this.parentNode){ const i=this.parentNode.children.indexOf(this); if(i>=0) this.parentNode.children.splice(i,1); } }
  addEventListener(){}
  scrollIntoView(){}
  closest(cls){ let n=this; cls=cls.replace(/^\./,""); while(n){ if(n._cls.has(cls)) return n; n=n.parentNode; } return null; }
  _walk(fn){ fn(this); this.children.forEach(c=>c._walk(fn)); }
  matches(sel){
    const parts = sel.trim().split(/(?=[.\[])/);
    let tag=null, classes=[], attrs=[];
    for(const p of parts){
      if(p[0]===".") classes.push(p.slice(1));
      else if(p[0]==="["){ const m=p.match(/\[([a-zA-Z-]+)(?:(=)"([^"]*)")?\]/); if(m && m[2]) attrs.push([m[1],m[3]]); else if(m) attrs.push([m[1],null]); }
      else tag=p.toLowerCase();
    }
    if(tag && this.tagName.toLowerCase()!==tag) return false;
    for(const c of classes) if(!this._cls.has(c)) return false;
    for(const [k,v] of attrs){
      let val = k==="class" ? this.className : (k.startsWith("data-") ? (this.dataset[k.slice(5)]===undefined?"":String(this.dataset[k.slice(5)])) : String(this.attrs[k]!==undefined?this.attrs[k]:""));
      if(v===null){ if(val==="") return false; }
      else if(val!==v) return false;
    }
    return true;
  }
  querySelectorAll(sel){ const out=[]; this.children.forEach(c=>c._walk(n=>{ if(n.matches(sel)) out.push(n); })); return out; }
  querySelector(sel){ return this.querySelectorAll(sel)[0]||null; }
}

const byId = {};
function parseHTML(parent, html){
  let pos=0; const stack=[parent];
  const tagRe=/<\/?([a-zA-Z][a-zA-Z0-9]*)((?:\s+[a-zA-Z-]+="[^"]*")*)\s*\/?>/;
  while(pos<html.length){
    const lt=html.indexOf("<",pos);
    if(lt<0){ stack[stack.length-1]._text+=html.slice(pos); break; }
    if(lt>pos) stack[stack.length-1]._text+=html.slice(pos,lt);
    const m=html.slice(lt).match(tagRe);
    if(!m){ stack[stack.length-1]._text+=html.slice(lt,lt+1); pos=lt+1; continue; }
    const full=m[0], tag=m[1].toLowerCase();
    pos=lt+full.length;
    if(full.startsWith("</")){ if(stack.length>1) stack.pop(); continue; }
    const el=new Elem(tag);
    const attrRe=/([a-zA-Z-]+)="([^"]*)"/g; let am;
    while((am=attrRe.exec(m[2]))){
      const k=am[1], v=am[2];
      if(k==="class") String(v).split(/\s+/).forEach(c=>c&&el._cls.add(c));
      else if(k==="id"){ el.attrs.id=v; byId[v]=el; }
      else if(k==="title") el.attrs.title=v;
      else if(k.startsWith("data-")) el.dataset[k.slice(5)]=v;
      else el.attrs[k]=v;
    }
    stack[stack.length-1].appendChild(el);
    if(!full.endsWith("/>") && !VOID.has(tag)) stack.push(el);
  }
}
function mk(tag,id){ const e=new Elem(tag); if(id){e.attrs.id=id; byId[id]=e;} return e; }

const ids=["breset","bprev","bnext","bplay","speed","prog","phases","trace","elcol","secv","modev","vt","dloc","dcode","ddesc","writes","descbox","descpanel","xregs","sysregs","console","sp0","sp1","sp2","sp3","v-pc","v-el","daifl","ts-pc","ts-el","ts-daif","bfpanel","bfselect","bfauto","bfvalue","bfchg","bfwrap","bfdecode"];
for(const id of ids) mk("div",id);
const elcol=byId["elcol"];
for(let el=3;el>=0;el--){ const r=new Elem("div"); r._cls.add("elrow"); r.dataset.el=String(el); elcol.appendChild(r); }
byId["speed"].value="400"; byId["bplay"].textContent=""; byId["bfauto"].checked=true;

const document = {
  getElementById:id=>byId[id]||null,
  createElement:t=>new Elem(t),
  querySelectorAll(sel){ const out=[]; Object.values(byId).forEach(r=>r._walk(n=>{ if(n!==r && n.matches(sel)) out.push(n); })); return out; },
  querySelector(sel){ return document.querySelectorAll(sel)[0]||null; },
  addEventListener:()=>{}
};

let err=null;
try{ new Function("document", js + "; globalThis.__gotoIdx = gotoIdx;")(document); }catch(e){ err=e; }
ok(err===null, "runtime error: " + (err ? err.stack.split("\n").slice(0,4).join(" | ") : ""));

ok(byId["trace"].querySelectorAll(".phdr").length===10, "10 phase headers");
ok(byId["trace"].querySelectorAll(".trow").length===122, "122 trace rows: " + byId["trace"].querySelectorAll(".trow").length);
ok(byId["vt"].querySelectorAll(".vcell").length===16, "16 vector cells");
ok(byId["xregs"].querySelectorAll(".rrow").length===30, "30 xreg rows");
ok(byId["sysregs"].querySelectorAll(".rrow").length===26, "26 sysreg rows");
ok(byId["prog"].textContent.includes("1/122"), "progress: " + byId["prog"].textContent);
ok(document.querySelectorAll("[data-bf]").length===11, "11 bf clickable rows: " + document.querySelectorAll("[data-bf]").length);
ok(byId["bfselect"].innerHTML.split("<option").length-1===11, "11 bfselect options");
ok(byId["bfvalue"].innerHTML.includes("尚未写入"), "empty at step 0");

const __gotoIdx = globalThis.__gotoIdx || (()=>{});

__gotoIdx(12);
ok(byId["console"].innerHTML.includes("AArch64 异常模型验证 启动"), "console banner");
ok(byId["v-pc"].textContent==="0x40000050", "PC at step 12: " + byId["v-pc"].textContent);

__gotoIdx(31);
let on31 = elcol.querySelectorAll(".elrow").filter(r=>r._cls.has("on"));
ok(on31.length===1 && on31[0].dataset.el==="0" && byId["secv"].textContent==="Non-secure", "EL0 + Non-secure at step 31");

__gotoIdx(36);
let on36 = elcol.querySelectorAll(".elrow").filter(r=>r._cls.has("on"));
ok(on36.length===1 && on36[0].dataset.el==="1", "EL1 at SVC entry");
ok(byId["bfvalue"].innerHTML.includes("SPSR_EL1") && byId["bfvalue"].innerHTML.includes("0x0000000080000000"), "SPSR_EL1 after entry: " + byId["bfvalue"].innerHTML);
ok(byId["bfchg"].textContent.includes("bit31"), "N flip: " + byId["bfchg"].textContent);
ok(byId["bfdecode"].innerHTML.includes("EL0t"), "M decode EL0t");

__gotoIdx(62);
ok(byId["bfvalue"].innerHTML.includes("SPSR_EL3") && byId["bfvalue"].innerHTML.includes("0x00000000800003c9"), "SPSR_EL3 after SMC entry: " + byId["bfvalue"].innerHTML);
ok(byId["secv"].textContent==="Secure", "Secure at EL3");

__gotoIdx(78);
ok(byId["bfvalue"].innerHTML.includes("ESR_EL1") && byId["bfvalue"].innerHTML.includes("0x00000000f2000000"), "ESR_EL1 at BRK entry: " + byId["bfvalue"].innerHTML);
ok(byId["bfdecode"].innerHTML.includes("BRK"), "EC decode BRK");
ok(byId["bfdecode"].innerHTML.includes("imm16"), "ISS imm16 decode");

__gotoIdx(110);
ok(byId["bfvalue"].innerHTML.includes("CNTP_CTL") && byId["bfvalue"].innerHTML.includes("0x0000000000000005"), "CNTP_CTL=0x5: " + byId["bfvalue"].innerHTML);
ok(byId["bfchg"].textContent.includes("bit2"), "ISTATUS flip: " + byId["bfchg"].textContent);
ok(byId["bfdecode"].innerHTML.includes("已断言"), "ISTATUS decode");

__gotoIdx(115);
ok(byId["console"].innerHTML.includes("GICC_IAR=0x01e"), "IRQ console line");
ok(byId["bfvalue"].innerHTML.includes("0x0000000000000000"), "CNTP_CTL=0 after handler");
ok(byId["bfchg"].textContent.includes("bit2(ISTATUS): 1→0"), "ISTATUS 1->0: " + byId["bfchg"].textContent);

__gotoIdx(121);
ok(byId["console"].innerHTML.includes("全部验证完成"), "final banner");
ok(byId["prog"].textContent.includes("122/122"), "final progress: " + byId["prog"].textContent);

const head = js.split("const $ = id=>")[0];
const ctx = new Function(head + "; return {STEPS:STEPS, freshState:freshState, applyStep:applyStep};")();

(function(){
  let st = ctx.freshState();
  for(let i=0;i<ctx.STEPS.length;i++) ctx.applyStep(st, ctx.STEPS[i]);
  ok(st.el===0 && st.mode==="EL0t", "final EL0t");
  ok(st.pc===0x4000220c, "final PC");
  ok(st.g_irq_ok===1, "g_irq_ok");
  const markers = [[0,3,"EL3h"],[15,2,"EL2h"],[23,1,"EL1h"],[31,0,"EL0t"],[36,1,"EL1h"],[44,0,"EL0t"],[55,2,"EL2h"],[62,3,"EL3h"],[70,2,"EL2h"],[74,0,"EL0t"],[78,1,"EL1h"],[87,0,"EL0t"],[91,1,"EL1h"],[100,0,"EL0t"],[112,1,"EL1h"],[117,0,"EL0t"]];
  for(const [i,el,mode] of markers){
    let s = ctx.freshState();
    for(let j=0;j<=i;j++) ctx.applyStep(s, ctx.STEPS[j]);
    ok(s.el===el && s.mode===mode, "after step "+i+" expect EL"+el+" "+mode+", got EL"+s.el+" "+s.mode);
  }
})();

if(fails===0){ console.log("PASS: visualizer self-check (" + idRefs.size + " ids, " + ctx.STEPS.length + " steps, DOM + bitfield + logic)"); process.exit(0); }
console.log(fails + " FAILURES");
process.exit(1);
