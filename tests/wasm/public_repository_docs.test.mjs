import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync, existsSync, readdirSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const read = (name) => readFileSync(path.join(root, name), "utf8");
const required = [
    "README.md", "STATUS.md", "LICENSE", "THIRD_PARTY_NOTICES.md",
    "docs/ARCHITECTURE.md", "docs/PERFORMANCE.md", "docs/FEATURES.md",
    "docs/SELF_HOSTED_NODES.md", "docs/README.md", "SECURITY.md",
    "CONTRIBUTING.md", "CODE_OF_CONDUCT.md", "CHANGELOG.md",
    ".github/ISSUE_TEMPLATE/bug_report.yml",
    ".github/ISSUE_TEMPLATE/feature_request.yml",
    ".github/PULL_REQUEST_TEMPLATE.md",
    "ui/assets/branding/genycaster-mark.svg"
];

test("public repository entry points and existing GenyCaster branding are available", () => {
    for (const file of required)
        assert.ok(existsSync(path.join(root, file)), file);
    const logo = read("ui/assets/branding/genycaster-mark.svg");
    assert.match(logo, /<svg\b/);
    assert.match(logo, /viewBox="48 48 416 416"/);
    assert.match(logo, /<path fill="#FFFFFF"/);
    assert.match(logo, /GenyCaster icon/);
});

test("repository root remains small and prototype-free after cleanup", () => {
    const allowed = [
        "README.md", "AGENTS.md", "CHANGELOG.md", "CODE_OF_CONDUCT.md",
        "CONTRIBUTING.md", "SECURITY.md", "STATUS.md", "THIRD_PARTY_NOTICES.md"
    ];
    const rootMd = readdirSync(root).filter(name => name.endsWith(".md")).sort();
    assert.deepEqual(rootMd, allowed.sort());
    assert.equal(existsSync(path.join(root, "prototypes")), false);
    const roadmap = read("docs/12-roadmap.md");
    assert.match(roadmap, /Development Roadmap/);
    assert.match(roadmap, /Phase 03/);
    assert.match(roadmap, /Phase 06/);
    assert.match(roadmap, /Complete/);
    assert.match(roadmap, /In progress/);
    assert.match(roadmap, /Planned/);
    assert.match(roadmap, /Concept/);
});

test("public landing page shows provider-neutral architecture without claiming benchmark wins", () => {
    const readme = read("README.md");
    const cmake = read("CMakeLists.txt");
    assert.match(readme, /<h1>GenyCaster<\/h1>/);
    assert.match(readme, /ui\/assets\/branding\/genycaster-mark\.svg/);
    assert.match(readme, /```mermaid[\s\S]*?flowchart/);
    assert.match(readme, /HyperSnap preferred/);
    assert.match(readme, /Snapchain/);
    assert.match(readme, /Local-first/);
    assert.match(readme, /GenyCaster is in \*\*Beta\*\*/);
    assert.match(readme, /Not yet published/);
    assert.match(readme, /Widgets/);
    assert.match(readme, /Native Apps/);
    assert.match(readme, /GENY ecosystem/);
    assert.doesNotMatch(readme, /not a website wrapped/i);
    assert.match(readme, /docs\/PERFORMANCE\.md/);
    assert.match(readme, /CONTRIBUTING\.md/);
    assert.match(readme, /SECURITY\.md/);
    assert.match(cmake, /set\(GENYCASTER_RELEASE_CHANNEL "rc"\)/);
    assert.match(cmake, /set\(GENYCASTER_RELEASE_CHANNEL_NUMBER "1"\)/);
    assert.match(read("docs/12-roadmap.md"), /v1\.0\.0-rc1/);
});

test("i18n and l10n are first-class, independently configurable product features", () => {
    const readme = read("README.md");
    const features = read("docs/FEATURES.md");
    const header = read("src/presentation/qt/localization_controller.hpp");
    const controller = read("src/presentation/qt/localization_controller.cpp");
    const settings = read("ui/components/LocaleSettingsPopup.qml");

    assert.ok(readme.includes("Internationalization and localization (i18n/l10n)"));
    assert.ok(readme.includes("Your language, your regional conventions"));
    assert.ok(readme.includes("Twelve UI languages"));
    assert.ok(readme.includes("independently selectable region, calendar, time zone"));

    assert.ok(features.includes("## Internationalization & localization (i18n/l10n)"));
    for (const preference of ["Interface language", "Text direction", "Regional locale",
                              "Calendar", "Time and dates", "Numbers and monetary display"])
        assert.ok(features.includes(preference), preference);

    for (const setter of ["setLanguageTag", "setRegionalLocaleTag", "setCalendarId",
                          "setTimestampDisplayMode", "setTimeZoneId", "setCurrencyCode"])
        assert.ok(header.includes("Q_INVOKABLE bool " + setter + "("), setter);
    for (const calendar of ["gregorian", "jalali", "shahanshahi", "islamic-civil",
                            "julian", "milankovic"])
        assert.ok(controller.includes('QStringLiteral("' + calendar + '")'), calendar);
    assert.ok(settings.includes("localizationController.availableRegionalLocales"));
    assert.ok(settings.includes("localizationController.availableTimeZones"));
    assert.ok(features.includes("does **not** convert balances"));
});

test("public introduction keeps language versions in build requirements, not the product tagline", () => {
    const readme = read("README.md");
    const hero = readme.split("</div>")[0];
    assert.match(hero, /One shared Modern C\+\+ core/);
    assert.doesNotMatch(hero, /C\+\+2[036]|C%2B%2B-2[036]/);
    assert.match(readme, /C\+\+23 and newer/);
    assert.match(readme, /C\+\+26-capable compiler/);
    assert.match(read("CMakeLists.txt"), /set\(CMAKE_CXX_STANDARD 26\)/);
});

test("architecture, benchmarking and vulnerability disclosure remain explicit", () => {
    const architecture = read("docs/ARCHITECTURE.md");
    assert.ok((architecture.match(/```mermaid/g) || []).length >= 3);
    assert.match(architecture, /per capability/);
    assert.match(architecture, /Separate trust/);
    const performance = read("docs/PERFORMANCE.md");
    assert.match(performance, /raw per-run records/);
    assert.match(performance, /p50/);
    assert.match(performance, /p95/);
    assert.match(performance, /p99/);
    assert.match(performance, /Not yet established/);
    const security = read("SECURITY.md");
    assert.match(security, /privately report a vulnerability/i);
    assert.match(security, /Do not.*public pull request/i);
    assert.match(security, /CORS is not access control/);
});

test("all local links in the new public entry documents resolve", () => {
    const names = [
        "README.md", "STATUS.md", "docs/README.md", "docs/ARCHITECTURE.md",
        "docs/PERFORMANCE.md", "docs/FEATURES.md", "docs/SELF_HOSTED_NODES.md", "CONTRIBUTING.md",
        "SECURITY.md", "CODE_OF_CONDUCT.md",
        "THIRD_PARTY_NOTICES.md", "CHANGELOG.md"
    ];
    const broken = [];
    for (const file of names) {
        const content = read(file);
        const links = [];
        for (const match of content.matchAll(/!?\[[^\]]+\]\(([^)\s]+)(?:\s+"[^"]*")?\)/g))
            links.push(match[1]);
        for (const match of content.matchAll(/(?:src|href)="([^"]+)"/g))
            links.push(match[1]);
        for (const href of links) {
            if (/^(?:https?:|mailto:|#|data:)/i.test(href)) continue;
            const pathname = decodeURIComponent(href.split("#")[0].split("?")[0]);
            if (!pathname) continue;
            if (!existsSync(path.resolve(root, path.dirname(file), pathname)))
                broken.push(file + " -> " + href);
        }
    }
    assert.deepEqual(broken, []);
});

test("the public status reflects the current Qt 6.12 release channel", () => {
    const status = read("STATUS.md");
    assert.match(status, /v1\.0\.0-rc1/);
    assert.match(status, /Qt 6\.12/);
    assert.match(status, /Beta/);
    assert.doesNotMatch(status, /Qt 6\.11|0\.4\.0|tools\/check_/);
});

test("OpenProof identity, opp runtime and independent product database are documented accurately", () => {
    const readme = read("README.md");
    const guide = read("docs/OPENPROOF_AND_SERVICES.md");
    const arch = read("docs/ARCHITECTURE.md");
    const account = read("services/account-sync/README.md");
    const media = read("services/media-gateway/README.md");
    const repo = read("docs/README.md");
    const auth = read("docs/08-security-auth-and-signing.md");
    for (const doc of [readme, guide, arch, account, media, repo, auth])
        assert.match(doc, /OpenProof/i);
    for (const doc of [readme, guide, arch, account]) {
        assert.match(doc, /github\.com\/genyleap\/openproof/);
        assert.match(doc, /genycaster_db/);
    }
    assert.match(guide, /apps\/opp/);
    assert.match(guide, /openproof\.genyleap\.com/);
    assert.match(guide, /PostgreSQL/);
    assert.match(guide, /not a C\+\+/);
    assert.match(guide, /HyperSnap/);
    assert.match(guide, /Snapchain/);
    assert.match(guide, /Guest\/public protocol reads do not depend/);
    assert.match(guide, /Mini App wallet permission/);
    assert.match(readme, /properties\/README\.md/);
    assert.match(read("services/account-sync/server.mjs"), /GENYCASTER_DATABASE_URL/);
    assert.match(read("services/account-sync/server.mjs"), /GENYCASTER_OPENPROOF_ISSUER/);
});

test("roadmap reports implemented foundations without concealing unfinished acceptance", () => {
    const roadmap = read("docs/12-roadmap.md");
    for (const phase of ["02", "03", "04", "06"])
        assert.match(roadmap, new RegExp("\\| \\*\\*" + phase +
            "\\*\\* \\|[^\\n]*\\| \\*\\*Complete\\*\\* \\|"));
    for (const phase of ["05", "08", "10"])
        assert.match(roadmap, new RegExp("\\| \\*\\*" + phase +
            "\\*\\* \\|[^\\n]*\\| \\*\\*In progress\\*\\* \\|"));
    assert.match(roadmap, /tracks \*\*implementation\*\* independently/);
    assert.match(roadmap, /security/i);
    assert.match(roadmap, /qualification/i);
    assert.match(roadmap, /Complete → Released/);
    assert.doesNotMatch(roadmap, /all platforms (are |have been )fully certified/i);
});

test("platform property and server dependency guides contain resolvable local links", () => {
    for (const file of ["properties/README.md", "docs/OPENPROOF_AND_SERVICES.md"]) {
        const dir = path.dirname(path.join(root, file));
        for (const [, target] of read(file).matchAll(/\]\(([^)]+)\)/g)) {
            if (/^(?:https?:|#|mailto:)/.test(target)) continue;
            const relativeTarget = target.split("#")[0];
            if (!relativeTarget) continue;
            assert.ok(existsSync(path.resolve(dir, relativeTarget)),
                      file + " → " + target);
        }
    }
});
