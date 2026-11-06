#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
errors: list[str] = []


def read(rel: str) -> str:
    return (root / rel).read_text(encoding="utf-8")


cmake = read("CMakeLists.txt")
transport = read("src/transport/qt_network/media_upload.cppm") + read("src/transport/qt_network/media_upload.cpp")
application = (read("src/application/media_upload.cppm") + read("src/application/media_upload.cpp") +
               read("src/application/media_lifecycle.cppm") + read("src/application/media_lifecycle.cpp"))
composer = read("ui/components/ComposerDrawer.qml")
presentation_bridge = read("src/presentation/qt/presentation_bridge.cpp")
runtime = (read("ui/app/main.cpp") + read("ui/app/runtime_composition.cppm") +
           read("ui/app/runtime_composition.cpp") +
           read("ui/app/runtime_ui_bindings.cpp"))
insert_panel = read("ui/components/ComposerInsertPanel.qml")
gateway = read("services/media-gateway/gateway.mjs")
storage = read("services/media-gateway/storage/quilibrium_sdk.mjs")
storage_factory = read("services/media-gateway/storage/index.mjs")
native_addon = read("services/media-gateway/native/quilibrium_storage_addon.cpp")
media_service_cmake = read("services/media-gateway/CMakeLists.txt")
env_example = read("services/media-gateway/.env.example")
doc = read("docs/44-media-upload-gateway.md")

if "FARCASTER_ENABLE_MEDIA_UPLOAD" not in cmake or "FARCASTER_DEFAULT_MEDIA_GATEWAY_BASE_URL" not in cmake:
    errors.append("Media upload must remain an explicit optional product composition with a first-party gateway default.")
if "https://storage-farcaster.genyleap.com" not in transport:
    errors.append("Desktop media transport must target the stable Genyleap upload gateway by default.")
if "qstorage.quilibrium.com" in transport or "GENYLEAP_MEDIA_STORAGE_" in transport:
    errors.append("Desktop media transport must not contain qStorage/provider storage semantics or credentials.")
if "IMediaUploadGateway" not in application or "IExecutor" not in application:
    errors.append("Application media upload must depend on the provider-neutral gateway port and application executor.")
for token in ("FileDialog", "mediaUpload.beginUpload", "richComposer.insertManagedMedia", "mediaUpload.busy"):
    if token not in composer:
        errors.append(f"Composer media flow is missing {token}.")
if "richComposer.protocolEmbeds.length > 0" not in composer:
    errors.append("Composer must permit protocol-valid media-only casts after a verified upload.")
if "mediaFileRequested" not in insert_panel:
    errors.append("Media insertion UI must request a local file instead of accepting a provider URL.")
for token in ("https://media-farcaster.genyleap.com", "createDownload", "downloadUrlLifetimeSeconds"):
    if token not in gateway:
        errors.append(f"Stable private media-delivery boundary is missing {token}.")
for token in (
    "beginMultipartUpload",
    "uploadPart",
    "completeMultipartUpload",
    "abortMultipartUpload",
    "deleteObject",
    "createDownload",
    "headObject",
    "presignGet",
    "presignHead",
):
    if token not in storage:
        errors.append(f"Quilibrium SDK storage adapter is missing {token}.")
if "storage.createUpload(" in gateway:
    errors.append("QStorage live upload path must not use direct presigned PUT query authentication.")

if "gateway-multipart" not in transport or "/parts/" not in transport:
    errors.append("Desktop media transport must use the bounded gateway multipart relay contract.")
if "uploadUrl" in transport:
    errors.append("Desktop media transport must not consume provider upload URLs after QStorage PUT query-auth incompatibility.")
for token in ("beginMultipartUpload", "uploadPart", "completeMultipartUpload", "abortMultipartUpload", "deleteObject", "managementToken", "gateway-multipart"):
    if token not in gateway:
        errors.append(f"Media gateway multipart relay is missing {token}.")
if "GENYLEAP_MEDIA_MULTIPART_PART_BYTES" not in env_example:
    errors.append("Gateway deployment example must document bounded multipart part sizing.")

if "quilibrium-qstorage" not in storage_factory:
    errors.append("Media gateway storage selection must remain behind an explicit provider adapter factory.")
for token in (
    "import quilibrium.qstorage;",
    "import quilibrium.sigv4;",
    "presign_get_object",
    "presign_head_object",
    "signCreateMultipart",
    "signUploadPart",
    "signCompleteMultipart",
    "signAbortMultipart",
    "signDeleteObject",
):
    if token not in native_addon:
        errors.append(f"Native media storage boundary is not delegating {token} through Quilibrium SDK/SigV4.")
if "thecompez/Quilibrium-SDK.git" not in media_service_cmake or "3599f81f6284797692dedc096f6ba0f35fe317a2" not in media_service_cmake:
    errors.append("Media gateway must pin the reviewed Quilibrium C++ SDK revision.")
# JavaScript deliberately retains a small SigV4 only at two live-validated
# boundaries: (1) QStorage read/completion verification, which requires the
# hashed `SHA256("")` canonical-payload presigned GET form (live QStorage
# returns HTTP 403 for the pinned SDK's default `UNSIGNED-PAYLOAD` presign),
# and (2) an Authorization-header DeleteObject fallback so a rolling deployment
# with an older native addon cannot turn client cleanup into HTTP 500. The
# multipart WRITE path must always be delegated to the native Quilibrium SDK
# signing exports; JavaScript must not implement write-signing itself.
write_native = all(
    token in storage
    for token in (
        "this.native.signCreateMultipart(",
        "this.native.signUploadPart(",
        "this.native.signCompleteMultipart(",
        "this.native.signAbortMultipart(",
    )
)
if not write_native:
    errors.append(
        "Multipart write signing (Create/Part/Complete/Abort) must be delegated to "
        "the native Quilibrium SDK exports in the JavaScript storage adapter."
    )
if "typeof this.native.signDeleteObject === 'function'" not in storage:
    errors.append(
        "Managed-object DeleteObject must prefer the native signDeleteObject export "
        "and only fall back to JavaScript SigV4 for older native addons."
    )
if "sha256Hex(Buffer.alloc(0))" not in storage or "payloadHash" not in storage:
    errors.append(
        "QStorage read/completion verification must use the live-accepted hashed "
        "SHA256(\"\") canonical-payload presigned GET form."
    )
if (root / "services/media-gateway/storage/s3_compatible.mjs").exists():
    errors.append("Legacy custom JavaScript S3/SigV4 adapter must be removed after Quilibrium SDK integration.")
if "publicOrigin" in storage or "GENYLEAP_MEDIA_STORAGE_PUBLIC_ORIGIN" in env_example:
    errors.append("Production media storage must not require a provider-public bucket origin; stable delivery uses signed reads.")
if "GENYLEAP_MEDIA_STORAGE_SECRET_ACCESS_KEY" not in env_example:
    errors.append("Gateway deployment example must document the server-only storage secret configuration.")
if "storage access/secret keys never ship in the desktop binary" not in doc:
    errors.append("Media feature gate must preserve the no-storage-credential desktop invariant.")

if "IMediaAssetLifecycleGateway" not in application or "/v1/assets" not in transport:
    errors.append("Managed-media deletion must remain behind the provider-neutral lifecycle gateway.")
if "managementToken" not in transport or "managementToken" not in gateway:
    errors.append("Upload completion must return an opaque local-only management token for lifecycle cleanup.")
if "deleteObject" not in storage or "signDeleteObject" not in native_addon:
    errors.append("QStorage DeleteObject must be delegated through the native SDK signing boundary.")
if 'm_writePhase == QStringLiteral("confirmed")' not in presentation_bridge:
    errors.append("Managed-media ownership must not be committed before PublishCast is confirmed.")
for token in ("WritePhase::Confirmed", "OperationKind::RemoveCast", "releaseManagedAssetsForCast"):
    if token not in runtime:
        errors.append(f"Confirmed RemoveCast cleanup wiring is missing {token}.")

client_files = list((root / "src").rglob("*")) + list((root / "ui").rglob("*"))
for path in client_files:
    if not path.is_file() or path.suffix.lower() not in {".cpp", ".cppm", ".hpp", ".qml", ".cmake", ".txt"}:
        continue
    text = path.read_text(encoding="utf-8", errors="ignore")
    if "qstorage.quilibrium.com" in text:
        errors.append(f"qStorage endpoint leaked into desktop/core source: {path.relative_to(root)}")
    if "GENYLEAP_MEDIA_STORAGE_SECRET_ACCESS_KEY" in text:
        errors.append(f"Storage secret variable leaked into desktop/core source: {path.relative_to(root)}")

if errors:
    print("Media upload contract guard: FAILED")
    for error in errors:
        print(" -", error)
    sys.exit(1)
print("Media upload contract guard: PASS")
