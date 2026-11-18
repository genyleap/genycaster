"""Ensure the first-party Mini App Swap preflight has 12-language Qt TS IDs.

Idempotent source-catalog update, not a substitute for the lupdate/lrelease
checks performed by CI. Product names, token addresses and amounts stay literal.
"""
from html import escape
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

for locale, translations in LOCALIZED.items():
    path = BASE / f"qml_{locale}.ts"
    source = path.read_text(encoding="utf-8")
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
        path.write_text(source, encoding="utf-8")
    print(f"{locale}: {len(blocks)} added")
