import {setTheme as setArtTheme} from "./art.js?v=glass-g-arc-1";
const app=document.getElementById("app");
const dark=document.getElementById("darkTheme");
const light=document.getElementById("lightTheme");
const stepContent=document.getElementById("stepContent");
const stepCounter=document.getElementById("stepCounter");
const progress=document.querySelector(".progress-bars");
const back=document.getElementById("backButton");
const next=document.getElementById("continueButton");
const iconPaths={
  lightning:'<path d="m13.7 2.8-9 10.3a.85.85 0 0 0 .65 1.4h5.2l-.8 6.7a.66.66 0 0 0 1.15.5l8.65-10.4a.86.86 0 0 0-.66-1.4h-5.1l1.1-6.6a.67.67 0 0 0-1.19-.5Z"/>',
  shield:'<path d="M12 2.5 20 6v5.3c0 5.3-3.3 8.2-8 10.2-4.7-2-8-4.9-8-10.2V6l8-3.5Zm0 3.2L6.8 8v3.3c0 3.5 2 5.9 5.2 7.4 3.2-1.5 5.2-3.9 5.2-7.4V8L12 5.7Z"/>',
  people:'<circle cx="12" cy="7" r="4"/><path d="M4 20v-2.2a6.3 6.3 0 0 1 6.3-6.3h3.4a6.3 6.3 0 0 1 6.3 6.3V20H4Zm15.1-12.6a3.2 3.2 0 1 1-.8 6.2 8.9 8.9 0 0 0-1.3-1.1 5.9 5.9 0 0 0 2.1-5.1Z"/>',
  check:'<path d="m4.4 12.2 4.7 4.8L20 6.7 18 4.8l-9 9-2.7-2.7-1.9 1.1Z"/>'
};
const steps=[
  {
    title:"Welcome to",brand:"GenyCaster",
    intro:"A fast, native and privacy-first client for the decentralized social web.",
    features:[
      ["electric","lightning","Fast & Native","Built for speed and a smooth experience on all your devices."],
      ["shield","shield","Privacy First","You control your data and identity."],
      ["community","people","Decentralized","A new home for the Farcaster community."]
    ]
  },
  {
    title:"Make it",brand:"yours.",
    intro:"A flexible experience that feels at home on every screen.",
    features:[
      ["electric","lightning","Beautiful in every theme","Choose a light or dark appearance that fits you."],
      ["shield","shield","Your preferences stay yours","Take control of the way GenyCaster looks and feels."],
      ["community","people","Made for every screen","A unified experience across desktop and mobile."]
    ]
  },
  {
    title:"Your identity,",brand:"your choice.",
    intro:"Bring your Farcaster identity with you. You remain in control.",
    features:[
      ["shield","shield","Recovery phrase first","Restore your existing identity through a secure local flow."],
      ["electric","lightning","Choose how to sign in","Supported identity providers are available in the real Setup."],
      ["community","people","Built on open protocols","Your social identity is yours, not the client's."]
    ]
  },
  {
    title:"You're",brand:"all set!",
    intro:"This is an interactive design preview. No account has been created or modified.",
    features:[
      ["community","check","Design walkthrough complete","You explored the page transitions and progress states."],
      ["electric","lightning","Explore both themes","Try the theme switch above to compare the two experiences."],
      ["shield","shield","Your real Setup is unchanged","This prototype doesn't access your recovery phrase or account."]
    ]
  }
];
let step=0;
function featureHtml([category,icon,title,description]){
  return `<article class="feature"><div class="feature-icon ${category}"><svg viewBox="0 0 24 24" aria-hidden="true">${iconPaths[icon]}</svg></div><div><h2>${title}</h2><p>${description}</p></div></article>`;
}
function render(){
  const s=steps[step];
  stepCounter.textContent=`Step ${step+1} of ${steps.length}`;
  progress.setAttribute("aria-label",`Setup preview: step ${step+1} of ${steps.length}`);
  [...progress.children].forEach((node,i)=>{
    node.classList.toggle("active",i===step);
    node.classList.toggle("completed",i<step);
  });
  stepContent.innerHTML=`<h1>${s.title} <span>${s.brand}</span></h1>
    <p class="intro">${s.intro}</p>
    <div class="feature-stack">${s.features.map(featureHtml).join("")}</div>`;
  stepContent.style.animation="none";
  void stepContent.offsetWidth;
  stepContent.style.animation="";
  app.dataset.step=String(step+1);
  back.hidden=step===0;
  next.innerHTML=step===steps.length-1?'Restart preview <span aria-hidden="true">↺</span>':'Continue <span aria-hidden="true">→</span>';
}
function applyTheme(value){
  app.classList.toggle("dark",value==="dark");
  app.classList.toggle("light",value==="light");
  dark.setAttribute("aria-pressed",String(value==="dark"));
  light.setAttribute("aria-pressed",String(value==="light"));
  document.documentElement.style.colorScheme=value;
  setArtTheme(value==="dark");
  const url=new URL(window.location.href);
  url.searchParams.set("theme",value);
  history.replaceState(null,"",url);
}
dark.addEventListener("click",()=>applyTheme("dark"));
light.addEventListener("click",()=>applyTheme("light"));
next.addEventListener("click",()=>{step=(step+1)%steps.length;render()});
back.addEventListener("click",()=>{step=Math.max(0,step-1);render()});
document.addEventListener("keydown",event=>{
  if(event.key==="Escape"&&step>0){step--;render()}
});
const query=new URLSearchParams(location.search);
document.body.classList.toggle("comparison",query.get("compare")==="1");
applyTheme(query.get("theme")==="light"?"light":"dark");
render();
