/*!
 * @file        ui/app/emoji_controller_fwd.hpp
 * @brief       Qt emoji controller forward declaration.
 *
 * @details
 * Keeps a MOC-compatible QObject name available to executable composition
 * without pulling Qt implementation headers into named module boundaries.
 *
 * @author      Kambiz Asadzadeh
 * @since       19 Oct 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

// Textual Qt/MOC compatibility boundary: keep QObject implementation headers
// out of the exported C++ module interface for EmojiRuntime.
class EmojiController;
