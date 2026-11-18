"""First-party Mini App SIWF wallet-verification guidance, every supported locale.

This text never asserts that a wallet signature is valid or skips protocol
verification. It only explains why a linked Farcaster wallet is required.
"""
from html import escape
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "ui" / "i18n"
SOURCE = (
    ("signin_wallet_title", "Mini App sign-in could not be verified"),
    ("signin_wallet_description",
     "Connect the Farcaster custody wallet or an authorized Farcaster auth-address wallet "
     "for your current FID, then try Sign in again. A separate EVM wallet cannot sign in "
     "as this Farcaster account. If the wallet is already linked, check network access "
     "and try again."),
)
TRANSLATIONS = {
    "en_US": (
        "Mini App sign-in could not be verified",
        "Connect the Farcaster custody wallet or an authorized Farcaster auth-address wallet for your current FID, then try Sign in again. A separate EVM wallet cannot sign in as this Farcaster account. If the wallet is already linked, check network access and try again."),
    "fa_IR": (
        "ورود به مینی‌اپ تأیید نشد",
        "کیف پول حضانتی فارکستر یا کیف پول آدرس احراز هویت مجاز مربوط به FID فعلی را متصل کنید و دوباره ورود را امتحان کنید. کیف پول EVM نامرتبط نمی‌تواند به‌جای این حساب فارکستر امضا کند. اگر کیف پول از قبل متصل است، اتصال شبکه را بررسی و دوباره تلاش کنید."),
    "ar_SA": (
        "تعذّر التحقق من تسجيل دخول التطبيق المصغّر",
        "اربط محفظة الحفظ الخاصة بفاركاستر أو محفظة عنوان مصادقة معتمدة لمعرّف FID الحالي، ثم أعد تسجيل الدخول. لا تستطيع محفظة EVM غير المرتبطة التوقيع نيابة عن حساب فاركاستر هذا. إذا كانت المحفظة مرتبطة، فتحقق من الاتصال بالشبكة وحاول مجددًا."),
    "de_DE": (
        "Mini-App-Anmeldung konnte nicht verifiziert werden",
        "Verbinden Sie die Farcaster-Custody-Wallet oder eine autorisierte Farcaster-Auth-Address-Wallet Ihrer aktuellen FID und versuchen Sie es erneut. Eine unabhängige EVM-Wallet kann diese Farcaster-Identität nicht bestätigen. Falls die Wallet bereits verknüpft ist, prüfen Sie Ihre Netzwerkverbindung."),
    "es_ES": (
        "No se pudo verificar el inicio de sesión de la miniaplicación",
        "Conecta la cartera de custodia de Farcaster o una cartera de dirección de autenticación autorizada para tu FID actual e inténtalo otra vez. Una cartera EVM no vinculada no puede firmar por esta cuenta de Farcaster. Si ya está vinculada, comprueba la conexión de red."),
    "fr_FR": (
        "Impossible de vérifier la connexion à la mini-application",
        "Connectez le portefeuille de garde Farcaster ou un portefeuille d'adresse d'authentification autorisé pour votre FID actuel, puis réessayez. Un portefeuille EVM sans lien ne peut pas signer pour ce compte Farcaster. Si le portefeuille est déjà lié, vérifiez la connexion réseau."),
    "hi_IN": (
        "मिनी ऐप साइन-इन सत्यापित नहीं हो सका",
        "अपने वर्तमान FID का Farcaster कस्टडी वॉलेट या अधिकृत Farcaster ऑथ-एड्रेस वॉलेट कनेक्ट करके दोबारा साइन इन करें। असंबद्ध EVM वॉलेट इस Farcaster खाते की ओर से हस्ताक्षर नहीं कर सकता। यदि वॉलेट पहले से लिंक है, तो नेटवर्क कनेक्शन जाँचें।"),
    "ja_JP": (
        "ミニアプリのサインインを確認できませんでした",
        "現在のFIDに対応するFarcasterのカストディウォレットまたは認可された認証アドレスのウォレットを接続して再試行してください。無関係なEVMウォレットではこのアカウントを認証できません。既に連携済みの場合はネットワーク接続を確認してください。"),
    "ko_KR": (
        "미니앱 로그인을 확인할 수 없습니다",
        "현재 FID의 Farcaster 커스터디 지갑 또는 승인된 인증 주소 지갑을 연결하고 다시 로그인하세요. 연결되지 않은 EVM 지갑으로는 이 Farcaster 계정을 인증할 수 없습니다. 이미 연결한 지갑이라면 네트워크 연결을 확인하세요."),
    "pt_BR": (
        "Não foi possível verificar o login do miniapp",
        "Conecte a carteira de custódia Farcaster ou uma carteira de endereço de autenticação autorizado para seu FID atual e tente novamente. Uma carteira EVM não vinculada não pode assinar por esta conta Farcaster. Se já estiver vinculada, verifique a rede."),
    "ru_RU": (
        "Не удалось проверить вход в мини-приложение",
        "Подключите кошелёк хранения Farcaster или кошелёк с авторизованным адресом аутентификации для текущего FID и повторите вход. Посторонний EVM-кошелёк не может подписывать за эту учётную запись. Если кошелёк уже привязан, проверьте сеть."),
    "zh_CN": (
        "无法验证小程序登录",
        "请连接当前 FID 对应的 Farcaster 托管钱包或已授权的身份验证地址钱包，然后重试登录。无关联的 EVM 钱包不能代表此 Farcaster 账户签名。如果钱包已关联，请检查网络连接后重试。"),
}
for locale, translations in TRANSLATIONS.items():
    assert len(translations) == len(SOURCE)
    path = ROOT / f"qml_{locale}.ts"
    old = path.read_text(encoding="utf-8")
    blocks = []
    for (key, source), translation in zip(SOURCE, translations):
        ident = f"genycaster.miniapp.{key}"
        if f'<message id="{ident}">' in old:
            continue
        blocks.append(
            f'        <message id="{ident}">\n'
            f'            <source>{escape(source)}</source>\n'
            f'            <translation>{escape(translation)}</translation>\n'
            f'        </message>\n'
        )
    if blocks:
        path.write_text(old.replace("    <context>\n", "    <context>\n" + "".join(blocks), 1), encoding="utf-8")
    print(locale, len(blocks), 'new ids')
