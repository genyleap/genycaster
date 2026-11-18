"""Ensure the first-party Mini App Swap preflight has 12-language Qt TS IDs.

Idempotent source-catalog update, not a substitute for the lupdate/lrelease
checks performed by CI. Product names, token addresses and amounts stay literal.
"""
from html import escape
import re
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent / "ui" / "i18n"

SOURCE = {
    "title": "Token swap requested",
    "network_base": "Network: Base",
    "sell_token": "Sell token",
    "buy_token": "Buy token",
    "amount_base_units": "Amount (base units)",
    "quote_unavailable":
        "A trusted swap quote and execution service is not available yet. "
        "No wallet transaction has been created, signed, or sent.",
}

LOCALIZED = {
    "en_US": [
        "Token swap requested", "Network: Base", "Sell token", "Buy token",
        "Amount (base units)",
        "A trusted swap quote and execution service is not available yet. "
        "No wallet transaction has been created, signed, or sent."],
    "fa_IR": [
        "درخواست مبادلهٔ توکن", "شبکه: بیس", "توکن فروش", "توکن خرید",
        "مقدار (واحد پایه)",
        "سرویس معتبر دریافت مظنه و اجرای مبادله هنوز در دسترس نیست. "
        "هیچ تراکنشی ساخته، امضا یا ارسال نشده است."],
    "ar_SA": [
        "طلب مبادلة الرموز", "الشبكة: بيس", "رمز البيع", "رمز الشراء",
        "المبلغ (بالوحدات الأساسية)",
        "خدمة عروض الأسعار وتنفيذ المبادلات الموثوقة غير متاحة بعد. "
        "لم يتم إنشاء أي معاملة في المحفظة أو توقيعها أو إرسالها."],
    "de_DE": [
        "Token-Tausch angefordert", "Netzwerk: Base", "Token verkaufen",
        "Token kaufen", "Betrag (Basiseinheiten)",
        "Ein vertrauenswürdiger Kurs- und Ausführungsdienst für Token-Tausche "
        "ist noch nicht verfügbar. Es wurde keine Wallet-Transaktion erstellt, "
        "signiert oder gesendet."],
    "es_ES": [
        "Intercambio de tokens solicitado", "Red: Base", "Token a vender",
        "Token a comprar", "Importe (unidades base)",
        "El servicio fiable de cotización y ejecución de intercambios aún "
        "no está disponible. No se ha creado, firmado ni enviado ninguna "
        "transacción de la cartera."],
    "fr_FR": [
        "Échange de jetons demandé", "Réseau : Base", "Jeton à vendre",
        "Jeton à acheter", "Montant (unités de base)",
        "Aucun service fiable de devis et d'exécution des échanges n'est encore "
        "disponible. Aucune transaction de portefeuille n'a été créée, signée "
        "ou envoyée."],
    "hi_IN": [
        "टोकन स्वैप का अनुरोध", "नेटवर्क: Base", "बेचने वाला टोकन",
        "खरीदने वाला टोकन", "राशि (मूल इकाइयों में)",
        "विश्वसनीय स्वैप कोटेशन और निष्पादन सेवा अभी उपलब्ध नहीं है। "
        "वॉलेट का कोई लेनदेन बनाया, हस्ताक्षरित या भेजा नहीं गया है।"],
    "ja_JP": [
        "トークンスワップのリクエスト", "ネットワーク: Base",
        "売却トークン", "購入トークン", "数量（最小単位）",
        "信頼できるスワップ見積もり・実行サービスはまだ利用できません。"
        "ウォレット取引の作成、署名、送信は行われていません。"],
    "ko_KR": [
        "토큰 스왑 요청", "네트워크: Base", "판매할 토큰", "구매할 토큰",
        "금액(최소 단위)", "신뢰할 수 있는 스왑 견적 및 실행 서비스가 아직 "
        "제공되지 않습니다. 지갑 거래는 생성, 서명 또는 전송되지 않았습니다."],
    "pt_BR": [
        "Troca de tokens solicitada", "Rede: Base", "Token para vender",
        "Token para comprar", "Valor (unidades básicas)",
        "O serviço confiável de cotação e execução de trocas ainda não "
        "está disponível. Nenhuma transação da carteira foi criada, "
        "assinada ou enviada."],
    "ru_RU": [
        "Запрос на обмен токенов", "Сеть: Base", "Продать токен",
        "Купить токен", "Сумма (минимальные единицы)",
        "Надёжный сервис котировок и исполнения обменов пока недоступен. "
        "Транзакция кошелька не создавалась, не подписывалась и не отправлялась."],
    "zh_CN": [
        "请求兑换代币", "网络：Base", "卖出代币", "买入代币",
        "数量（最小单位）", "可信的兑换报价和执行服务尚不可用。"
        "尚未创建、签名或发送任何钱包交易。"],
}

SOURCE.update({
    "get_quote": "Get live quote",
    "min_receive": "Minimum receive (base units)",
    "network_fee": "Estimated network fee (wei)",
    "approval_required": "Exact ERC-20 token approval will be required to execute.",
    "approval_not_required": "No ERC-20 approval is required.",
    "quote_notice": "The quote is read-only. Wallet approvals and token transfers are disabled until the secure execution flow is verified.",
})
EXTRA_LOCALIZED = {
    "en_US": ["Get live quote","Minimum receive (base units)","Estimated network fee (wei)",
        "Exact ERC-20 token approval will be required to execute.",
        "No ERC-20 approval is required.",
        "The quote is read-only. Wallet approvals and token transfers are disabled until the secure execution flow is verified."],
    "fa_IR": ["دریافت مظنهٔ زنده","حداقل دریافتی (واحد پایه)","کارمزد تخمینی شبکه (وی)",
        "برای اجرای مبادله، مجوز دقیق ERC-20 لازم خواهد بود.",
        "نیازی به مجوز ERC-20 نیست.",
        "این مظنه فقط برای مشاهده است. تا تأیید کامل مسیر اجرای امن، مجوز کیف پول و انتقال توکن غیرفعال است."],
    "ar_SA": ["الحصول على عرض سعر مباشر","الحد الأدنى للاستلام (وحدات أساسية)","رسوم الشبكة المقدرة (وي)",
        "سيتطلب التنفيذ موافقة محددة لرمز ERC-20.",
        "لا حاجة إلى موافقة ERC-20.",
        "عرض السعر للعرض فقط. تم تعطيل موافقات المحفظة وتحويلات الرموز إلى حين التحقق من مسار التنفيذ الآمن."],
    "de_DE": ["Live-Angebot abrufen","Mindesterhalt (Basiseinheiten)","Geschätzte Netzwerkgebühr (Wei)",
        "Zur Ausführung ist eine genaue ERC-20-Freigabe erforderlich.",
        "Keine ERC-20-Freigabe erforderlich.",
        "Dieses Angebot dient nur zur Ansicht. Wallet-Freigaben und Token-Übertragungen bleiben bis zur Prüfung der sicheren Ausführung deaktiviert."],
    "es_ES": ["Obtener cotización en vivo","Recepción mínima (unidades base)","Tarifa de red estimada (wei)",
        "Se necesitará una aprobación ERC-20 limitada para ejecutar.",
        "No se necesita aprobación ERC-20.",
        "La cotización es solo informativa. Las aprobaciones y transferencias están deshabilitadas hasta verificar la ejecución segura."],
    "fr_FR": ["Obtenir un devis en direct","Réception minimale (unités de base)","Frais réseau estimés (wei)",
        "Une autorisation ERC-20 limitée sera nécessaire pour exécuter.",
        "Aucune autorisation ERC-20 n'est nécessaire.",
        "Le devis est uniquement indicatif. Les autorisations et transferts restent désactivés jusqu'à validation de l'exécution sécurisée."],
    "hi_IN": ["लाइव कोट प्राप्त करें","न्यूनतम प्राप्ति (मूल इकाइयाँ)","अनुमानित नेटवर्क शुल्क (वेई)",
        "निष्पादन के लिए सीमित ERC-20 अनुमति आवश्यक होगी।",
        "ERC-20 अनुमति आवश्यक नहीं है।",
        "कोट केवल जानकारी के लिए है। सुरक्षित निष्पादन सत्यापित होने तक वॉलेट अनुमतियाँ और टोकन स्थानांतरण बंद हैं।"],
    "ja_JP": ["リアルタイム見積もりを取得","最低受取額（最小単位）","推定ネットワーク手数料（wei）",
        "実行には指定数量のERC-20承認が必要です。",
        "ERC-20承認は不要です。",
        "見積もりは閲覧専用です。安全な実行手順の検証が終わるまで承認と送金は無効です。"],
    "ko_KR": ["실시간 견적 받기","최소 수령액(최소 단위)","예상 네트워크 수수료(wei)",
        "실행하려면 정확한 ERC-20 승인이 필요합니다.",
        "ERC-20 승인이 필요하지 않습니다.",
        "견적은 읽기 전용입니다. 안전한 실행 경로 검증 전까지 지갑 승인과 토큰 전송이 비활성화됩니다."],
    "pt_BR": ["Obter cotação ao vivo","Recebimento mínimo (unidades básicas)","Taxa de rede estimada (wei)",
        "A execução exigirá uma aprovação ERC-20 limitada.",
        "Nenhuma aprovação ERC-20 é necessária.",
        "A cotação é somente informativa. Aprovações e transferências estão desativadas até a validação da execução segura."],
    "ru_RU": ["Получить актуальную котировку","Минимум к получению (базовые единицы)","Ожидаемая комиссия сети (wei)",
        "Для исполнения потребуется точное разрешение ERC-20.",
        "Разрешение ERC-20 не требуется.",
        "Котировка доступна только для просмотра. Разрешения и переводы отключены до проверки безопасного исполнения."],
    "zh_CN": ["获取实时报价","最低到账量（最小单位）","预估网络费用（wei）",
        "执行交易前需授权精确数量的 ERC-20。",
        "无需 ERC-20 授权。",
        "报价仅供查看。在安全执行流程通过验证前，钱包授权和代币转账均已禁用。"],
}
SOURCE.pop("quote_unavailable")
for locale, labels in EXTRA_LOCALIZED.items():
    LOCALIZED[locale].pop()  # Superseded by the more accurate read-only quote notice.
    LOCALIZED[locale].extend(labels)


# Execution UI is a separate, explicit user gesture after every on-chain
# approval. State labels must not imply a submitted transaction is reversible.
EXECUTION_SOURCE = {
    "approve_exact": "Approve exact token amount",
    "confirm_swap": "Confirm token swap",
    "wallet_wait": "Waiting for wallet confirmation…",
    "approval_receipt_wait": "Waiting for Base token approval receipt…",
    "swap_receipt_wait": "Verifying Base swap receipt…",
    "quote_refreshing": "Refreshing quote after token approval…",
    "quote_loading": "Retrieving live quote…",
    "execution_notice": "Review each transaction in your wallet. Your swap is complete only after Base confirms it.",
}
EXECUTION_TRANSLATIONS = {
    "en_US": ["Approve exact token amount","Confirm token swap","Waiting for wallet confirmation…","Waiting for Base token approval receipt…","Verifying Base swap receipt…","Refreshing quote after token approval…","Retrieving live quote…","Review each transaction in your wallet. Your swap is complete only after Base confirms it."],
    "fa_IR": ["تأیید مجوز مقدار دقیق توکن","تأیید مبادلهٔ توکن","در انتظار تأیید کیف پول…","در انتظار تأیید مجوز توکن در شبکهٔ Base…","بررسی تأیید مبادله در Base…","دریافت مجدد مظنه پس از مجوز توکن…","دریافت مظنهٔ زنده…","هر تراکنش را در کیف پول بررسی و تأیید کنید. مبادله تنها پس از تأیید شبکهٔ Base نهایی می‌شود."],
    "ar_SA": ["الموافقة على مقدار الرمز المحدد","تأكيد مبادلة الرموز","بانتظار تأكيد المحفظة…","بانتظار تأكيد موافقة الرمز على Base…","التحقق من إيصال مبادلة Base…","تحديث عرض السعر بعد الموافقة…","جلب عرض سعر مباشر…","راجع كل معاملة في محفظتك. لا تكتمل المبادلة إلا بعد تأكيد شبكة Base."],
    "de_DE": ["Exakten Tokenbetrag freigeben","Token-Tausch bestätigen","Auf Wallet-Bestätigung warten…","Auf Bestätigung der Base-Tokenfreigabe warten…","Base-Transaktion wird überprüft…","Kurs nach der Freigabe aktualisieren…","Live-Angebot wird abgerufen…","Prüfen Sie jede Transaktion in Ihrer Wallet. Der Tausch ist erst nach der Bestätigung durch Base abgeschlossen."],
    "es_ES": ["Aprobar cantidad exacta de tokens","Confirmar intercambio","Esperando confirmación de la cartera…","Esperando confirmación de aprobación en Base…","Verificando el recibo de intercambio en Base…","Actualizando cotización tras la aprobación…","Obteniendo cotización en vivo…","Revise cada transacción en su cartera. El intercambio se completa solo tras la confirmación de Base."],
    "fr_FR": ["Autoriser le montant exact des jetons","Confirmer l'échange","En attente de confirmation du portefeuille…","En attente du reçu d'autorisation sur Base…","Vérification du reçu d'échange sur Base…","Actualisation du devis après autorisation…","Récupération du devis en direct…","Vérifiez chaque transaction dans votre portefeuille. L'échange n'est terminé qu'après confirmation par Base."],
    "hi_IN": ["टोकन की सटीक मात्रा अनुमोदित करें","टोकन स्वैप की पुष्टि करें","वॉलेट पुष्टि की प्रतीक्षा…","Base टोकन अनुमति की पुष्टि की प्रतीक्षा…","Base स्वैप रसीद की जाँच…","अनुमति के बाद नया कोट प्राप्त किया जा रहा है…","लाइव कोट प्राप्त किया जा रहा है…","प्रत्येक लेनदेन की वॉलेट में समीक्षा करें। Base द्वारा पुष्टि के बाद ही स्वैप पूरा होता है।"],
    "ja_JP": ["正確なトークン数量を承認","トークンスワップを確認","ウォレットの確認待ち…","Base上のトークン承認確認待ち…","Baseのスワップ取引確認中…","承認後の見積もりを更新中…","最新の見積もりを取得中…","各取引をウォレットで確認してください。スワップはBaseの承認後に完了します。"],
    "ko_KR": ["정확한 토큰 수량 승인","토큰 스왑 확인","지갑 확인 대기 중…","Base 토큰 승인 내역 대기 중…","Base 스왑 영수증 확인 중…","승인 후 견적 갱신 중…","실시간 견적 조회 중…","지갑에서 각 거래를 확인하세요. Base 네트워크의 승인이 있어야 스왑이 완료됩니다."],
    "pt_BR": ["Aprovar quantidade exata de tokens","Confirmar troca de tokens","Aguardando confirmação da carteira…","Aguardando recibo de aprovação em Base…","Verificando recibo da troca em Base…","Atualizando cotação após aprovação…","Obtendo cotação em tempo real…","Revise cada transação na carteira. A troca só termina após confirmação da rede Base."],
    "ru_RU": ["Разрешить точное количество токенов","Подтвердить обмен","Ожидание подтверждения кошелька…","Ожидание подтверждения разрешения в Base…","Проверка квитанции обмена в Base…","Обновление котировки после разрешения…","Получение актуальной котировки…","Проверьте каждую транзакцию в кошельке. Обмен завершится только после подтверждения сетью Base."],
    "zh_CN": ["授权精确数量的代币","确认代币兑换","等待钱包确认…","等待 Base 代币授权交易确认…","验证 Base 兑换交易收据…","授权后刷新兑换报价…","正在获取实时兑换报价…","请在钱包中逐笔确认交易。仅在 Base 网络确认后，兑换才算完成。"],
}
SOURCE.pop("quote_notice")
SOURCE.update(EXECUTION_SOURCE)
for locale, translation in EXECUTION_TRANSLATIONS.items():
    LOCALIZED[locale].pop()  # Remove obsolete read-only notice.
    LOCALIZED[locale].extend(translation)

SOURCE["max_slippage"] = "Maximum slippage (basis points: 10–300)"
SLIPPAGE_LABELS = {
    "en_US": "Maximum slippage (basis points: 10–300)",
    "fa_IR": "حداکثر لغزش قیمت (واحد پایهٔ درصد: ۱۰ تا ۳۰۰)",
    "ar_SA": "الانزلاق الأقصى (نقاط أساس: ١٠–٣٠٠)",
    "de_DE": "Maximale Slippage (Basispunkte: 10–300)",
    "es_ES": "Deslizamiento máximo (puntos básicos: 10–300)",
    "fr_FR": "Glissement maximal (points de base : 10–300)",
    "hi_IN": "अधिकतम स्लिपेज (आधार अंक: 10–300)",
    "ja_JP": "最大スリッページ（ベーシスポイント：10～300）",
    "ko_KR": "최대 슬리피지(베이시스 포인트: 10–300)",
    "pt_BR": "Slippage máximo (pontos-base: 10–300)",
    "ru_RU": "Максимальное проскальзывание (базисные пункты: 10–300)",
    "zh_CN": "最大滑点（基点：10–300）",
}
for locale, label in SLIPPAGE_LABELS.items():
    LOCALIZED[locale].append(label)

for locale, translations in LOCALIZED.items():
    path = BASE / f"qml_{locale}.ts"
    original_source = path.read_text(encoding="utf-8")
    source = re.sub(
        r'        <message id="genycaster\.miniapp\.swap\.quote_unavailable">.*?</message>\n',
        '', original_source, flags=re.S)
    source = re.sub(
        r'        <message id="genycaster\.miniapp\.swap\.quote_notice">.*?</message>\n',
        '', source, flags=re.S)
    assert len(translations) == len(SOURCE)
    assert "    <context>\n" in source, path
    blocks = []
    for (identifier, original), translation in zip(SOURCE.items(), translations):
        key = f"genycaster.miniapp.swap.{identifier}"
        if f'<message id="{key}">' in source:
            continue
        blocks.append(
            f'        <message id="{key}">\n'
            f'            <source>{escape(original)}</source>\n'
            f'            <translation>{escape(translation)}</translation>\n'
            '        </message>\n'
        )
    if blocks:
        source = source.replace("    <context>\n",
                                "    <context>\n" + "".join(blocks), 1)
    if source != original_source:
        path.write_text(source, encoding="utf-8")
    print(f"{locale}: {len(blocks)} added")
