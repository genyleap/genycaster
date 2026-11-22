#!/usr/bin/env python3
"""Insert original Genycaster Sticker translations into all release locales.

Safe to run repeatedly: existing IDs are not changed.
"""
from pathlib import Path
from xml.sax.saxutils import escape

ROOT=Path(__file__).resolve().parents[1]/"ui/i18n"

EN = [
    ("title", "Stickers"),
    ("subtitle", "Add a sticker to your cast"),
    ("search", "Search stickers"),
    ("all", "All"),
    ("originals", "Originals"),
    ("reactions", "Reactions"),
    ("web3", "Web3"),
    ("favorites", "Favorites"),
    ("recent", "Recent"),
    ("add", "Add sticker"),
    ("add_favorite", "Add to favorites"),
    ("remove_favorite", "Remove from favorites"),
    ("empty", "No stickers found"),
    ("media_slot", "Stickers use one media slot in your cast."),
    ("limit", "Media limit reached. Remove an attachment to add a sticker."),
]
LOCALES={
    "fa_IR":[
        "استیکرها","یک استیکر به پست خود اضافه کنید","جست‌وجوی استیکر","همه",
        "اصلی","واکنش‌ها","وب۳","علاقه‌مندی‌ها","اخیر",
        "افزودن استیکر","افزودن به علاقه‌مندی‌ها","حذف از علاقه‌مندی‌ها",
        "استیکری پیدا نشد","هر استیکر یک جایگاه رسانه از پست را اشغال می‌کند.",
        "ظرفیت رسانه تکمیل شده است؛ برای افزودن استیکر، یک پیوست را حذف کنید."],
    "ar_SA":[
        "ملصقات","أضف ملصقًا إلى منشورك","بحث عن ملصقات","الكل",
        "الأصلية","التفاعلات","ويب 3","المفضلة","الأخيرة",
        "إضافة ملصق","إضافة إلى المفضلة","إزالة من المفضلة",
        "لم يُعثر على ملصقات","يستخدم كل ملصق مكان مرفق وسائط واحد في المنشور.",
        "وصلت إلى حد الوسائط؛ احذف مرفقًا لإضافة ملصق."],
    "de_DE":[
        "Sticker","Sticker zum Cast hinzufügen","Sticker suchen","Alle",
        "Originale","Reaktionen","Web3-Kultur","Favoriten","Zuletzt",
        "Sticker hinzufügen","Zu Favoriten hinzufügen","Aus Favoriten entfernen",
        "Keine Sticker gefunden","Sticker belegen einen Medienplatz im Cast.",
        "Medienlimit erreicht. Entferne einen Anhang, um einen Sticker hinzuzufügen."],
    "es_ES":[
        "Pegatinas","Añade un sticker al cast","Buscar stickers","Todos",
        "Originales","Reacciones","Cultura Web3","Favoritos","Recientes",
        "Añadir sticker","Añadir a favoritos","Quitar de favoritos",
        "No se encontraron stickers","Cada sticker ocupa un espacio de medios en tu cast.",
        "Se alcanzó el límite de medios. Elimina un adjunto para añadir un sticker."],
    "fr_FR":[
        "Autocollants","Ajouter un sticker au cast","Rechercher des stickers","Tout",
        "Originaux","Réactions","Culture Web3","Favoris","Récents",
        "Ajouter le sticker","Ajouter aux favoris","Retirer des favoris",
        "Aucun sticker trouvé","Chaque sticker utilise une place média dans le cast.",
        "Limite de médias atteinte. Supprimez une pièce jointe pour ajouter un sticker."],
    "hi_IN":[
        "स्टिकर","अपनी पोस्ट में स्टिकर जोड़ें","स्टिकर खोजें","सभी",
        "मूल","प्रतिक्रियाएँ","वेब3","पसंदीदा","हाल में",
        "स्टिकर जोड़ें","पसंदीदा में जोड़ें","पसंदीदा से हटाएँ",
        "कोई स्टिकर नहीं मिला","हर स्टिकर पोस्ट में एक मीडिया स्थान लेता है।",
        "मीडिया सीमा पूरी हो गई है। स्टिकर जोड़ने के लिए एक अटैचमेंट हटाएँ।"],
    "ja_JP":[
        "ステッカー","キャストにステッカーを追加","ステッカーを検索","すべて",
        "オリジナル","リアクション","Web3文化","お気に入り","最近",
        "ステッカーを追加","お気に入りに追加","お気に入りから削除",
        "ステッカーが見つかりません","ステッカーはキャストのメディア枠を1つ使用します。",
        "メディアの上限に達しました。添付を削除してから追加してください。"],
    "ko_KR":[
        "스티커","캐스트에 스티커 추가","스티커 검색","전체",
        "오리지널","반응","웹3","즐겨찾기","최근",
        "스티커 추가","즐겨찾기에 추가","즐겨찾기에서 제거",
        "스티커를 찾을 수 없습니다","스티커는 캐스트의 미디어 슬롯 하나를 차지합니다.",
        "미디어 한도에 도달했습니다. 첨부 파일을 삭제한 후 추가하세요."],
    "pt_BR":[
        "Figurinhas","Adicione uma figurinha ao cast","Buscar figurinhas","Todas",
        "Originais","Reações","Cultura Web3","Favoritas","Recentes",
        "Adicionar figurinha","Adicionar aos favoritos","Remover dos favoritos",
        "Nenhuma figurinha encontrada","Cada figurinha ocupa uma vaga de mídia no cast.",
        "Limite de mídia atingido. Remova um anexo para adicionar uma figurinha."],
    "ru_RU":[
        "Стикеры","Добавить стикер к касту","Поиск стикеров","Все",
        "Оригинальные","Реакции","Культура Web3","Избранное","Недавние",
        "Добавить стикер","В избранное","Убрать из избранного",
        "Стикеры не найдены","Стикер занимает один медиа-слот в касте.",
        "Достигнут лимит медиа. Удалите вложение, чтобы добавить стикер."],
    "zh_CN":[
        "贴纸","为动态添加贴纸","搜索贴纸","全部",
        "原创","反应","Web3文化","收藏","最近",
        "添加贴纸","加入收藏","移除收藏",
        "未找到贴纸","贴纸将占用动态中的一个媒体名额。",
        "已达到媒体数量上限，请先移除一个附件。"],
}

def main():
    assert len(EN)==15
    assert len(LOCALES)==11
    for locale in ["en_US"]+list(LOCALES):
        path=ROOT/f"qml_{locale}.ts"
        text=path.read_text(encoding="utf-8")
        anchor='        <message id="genycaster.composer.emoji">'
        assert anchor in text
        values = [source for _,source in EN] if locale=="en_US" else LOCALES[locale]
        assert len(values)==len(EN)
        new=[]
        for (key,source),translation in zip(EN,values):
            msg=f'genycaster.stickers.{key}'
            if f'<message id="{msg}">' in text:
                continue
            new.append('        <message id="'+msg+'">\n'
                +'            <source>'+escape(source)+'</source>\n'
                +'            <translation>'+escape(translation)+'</translation>\n'
                +'        </message>\n')
        text=text.replace(anchor,"".join(new)+anchor,1)
        path.write_text(text,encoding="utf-8")
        print(locale,len(new))

if __name__=="__main__":
    main()
