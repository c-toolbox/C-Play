/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Shared text handling for both DecimalSpinBox implementations (Qt >= 6.11 and Legacy).
.pragma library

function escapeRegExp(text) {
    return text.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

// Accepts '.' or ',' as decimal point regardless of locale, and an optional suffix
function inputPattern(decimals, allowNegative, suffix) {
    let number = (allowNegative ? "-?" : "") + "\\d*";
    if (decimals > 0)
        number += "([.,]\\d{0," + decimals + "})?";
    const unit = suffix.trim().length > 0 ? "(\\s*" + escapeRegExp(suffix.trim()) + ")?" : "";
    return new RegExp("^\\s*" + number + unit + "\\s*$");
}

function format(value, decimals, suffix, locale) {
    return Number(value).toLocaleString(locale, 'f', decimals) + suffix;
}

// Returns NaN when the text holds no number (e.g. empty or just "-")
function parse(text, suffix) {
    let number = text.trim();
    const unit = suffix.trim();
    if (unit.length > 0 && number.endsWith(unit))
        number = number.slice(0, -unit.length);
    return parseFloat(number.trim().replace(",", "."));
}
