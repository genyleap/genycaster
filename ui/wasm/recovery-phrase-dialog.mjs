// Browser-only recovery UI. Never send phrase text through Qt, C++,
// localStorage, IndexedDB, analytics, console or network requests.
// Onboarding reference: Quorum Mobile FarcasterRecovery / ReimportSheet.
// This is explicitly invoked by the account owner's Enable Interactions
// action, not by an implicit sign-in side effect.
const strings={
 en:{title:'Import your Farcaster recovery phrase',subtitle:'Authorize Genycaster to publish with your Farcaster identity without opening a wallet extension.',label:'Farcaster recovery phrase (12 or 24 words)',placeholder:'Enter the recovery words in the correct order',privacy:'Your phrase is processed on this device, is never uploaded, and is not stored. Only a separately authorized Ed25519 signer remains. Use this only on a trusted device.',hint:'Use the recovery phrase for your Farcaster / Warpcast account — not your Genyleap or Quorum account.',cancel:'Cancel',import:'Import and authorize',invalid:'Enter exactly 12 or 24 recovery words.',close:'Close'},
 fa:{title:'بازیابی حساب فارکستر',subtitle:'بدون نیاز به بازکردن افزونهٔ کیف پول، مجوز انتشار را در جنی‌کستر فعال کنید.',label:'عبارت بازیابی فارکستر (۱۲ یا ۲۴ کلمه)',placeholder:'کلمات بازیابی را به ترتیب وارد کنید',privacy:'این عبارت فقط روی دستگاه شما پردازش می‌شود، نه ارسال می‌شود و نه ذخیره. فقط کلید امضای مستقل و مجاز باقی می‌ماند. فقط روی دستگاه مطمئن استفاده کنید.',hint:'عبارت بازیابی حساب Farcaster / Warpcast را وارد کنید؛ نه عبارت Genyleap یا Quorum.',cancel:'انصراف',import:'ورود و فعال‌سازی',invalid:'عبارت بازیابی باید دقیقاً ۱۲ یا ۲۴ کلمه باشد.',close:'بستن'},
 de:{title:'Farcaster-Wiederherstellungsphrase importieren',subtitle:'Genycaster ohne Wallet-Erweiterung zum Veröffentlichen autorisieren.',label:'Farcaster-Wiederherstellungsphrase (12 oder 24 Wörter)',placeholder:'Wörter in der richtigen Reihenfolge eingeben',privacy:'Die Phrase wird nur auf diesem Gerät verarbeitet, nicht hochgeladen und nicht gespeichert. Nur ein separat autorisierter Signer bleibt erhalten.',hint:'Verwende die Farcaster-/Warpcast-Phrase, nicht die Genyleap-/Quorum-Phrase.',cancel:'Abbrechen',import:'Importieren und autorisieren',invalid:'Gib genau 12 oder 24 Wörter ein.',close:'Schließen'},
 ar:{title:'استيراد عبارة استرداد Farcaster',subtitle:'السماح بالنشر من Genycaster دون فتح إضافة محفظة.',label:'عبارة Farcaster المكوّنة من 12 أو 24 كلمة',placeholder:'أدخل الكلمات بالترتيب الصحيح',privacy:'تُعالج العبارة على جهازك فقط؛ لا تُرفع ولا تُحفظ. سيُحتفظ فقط بمفتاح توقيع مستقل مُصرّح به.',hint:'استخدم عبارة Farcaster / Warpcast وليس Genyleap أو Quorum.',cancel:'إلغاء',import:'استيراد وتفويض',invalid:'أدخل 12 أو 24 كلمة بالضبط.',close:'إغلاق'},
 fr:{title:'Importer la phrase de récupération Farcaster',subtitle:'Autoriser Genycaster à publier sans ouvrir de portefeuille externe.',label:'Phrase de récupération (12 ou 24 mots)',placeholder:'Saisir les mots dans le bon ordre',privacy:'Votre phrase est traitée uniquement sur cet appareil, jamais envoyée ni stockée. Seule une clé de signature autorisée est conservée.',hint:'Utilisez la phrase Farcaster / Warpcast, pas celle de Genyleap / Quorum.',cancel:'Annuler',import:'Importer et autoriser',invalid:'Saisissez 12 ou 24 mots exactement.',close:'Fermer'},
 es:{title:'Importar frase de recuperación de Farcaster',subtitle:'Autoriza a Genycaster a publicar sin abrir una extensión de cartera.',label:'Frase de recuperación de 12 o 24 palabras',placeholder:'Escribe las palabras en orden',privacy:'La frase solo se procesa en este dispositivo; no se envía ni se guarda. Solo se conserva un firmante autorizado.',hint:'Usa la frase de Farcaster / Warpcast, no la de Genyleap / Quorum.',cancel:'Cancelar',import:'Importar y autorizar',invalid:'Introduce exactamente 12 o 24 palabras.',close:'Cerrar'},
 pt:{title:'Importar frase de recuperação Farcaster',subtitle:'Autorize o Genycaster a publicar sem abrir uma extensão de carteira.',label:'Frase de 12 ou 24 palavras',placeholder:'Digite as palavras na ordem correta',privacy:'Sua frase é processada apenas neste dispositivo; não é enviada nem armazenada. Só permanece uma chave de assinatura autorizada.',hint:'Use a frase do Farcaster / Warpcast, não do Genyleap / Quorum.',cancel:'Cancelar',import:'Importar e autorizar',invalid:'Digite exatamente 12 ou 24 palavras.',close:'Fechar'},
 ru:{title:'Импорт фразы восстановления Farcaster',subtitle:'Разрешите публикацию из Genycaster без расширения кошелька.',label:'Фраза восстановления (12 или 24 слова)',placeholder:'Введите слова по порядку',privacy:'Фраза обрабатывается только на этом устройстве и не передается и не сохраняется. Остается лишь отдельный разрешенный ключ подписи.',hint:'Используйте фразу Farcaster / Warpcast, а не Genyleap / Quorum.',cancel:'Отмена',import:'Импорт и разрешение',invalid:'Введите ровно 12 или 24 слова.',close:'Закрыть'},
 ja:{title:'Farcaster のリカバリーフレーズをインポート',subtitle:'外部ウォレットを開かずに Genycaster で投稿を有効にします。',label:'リカバリーフレーズ（12 または 24 語）',placeholder:'正しい順序で入力',privacy:'フレーズはこの端末のみで処理され、送信も保存もされません。承認済みの署名鍵だけを保持します。',hint:'Genyleap / Quorum ではなく Farcaster / Warpcast のフレーズを使用してください。',cancel:'キャンセル',import:'インポートして承認',invalid:'12 または 24 語を入力してください。',close:'閉じる'},
 ko:{title:'Farcaster 복구 구문 가져오기',subtitle:'외부 지갑을 열지 않고 Genycaster 게시 권한을 설정합니다.',label:'복구 구문 (12 또는 24개 단어)',placeholder:'단어를 순서대로 입력',privacy:'구문은 이 기기에서만 처리하며 전송하거나 저장하지 않습니다. 승인된 서명 키만 남습니다.',hint:'Genyleap / Quorum이 아닌 Farcaster / Warpcast의 복구 구문을 사용하세요.',cancel:'취소',import:'가져와서 승인',invalid:'12개 또는 24개의 단어를 입력하세요.',close:'닫기'},
 zh:{title:'导入 Farcaster 恢复短语',subtitle:'无需打开外部钱包扩展，即可授权 Genycaster 发布内容。',label:'恢复短语（12 或 24 个单词）',placeholder:'按正确顺序输入单词',privacy:'短语只在本设备处理，不上传也不保存。仅保留独立授权的签名密钥。',hint:'请使用 Farcaster / Warpcast 恢复短语，而非 Genyleap / Quorum。',cancel:'取消',import:'导入并授权',invalid:'请准确输入 12 或 24 个单词。',close:'关闭'},
 hi:{title:'Farcaster रिकवरी वाक्यांश आयात करें',subtitle:'बाहरी वॉलेट खोले बिना Genycaster में प्रकाशन अधिकृत करें।',label:'रिकवरी वाक्यांश (12 या 24 शब्द)',placeholder:'शब्द सही क्रम में दर्ज करें',privacy:'यह वाक्यांश केवल इसी डिवाइस पर संसाधित होता है; न अपलोड होता है, न संग्रहीत। केवल अधिकृत साइनर कुंजी रहती है।',hint:'Genyleap / Quorum नहीं, Farcaster / Warpcast वाला वाक्यांश इस्तेमाल करें।',cancel:'रद्द करें',import:'आयात और अधिकृत करें',invalid:'ठीक 12 या 24 शब्द दर्ज करें।',close:'बंद करें'},
};

export function promptForFarcasterRecovery(browser=globalThis,signal=null) {
    if(signal?.aborted) return Promise.reject(new Error('Recovery import cancelled'));
    const document=browser.document;
    if(!document?.body || browser.isSecureContext!==true)
        return Promise.reject(new Error('A secure HTTPS or loopback browser origin is required'));
    if(document.getElementById('genycaster-farcaster-recovery'))
        return Promise.reject(new Error('Recovery dialog is already open'));
    const locale=(browser.__genycasterPreferredLanguage
        ||browser.navigator?.language||'en').split(/[-_]/)[0].toLowerCase();
    const lang=strings[locale]?locale:'en',s=strings[lang];
    const light=browser.matchMedia?.('(prefers-color-scheme: light)').matches===true;
    const foreground=light?'#1b2234':'#f4f6ff';
    const muted=light?'#586276':'#b7bdd0';
    const surface=light?'#ffffff':'#171a23';
    const inputSurface=light?'#f4f6fa':'#0f1118';
    const outline=light?'#d9deed':'#333b53';
    return new Promise((resolve,reject)=>{
        const backdrop=document.createElement('div');
        backdrop.id='genycaster-farcaster-recovery';
        backdrop.style.cssText='position:fixed;inset:0;z-index:2147483647;display:flex;align-items:center;justify-content:center;padding:16px;box-sizing:border-box;background:rgba(2,6,18,.65);backdrop-filter:blur(5px);font:15px system-ui,sans-serif;color:'+foreground;
        const card=document.createElement('section');
        card.setAttribute('role','dialog');card.setAttribute('aria-modal','true');
        card.setAttribute('aria-labelledby','genycaster-recovery-title');
        card.dir=['ar','fa'].includes(lang)?'rtl':'ltr';
        card.style.cssText='width:min(520px,100%);max-height:92vh;overflow:auto;box-sizing:border-box;background:'+surface+';border:1px solid '+outline+';border-radius:20px;padding:26px;box-shadow:0 20px 90px #0009;color:'+foreground;
        const text=(tag,value,css,id)=>{
            const el=document.createElement(tag);el.textContent=value;
            if(css)el.style.cssText=css;
            if(id)el.id=id;
            card.appendChild(el);return el;
        };
        text('h2',s.title,'margin:0 0 9px;font-size:23px;font-weight:650','genycaster-recovery-title');
        text('p',s.subtitle,'margin:0 0 24px;color:'+muted+';line-height:1.5');
        const label=text('label',s.label,'display:block;margin-bottom:9px;font-weight:600');
        const input=document.createElement('textarea');
        input.rows=4;input.placeholder=s.placeholder;
        input.autocapitalize='off';input.autocomplete='off';
        input.spellcheck=false;input.setAttribute('data-lpignore','true');
        input.setAttribute('aria-label',s.label);
        input.style.cssText='box-sizing:border-box;width:100%;resize:vertical;border-radius:12px;border:1px solid #8691a8;background:'+inputSurface+';color:'+foreground+';padding:12px 14px;font:15px system-ui;line-height:1.6;outline-color:#4f70ff';
        label.setAttribute('for','genycaster-recovery-words');
        input.id='genycaster-recovery-words';
        card.appendChild(input);
        const hint=text('p',s.hint,'font-size:13px;color:#c9ad7f;margin:12px 0 10px;line-height:1.5');
        const warning=text('p',s.privacy,'font-size:13px;color:'+muted+';line-height:1.55;margin:0 0 20px');
        const error=text('p','','color:#ff8e90;min-height:1.3em;margin:0 0 10px;font-size:13px');
        error.setAttribute('aria-live','polite');
        const actions=document.createElement('div');
        actions.style.cssText='display:flex;gap:10px;justify-content:flex-end;flex-wrap:wrap';
        const button=(value,primary)=>{
            const el=document.createElement('button');el.type='button';el.textContent=value;
            el.style.cssText='cursor:pointer;border-radius:12px;padding:12px 20px;font:600 14px system-ui;border:1px solid '+(primary?'#4460f3':'#50586c')+';background:'+(primary?'#4266f8':'transparent')+';color:'+(primary?'#fff':foreground);
            actions.appendChild(el);return el;
        };
        const cancel=button(s.cancel,false),submit=button(s.import,true);
        card.appendChild(actions);backdrop.appendChild(card);
        const oldFocus=document.activeElement;
        const oldOverflow=document.body.style.overflow;
        document.body.style.overflow='hidden';
        document.body.appendChild(backdrop);
        const cleanup=()=>{
            input.value='';input.placeholder='';
            signal?.removeEventListener('abort',dismiss);
            backdrop.removeEventListener('keydown',keydown);
            backdrop.remove();document.body.style.overflow=oldOverflow;
            oldFocus?.focus?.();
        };
        const dismiss=()=>{cleanup();const e=new Error('Recovery import cancelled');e.code='user-cancelled';reject(e);};
        const keydown=event=>{
            if(event.key==='Escape'){event.preventDefault();dismiss();}
            if(event.key==='Tab'){
                const targets=[input,cancel,submit],current=targets.indexOf(document.activeElement);
                if(current>=0){event.preventDefault();
                    targets[(current+(event.shiftKey?2:1))%3].focus();}
            }
        };
        backdrop.addEventListener('keydown',keydown);
        signal?.addEventListener('abort',dismiss,{once:true});
        cancel.addEventListener('click',dismiss,{once:true});
        backdrop.addEventListener('click',event=>{if(event.target===backdrop)dismiss();});
        submit.addEventListener('click',()=>{
            const words=input.value.trim().split(/\s+/u).filter(Boolean);
            if(words.length!==12&&words.length!==24){error.textContent=s.invalid;return;}
            // Never copy the phrase into Qt state or send it to any API.
            const phrase=input.value;
            cleanup();
            resolve(phrase);
        },{once:false});
        input.focus();
    });
}
