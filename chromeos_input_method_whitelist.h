// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_INPUT_METHOD_WHITELIST_H_
#define CHROMEOS_INPUT_METHOD_WHITELIST_H_

namespace {

// The list of input method IDs that we handle. This filtering is necessary
// since some input methods are definitely unnecessary for us. For example, we
// should disable "ja:anthy", "zh:cangjie", and "zh:pinyin" engines in
// ibus-m17n since we (will) have better equivalents outside of ibus-m17n.
//
// The list will be processed by scripts. Please keep the formatting simple.
const char* kInputMethodIdsWhitelist[] = {
  "chewing",  // ibus-chewing - Traditional Chinese
  "hangul",   // ibus-hangul - Korean
  "mozc",     // ibus-mozc - Japanese (with English keyboard)
  "mozc-dv",  // ibus-mozc - Japanese (with Dvorak keyboard)
  "mozc-jp",  // ibus-mozc - Japanese (with Japanese keyboard)
  "pinyin",   // Pinyin engine in ibus-pinyin - Simplified Chinese

  // ibus-m17n input methods (language neutral ones).
  "m17n:t:latn-pre",
  "m17n:t:latn-post",

  // ibus-m17n input methods.
  "m17n:ar:kbd",         // Arabic
  "m17n:he:kbd",         // Hebrew
  "m17n:hi:itrans",      // Hindi
  // Note: the m17n-contrib package has some more Hindi definitions.
  "m17n:fa:isiri",       // Persian
  "m17n:th:kesmanee",    // Thai (simulate the Kesmanee keyboard)
  "m17n:th:pattachote",  // Thai (simulate the Pattachote keyboard)
  "m17n:th:tis820",      // Thai (simulate the TIS-820.2538 keyboard)
  "m17n:vi:tcvn",        // Vietnames (TCVN6064 sequence)
  "m17n:vi:telex",       // Vietnames (TELEX key sequence)
  "m17n:vi:viqr",        // Vietnames (VIQR key sequence)
  "m17n:vi:vni",         // Vietnames (VNI key sequence)
  // Note: Since ibus-m17n does not support "get-surrounding-text" feature yet,
  // Vietnames input methods, except 4 input methods above, in m17n-db should
  // not work fine. The 4 input methods in m17n-db (>= 1.6.0) don't require the
  // feature.
  "m17n:zh:cangjie",     // Traditional Chinese (Cangjie)
  "m17n:zh:quick",       // Traditional Chinese (Quick) in m17n-contrib
  // TODO(suzhe): Add CantonHK and Stroke5 if (it's really) necessary.

  // ibux-xkb-layouts input methods (keyboard layouts).
  "xkb:be::fra",        // Belgium - French
  "xkb:br::por",        // Brazil - Portuguese
  "xkb:bg::bul",        // Bulgaria - Bulgarian
  "xkb:cz::cze",        // Czech Republic - Czech
  "xkb:de::ger",        // Germany - German
  "xkb:ee::est",        // Estonia - Estonian
  "xkb:es::spa",        // Spain - Spanish
  "xkb:es:cat:cat",     // Spain - Catalan
  "xkb:dk::dan",        // Denmark - Danish
  "xkb:gr::gre",        // Greece - Greek
  "xkb:lt::lit",        // Lithuania - Lithuanian
  "xkb:lv::lav",        // Latvia - Latvian
  "xkb:hr::scr",        // Croatia - Croatian
  "xkb:nl::nld",        // Netherlands - Dutch
  "xkb:gb:extd:eng",    // United Kingdom - English
  "xkb:fi::fin",        // Finland - Finnish
  "xkb:fr::fra",        // France - French
  "xkb:hu::hun",        // Hungary - Hungarian
  "xkb:it::ita",        // Italy - Italian
  "xkb:jp::jpn",        // Japan - Japanese
  "xkb:no::nor",        // Norway - Norwegian
  "xkb:pl::pol",        // Poland - Polish
  "xkb:pt::por",        // Portugal - Portuguese
  "xkb:ro::rum",        // Romania - Romanian
  "xkb:se::swe",        // Sweden - Swedish
  "xkb:sk::slo",        // Slovakia - Slovak
  "xkb:si::slv",        // Slovenia - Slovene (Slovenian)
  "xkb:rs::srp",        // Serbia - Serbian
  "xkb:ch::ger",        // Switzerland - German
  "xkb:ru::rus",        // Russia - Russian
  "xkb:tr::tur",        // Turkey - Turkish
  "xkb:ua::ukr",        // Ukraine - Ukrainian
  "xkb:us::eng",        // US - English
  "xkb:us:dvorak:eng",  // US - Dvorak - English
  // TODO(satorux,jshin): Add more keyboard layouts.
};

}  // namespace

#endif  // CHROMEOS_INPUT_METHOD_WHITELIST_H_
