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
  progressNote:'Proof is computed locally. The counter shows hash attempts, not blockchain confirmation.',
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
  progressNote:'اثبات روی دستگاه محاسبه می‌شود. شمارنده، تعداد تلاش‌های محاسباتی است؛ نه تأیید بلاک‌چین.',
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
  progressNote:"يُحسب الإثبات محليًا. يعرض العداد محاولات التجزئة، وليس تأكيد التسجيل على الشبكة.",
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
  progressNote:"Der Nachweis wird lokal berechnet. Der Zähler zeigt Hash-Versuche, keine Blockchain-Bestätigung.",
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
  progressNote:"La prueba se calcula en el dispositivo. El contador muestra intentos de hash, no una confirmación de la red.",
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
  progressNote:"La preuve est calculée localement. Le compteur montre les essais de hachage, pas une confirmation du réseau.",
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
  progressNote:"प्रमाण की गणना डिवाइस पर होती है। यह काउंटर हैश प्रयास दिखाता है, ब्लॉकचेन पुष्टि नहीं।",
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
  progressNote:"証明は端末内で計算されます。カウンターはハッシュ試行回数であり、ネットワークの承認ではありません。",
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
  progressNote:"증명은 기기에서 계산됩니다. 카운터는 해시 시도 횟수이며 블록체인 승인이 아닙니다.",
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
  progressNote:"A prova é calculada no dispositivo. O contador mostra tentativas de hash, não a confirmação da rede.",
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
  progressNote:"Доказательство вычисляется локально. Счётчик показывает попытки хеширования, а не подтверждение сети.",
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
  progressNote:"注册证明在本地计算。计数器显示哈希尝试次数，而非区块链确认进度。",
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
// Localized, real registration stages. These never reveal recovery words,
// private keys, custody signatures, or proof material to the visible UI.
const phases={
 en:{network:'Checking shard-0 blockchain readiness…',challenge:'Getting the current registration challenge…',
  lookup:'Checking whether this wallet already has an account…',proof:'Computing the registration proof locally…',
  signing:'Authorizing the registration on this device…',retry:'Try again',
  notSubmitted:'Registration was not submitted. Check the network and try again.'},
 fa:{network:'در حال بررسی آماده‌بودن بلاک‌های شارد صفر…',challenge:'دریافت اطلاعات به‌روز ثبت‌نام از شبکه…',
  lookup:'بررسی اینکه این کیف پول قبلاً ثبت شده است یا نه…',proof:'در حال محاسبهٔ اثبات ثبت‌نام روی دستگاه…',
  signing:'در حال تأیید ثبت‌نام با کیف پول روی همین دستگاه…',retry:'تلاش دوباره',
  notSubmitted:'ثبت‌نام به شبکه ارسال نشد. وضعیت شبکه را بررسی کنید و دوباره تلاش کنید.'},
 ar:{network:'جارٍ فحص جاهزية الكتل في الشارد صفر…',challenge:'جارٍ جلب تحدي التسجيل الحالي…',
  lookup:'جارٍ التحقق مما إذا كانت المحفظة مسجلة بالفعل…',proof:'جارٍ حساب إثبات التسجيل محليًا…',
  signing:'جارٍ تفويض التسجيل على هذا الجهاز…',retry:'إعادة المحاولة',
  notSubmitted:'لم يبدأ التسجيل ولم يُرسل أي طلب. تحقق من الشبكة وأعد المحاولة.'},
 de:{network:'Blockbereitschaft von Shard 0 wird geprüft…',challenge:'Aktuelle Registrierungsaufgabe wird geladen…',
  lookup:'Es wird geprüft, ob diese Wallet bereits registriert ist…',proof:'Registrierungsnachweis wird lokal berechnet…',
  signing:'Registrierung wird auf diesem Gerät autorisiert…',retry:'Erneut versuchen',
  notSubmitted:'Registrierung konnte nicht starten. Keine Anfrage wurde gesendet. Netzwerk prüfen und erneut versuchen.'},
 es:{network:'Comprobando los bloques del fragmento 0…',challenge:'Obteniendo el desafío de registro actual…',
  lookup:'Comprobando si esta cartera ya está registrada…',proof:'Calculando la prueba de registro en el dispositivo…',
  signing:'Autorizando el registro en este dispositivo…',retry:'Reintentar',
  notSubmitted:'No se pudo iniciar el registro. No se envió ninguna solicitud. Comprueba la red y vuelve a intentarlo.'},
 fr:{network:'Vérification des blocs du shard 0…',challenge:'Récupération du défi d’inscription actuel…',
  lookup:'Vérification de l’inscription éventuelle de ce portefeuille…',proof:'Calcul local de la preuve d’inscription…',
  signing:'Autorisation de l’inscription sur cet appareil…',retry:'Réessayer',
  notSubmitted:'L’inscription n’a pas démarré. Aucune demande envoyée. Vérifiez le réseau et réessayez.'},
 hi:{network:'शार्ड 0 की ब्लॉक स्थिति जाँची जा रही है…',challenge:'वर्तमान पंजीकरण चुनौती प्राप्त की जा रही है…',
  lookup:'जाँचा जा रहा है कि यह वॉलेट पहले से पंजीकृत है या नहीं…',proof:'डिवाइस पर पंजीकरण प्रमाण की गणना की जा रही है…',
  signing:'इस डिवाइस पर पंजीकरण अधिकृत किया जा रहा है…',retry:'फिर से कोशिश करें',
  notSubmitted:'पंजीकरण शुरू नहीं हुआ। कोई अनुरोध भेजा नहीं गया। नेटवर्क जाँचें और दोबारा कोशिश करें।'},
 ja:{network:'シャード0のブロック状態を確認中…',challenge:'最新の登録チャレンジを取得中…',
  lookup:'このウォレットが登録済みか確認中…',proof:'端末内で登録証明を計算中…',
  signing:'この端末で登録を承認中…',retry:'再試行',
  notSubmitted:'登録を開始できず、要求は送信されていません。ネットワークを確認して再試行してください。'},
 ko:{network:'샤드 0 블록 준비 상태를 확인하는 중…',challenge:'최신 등록 챌린지를 가져오는 중…',
  lookup:'이 지갑이 이미 등록되었는지 확인하는 중…',proof:'기기에서 등록 증명을 계산하는 중…',
  signing:'이 기기에서 등록을 승인하는 중…',retry:'다시 시도',
  notSubmitted:'등록이 시작되지 않았으며 요청을 전송하지 않았습니다. 네트워크를 확인한 후 다시 시도하세요.'},
 pt:{network:'Verificando os blocos do shard 0…',challenge:'Obtendo o desafio atual de cadastro…',
  lookup:'Verificando se esta carteira já está registrada…',proof:'Calculando a prova de cadastro neste dispositivo…',
  signing:'Autorizando o cadastro neste dispositivo…',retry:'Tentar novamente',
  notSubmitted:'O cadastro não começou. Nenhuma solicitação foi enviada. Verifique a rede e tente novamente.'},
 ru:{network:'Проверка готовности блоков шарда 0…',challenge:'Получение актуального запроса регистрации…',
  lookup:'Проверка того, зарегистрирован ли этот кошелёк…',proof:'Локальное вычисление доказательства регистрации…',
  signing:'Авторизация регистрации на этом устройстве…',retry:'Повторить',
  notSubmitted:'Регистрация не началась. Запрос не отправлен. Проверьте сеть и повторите попытку.'},
 zh:{network:'正在检查分片 0 的区块状态…',challenge:'正在获取最新注册挑战…',
  lookup:'正在检查此钱包是否已注册…',proof:'正在设备本地计算注册证明…',
  signing:'正在此设备上授权注册…',retry:'重试',
  notSubmitted:'注册未能启动，且没有发送请求。请检查网络后重试。'}
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
 // This browser-only custody surface must have the same complete interaction
 // states as Qt's product-owned controls. Scope everything to this overlay.
 const skin=document.createElement('style');
 skin.textContent=`
 #genycaster-browser-new-wallet {--wallet-accent:${light?'#315cf5':'#829aff'};--wallet-ink:${colors.fg};--wallet-bg:${colors.bg};--wallet-cell:${colors.cell};--wallet-border:${colors.border};--wallet-muted:${colors.muted};--wallet-focus:${light?'#315cf5':'#a4b5ff'};--wallet-accent-ink:${light?'#fff':'#101116'};}
 #genycaster-browser-new-wallet *,#genycaster-browser-new-wallet *::before,#genycaster-browser-new-wallet *::after {box-sizing:border-box;}
 #genycaster-browser-new-wallet button,#genycaster-browser-new-wallet input {font-family:inherit;-webkit-tap-highlight-color:transparent;}
 #genycaster-browser-new-wallet .wallet-verification-label {display:grid;grid-template-columns:auto minmax(0,1fr);min-width:0;align-items:center;gap:10px;}
 #genycaster-browser-new-wallet .wallet-verification-label input {width:100%;min-width:0;max-width:100%;padding:14px 15px;border:1px solid var(--wallet-border);border-radius:14px;corner-shape:superellipse(4);background:var(--wallet-cell);color:var(--wallet-ink);font-size:16px;outline:none;transition:border-color .15s ease,background .15s ease,box-shadow .15s ease;}
 #genycaster-browser-new-wallet .wallet-verification-label input:hover {border-color:var(--wallet-muted);}
 #genycaster-browser-new-wallet .wallet-verification-label input:focus-visible {border-color:var(--wallet-focus);box-shadow:0 0 0 3px color-mix(in srgb,var(--wallet-focus) 22%,transparent);}
 #genycaster-browser-new-wallet .wallet-action {display:inline-flex;align-items:center;justify-content:center;gap:10px;min-width:0;min-height:50px;padding:12px 16px;border:1px solid var(--wallet-border);border-radius:17px;corner-shape:superellipse(4);background:var(--wallet-cell);color:var(--wallet-ink);font-size:15px;font-weight:650;line-height:1.35;cursor:pointer;transition:background .16s ease,border-color .16s ease,box-shadow .16s ease,transform .12s ease;}
 #genycaster-browser-new-wallet .wallet-action:hover:not(:disabled){border-color:var(--wallet-accent);background:color-mix(in srgb,var(--wallet-accent) 10%,var(--wallet-cell));}
 #genycaster-browser-new-wallet .wallet-action:active:not(:disabled){transform:scale(.985);}
 #genycaster-browser-new-wallet .wallet-action:focus-visible{outline:3px solid var(--wallet-focus);outline-offset:3px;}
 #genycaster-browser-new-wallet .wallet-action--primary{background:var(--wallet-accent);border-color:var(--wallet-accent);color:var(--wallet-accent-ink);}
 #genycaster-browser-new-wallet .wallet-action--primary:hover:not(:disabled){background:color-mix(in srgb,var(--wallet-accent) 86%,var(--wallet-ink));border-color:var(--wallet-focus);}
 #genycaster-browser-new-wallet .wallet-action:disabled{cursor:not-allowed;opacity:.52;box-shadow:none;}
 #genycaster-browser-new-wallet .wallet-action[data-busy="true"]{opacity:1;cursor:progress;}
 #genycaster-browser-new-wallet .wallet-action[data-busy="true"]::before{content:"";width:16px;height:16px;border:2px solid currentColor;border-inline-start-color:transparent;border-radius:50%;animation:walletSpin .75s linear infinite;}
 #genycaster-browser-new-wallet .wallet-step[data-state="active"]{background:color-mix(in srgb,var(--wallet-accent) 15%,var(--wallet-cell));border-color:var(--wallet-accent);color:var(--wallet-ink);}
 #genycaster-browser-new-wallet .wallet-step[data-state="done"]{background:color-mix(in srgb,#36ca91 12%,var(--wallet-cell));border-color:color-mix(in srgb,#36ca91 28%,var(--wallet-border));color:var(--wallet-ink);}
 #genycaster-browser-new-wallet .wallet-step[data-state="waiting"]{color:var(--wallet-muted);}
 #genycaster-browser-new-wallet .wallet-progress-spinner{width:18px;height:18px;flex:none;border-radius:50%;border:2px solid var(--wallet-border);border-top-color:var(--wallet-accent);animation:walletSpin .8s linear infinite;}
 #genycaster-browser-new-wallet .wallet-progress-status{display:flex;gap:12px;align-items:flex-start;}
 #genycaster-browser-new-wallet .wallet-progress-status p{flex:1;min-width:0;margin:0;}
 #genycaster-browser-new-wallet .wallet-footer{flex:none;display:flex;gap:12px;padding:14px 0 max(12px,env(safe-area-inset-bottom));border-top:1px solid var(--wallet-border);background:var(--wallet-bg);}
 #genycaster-browser-new-wallet .wallet-footer .wallet-action{flex:1;min-width:0;}
 #genycaster-browser-new-wallet .wallet-consent{padding:8px 10px;border-radius:13px;cursor:pointer;transition:background .15s ease;}
 #genycaster-browser-new-wallet .wallet-consent:hover{background:color-mix(in srgb,var(--wallet-accent) 8%,transparent);}
 #genycaster-browser-new-wallet .wallet-consent:focus-within{background:color-mix(in srgb,var(--wallet-accent) 12%,transparent);}
 #genycaster-browser-new-wallet input[type="checkbox"]{accent-color:var(--wallet-accent);cursor:pointer;}
 #genycaster-browser-new-wallet input[type="checkbox"]:focus-visible{outline:2px solid var(--wallet-focus);outline-offset:4px;}
 @media(max-width:560px){#genycaster-browser-new-wallet .wallet-challenge{grid-template-columns:minmax(0,1fr)!important;}#genycaster-browser-new-wallet .wallet-footer{gap:8px;}#genycaster-browser-new-wallet .wallet-action{padding:11px 12px;min-height:48px;font-size:14px;}}
 @media(prefers-reduced-motion:reduce){#genycaster-browser-new-wallet .wallet-action,#genycaster-browser-new-wallet .wallet-verification-label input{transition:none;}#genycaster-browser-new-wallet .wallet-action:active{transform:none;}#genycaster-browser-new-wallet .wallet-progress-spinner,#genycaster-browser-new-wallet .wallet-action[data-busy="true"]::before{animation:none;}}
 @keyframes walletSpin{to{transform:rotate(360deg);}}
 `;
 layer.appendChild(skin);
 form.dir=isRtl?'rtl':'ltr';form.setAttribute('role','dialog');form.setAttribute('aria-modal','true');
 form.setAttribute('aria-label',t.title);
 form.style.cssText='width:100%;max-width:820px;margin:auto;box-sizing:border-box;height:100%;display:flex;flex-direction:column;padding:max(18px,env(safe-area-inset-top)) clamp(16px,3vw,24px) 0;gap:14px;';
 const content=document.createElement('div');
 content.style.cssText='overflow-y:auto;min-height:0;flex:1;padding:0 2px;overscroll-behavior:contain;';
 const heading=document.createElement('h1');heading.textContent=t.title;
 heading.id='genycaster-wallet-dialog-title';
 heading.tabIndex=-1;
 form.setAttribute('aria-labelledby',heading.id);
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
 progressCaption.style.cssText='font-size:15px;line-height:1.6;color:'+colors.fg+';overflow-wrap:anywhere;';
 const progressStatus=document.createElement('div');
 progressStatus.className='wallet-progress-status';
 progressStatus.style.cssText='margin:0 0 16px;';
 const progressSpinner=document.createElement('span');
 progressSpinner.className='wallet-progress-spinner';
 progressSpinner.setAttribute('aria-hidden','true');
 progressStatus.append(progressSpinner,progressCaption);
 const progressTrack=document.createElement('div');progressTrack.style.cssText='display:none;height:6px;background:'+colors.border+';border-radius:9px;overflow:hidden;';
 const progressFill=document.createElement('div');progressFill.style.cssText='height:100%;width:0%;border-radius:9px;background:#008bff;transition:width 150ms linear;';
 progressTrack.appendChild(progressFill);
 const progressNote=document.createElement('p');progressNote.textContent=t.progressNote;
 progressNote.style.cssText='display:none;font-size:12px;line-height:1.6;color:'+colors.muted+';margin:10px 0 0';
 const stepList=document.createElement('ol');
 stepList.style.cssText='list-style:none;padding:0;margin:16px 0 0;display:grid;gap:11px;';
 const stepNames=[t.stepOne,protocol==='hypersnap-v2'?t.stepTwo:t.paidStepTwo,
   protocol==='hypersnap-v2'?t.stepThree:t.paidStepThree];
 const stepEntries=stepNames.map((name,index)=>{
   const item=document.createElement('li');
   item.textContent=(index+1)+'.  '+name;
   item.className='wallet-step';
   item.style.cssText='padding:12px 14px;border:1px solid transparent;border-radius:14px;corner-shape:superellipse(4);font-size:14px;line-height:1.5;font-weight:600;';
   stepList.appendChild(item);return item;
 });
 const showProgressStep=step=>{
   stepEntries.forEach((item,index)=>{
     const state=index===step?'active':index<step?'done':'waiting';
     item.dataset.state=state;
     if(index===step)item.setAttribute('aria-current','step');
     else item.removeAttribute?.('aria-current');
   });
 };
 progressPanel.append(progressHeading,progressStatus,progressTrack,progressNote,stepList);
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
 backupAction.className='wallet-action';
 backupAction.style.cssText='margin:16px 0;';
 const challengeHeading=document.createElement('h2');challengeHeading.textContent=t.challenge;
 challengeHeading.style.cssText='font-size:17px;margin:14px 0 12px;font-weight:600';
 const challenge=document.createElement('div');
 challenge.className='wallet-challenge';
 challenge.style.cssText='display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px';
 const checks=[];
 indices.forEach(index=>{
  const label=document.createElement('label');
  label.textContent='#'+String(index+1).padStart(2,'0');
  label.className='wallet-verification-label';
  const input=document.createElement('input');
  input.type='password';input.autocomplete='off';input.autocorrect='off';
  input.autocapitalize='off';input.spellcheck=false;
  input.setAttribute('aria-label',t.challenge+' '+(index+1));
  input.setAttribute('inputmode','text');
  input.setAttribute('enterkeyhint','next');
  label.appendChild(input);challenge.appendChild(label);checks.push(input);
 });
 const confirmation=(message)=>{
  const label=document.createElement('label');
  label.className='wallet-consent';
  label.style.cssText='display:flex;align-items:flex-start;gap:12px;margin:10px 0;line-height:1.6';
  const input=document.createElement('input');input.type='checkbox';
  input.style.cssText='width:22px;height:22px;flex-shrink:0;';
  const span=document.createElement('span');span.textContent=message;
  label.append(input,span);content.appendChild(label);
  return input;
 };
 const result=document.createElement('p');
 result.setAttribute('aria-live','polite');
 result.style.cssText='min-height:25px;line-height:1.6;color:'+(light?'#a4530c':'#ffbd71')+';overflow-wrap:anywhere';
 const buttons=document.createElement('div');
 buttons.className='wallet-footer';
 const button=(text,primary)=>{
  const b=document.createElement('button');b.type='button';b.textContent=text;
  b.className='wallet-action'+(primary?' wallet-action--primary':'');
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
 // The footer stays visible while users scroll through the 24-word form.
 form.append(content,buttons);layer.appendChild(form);
 const oldFocus=document.activeElement;
 const oldOverflow=document.body.style.overflow;
 document.body.style.overflow='hidden';document.body.appendChild(layer);
 heading.focus?.();
 let attempts=0,verifiedBackup=false;
 const phaseText=phases[lang];
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
  layer.addEventListener?.('keydown',event=>{
   if(event.key==='Escape' && !cancel.disabled){
    event.preventDefault();
    cancel.onclick();
   }else if(event.key==='Tab'){
    // Keep keyboard navigation inside the modal even if an unrelated Qt
    // canvas element previously had focus.
    const focusable=[...(form.querySelectorAll?.('button:not(:disabled),input:not(:disabled),a[href]')||[])]
      .filter(node=>node.getClientRects?.().length>0);
    if(!focusable.length)return;
    const first=focusable[0],last=focusable[focusable.length-1];
    if(event.shiftKey && (document.activeElement===first
       ||!form.contains(document.activeElement))){
      event.preventDefault();last.focus();
    }else if(!event.shiftKey && (document.activeElement===last
       ||!form.contains(document.activeElement))){
      event.preventDefault();first.focus();
    }
   }
  });
  backupAction.onclick=async()=>{
   try{
    await browser.navigator.clipboard.writeText(phrase);
    result.textContent=t.copied;
   }catch{result.textContent=t.warning;}
  };
  submit.onclick=async()=>{
   if(processing||settled)return;
   if(!verifiedBackup){
    const match=indices.every((index,i)=>checks[i].value.trim().toLowerCase()===words[index]);
    if(!match||!saved.checked||!consent.checked){
     attempts++;
     result.textContent=t.failed;
     if(attempts>=5)finish(new Error('Backup verification attempt limit reached'));
     return;
    }
    verifiedBackup=true;
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
   progressPanel.setAttribute('aria-busy','true');
   showProgressStep(1);
   content.scrollTop=0;
   processing=true;submit.disabled=true;cancel.disabled=false;
   progressSpinner.style.display='';
   submit.dataset.busy='true';
   submit.textContent=t.busy;
   progressCaption.textContent=t.busy;
   result.textContent='';
   let shownPhase='';
   const update=(done,total,phase)=>{
    if(settled)return;
    if(phase==='submitting'||phase==='checking'||phase==='outcome-unknown'
       ||(protocol==='farcaster-snapchain'
          && ['reserve','submit','verify'].includes(phase))) {
      // Paid reservation and HyperSnap submit can both be irreversible.
      // Never enable cancellation/retry after crossing either boundary.
      submissionStarted=true;
      if(phase!==shownPhase){
       showProgressStep(phase==='submitting'||phase==='reserve'?1:2);
       progressCaption.textContent=phase==='reserve'?t.paidReserve
         :phase==='submitting'||phase==='submit'
           ? (protocol==='hypersnap-v2'?t.submitting:t.paidSubmit)
         :phase==='checking'||phase==='verify'
           ? (protocol==='hypersnap-v2'?t.checking:t.paidVerify)
           :t.pending;
       progressNote.textContent=t.chainPending;
       progressNote.style.display='block';
      }
      phrase=''; // Only the registry's ephemeral in-flight custody owns it now.
      cancel.disabled=true;
      progressTrack.style.display='none';
    }else if(protocol==='farcaster-snapchain'){
      if(phase!==shownPhase)progressCaption.textContent=phase==='sign'
        ?t.paidSign:t.busy;
      progressTrack.style.display='none';
      progressNote.style.display='none';
    }else if(phase!==shownPhase){
      // PoW is a random search, NOT a deterministic percentage of eventual
      // success. Stage labels are updated only when the actual operation
      // changes, not once for every 256 hash attempts.
      progressCaption.textContent=phaseText[phase]||t.progress;
      progressTrack.style.display='none';
      progressNote.textContent=t.progressNote;
      progressNote.style.display=phase==='proof'?'block':'none';
    }
    // Show verifiable activity without presenting a probability-based proof
    // search as a falsely deterministic progress bar. Throttle DOM updates.
    if(phase==='proof' && done>0 && done%131072===0){
      const digits=new Intl.NumberFormat(lang);
      progressNote.textContent=t.progressNote+' · '
        +digits.format(done)+' / '+digits.format(total);
    }
    shownPhase=phase;
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
    if(operationController.signal.aborted){
      finish(error); // Explicit cancellation is only allowed before POST.
      return;
    }
    processing=false;
    submit.dataset.busy='false';
    progressSpinner.style.display='none';
    progressPanel.setAttribute('aria-busy','false');
    progressTrack.style.display='none';
    progressNote.style.display='none';
    if(error.outcomeUnknown){
      progressCaption.textContent=t.pending;
      result.textContent='';
      submit.disabled=true;cancel.disabled=false;
      cancel.textContent=t.close;
      cancel.onclick=()=>finish(error); // Never retry an ambiguous write.
    }else if(submissionStarted){
      // Even a known rejection has crossed the write boundary. Explain it
      // before closing rather than recreating or silently resending a FID.
      progressCaption.textContent=String(error?.message||t.pending).slice(0,180);
      submit.disabled=true;cancel.disabled=false;
      cancel.textContent=t.close;
      cancel.onclick=()=>finish(error);
    }else{
      // Before the first POST it is safe to retry the SAME backed-up wallet
      // with a fresh network challenge, without asking for 24 words again.
      progressCaption.textContent=phaseText.notSubmitted;
      result.textContent=String(error?.message||'').slice(0,180);
      submit.disabled=false;submit.textContent=phaseText.retry;
      cancel.disabled=false;
    }
   }
  };
 });
}
