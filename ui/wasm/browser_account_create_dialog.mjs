/**
 * Private browser onboarding screen for a newly generated 24-word custody
 * wallet. The seed is held in a lexical JS scope and cleared from DOM on
 * exit, never crossing the Qt/WASM interface or HTTP boundary.
 */
import {generateNativePhrase} from './hypersnap_native_account.mjs';

const translations={
 en:{
  title:'Back up your new wallet',subtitle:'This recovery phrase controls your new identity on the selected protocol.',
  warning:'Write down all 24 words in order, offline. Never share them. Browser storage is not a safe backup.',
  challenge:'Verify your saved recovery phrase',complete:'I saved my recovery phrase in a safe place',
  consent:'I authorize the one-time registration of this wallet on the selected network.',
  create:'Create account',cancel:'Cancel',close:'Close',copy:'Copy words',copied:'Words copied — clear your clipboard afterward',
  failed:'The selected recovery words do not match. Confirm your backup.',
  busy:'Preparing your account…',progress:'Preparing your registration',
  stopping:'Stopping the proof before network submission…',
  submitting:'Sending your account request to HyperSnap…',
  checking:'Waiting for confirmation. Checking automatically.',
  working:'Creating your account',blockchainCaption:'HYPERSNAP BLOCKCHAIN',farcasterCaption:'FARCASTER NETWORK',
  progressNote:'Proof search is running. This percentage is the hash-search budget, not network confirmation.',
  pending:'Request status unknown. Do not retry. Return with your 24 words.',
  stepOne:'Backup',stepTwo:'Request',stepThree:'Confirmation',
  paidStepTwo:'Sponsored registration authorized',paidStepThree:'Farcaster confirms your account',
  chainPending:'Keep your 24 recovery words. Return anytime.',
  voucher:'Genycaster sponsorship code',username:'Choose a Farcaster username',
  voucherHelp:'A sponsorship code is provided by the Genycaster operator after verified payment or sponsorship. It is not a Neynar API key or wallet password. Codes cannot currently be purchased inside Genycaster.',
  voucherLink:'No code? Create an account on Farcaster',
  paidReserve:'Preparing sponsored registration…',paidSign:'Confirming wallet ownership…',
  paidSubmit:'Submitting Farcaster registration…',paidVerify:'Waiting for Farcaster blockchain confirmation…'
 },
 fa:{
  title:'پشتیبان‌گیری از کیف پول جدید',subtitle:'واژه‌های بازیابی، مالکیت هویت جدید شما در پروتکل انتخاب‌شده را تعیین می‌کنند.',
  warning:'هر ۲۴ واژه را به‌ترتیب و در محیط امن یادداشت کنید. آن‌ها را در اختیار هیچ‌کس قرار ندهید. ذخیره‌سازی مرورگر نسخهٔ پشتیبان امنی نیست.',
  challenge:'واژه‌های بازیابی ذخیره‌شده را تأیید کنید',complete:'عبارت بازیابی را در محل امن ذخیره کرده‌ام',
  consent:'ثبت یک‌بارهٔ این کیف پول در شبکهٔ انتخاب‌شده را تأیید می‌کنم.',
  create:'ساخت حساب',cancel:'انصراف',close:'بستن',copy:'کپی واژه‌ها',copied:'واژه‌ها کپی شدند؛ پس از استفاده کلیپ‌بورد را پاک کنید',
  failed:'واژه‌های انتخاب‌شده درست نیستند. نسخهٔ پشتیبان را بررسی کنید.',
  busy:'در حال آماده‌سازی حساب…',progress:'در حال آماده‌سازی ثبت‌نام',
  stopping:'در حال توقف محاسبه پیش از ارسال شبکه…',
  submitting:'در حال ارسال درخواست ساخت حساب به هایپراسنپ…',
  checking:'در انتظار تأیید شبکه؛ بررسی خودکار ادامه دارد.',
  working:'در حال ایجاد حساب',blockchainCaption:'بلاک‌چین هایپراسنپ',farcasterCaption:'شبکه فارکستر',
  progressNote:'درصد، میزان جستجوی اثبات را نشان می‌دهد، نه تأیید ثبت در شبکه.',
  pending:'وضعیت درخواست نامشخص است. دوباره ارسال نکنید؛ با همان ۲۴ واژه برگردید.',
  stepOne:'پشتیبان',stepTwo:'درخواست',stepThree:'تأیید شبکه',
  paidStepTwo:'ثبت‌نام حمایت‌شده تأیید شد',paidStepThree:'تأیید حساب در فارکستر',
  chainPending:'۲۴ واژه را نگه دارید؛ بعداً می‌توانید برگردید.',
  voucher:'کد حمایت مالی جنی‌کستر',username:'نام کاربری فارکستر',
  voucherHelp:'کد حمایت مالی را اپراتور جنی‌کستر پس از پرداخت یا حمایت تأییدشده صادر می‌کند. این کد، کلید API نینار یا رمز کیف‌پول نیست و فعلاً در خود جنی‌کستر فروخته نمی‌شود.',
  voucherLink:'کد ندارید؟ در فارکستر حساب بسازید',
  paidReserve:'در حال آماده‌سازی ثبت‌نام حمایت‌شده…',paidSign:'در حال تأیید مالکیت کیف‌پول…',
  paidSubmit:'در حال ارسال ثبت‌نام فارکستر…',paidVerify:'در انتظار تأیید ثبت‌نام در بلاک‌چین فارکستر…'
 },
 ar:{
  title:"احفظ نسخة احتياطية من محفظتك الجديدة",
  subtitle:"تتحكم عبارة الاسترداد هذه في هويتك الجديدة على البروتوكول المحدد.",
  warning:"اكتب الكلمات الأربع والعشرين بالترتيب دون اتصال بالإنترنت. لا تشاركها أبدًا. تخزين المتصفح ليس نسخة احتياطية آمنة.",
  challenge:"تحقق من عبارة الاسترداد المحفوظة",
  complete:"حفظت عبارة الاسترداد في مكان آمن.",
  consent:"أوافق على تسجيل هذه المحفظة مرة واحدة على الشبكة المحددة.",
  create:"إنشاء حساب",
  cancel:"إلغاء",
  close:"إغلاق",
  copy:"نسخ الكلمات",
  copied:"نُسخت الكلمات؛ امسح الحافظة بعد الاستخدام.",
  failed:"كلمات الاسترداد المختارة غير متطابقة. تحقق من النسخة الاحتياطية.",
  busy:"جارٍ تجهيز حسابك…",
  progress:"جارٍ تجهيز التسجيل",
  stopping:"جارٍ إيقاف حساب الإثبات قبل الإرسال إلى الشبكة…",
  submitting:"جارٍ إرسال طلب حسابك إلى هايبرسناب…",
  checking:"بانتظار التأكيد؛ التحقق الآلي مستمر.",
  working:"جارٍ إنشاء حسابك",
  blockchainCaption:"سلسلة كتل هايبرسناب",
  farcasterCaption:"شبكة فاركاستر",
  progressNote:"البحث عن الإثبات مستمر. النسبة المئوية تمثل ميزانية البحث عن التجزئة، لا تأكيد الشبكة.",
  pending:"حالة الطلب غير معروفة. لا تُعد الإرسال. عُد بكلماتك الأربع والعشرين.",
  stepOne:"النسخ الاحتياطي",
  stepTwo:"الطلب",
  stepThree:"التأكيد",
  paidStepTwo:"تم تفويض التسجيل المدعوم",
  paidStepThree:"فاركاستر يؤكد حسابك",
  chainPending:"احتفظ بكلمات الاسترداد الأربع والعشرين. يمكنك العودة في أي وقت.",
  voucher:"رمز رعاية جني‌كاستر",
  username:"اختر اسم مستخدم فاركاستر",
  voucherHelp:"يقدم مشغّل جني‌كاستر رمز الرعاية بعد التحقق من الدفع أو الرعاية. ليس مفتاح API من Neynar أو كلمة مرور المحفظة. لا يمكن شراء الرموز داخل جني‌كاستر حاليًا.",
  voucherLink:"لا يوجد رمز؟ أنشئ حسابًا في فاركاستر",
  paidReserve:"جارٍ تجهيز التسجيل المدعوم…",
  paidSign:"جارٍ تأكيد ملكية المحفظة…",
  paidSubmit:"جارٍ إرسال تسجيل فاركاستر…",
  paidVerify:"بانتظار تأكيد سلسلة كتل فاركاستر…"
 },
 de:{
  title:"Sichern Sie Ihre neue Wallet",
  subtitle:"Diese Wiederherstellungsphrase kontrolliert Ihre neue Identität im ausgewählten Protokoll.",
  warning:"Notieren Sie alle 24 Wörter in der richtigen Reihenfolge offline. Geben Sie sie niemals weiter. Der Browser-Speicher ist kein sicherer Sicherungsort.",
  challenge:"Gespeicherte Wiederherstellungsphrase prüfen",
  complete:"Ich habe meine Wiederherstellungsphrase sicher aufbewahrt.",
  consent:"Ich erlaube die einmalige Registrierung dieser Wallet im ausgewählten Netzwerk.",
  create:"Konto erstellen",
  cancel:"Abbrechen",
  close:"Schließen",
  copy:"Wörter kopieren",
  copied:"Wörter kopiert – löschen Sie anschließend die Zwischenablage.",
  failed:"Die ausgewählten Wiederherstellungswörter stimmen nicht überein. Prüfen Sie Ihre Sicherung.",
  busy:"Konto wird vorbereitet…",
  progress:"Registrierung wird vorbereitet",
  stopping:"Proof-Berechnung wird vor der Netzwerkübermittlung angehalten…",
  submitting:"Registrierungsanfrage wird an HyperSnap gesendet…",
  checking:"Warten auf Bestätigung. Automatische Prüfung läuft.",
  working:"Konto wird erstellt",
  blockchainCaption:"HYPERSNAP-BLOCKCHAIN",
  farcasterCaption:"FARCASTER-NETZWERK",
  progressNote:"Die Proof-Suche läuft. Der Prozentsatz bezeichnet das Hash-Suchbudget, nicht die Netzwerkbestätigung.",
  pending:"Anfragestatus unbekannt. Nicht erneut senden. Kehren Sie mit Ihren 24 Wörtern zurück.",
  stepOne:"Sicherung",
  stepTwo:"Anfrage",
  stepThree:"Bestätigung",
  paidStepTwo:"Gesponserte Registrierung autorisiert",
  paidStepThree:"Farcaster bestätigt Ihr Konto",
  chainPending:"Bewahre deine 24 Wiederherstellungswörter auf. Du kannst jederzeit zurückkehren.",
  voucher:"Genycaster-Sponsoringcode",
  username:"Farcaster-Benutzernamen wählen",
  voucherHelp:"Ein Sponsoringcode wird vom Genycaster-Betreiber nach bestätigter Zahlung oder Förderung bereitgestellt. Er ist kein Neynar-API-Schlüssel und kein Wallet-Passwort. Codes können derzeit nicht in Genycaster gekauft werden.",
  voucherLink:"Kein Code? Konto bei Farcaster erstellen",
  paidReserve:"Gesponserte Registrierung wird vorbereitet…",
  paidSign:"Wallet-Besitz wird bestätigt…",
  paidSubmit:"Farcaster-Registrierung wird gesendet…",
  paidVerify:"Warten auf Farcaster-Blockchain-Bestätigung…"
 },
 es:{
  title:"Haz una copia de seguridad de tu nueva cartera",
  subtitle:"Esta frase de recuperación controla tu nueva identidad en el protocolo seleccionado.",
  warning:"Anota las 24 palabras, en orden y sin conexión. Nunca las compartas. El almacenamiento del navegador no es una copia segura.",
  challenge:"Verifica la frase de recuperación guardada",
  complete:"He guardado mi frase de recuperación en un lugar seguro.",
  consent:"Autorizo el registro de esta cartera una sola vez en la red seleccionada.",
  create:"Crear cuenta",
  cancel:"Cancelar",
  close:"Cerrar",
  copy:"Copiar palabras",
  copied:"Palabras copiadas. Borra después el portapapeles.",
  failed:"Las palabras de recuperación elegidas no coinciden. Revisa la copia de seguridad.",
  busy:"Preparando tu cuenta…",
  progress:"Preparando el registro",
  stopping:"Deteniendo la prueba antes de enviarla a la red…",
  submitting:"Enviando tu solicitud de cuenta a HyperSnap…",
  checking:"Esperando confirmación. Comprobación automática en curso.",
  working:"Creando tu cuenta",
  blockchainCaption:"CADENA DE BLOQUES HYPERSNAP",
  farcasterCaption:"RED FARCASTER",
  progressNote:"La búsqueda de prueba está en curso. El porcentaje representa el presupuesto de búsqueda de hashes, no la confirmación de la red.",
  pending:"Se desconoce el estado de la solicitud. No la reenvíes. Vuelve con tus 24 palabras.",
  stepOne:"Copia de seguridad",
  stepTwo:"Solicitud",
  stepThree:"Confirmación",
  paidStepTwo:"Registro patrocinado autorizado",
  paidStepThree:"Farcaster confirma tu cuenta",
  chainPending:"Guarda tus 24 palabras de recuperación. Puedes volver cuando quieras.",
  voucher:"Código de patrocinio de Genycaster",
  username:"Elige un nombre de usuario de Farcaster",
  voucherHelp:"El operador de Genycaster proporciona un código de patrocinio después de verificar el pago o patrocinio. No es una clave API de Neynar ni la contraseña de una cartera. Actualmente no puedes comprar códigos dentro de Genycaster.",
  voucherLink:"¿No tienes código? Crea una cuenta en Farcaster",
  paidReserve:"Preparando el registro patrocinado…",
  paidSign:"Confirmando la propiedad de la cartera…",
  paidSubmit:"Enviando el registro a Farcaster…",
  paidVerify:"Esperando confirmación de la cadena de bloques de Farcaster…"
 },
 fr:{
  title:"Sauvegarder votre nouveau portefeuille",
  subtitle:"Cette phrase de récupération contrôle votre nouvelle identité sur le protocole choisi.",
  warning:"Notez les 24 mots dans l’ordre, hors ligne. Ne les partagez jamais. Le stockage du navigateur n’est pas une sauvegarde sûre.",
  challenge:"Vérifier votre phrase de récupération",
  complete:"J’ai sauvegardé ma phrase de récupération dans un endroit sûr.",
  consent:"J’autorise une seule inscription de ce portefeuille sur le réseau sélectionné.",
  create:"Créer un compte",
  cancel:"Annuler",
  close:"Fermer",
  copy:"Copier les mots",
  copied:"Mots copiés : videz ensuite votre presse-papiers.",
  failed:"Les mots de récupération sélectionnés ne correspondent pas. Vérifiez votre sauvegarde.",
  busy:"Préparation de votre compte…",
  progress:"Préparation de l’inscription",
  stopping:"Arrêt du calcul de preuve avant l’envoi au réseau…",
  submitting:"Envoi de votre demande de compte à HyperSnap…",
  checking:"En attente de confirmation. Vérification automatique en cours.",
  working:"Création de votre compte",
  blockchainCaption:"BLOCKCHAIN HYPERSNAP",
  farcasterCaption:"RÉSEAU FARCASTER",
  progressNote:"La recherche de preuve est en cours. Ce pourcentage indique le budget de recherche de hachage, pas la confirmation du réseau.",
  pending:"État de la demande inconnu. Ne réessayez pas. Revenez avec vos 24 mots.",
  stepOne:"Sauvegarde",
  stepTwo:"Demande",
  stepThree:"Confirmation",
  paidStepTwo:"Inscription parrainée autorisée",
  paidStepThree:"Farcaster confirme votre compte",
  chainPending:"Conservez vos 24 mots de récupération. Vous pourrez revenir à tout moment.",
  voucher:"Code de parrainage Genycaster",
  username:"Choisissez un nom d’utilisateur Farcaster",
  voucherHelp:"Un code de parrainage est fourni par l’opérateur Genycaster après vérification du paiement ou du parrainage. Ce n’est ni une clé API Neynar ni le mot de passe du portefeuille. Ces codes ne sont pas vendus actuellement dans Genycaster.",
  voucherLink:"Pas de code ? Créez un compte sur Farcaster",
  paidReserve:"Préparation de l’inscription parrainée…",
  paidSign:"Vérification de la propriété du portefeuille…",
  paidSubmit:"Envoi de l’inscription Farcaster…",
  paidVerify:"En attente de confirmation par la blockchain Farcaster…"
 },
 hi:{
  title:"अपने नए वॉलेट का बैकअप लें",
  subtitle:"यह रिकवरी वाक्यांश चयनित प्रोटोकॉल पर आपकी नई पहचान को नियंत्रित करता है।",
  warning:"सभी 24 शब्द क्रम से ऑफ़लाइन लिखें। किसी से साझा न करें। ब्राउज़र का स्टोरेज सुरक्षित बैकअप नहीं है।",
  challenge:"सहेजे गए रिकवरी वाक्यांश को सत्यापित करें",
  complete:"मैंने रिकवरी वाक्यांश सुरक्षित जगह पर रख दिया है।",
  consent:"मैं चयनित नेटवर्क पर इस वॉलेट के एक बार पंजीकरण की अनुमति देता/देती हूँ।",
  create:"खाता बनाएँ",
  cancel:"रद्द करें",
  close:"बंद करें",
  copy:"शब्द कॉपी करें",
  copied:"शब्द कॉपी हो गए — बाद में क्लिपबोर्ड साफ़ करें।",
  failed:"चुने हुए रिकवरी शब्द मेल नहीं खाते। अपना बैकअप जाँचें।",
  busy:"आपका खाता तैयार हो रहा है…",
  progress:"पंजीकरण तैयार हो रहा है",
  stopping:"नेटवर्क पर भेजने से पहले प्रमाण की प्रक्रिया रोकी जा रही है…",
  submitting:"आपका खाता अनुरोध HyperSnap पर भेजा जा रहा है…",
  checking:"पुष्टि की प्रतीक्षा। स्वचालित जाँच जारी है।",
  working:"आपका खाता बनाया जा रहा है",
  blockchainCaption:"HYPERSNAP ब्लॉकचेन",
  farcasterCaption:"FARCASTER नेटवर्क",
  progressNote:"प्रमाण की खोज जारी है। यह प्रतिशत हैश खोज की सीमा बताता है, नेटवर्क की पुष्टि नहीं।",
  pending:"अनुरोध की स्थिति अज्ञात है। फिर से न भेजें। अपने 24 शब्दों के साथ वापस आएँ।",
  stepOne:"बैकअप",
  stepTwo:"अनुरोध",
  stepThree:"पुष्टि",
  paidStepTwo:"प्रायोजित पंजीकरण अधिकृत",
  paidStepThree:"Farcaster आपके खाते की पुष्टि करता है",
  chainPending:"अपने 24 रिकवरी शब्द सँभालकर रखें। आप कभी भी लौट सकते हैं।",
  voucher:"Genycaster प्रायोजन कोड",
  username:"Farcaster उपयोगकर्ता नाम चुनें",
  voucherHelp:"भुगतान या प्रायोजन सत्यापित होने के बाद Genycaster संचालक प्रायोजन कोड देता है। यह Neynar API कुंजी या वॉलेट पासवर्ड नहीं है। अभी Genycaster में ये कोड खरीदे नहीं जा सकते।",
  voucherLink:"कोड नहीं है? Farcaster पर खाता बनाएँ",
  paidReserve:"प्रायोजित पंजीकरण तैयार हो रहा है…",
  paidSign:"वॉलेट स्वामित्व की पुष्टि हो रही है…",
  paidSubmit:"Farcaster पंजीकरण भेजा जा रहा है…",
  paidVerify:"Farcaster ब्लॉकचेन की पुष्टि की प्रतीक्षा…"
 },
 ja:{
  title:"新しいウォレットをバックアップ",
  subtitle:"この復元フレーズで、選択したプロトコル上の新しいIDを管理します。",
  warning:"24個の単語を順番にオフラインで書き留めてください。他人に教えないでください。ブラウザー保存は安全なバックアップではありません。",
  challenge:"保存した復元フレーズを確認",
  complete:"復元フレーズを安全な場所に保存しました。",
  consent:"選択したネットワークにこのウォレットを一度だけ登録することを許可します。",
  create:"アカウントを作成",
  cancel:"キャンセル",
  close:"閉じる",
  copy:"単語をコピー",
  copied:"単語をコピーしました。後でクリップボードを消去してください。",
  failed:"選択した復元ワードが一致しません。バックアップを確認してください。",
  busy:"アカウントを準備中…",
  progress:"登録を準備中",
  stopping:"ネットワーク送信前に証明計算を停止中…",
  submitting:"アカウント登録リクエストをHyperSnapに送信中…",
  checking:"確認待ちです。自動チェックを続けています。",
  working:"アカウントを作成中",
  blockchainCaption:"HYPERSNAPブロックチェーン",
  farcasterCaption:"FARCASTERネットワーク",
  progressNote:"証明の探索中です。この割合はハッシュ探索の予算であり、ネットワーク確認ではありません。",
  pending:"リクエストの状態が不明です。再送しないでください。24個の単語を使って戻ってください。",
  stepOne:"バックアップ",
  stepTwo:"リクエスト",
  stepThree:"確認",
  paidStepTwo:"スポンサー登録が承認されました",
  paidStepThree:"Farcasterがアカウントを確認中",
  chainPending:"24個の復元ワードを保管してください。いつでも戻れます。",
  voucher:"Genycasterスポンサーコード",
  username:"Farcasterユーザー名を選択",
  voucherHelp:"スポンサーコードは支払いまたはスポンサー提供の確認後にGenycaster運営者から提供されます。NeynarのAPIキーやウォレットのパスワードではありません。現在、Genycaster内では購入できません。",
  voucherLink:"コードがない場合はFarcasterでアカウントを作成",
  paidReserve:"スポンサー登録を準備中…",
  paidSign:"ウォレットの所有権を確認中…",
  paidSubmit:"Farcaster登録を送信中…",
  paidVerify:"Farcasterブロックチェーンの確認待ち…"
 },
 ko:{
  title:"새 지갑 백업하기",
  subtitle:"이 복구 문구는 선택한 프로토콜에서 새 ID를 제어합니다.",
  warning:"24개 단어를 순서대로 인터넷 연결 없이 적어 두세요. 절대 공유하지 마세요. 브라우저 저장소는 안전한 백업이 아닙니다.",
  challenge:"저장한 복구 문구 확인",
  complete:"복구 문구를 안전한 곳에 저장했습니다.",
  consent:"선택한 네트워크에 이 지갑을 한 번 등록하도록 승인합니다.",
  create:"계정 만들기",
  cancel:"취소",
  close:"닫기",
  copy:"단어 복사",
  copied:"단어가 복사되었습니다. 나중에 클립보드를 지우세요.",
  failed:"선택한 복구 단어가 일치하지 않습니다. 백업을 확인하세요.",
  busy:"계정 준비 중…",
  progress:"등록 준비 중",
  stopping:"네트워크 전송 전에 증명 연산 중지 중…",
  submitting:"HyperSnap에 계정 요청 전송 중…",
  checking:"네트워크 확인을 기다리는 중입니다. 자동 검사를 계속합니다.",
  working:"계정 생성 중",
  blockchainCaption:"HYPERSNAP 블록체인",
  farcasterCaption:"FARCASTER 네트워크",
  progressNote:"증명 검색이 진행 중입니다. 이 비율은 해시 검색 예산이며 네트워크 확인이 아닙니다.",
  pending:"요청 상태를 알 수 없습니다. 다시 보내지 마세요. 24개 단어로 돌아오세요.",
  stepOne:"백업",
  stepTwo:"요청",
  stepThree:"확인",
  paidStepTwo:"후원 등록 승인됨",
  paidStepThree:"Farcaster가 계정 확인 중",
  chainPending:"24개의 복구 단어를 안전하게 보관하세요. 언제든 다시 돌아올 수 있습니다.",
  voucher:"Genycaster 후원 코드",
  username:"Farcaster 사용자 이름 선택",
  voucherHelp:"후원 코드는 결제나 후원 확인 후 Genycaster 운영자가 제공하며 Neynar API 키나 지갑 비밀번호가 아닙니다. 현재 Genycaster에서는 코드를 구매할 수 없습니다.",
  voucherLink:"코드가 없나요? Farcaster에서 계정 만들기",
  paidReserve:"후원 등록 준비 중…",
  paidSign:"지갑 소유권 확인 중…",
  paidSubmit:"Farcaster 등록 전송 중…",
  paidVerify:"Farcaster 블록체인 확인 대기 중…"
 },
 pt:{
  title:"Faça backup da sua nova carteira",
  subtitle:"Esta frase de recuperação controla sua nova identidade no protocolo escolhido.",
  warning:"Anote as 24 palavras, em ordem, sem conexão. Nunca as compartilhe. O armazenamento do navegador não é um backup seguro.",
  challenge:"Verifique a frase de recuperação salva",
  complete:"Salvei minha frase de recuperação em um lugar seguro.",
  consent:"Autorizo o registro único desta carteira na rede selecionada.",
  create:"Criar conta",
  cancel:"Cancelar",
  close:"Fechar",
  copy:"Copiar palavras",
  copied:"Palavras copiadas. Limpe a área de transferência depois.",
  failed:"As palavras de recuperação selecionadas não correspondem. Confira seu backup.",
  busy:"Preparando sua conta…",
  progress:"Preparando seu cadastro",
  stopping:"Interrompendo a prova antes do envio à rede…",
  submitting:"Enviando sua solicitação de conta ao HyperSnap…",
  checking:"Aguardando confirmação. Verificação automática em andamento.",
  working:"Criando sua conta",
  blockchainCaption:"BLOCKCHAIN HYPERSNAP",
  farcasterCaption:"REDE FARCASTER",
  progressNote:"A busca de prova está em andamento. Essa porcentagem representa o limite da busca de hashes, não a confirmação da rede.",
  pending:"Status da solicitação desconhecido. Não reenvie. Volte com suas 24 palavras.",
  stepOne:"Cópia de segurança",
  stepTwo:"Solicitação",
  stepThree:"Confirmação",
  paidStepTwo:"Cadastro patrocinado autorizado",
  paidStepThree:"Farcaster confirma sua conta",
  chainPending:"Guarde suas 24 palavras de recuperação. Você pode voltar a qualquer momento.",
  voucher:"Código de patrocínio do Genycaster",
  username:"Escolha um nome de usuário Farcaster",
  voucherHelp:"Um código de patrocínio é fornecido pelo operador do Genycaster após a verificação do pagamento ou patrocínio. Não é uma chave API da Neynar nem a senha da carteira. Os códigos ainda não são vendidos no Genycaster.",
  voucherLink:"Sem código? Crie uma conta no Farcaster",
  paidReserve:"Preparando cadastro patrocinado…",
  paidSign:"Confirmando a posse da carteira…",
  paidSubmit:"Enviando cadastro ao Farcaster…",
  paidVerify:"Aguardando confirmação na blockchain Farcaster…"
 },
 ru:{
  title:"Создайте резервную копию нового кошелька",
  subtitle:"Эта фраза восстановления даёт контроль над новой идентичностью в выбранном протоколе.",
  warning:"Запишите все 24 слова по порядку без подключения к сети. Никому их не передавайте. Хранилище браузера не подходит для безопасной копии.",
  challenge:"Проверьте сохранённую фразу восстановления",
  complete:"Я сохранил фразу восстановления в безопасном месте.",
  consent:"Разрешаю однократную регистрацию этого кошелька в выбранной сети.",
  create:"Создать аккаунт",
  cancel:"Отмена",
  close:"Закрыть",
  copy:"Копировать слова",
  copied:"Слова скопированы — после этого очистите буфер обмена.",
  failed:"Выбранные слова восстановления не совпадают. Проверьте резервную копию.",
  busy:"Подготовка аккаунта…",
  progress:"Подготовка регистрации",
  stopping:"Остановка расчёта доказательства до отправки в сеть…",
  submitting:"Отправка запроса на создание аккаунта в HyperSnap…",
  checking:"Ожидание подтверждения. Выполняется автоматическая проверка.",
  working:"Создание аккаунта",
  blockchainCaption:"БЛОКЧЕЙН HYPERSNAP",
  farcasterCaption:"СЕТЬ FARCASTER",
  progressNote:"Поиск доказательства продолжается. Процент отражает лимит поиска хешей, а не подтверждение сети.",
  pending:"Статус запроса неизвестен. Не отправляйте повторно. Вернитесь с 24 словами.",
  stepOne:"Резервная копия",
  stepTwo:"Запрос",
  stepThree:"Подтверждение",
  paidStepTwo:"Спонсируемая регистрация разрешена",
  paidStepThree:"Farcaster подтверждает аккаунт",
  chainPending:"Сохраните 24 слова восстановления. Вы можете вернуться в любое время.",
  voucher:"Спонсорский код Genycaster",
  username:"Выберите имя пользователя Farcaster",
  voucherHelp:"Спонсорский код предоставляется оператором Genycaster после подтверждения оплаты или спонсорства. Это не API-ключ Neynar и не пароль кошелька. Купить эти коды в Genycaster сейчас нельзя.",
  voucherLink:"Нет кода? Создайте аккаунт в Farcaster",
  paidReserve:"Подготовка спонсируемой регистрации…",
  paidSign:"Подтверждение владения кошельком…",
  paidSubmit:"Отправка регистрации Farcaster…",
  paidVerify:"Ожидание подтверждения блокчейном Farcaster…"
 },
 zh:{
  title:"备份您的新钱包",
  subtitle:"此恢复短语可控制您在所选协议上的新身份。",
  warning:"请离线按顺序记下全部 24 个单词，切勿分享。浏览器存储不是安全的备份方式。",
  challenge:"验证已保存的恢复短语",
  complete:"我已将恢复短语保存在安全的位置。",
  consent:"我授权此钱包在所选网络上进行一次注册。",
  create:"创建账户",
  cancel:"取消",
  close:"关闭",
  copy:"复制助记词",
  copied:"助记词已复制，请随后清空剪贴板。",
  failed:"所选恢复词不匹配，请检查备份。",
  busy:"正在准备账户…",
  progress:"正在准备注册",
  stopping:"在提交网络前停止证明计算…",
  submitting:"正在向 HyperSnap 发送账户请求…",
  checking:"等待确认，正在自动检查。",
  working:"正在创建账户",
  blockchainCaption:"HYPERSNAP 区块链",
  farcasterCaption:"FARCASTER 网络",
  progressNote:"正在搜索证明。此百分比代表哈希搜索预算，并非网络确认进度。",
  pending:"请求状态未知。请勿重试，请使用 24 个恢复词返回。",
  stepOne:"备份",
  stepTwo:"请求",
  stepThree:"确认",
  paidStepTwo:"赞助注册已获授权",
  paidStepThree:"Farcaster 正在确认您的账户",
  chainPending:"请妥善保管 24 个恢复词，随时可以返回。",
  voucher:"Genycaster 赞助码",
  username:"选择 Farcaster 用户名",
  voucherHelp:"Genycaster 运营方在验证付款或赞助后提供赞助码。此码不是 Neynar API 密钥或钱包密码。目前无法在 Genycaster 内购买。",
  voucherLink:"没有赞助码？前往 Farcaster 创建账户",
  paidReserve:"正在准备赞助注册…",
  paidSign:"正在确认钱包所有权…",
  paidSubmit:"正在提交 Farcaster 注册…",
  paidVerify:"等待 Farcaster 区块链确认…"
 }
};
const titleFor=network=>network==='hypersnap-v2'?'HyperSnap':'Farcaster';
const clearNode=node=>{if(node){node.textContent='';node.replaceChildren();}};
export async function promptCreateBrowserWallet({
 browser=globalThis,signal=null,protocol='hypersnap-v2',
 onRegister,onProgress=()=>{},generatePhrase=generateNativePhrase
}={}) {
 if(signal?.aborted)throw new Error('Account creation cancelled');
 if(!['hypersnap-v2','farcaster-snapchain'].includes(protocol))
   throw new Error('Invalid browser registration protocol');
 if(browser.isSecureContext!==true||!browser.document?.body)
   throw new Error('Account creation requires HTTPS');
 if(typeof onRegister!=='function')throw new Error('Registration service is missing');
 const document=browser.document,existing=document.getElementById('genycaster-browser-new-wallet');
 if(existing)throw new Error('Wallet registration is already open');
 const language=(browser.__genycasterPreferredLanguage||browser.navigator?.language||'en').split(/[-_]/)[0];
 const lang=translations[language]?language:'en',t=translations[lang];
 const isRtl=['fa','ar'].includes(lang);
 const light=document.documentElement?.dataset?.theme==='light'
   ||(!document.documentElement?.dataset?.theme&&browser.matchMedia?.('(prefers-color-scheme:light)').matches);
 const colors=light
  ?{bg:'#fff',fg:'#17181c',muted:'#777b86',border:'#dedfe4',cell:'#f8f8fa'}
  :{bg:'#101116',fg:'#f4f6ff',muted:'#adb1bb',border:'#323640',cell:'#191c23'};
 let phrase=await generatePhrase(browser),settled=false,processing=false;
 if(signal?.aborted){phrase='';throw new Error('Account creation cancelled');}
 const words=phrase.split(' ');
 const indices=[];
 const rand=new Uint32Array(2);
 browser.crypto.getRandomValues(rand);
 indices.push(rand[0]%24);
 indices.push((rand[1]%23+indices[0]+1)%24);
 const layer=document.createElement('div'),form=document.createElement('section');
 layer.id='genycaster-browser-new-wallet';
 layer.style.cssText='position:fixed;inset:0;z-index:2147483647;background:'+colors.bg+';font:15px system-ui,sans-serif;color:'+colors.fg+';box-sizing:border-box;';
 form.dir=isRtl?'rtl':'ltr';form.setAttribute('role','dialog');form.setAttribute('aria-modal','true');
 form.setAttribute('aria-label',t.title);
 form.style.cssText='max-width:740px;margin:auto;box-sizing:border-box;height:100%;display:flex;flex-direction:column;padding:max(30px,env(safe-area-inset-top)) 24px max(24px,env(safe-area-inset-bottom));gap:14px;';
 const content=document.createElement('div');
 content.style.cssText='overflow-y:auto;min-height:0;flex:1;padding:0 2px;overscroll-behavior:contain;';
 const heading=document.createElement('h1');heading.textContent=t.title;
 heading.style.cssText='font-size:clamp(22px,4vw,32px);margin:18px 0 10px;font-weight:700;';
 const subtitle=document.createElement('p');
 subtitle.textContent=t.subtitle+' '+titleFor(protocol);
 subtitle.style.cssText='color:'+colors.muted+';line-height:1.6;margin:0 0 16px';
 const warning=document.createElement('p');warning.textContent=t.warning;
 warning.style.cssText='line-height:1.6;color:'+colors.muted+';margin:0 0 16px';
 const grid=document.createElement('div');
 grid.style.cssText='display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px;direction:ltr;'
 const progressPanel=document.createElement('section');
 progressPanel.hidden=true;
 progressPanel.style.cssText='display:none;margin:22px 0;padding:20px;border:1px solid '+colors.border+';border-radius:18px;background:'+colors.cell+';';
 const progressHeading=document.createElement('h2');
 progressHeading.textContent=protocol==='hypersnap-v2'?t.blockchainCaption:t.farcasterCaption;
 progressHeading.style.cssText='font-size:12px;font-weight:700;letter-spacing:.09em;margin:0 0 12px;color:'+colors.muted;
 const progressCaption=document.createElement('p');progressCaption.textContent=t.busy;
 progressCaption.setAttribute('aria-live','polite');
 progressCaption.style.cssText='font-size:15px;line-height:1.6;color:'+colors.muted+';margin:0 0 12px;overflow-wrap:anywhere;';
 const progressTrack=document.createElement('div');progressTrack.style.cssText='height:6px;background:'+colors.border+';border-radius:9px;overflow:hidden;';
 const progressFill=document.createElement('div');progressFill.style.cssText='height:100%;width:0%;border-radius:9px;background:#008bff;transition:width 150ms linear;';
 progressTrack.appendChild(progressFill);
 const progressNote=document.createElement('p');progressNote.textContent=t.progressNote;
 progressNote.style.cssText='font-size:12px;line-height:1.6;color:'+colors.muted+';margin:10px 0 0';
 const stepList=document.createElement('ol');
 stepList.style.cssText='list-style:none;padding:0;margin:16px 0 0;display:grid;gap:11px;';
 const stepNames=[t.stepOne,protocol==='hypersnap-v2'?t.stepTwo:t.paidStepTwo,
   protocol==='hypersnap-v2'?t.stepThree:t.paidStepThree];
 const stepEntries=stepNames.map((name,index)=>{
   const item=document.createElement('li');
   item.textContent=(index+1)+'.  '+name;
   item.style.cssText='padding:8px 12px;border-radius:11px;background:'+
     (index===0?'rgba(12,194,129,.12)':'transparent')+
     ';color:'+(index===0?colors.fg:colors.muted)+';font-size:14px;line-height:1.5;';
   stepList.appendChild(item);return item;
 });
 const showProgressStep=step=>{
   stepEntries.forEach((item,index)=>{
     item.style.background=index===step?'rgba(0,126,255,.13)':
       index<step?'rgba(12,194,129,.10)':'transparent';
     item.style.color=index<=step?colors.fg:colors.muted;
   });
 };
 progressPanel.append(progressHeading,progressCaption,progressTrack,progressNote,stepList);
 const cells=[];
 words.forEach((word,i)=>{
  const cell=document.createElement('div');
  cell.style.cssText='display:flex;align-items:center;min-width:0;gap:12px;padding:11px;border:1px solid '+colors.border+';border-radius:12px;background:'+colors.cell;
  const index=document.createElement('span');index.textContent=String(i+1).padStart(2,'0');
  index.style.cssText='color:'+colors.muted+';font:600 12px ui-monospace,monospace;';
  const value=document.createElement('strong');value.textContent=word;value.style.cssText='font-weight:600';
  cell.append(index,value);grid.appendChild(cell);cells.push(cell);
 });
 const backupAction=document.createElement('button');backupAction.type='button';backupAction.textContent=t.copy;
 backupAction.style.cssText='margin:16px 0;padding:12px 16px;border:1px solid '+colors.border+';background:'+colors.cell+';color:'+colors.fg+';border-radius:12px;font-weight:600;';
 const challengeHeading=document.createElement('h2');challengeHeading.textContent=t.challenge;
 challengeHeading.style.cssText='font-size:17px;margin:14px 0 12px;font-weight:600';
 const challenge=document.createElement('div');
 challenge.style.cssText='display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px';
 const checks=[];
 indices.forEach(index=>{
  const label=document.createElement('label');
  label.textContent='#'+String(index+1).padStart(2,'0');
  label.style.cssText='display:flex;align-items:center;gap:10px;';
  const input=document.createElement('input');
  input.type='password';input.autocomplete='off';input.autocorrect='off';
  input.autocapitalize='off';input.spellcheck=false;
  input.setAttribute('aria-label',t.challenge+' '+(index+1));
  input.style.cssText='width:100%;min-width:0;padding:14px;border:1px solid '+colors.border+';border-radius:12px;background:'+colors.cell+';color:'+colors.fg+';font:16px system-ui';
  label.appendChild(input);challenge.appendChild(label);checks.push(input);
 });
 const confirmation=(message)=>{
  const label=document.createElement('label');
  label.style.cssText='display:flex;align-items:flex-start;gap:12px;margin:16px 0;line-height:1.6';
  const input=document.createElement('input');input.type='checkbox';
  input.style.cssText='width:22px;height:22px;flex-shrink:0;accent-color:'+colors.fg;
  const span=document.createElement('span');span.textContent=message;
  label.append(input,span);content.appendChild(label);
  return input;
 };
 const result=document.createElement('p');
 result.setAttribute('aria-live','polite');
 result.style.cssText='min-height:25px;line-height:1.6;color:'+(light?'#a4530c':'#ffbd71')+';overflow-wrap:anywhere';
 const buttons=document.createElement('div');
 buttons.style.cssText='display:flex;gap:12px;padding-top:15px;border-top:1px solid '+colors.border;
 const button=(text,primary)=>{
  const b=document.createElement('button');b.type='button';b.textContent=text;
  b.style.cssText='flex:1;min-height:50px;padding:12px;border-radius:14px;font:600 16px system-ui;background:'+
    (primary?colors.fg:colors.bg)+';color:'+(primary?colors.bg:colors.fg)+
    ';border:1px solid '+colors.border+';cursor:pointer;';
  buttons.appendChild(b);return b;
 };
 content.append(heading,subtitle,warning,grid,progressPanel,backupAction,challengeHeading,challenge);
 const saved=confirmation(t.complete),consent=confirmation(t.consent);
 const credentialFields={};
 if(protocol==='farcaster-snapchain'){
  for(const [field,label,limit,secret] of [
    ['voucher',t.voucher,1100,true],['username',t.username,16,false]]) {
    if(field==='voucher'){
      const help=document.createElement('p');
      help.textContent=t.voucherHelp;
      help.style.cssText='margin:18px 0 8px;color:'+colors.muted+';line-height:1.6;font-size:14px;';
      const official=document.createElement('a');
      official.href='https://farcaster.xyz/';
      official.target='_blank';official.rel='noopener noreferrer';
      official.textContent=t.voucherLink;
      official.style.cssText='display:inline-block;margin:0 0 12px;color:#218cff;text-decoration:underline;font-weight:600;';
      content.append(help,official);
    }
    const container=document.createElement('label');
    container.textContent=label;
    container.style.cssText='display:flex;flex-direction:column;gap:7px;margin:12px 0;font-weight:600;';
    const input=document.createElement('input');
    input.type=secret?'password':'text';input.maxLength=limit;
    input.autocomplete='off';input.autocapitalize='off';input.spellcheck=false;
    input.style.cssText='width:100%;box-sizing:border-box;padding:12px;border-radius:12px;border:1px solid '+colors.border+';background:'+colors.cell+';color:'+colors.fg+';font:16px system-ui;direction:ltr;';
    container.appendChild(input);content.appendChild(container);
    credentialFields[field]=input;
  }
 }
 content.appendChild(result);
 const cancel=button(t.cancel,false),submit=button(t.create,true);
 // The action stays directly below its form, not detached at screen bottom.
 content.appendChild(buttons);
 form.append(content);layer.appendChild(form);
 const oldFocus=document.activeElement;
 const oldOverflow=document.body.style.overflow;
 document.body.style.overflow='hidden';document.body.appendChild(layer);
 let attempts=0;
 const operationController=new AbortController();
 const cancelOperation=()=>operationController.abort(new Error('Account creation cancelled'));
 signal?.addEventListener('abort',cancelOperation,{once:true});
 const wipe=()=>{
  for(const input of checks)input.value='';
  for(const input of Object.values(credentialFields))input.value='';
  words.fill('');
  for(const cell of cells)clearNode(cell);
  saved.checked=false;consent.checked=false;phrase='';
  if(layer.isConnected)layer.remove();
  document.body.style.overflow=oldOverflow;
  oldFocus?.focus?.();
 };
 return new Promise((resolve,reject)=>{
  const finish=(error,value)=>{
   if(settled)return;
   settled=true;signal?.removeEventListener('abort',abort);
   signal?.removeEventListener('abort',cancelOperation);
   wipe();if(error)reject(error);else resolve(value);
  };
  let submissionStarted=false;
  const abort=()=>{
   if(!processing) { finish(new Error('Account creation cancelled'));return; }
   if(submissionStarted||operationController.signal.aborted)return;
   progressCaption.textContent=t.stopping;
   cancel.disabled=true;
   cancelOperation(); // Stop only before the irreversible network POST.
  };
  signal?.addEventListener('abort',abort,{once:true});
  cancel.onclick=()=>abort();
  backupAction.onclick=async()=>{
   try{
    await browser.navigator.clipboard.writeText(phrase);
    result.textContent=t.copied;
   }catch{result.textContent=t.warning;}
  };
  submit.onclick=async()=>{
   if(processing||settled)return;
   const match=indices.every((index,i)=>checks[i].value.trim().toLowerCase()===words[index]);
   if(!match||!saved.checked||!consent.checked){
    attempts++;
    result.textContent=t.failed;
    if(attempts>=5)finish(new Error('Backup verification attempt limit reached'));
    return;
   }
   // Hide secret words and their now-empty 24 cards entirely. Replace the
   // backup form with a responsive progress panel; no mnemonic enters logs,
   // URLs, QML, or the proof worker. Cancellation stays available until POST.
   for(const cell of cells)clearNode(cell);
   words.fill(''); // The verified word array is not needed during PoW.
   for(const element of [grid,backupAction,challengeHeading,challenge,warning])
     element.style.display='none';
   saved.parentElement.style.display='none';
   consent.parentElement.style.display='none';
   for(const field of Object.values(credentialFields))field.parentElement.style.display='none';
   // One clear heading for the new operation, not the obsolete wallet
   // backup title plus a second "Creating your account" heading.
   heading.textContent=t.working;
   subtitle.style.display='none';
   progressPanel.hidden=false;progressPanel.style.display='block';
   showProgressStep(0);
   content.scrollTop=0;
   processing=true;submit.disabled=true;cancel.disabled=false;
   result.textContent='';
   const update=(done,total,phase)=>{
    if(settled)return;
    if(phase==='submitting'||phase==='checking'||phase==='outcome-unknown'
       ||(protocol==='farcaster-snapchain'
          && ['reserve','submit','verify'].includes(phase))) {
      // Neynar FID reservation can itself be paid/irrevocable. Disable
      // cancellation BEFORE the reserve POST, not merely before submit.
      submissionStarted=true;
      showProgressStep(phase==='submitting'||phase==='reserve'?1:2);
      phrase=''; // Only the registry's ephemeral in-flight custody owns it now.
      cancel.disabled=true;
      progressCaption.textContent=phase==='reserve'?t.paidReserve
        :phase==='submitting'||phase==='submit'
          ? (protocol==='hypersnap-v2'?t.submitting:t.paidSubmit)
        :phase==='checking'||phase==='verify'
          ? (protocol==='hypersnap-v2'?t.checking:t.paidVerify)
          :t.pending;
      progressTrack.style.display='none';
      progressNote.style.display='none';
    }else if(protocol==='farcaster-snapchain'){
      progressCaption.textContent=phase==='reserve'?t.paidReserve:
        phase==='sign'?t.paidSign:
        phase==='verify'?t.paidVerify:t.busy;
      progressFill.style.width=Math.max(0,Math.min(100,Math.floor(done/Math.max(1,total)*100)))+'%';
    }else{
      // PoW is a random search, NOT a deterministic completion percentage.
      // Keep the UI responsive and report the actual stage instead of a
      // misleading percent-complete indicator.
      progressCaption.textContent=t.progress;
      progressTrack.style.display='none';
      progressNote.style.display='none';
    }
    onProgress(done,total,phase);
   };
   try{
    const voucher=credentialFields.voucher?.value||'';
    const username=(credentialFields.username?.value||'').trim().toLowerCase();
    const verified=await onRegister(phrase,{signal:operationController.signal,onProgress:update,
      confirmIrreversible:true,voucher,username});
    credentialFields.voucher && (credentialFields.voucher.value='');
    finish(null,verified);
   }catch(error){
    if(error.outcomeUnknown){
      progressCaption.textContent=t.pending;
      result.textContent='';
      progressTrack.style.display='none';progressNote.style.display='none';
      submit.disabled=true;cancel.disabled=false;
      cancel.textContent=t.close; // The network write cannot be cancelled.
      processing=false;cancel.onclick=()=>finish(error);
    }else{
      finish(error);
    }
   }
  };
 });
}
