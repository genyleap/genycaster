/*!
 * @file        src/presentation/qt/media_classifier_controller.hpp
 * @brief       Qt media classification adapter.
 *
 * @details
 * Converts canonical cast embeds into presentation-friendly media
 * categories while leaving provider decoding outside QML.
 *
 * @author      Kambiz Asadzadeh
 * @since       05 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

// Textual compatibility boundary: Qt MOC requires this QObject declaration in a .hpp file.
#pragma once

#include <QObject>
#include <QVariantMap>

/**
 * @brief QML-facing canonical embed media classifier.
 *
 * Thin adapter over `farcaster.presentation.media` (the single classification
 * rule base). QML presentation surfaces never maintain their own extension or
 * host tables; they ask this object so every Cast renders through identical
 * media semantics.
 */
class MediaClassifierController final : public QObject {
    Q_OBJECT

public:
    /** @brief Creates an idle classifier adapter. */
    explicit MediaClassifierController(QObject* parent = nullptr);

    /**
     * @brief Classifies one embed for presentation dispatch.
     * @param embed    Embed record: kind, url/value, mediaType, contentType.
     * @param metadata Optional metadata from the trusted link resolver:
     *                 providerKind/contentType describing an observed direct body.
     * @return Map with "family" ("image"|"video"|"audio"|"file"|"web"),
     *         "visual" (bool) and "directPlayable" (bool).
     */
    Q_INVOKABLE QVariantMap classify(const QVariantMap& embed, const QVariantMap& metadata = {}) const;

    /** @brief Classifies a bare URL (no surrounding metadata). */
    Q_INVOKABLE QVariantMap classifyUrl(const QString& url) const;
};
