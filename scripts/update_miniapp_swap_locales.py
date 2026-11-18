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

for locale, translations in LOCALIZED.items():
    path = BASE / f"qml_{locale}.ts"
    original_source = path.read_text(encoding="utf-8")
    source = re.sub(
        r'        <message id="genycaster\.miniapp\.swap\.quote_unavailable">.*?</message>\n',
        '', original_source, flags=re.S)
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
