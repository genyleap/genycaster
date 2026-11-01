import { wordlist as bip39Words } from './vendor/bip39-english.mjs';
const knownWords=new Set(bip39Words);
// Browser-only recovery UI. Never send phrase text through Qt, C++,
// localStorage, IndexedDB, analytics, console or network requests.
// Onboarding reference: Quorum Mobile FarcasterRecovery / ReimportSheet.
// Invoked only after an explicit recovery / enable-publishing action.
const strings={
 en:{title:'Import your Farcaster recovery phrase',subtitle:'Recover your Farcaster account and securely establish the same central OpenProof identity, with no extra registration.',label:'Farcaster recovery phrase (24 words)',placeholder:'Enter the recovery words in the correct order',privacy:'Your recovery phrase never leaves this device and is not stored. A fresh signed identity challenge—not your phrase—is sent to OpenProof to resolve or create your shared account. Only a separately authorized Ed25519 signer remains. Use a trusted device.',hint:'Use the recovery phrase for your Farcaster / Warpcast account — not your Genyleap or Quorum account.',cancel:'Cancel',import:'Import and authorize',invalid:'Enter exactly 24 recovery words.',close:'Close'},
 fa:{title:'بازیابی حساب فارکستر',subtitle:'حساب فارکستر را بازیابی کنید؛ جنی‌کستر با امضای امن، همان حساب مرکزی OpenProof را هم بدون ثبت‌نام جداگانه پیدا یا ایجاد می‌کند.',label:'عبارت بازیابی فارکستر (۲۴ واژه)',placeholder:'کلمات بازیابی را به ترتیب وارد کنید',privacy:'عبارت بازیابی هرگز ارسال یا ذخیره نمی‌شود. فقط یک چالش تازه با کلید محلی امضا می‌شود تا OpenProof حساب مرکزی را شناسایی یا ایجاد کند. کلید انتشار مستقل باقی می‌ماند. فقط روی دستگاه مطمئن ادامه دهید.',hint:'عبارت بازیابی حساب فارکستر را وارد کنید؛ نه عبارت حساب جنی‌لیپ یا کیف پول دیگری.',cancel:'انصراف',import:'ورود و فعال‌سازی',invalid:'عبارت بازیابی باید دقیقاً ۲۴ واژه باشد.',close:'بستن'},
 de:{title:'Farcaster-Wiederherstellungsphrase importieren',subtitle:'Genycaster ohne Wallet-Erweiterung zum Veröffentlichen autorisieren.',label:'Farcaster-Wiederherstellungsphrase (24 Wörter)',placeholder:'Wörter in der richtigen Reihenfolge eingeben',privacy:'Die Phrase wird nur auf diesem Gerät verarbeitet, nicht hochgeladen und nicht gespeichert. Nur ein separat autorisierter Signer bleibt erhalten.',hint:'Verwende die Farcaster-/Warpcast-Phrase, nicht die Genyleap-/Quorum-Phrase.',cancel:'Abbrechen',import:'Importieren und autorisieren',invalid:'Gib genau 24 Wörter ein.',close:'Schließen'},
 ar:{title:'استيراد عبارة استرداد Farcaster',subtitle:'السماح بالنشر من Genycaster دون فتح إضافة محفظة.',label:'عبارة Farcaster المكوّنة من 24 كلمة',placeholder:'أدخل الكلمات بالترتيب الصحيح',privacy:'تُعالج العبارة على جهازك فقط؛ لا تُرفع ولا تُحفظ. سيُحتفظ فقط بمفتاح توقيع مستقل مُصرّح به.',hint:'استخدم عبارة Farcaster / Warpcast وليس Genyleap أو Quorum.',cancel:'إلغاء',import:'استيراد وتفويض',invalid:'أدخل 24 كلمة بالضبط.',close:'إغلاق'},
 fr:{title:'Importer la phrase de récupération Farcaster',subtitle:'Autoriser Genycaster à publier sans ouvrir de portefeuille externe.',label:'Phrase de récupération (24 mots)',placeholder:'Saisir les mots dans le bon ordre',privacy:'Votre phrase est traitée uniquement sur cet appareil, jamais envoyée ni stockée. Seule une clé de signature autorisée est conservée.',hint:'Utilisez la phrase Farcaster / Warpcast, pas celle de Genyleap / Quorum.',cancel:'Annuler',import:'Importer et autoriser',invalid:'Saisissez exactement 24 mots.',close:'Fermer'},
 es:{title:'Importar frase de recuperación de Farcaster',subtitle:'Autoriza a Genycaster a publicar sin abrir una extensión de cartera.',label:'Frase de recuperación de 24 palabras',placeholder:'Escribe las palabras en orden',privacy:'La frase solo se procesa en este dispositivo; no se envía ni se guarda. Solo se conserva un firmante autorizado.',hint:'Usa la frase de Farcaster / Warpcast, no la de Genyleap / Quorum.',cancel:'Cancelar',import:'Importar y autorizar',invalid:'Introduce exactamente 24 palabras.',close:'Cerrar'},
 pt:{title:'Importar frase de recuperação Farcaster',subtitle:'Autorize o Genycaster a publicar sem abrir uma extensão de carteira.',label:'Frase de 24 palavras',placeholder:'Digite as palavras na ordem correta',privacy:'Sua frase é processada apenas neste dispositivo; não é enviada nem armazenada. Só permanece uma chave de assinatura autorizada.',hint:'Use a frase do Farcaster / Warpcast, não do Genyleap / Quorum.',cancel:'Cancelar',import:'Importar e autorizar',invalid:'Digite exatamente 24 palavras.',close:'Fechar'},
 ru:{title:'Импорт фразы восстановления Farcaster',subtitle:'Разрешите публикацию из Genycaster без расширения кошелька.',label:'Фраза восстановления (24 слова)',placeholder:'Введите слова по порядку',privacy:'Фраза обрабатывается только на этом устройстве и не передается и не сохраняется. Остается лишь отдельный разрешенный ключ подписи.',hint:'Используйте фразу Farcaster / Warpcast, а не Genyleap / Quorum.',cancel:'Отмена',import:'Импорт и разрешение',invalid:'Введите ровно 24 слова.',close:'Закрыть'},
 ja:{title:'Farcaster のリカバリーフレーズをインポート',subtitle:'外部ウォレットを開かずに Genycaster で投稿を有効にします。',label:'リカバリーフレーズ（24 語）',placeholder:'正しい順序で入力',privacy:'フレーズはこの端末のみで処理され、送信も保存もされません。承認済みの署名鍵だけを保持します。',hint:'Genyleap / Quorum ではなく Farcaster / Warpcast のフレーズを使用してください。',cancel:'キャンセル',import:'インポートして承認',invalid:'24 語を入力してください。',close:'閉じる'},
 ko:{title:'Farcaster 복구 구문 가져오기',subtitle:'외부 지갑을 열지 않고 Genycaster 게시 권한을 설정합니다.',label:'복구 구문 (24개 단어)',placeholder:'단어를 순서대로 입력',privacy:'구문은 이 기기에서만 처리하며 전송하거나 저장하지 않습니다. 승인된 서명 키만 남습니다.',hint:'Genyleap / Quorum이 아닌 Farcaster / Warpcast의 복구 구문을 사용하세요.',cancel:'취소',import:'가져와서 승인',invalid:'24개의 단어를 입력하세요.',close:'닫기'},
 zh:{title:'导入 Farcaster 恢复短语',subtitle:'无需打开外部钱包扩展，即可授权 Genycaster 发布内容。',label:'恢复短语（24 个单词）',placeholder:'按正确顺序输入单词',privacy:'短语只在本设备处理，不上传也不保存。仅保留独立授权的签名密钥。',hint:'请使用 Farcaster / Warpcast 恢复短语，而非 Genyleap / Quorum。',cancel:'取消',import:'导入并授权',invalid:'请准确输入 24 个单词。',close:'关闭'},
 hi:{title:'Farcaster रिकवरी वाक्यांश आयात करें',subtitle:'बाहरी वॉलेट खोले बिना Genycaster में प्रकाशन अधिकृत करें।',label:'रिकवरी वाक्यांश (24 शब्द)',placeholder:'शब्द सही क्रम में दर्ज करें',privacy:'यह वाक्यांश केवल इसी डिवाइस पर संसाधित होता है; न अपलोड होता है, न संग्रहीत। केवल अधिकृत साइनर कुंजी रहती है।',hint:'Genyleap / Quorum नहीं, Farcaster / Warpcast वाला वाक्यांश इस्तेमाल करें।',cancel:'रद्द करें',import:'आयात और अधिकृत करें',invalid:'ठीक 24 शब्द दर्ज करें。',close:'बंद करें'},
};

const revealLabels={
 en:'Show recovery words',fa:'نمایش کلمات بازیابی',ar:'إظهار كلمات الاسترداد',
 de:'Wiederherstellungswörter anzeigen',fr:'Afficher les mots de récupération',
 es:'Mostrar palabras de recuperación',pt:'Mostrar palavras de recuperação',
 ru:'Показать слова восстановления',ja:'復元フレーズを表示',
 ko:'복구 단어 표시',zh:'显示恢复词',hi:'रिकवरी शब्द दिखाएँ',
};

export function promptForFarcasterRecovery(browser=globalThis,signal=null,onAuthorize=null,options={}) {
    if(signal?.aborted) return Promise.reject(new Error('Recovery import cancelled'));
    const document=browser.document;
    if(!document?.body || browser.isSecureContext!==true)
        return Promise.reject(new Error('A secure HTTPS or loopback browser origin is required'));
    if(document.getElementById('genycaster-farcaster-recovery'))
        return Promise.reject(new Error('Recovery dialog is already open'));
    const locale=(browser.__genycasterPreferredLanguage
        ||browser.navigator?.language||'en').split(/[-_]/)[0].toLowerCase();
    const lang=strings[locale]?locale:'en',s=strings[lang];
    const light=browser.document?.documentElement?.dataset?.theme === 'light'
        || (browser.document?.documentElement?.dataset?.theme !== 'dark'
            && browser.matchMedia?.('(prefers-color-scheme: light)').matches===true);
    const foreground=light?'#17181c':'#f4f6ff';
    const muted=light?'#73757e':'#aaafbc';
    const surface=light?'#ffffff':'#101116';
    const inputSurface=light?'#f8f8fa':'#1a1c23';
    const outline=light?'#e5e5eb':'#343840';
    return new Promise((resolve,reject)=>{
        const backdrop=document.createElement('div');
        backdrop.id='genycaster-farcaster-recovery';
        // A secure DOM layer occupies the existing Setup content column.
        // It never transports secret words to Qt/QML/WASM or covers the GPU rail.
        backdrop.style.cssText='position:fixed;inset:0;z-index:2147483647;display:block;box-sizing:border-box;background:transparent;font:15px system-ui,sans-serif;color:'+foreground;
        const card=document.createElement('section');
        const accountMode=options?.presentation==='full';
        card.setAttribute('role',accountMode?'dialog':'region');
        if(accountMode)card.setAttribute('aria-modal','true');
        card.setAttribute('aria-labelledby','genycaster-recovery-title');
        card.dir=['ar','fa'].includes(lang)?'rtl':'ltr';
        card.style.cssText='position:absolute;inset:0;width:100%;height:100%;min-height:0;display:flex;flex-direction:column;overflow:hidden;overscroll-behavior:contain;box-sizing:border-box;background:'+surface+';border:none;border-radius:0;padding:clamp(80px,11vh,128px) clamp(24px,5vw,60px) max(28px,env(safe-area-inset-bottom));box-shadow:none;color:'+foreground+';';
        // The desktop right pane starts at the same 29% (max 430px) rail
        // boundary as InitialSetup.qml; intermediate and phone sizes fill
        // the screen because the QML brand rail is hidden in those layouts.
        const style=document.createElement('style');
        if(accountMode)backdrop.dataset.presentation='full';
        style.textContent=
            '@media(min-width:980px){#genycaster-farcaster-recovery>section{'
            +'left:'+ (accountMode?'0':'min(29vw,430px)') +'!important;right:0!important;width:auto!important;'
            +'padding-top:clamp(94px,12vh,138px)!important}}'
            +'#genycaster-farcaster-recovery input:focus-visible{outline:2px solid '
            +(light?'#626c7c':'#b3c0d3')+';outline-offset:2px}'
            +'@media(max-width:680px){#genycaster-recovery-words{grid-template-columns:repeat(2,minmax(0,1fr))!important}}'
            +'#genycaster-recovery-reveal:focus-visible{outline:2px solid '
            +(light?'#626c7c':'#b3c0d3')+'!important;outline-offset:3px!important}'
            +'#genycaster-recovery-reveal{accent-color:'
            +(light?'#24272d':'#e6edf5')+';box-shadow:none!important;-webkit-appearance:none!important;appearance:none!important}'
            +'#genycaster-farcaster-recovery button:focus-visible{outline:2px solid '
            +(light?'#626c7c':'#b3c0d3')+';outline-offset:2px}'
            +'@media(max-width:680px){#genycaster-farcaster-recovery>section{'
            +'padding:max(24px,env(safe-area-inset-top)) 20px max(18px,env(safe-area-inset-bottom))!important}}'
            +'@media(prefers-reduced-motion:reduce){#genycaster-farcaster-recovery *{scroll-behavior:auto!important}}'
            +'@keyframes genycaster-recovery-spin{to{transform:rotate(360deg)}}'
            +'#genycaster-farcaster-recovery button[aria-busy="true"]::before{content:"";'
            +'display:inline-block;width:14px;height:14px;vertical-align:-2px;'
            +'margin-inline-end:9px;border:2px solid currentColor;'
            +'border-top-color:transparent;border-radius:50%;'
            +'animation:genycaster-recovery-spin .8s linear infinite}';
        // Scroll only the recovery words. Keep the action row outside this
        // scroller so 24 entries never displace or overlap Cancel / Import.
        const body=document.createElement('div');
        body.style.cssText='flex:1 1 auto;min-height:0;overflow-y:auto;overscroll-behavior:contain;scrollbar-width:thin;padding:0 1px 12px;width:100%;max-width:650px;margin:0 auto;box-sizing:border-box;';
        card.appendChild(body);
        const text=(tag,value,css,id)=>{
            const el=document.createElement(tag);el.textContent=value;
            if(css)el.style.cssText=css;
            if(id)el.id=id;
            body.appendChild(el);return el;
        };
        text('h2',s.title,'margin:0 0 12px;font-size:clamp(23px,2.4vw,30px);font-weight:700','genycaster-recovery-title');
        text('p',s.subtitle,'margin:0 0 24px;color:'+muted+';line-height:1.5');
        text('p',s.label,'margin:0 0 12px;font-weight:600');
        // One fixed 24-word form on desktop, phone and WASM.
        const grid=document.createElement('div');
        grid.id='genycaster-recovery-words';
        grid.style.cssText='display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:9px;direction:ltr';
        const inputs=[];
        let count=24;
        let pending=false, settled=false;
         const cellList=[];
        const badStyle='border:2px solid #e45966;background:rgba(227,67,80,.09)';
        const plainStyle='border:1px solid '+outline+';background:'+inputSurface;
        const mark=(index,bad)=>{
            const cell=cellList[index];if(!cell)return;
            cell.style.border=bad?'2px solid #e45966':'1px solid '+outline;
            cell.style.background=bad?'rgba(227,67,80,.09)':inputSurface;
            inputs[index]?.setAttribute('aria-invalid',bad?'true':'false');
        };
        const clearErrors=()=>{for(let i=0;i<count;i++)mark(i,false);error.textContent='';};
        const showError=(message)=>{error.textContent=message;error.setAttribute('role','alert');};

        const createInput=(index)=>{
            const cell=document.createElement('label');
            cell.style.cssText='display:flex;align-items:center;gap:7px;min-width:0;background:'+inputSurface+';border:1px solid '+outline+';border-radius:11px;padding:7px 10px';
            const number=document.createElement('span');
            number.textContent=String(index+1).padStart(2,'0');
            number.style.cssText='color:'+muted+';font:600 12px ui-monospace,monospace;flex-shrink:0';
            const word=document.createElement('input');
            word.type='password';word.autocomplete='off';word.autocapitalize='off';
            word.spellcheck=false;word.setAttribute('data-lpignore','true');
            word.setAttribute('aria-label',s.label+' '+(index+1));
            word.style.cssText='width:100%;min-width:0;background:transparent;border:0;outline:0;color:'+foreground+';font:16px system-ui;direction:ltr;touch-action:manipulation;-webkit-text-size-adjust:100%';
            cell.appendChild(number);cell.appendChild(word);
            inputs.push(word);cellList.push(cell);
            const pasteWords=(raw)=>{
                const words=raw.trim().toLowerCase().split(/\s+/u).filter(Boolean);
                const entirePhrase=words.length===24;
                if(entirePhrase)renderWords(words.length);
                const offset=entirePhrase ? 0 : index;
                for(let i=0;i<words.length&&offset+i<count;i++)
                    inputs[offset+i].value=words[i].replace(/[^a-z]/gu,'');
                // Pasting a complete phrase should dismiss the software keyboard,
                // not jump focus through narrow fields (and zoom mobile Safari).
                if(entirePhrase)submit.focus();
                else inputs[Math.min(count-1,offset+words.length)]?.focus();
                clearErrors();
            };
            word.addEventListener('paste',event=>{
                const pasted=event.clipboardData?.getData('text');
                if(typeof pasted!=='string')return;
                event.preventDefault();
                pasteWords(pasted);
            });
            word.addEventListener('input',()=>{
                const value=word.value.toLowerCase();
                if(/\s/u.test(value)){pasteWords(value);return;}
                word.value=value.replace(/[^a-z]/gu,'');
                mark(index,false);
                error.textContent='';
            });
            word.addEventListener('blur',()=>{
                const value=word.value.trim().toLowerCase();
                mark(index,value.length>0&&!knownWords.has(value));
            });
            word.addEventListener('keydown',event=>{
                if((event.key===' '||event.key==='Enter')&&index<count-1){
                    event.preventDefault();inputs[index+1]?.focus();
                }else if(event.key==='Backspace'&&!word.value&&index>0){
                    inputs[index-1]?.focus();
                }
            });
            return cell;
        };
        const renderWords=(number)=>{
            const snapshot=inputs.map(word=>word.value);
            for(const word of inputs)word.value='';
            inputs.length=0;cellList.length=0;
            while(grid.firstChild)grid.removeChild(grid.firstChild);
            count=number;
            for(let i=0;i<number;i++){
                const cell=createInput(i);grid.appendChild(cell);
                inputs[i].value=snapshot[i]||'';
            }
            snapshot.fill('');
         };
        body.appendChild(grid);
        const revealRow=document.createElement('label');
        revealRow.style.cssText='display:flex;align-items:center;gap:10px;margin:15px 0 10px;cursor:pointer;color:'+foreground+';font:500 14px system-ui';
        const reveal=document.createElement('input');
        reveal.type='checkbox';reveal.checked=false;
        reveal.id='genycaster-recovery-reveal';
        reveal.style.cssText='appearance:none;-webkit-appearance:none;outline:none;width:22px;height:22px;border:1px solid '+outline+';border-radius:6px;background:'+surface+';cursor:pointer;flex:none;margin:0;box-shadow:none';
        const indicator=document.createElement('span');
        indicator.textContent='✓';
        indicator.setAttribute('aria-hidden','true');
        indicator.style.cssText='position:absolute;opacity:0;color:'+surface+';font-size:15px;pointer-events:none';
        const caption=document.createElement('span');
        caption.textContent=revealLabels[lang]||revealLabels.en;
        const updateReveal=()=>{
            for(const field of inputs)field.type=reveal.checked?'text':'password';
            reveal.style.background=reveal.checked?foreground:surface;
            reveal.style.borderColor=reveal.checked?foreground:outline;
            indicator.style.opacity=reveal.checked?'1':'0';
        };
        reveal.addEventListener('change',updateReveal);
        const checkBox=document.createElement('span');
        checkBox.style.cssText='position:relative;display:inline-flex;align-items:center;justify-content:center';
        checkBox.appendChild(reveal);checkBox.appendChild(indicator);
        revealRow.appendChild(checkBox);revealRow.appendChild(caption);
        body.appendChild(revealRow);
        const hint=text('p',s.hint,'font-size:13px;color:'+muted+';margin:12px 0 10px;line-height:1.5');
        const warning=text('p',s.privacy,'font-size:13px;color:'+muted+';line-height:1.55;margin:0 0 20px');
        const error=text('p','','color:#ff8e90;min-height:1.3em;margin:0 0 10px;font-size:13px');
        error.setAttribute('aria-live','polite');
        renderWords(24);
        const actions=document.createElement('div');
        actions.style.cssText='display:flex;flex:0 0 auto;gap:10px;justify-content:flex-end;flex-wrap:wrap;min-width:0;width:100%;max-width:650px;box-sizing:border-box;margin:0 auto;background:'+surface+';padding:14px 0 2px;border-top:1px solid '+outline+';z-index:2';
        const button=(value,primary)=>{
            const el=document.createElement('button');el.type='button';el.textContent=value;
            el.style.cssText='cursor:pointer;flex:1 1 auto;min-width:0;max-width:100%;min-height:50px;white-space:normal;overflow-wrap:anywhere;border-radius:13px;padding:13px 16px;font:600 14px system-ui;border:1px solid '+(primary?(light?'#17181c':'#f4f6ff'):outline)+';background:'+(primary?(light?'#17181c':'#f4f6ff'):'transparent')+';color:'+(primary?(light?'#ffffff':'#17181c'):foreground);
            actions.appendChild(el);return el;
        };
        const cancel=button(s.cancel,false),submit=button(s.import,true);
        cancel.style.flex='0 1 auto';
        submit.style.flex='1 1 165px';
        card.appendChild(actions);backdrop.appendChild(card);backdrop.appendChild(style);
        const oldFocus=document.activeElement;
        const oldOverflow=document.body.style.overflow;
        document.body.style.overflow='hidden';
        document.body.appendChild(backdrop);
        const cleanup=()=>{
            if(settled)return;
            settled=true;
            for(const word of inputs){word.value='';word.placeholder='';}
            reveal.checked=false;
            signal?.removeEventListener('abort',dismiss);
            backdrop.removeEventListener('keydown',keydown);
            backdrop.remove();document.body.style.overflow=oldOverflow;
            oldFocus?.focus?.();
        };
        const dismiss=()=>{if(settled)return;cleanup();const e=new Error('Recovery import cancelled');e.code='user-cancelled';reject(e);};
        const keydown=event=>{
            if(event.key==='Escape'){event.preventDefault();dismiss();}
            if(event.key==='Tab'){
                const targets=[reveal,...inputs,cancel,submit],current=targets.indexOf(document.activeElement);
                if(current>=0){event.preventDefault();
                    targets[(current+(event.shiftKey?targets.length-1:1))%targets.length].focus();}
            }
        };
        backdrop.addEventListener('keydown',keydown);
        signal?.addEventListener('abort',dismiss,{once:true});
        cancel.addEventListener('click',dismiss,{once:true});
        backdrop.addEventListener('click',event=>{if(event.target===backdrop)dismiss();});
        submit.addEventListener('click',async()=>{
            if(pending||settled)return;
            clearErrors();
            const words=inputs.map(word=>word.value.trim().toLowerCase());
            let invalid=false,first=-1;
            for(let i=0;i<words.length;i++){
                const bad=!knownWords.has(words[i]);
                mark(i,bad);
                if(bad){invalid=true;if(first<0)first=i;}
            }
            if(invalid){
                showError(lang==='fa'
                    ? 'واژه‌های قرمز معتبر نیستند یا خالی مانده‌اند. آن‌ها را اصلاح کنید.'
                    : 'Check the highlighted words: they are missing or not in the recovery word list.');
                inputs[first]?.focus();words.fill('');return;
            }
            const phrase=words.join(' ');
            words.fill('');
            pending=true;submit.disabled=true;
            submit.setAttribute('aria-busy','true');
            submit.style.opacity='.75';
            submit.textContent=lang==='fa'?'در حال بررسی امن…':'Verifying securely…';
            try{
                const {validateRecoveryWords}=await import('./farcaster_recovery.mjs');
                await validateRecoveryWords(phrase);
                if(settled||signal?.aborted)return;
                const result=onAuthorize?await onAuthorize(phrase):phrase;
                if(settled||signal?.aborted)return;
                cleanup();resolve(result);
            }catch(cause){
                if(settled||signal?.aborted)return;
                // A BIP39 checksum does not identify the specific incorrect
                // word; avoid falsely marking the last word as the culprit.
                const message=String(cause?.message||cause);
                const checksum=/checksum/i.test(message);
                const category=cause?.code;
                const ownership=category==='recovery-unregistered'
                    ||category==='recovery-ownership'
                    ||/custody|does not control|does not own|registered Farcaster FID/i.test(message);
                const network=category==='recovery-network'
                    ||/HTTP|RPC|Snapchain|timeout|fetch|network|CORS|AbortError/i.test(message);
                const signer=category==='recovery-signer';
                const storage=category==='recovery-storage';
                const derivation=category==='recovery-derivation';
                const verification=category==='recovery-verification';
                showError(checksum
                    ? (lang==='fa'?'ترتیب یا یکی از واژه‌ها اشتباه است؛ همه واژه‌ها معتبرند اما جمع کنترل عبارت درست نیست.':
                        'The words are valid individually, but the phrase checksum is wrong. Check their order and spelling.')
                    : ownership
                    ? (lang==='fa'?'عبارت از نظر کلمات معتبر است، اما مالکیت حساب ثبت‌شدهٔ Farcaster با آن تأیید نشد. مطمئن شوید عبارت بازیابی همان حساب را وارد کرده‌اید.':
                        'The words are valid, but they do not identify a registered Farcaster custody account. Check that this is your Farcaster recovery phrase.')
                    : storage
                    ? (lang==='fa'?'عبارت بازیابی معتبر است، اما ذخیرهٔ امن کلید امضا در مرورگر امکان‌پذیر نیست. فضای ذخیره‌سازی سایت (IndexedDB) را فعال کنید و در پنجرهٔ عادی مرورگر دوباره تلاش کنید.':
                        'Your recovery phrase is valid, but secure browser signer storage is unavailable. Enable site storage (IndexedDB) and retry in a normal browser window.')
                    : signer
                    ? (lang==='fa'?'حساب شناسایی شد، اما مجوز انتشار هنوز توسط Snapchain تأیید نشده است. پس از بررسی اتصال دوباره تلاش کنید.':
                        'The account was identified, but Snapchain has not confirmed publishing authorization. Check your connection and retry.')
                    : derivation
                    ? (lang==='fa'?'ساخت کیف پول بازیابی روی این دستگاه کامل نشد. از مرورگر به‌روز و اتصال امن استفاده کنید.':
                        'Could not derive the Farcaster custody wallet on this device. Try an up-to-date browser on HTTPS.')
                    : verification
                    ? (lang==='fa'?'تأیید مالکیت حساب روی شبکه کامل نشد. دوباره تلاش کنید؛ در صورت تکرار از اتصال دیگری استفاده کنید.':
                        'Farcaster account ownership could not be confirmed. Retry or use a different network connection.')
                    : network
                    ? (lang==='fa'?'بررسی حساب یا مجوز انتشار به‌دلیل خطای اتصال کامل نشد. واژه‌ها حفظ شده‌اند؛ دوباره تلاش کنید.':
                        'Account or signer verification could not reach the network. Your entries are still here; please retry.')
                    : (lang==='fa'?'واژه‌ها به‌صورت محلی معتبرند، اما فعال‌سازی حساب کامل نشد. ورودی‌ها حفظ شده‌اند؛ دوباره تلاش کنید.':
                        'The recovery words passed local validation, but account authorization did not complete. Your entries are preserved.'));
            }finally{
                pending=false;submit.disabled=false;submit.setAttribute('aria-busy','false');
                submit.style.opacity='1';submit.textContent=s.import;
            }
        });
        inputs[0]?.focus();
    });
}
