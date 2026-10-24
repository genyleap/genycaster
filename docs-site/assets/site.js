"use strict";
const groups=[
 ["Start", [["Overview","/"],["Balaji's $2048 challenge","/quickstart/"],["Current status","/status/"]]],
 ["Architecture",[["Architecture map","/architecture/"],["Provider and node routing","/providers/"],["Switching clients","/client-switching/"]]],
 ["Developers",[["Build your own client","/build-your-own/"],["Operations and node setup","/operations/"]]]
];
const pages=groups.flatMap(g=>g[1]);
const pagePath=location.pathname.replace(/index\.html$/,"").replace(/\/?$/,"/");
function navLink([label,url]){const a=document.createElement("a");a.href=url;a.textContent=label;if(url===pagePath){a.classList.add("active");a.setAttribute("aria-current","page")}return a}
const side=document.querySelector(".side");
if(side){for(const [group,links] of groups){const h=document.createElement("h3");h.textContent=group;side.append(h);for(const link of links)side.append(navLink(link));}}
document.querySelectorAll("[data-toc]").forEach(area=>{const headings=[...document.querySelectorAll(".main h2[id],.main h3[id]")];for(const h of headings){const a=document.createElement("a");a.href="#"+h.id;a.textContent=h.textContent;area.append(a)}});
document.querySelectorAll(".copy").forEach(button=>button.addEventListener("click",async()=>{const code=button.closest(".terminal")?.querySelector("code");if(!code)return;try{await navigator.clipboard.writeText(code.textContent);button.textContent="Copied";setTimeout(()=>button.textContent="Copy",1400)}catch{button.textContent="Select code"}}));
const overlay=document.querySelector(".search");
const field=overlay?.querySelector("input");
const results=overlay?.querySelector(".search-results");
function showSearch(){if(!overlay)return;overlay.hidden=false;field.focus();field.dispatchEvent(new Event("input"))}
function hideSearch(){if(!overlay)return;overlay.hidden=true;field.value=""}
document.querySelectorAll("[data-open-search]").forEach(b=>b.addEventListener("click",showSearch));
document.querySelectorAll("[data-close-search]").forEach(b=>b.addEventListener("click",hideSearch));
overlay?.addEventListener("click",e=>{if(e.target===overlay)hideSearch()});
field?.addEventListener("input",()=>{const q=field.value.trim().toLowerCase();results.replaceChildren();const hits=pages.filter(p=>!q||p[0].toLowerCase().includes(q)||p[1].toLowerCase().includes(q));for(const item of hits){const a=document.createElement("a");a.href=item[1];a.textContent=item[0];const small=document.createElement("span");small.textContent=item[1];a.append(small);results.append(a)}}); 
document.addEventListener("keydown",e=>{if((e.metaKey||e.ctrlKey)&&e.key.toLowerCase()==="k"){e.preventDefault();showSearch()}if(e.key==="Escape")hideSearch();if(e.key==="/"&&!(e.target instanceof HTMLInputElement)){e.preventDefault();showSearch()}});
document.querySelectorAll("[data-mobile-menu]").forEach(b=>b.addEventListener("click",()=>{const open=side.classList.toggle("open");b.setAttribute("aria-expanded",open?"true":"false")}));
document.querySelectorAll(".side a").forEach(a=>a.addEventListener("click",()=>side.classList.remove("open")));
