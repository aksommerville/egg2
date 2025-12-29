/* egg_language_codes.h
 * In case you need constants for the various language codes.
 * In a dynamic context, EGG_LANG_FROM_STRING and EGG_STRING_FROM_LANG are preferred.
 * Language codes are a nonzero 10-bit integer containing the first letter (1..26) in the top five bits, and second letter in the bottom five.
 * We shift that by 6 bits and combine with sub-rid 1..63 to form rids for "strings" resources.
 */
 
#ifndef EGG_LANGUAGE_CODES_H
#define EGG_LANGUAGE_CODES_H

// https://en.wikipedia.org/wiki/List_of_ISO_639_language_codes
#define EGG_LANG_aa  33 // Afar
#define EGG_LANG_ab  34 // Abkhazian
#define EGG_LANG_ae  37 // Avestan
#define EGG_LANG_af  38 // Afrikaans
#define EGG_LANG_ak  43 // Akan
#define EGG_LANG_am  45 // Amharic
#define EGG_LANG_an  46 // Aragonese
#define EGG_LANG_ar  50 // Arabic
#define EGG_LANG_as  51 // Assamese
#define EGG_LANG_av  54 // Avaric
#define EGG_LANG_ay  57 // Aymara
#define EGG_LANG_az  58 // Azerbaijani
#define EGG_LANG_ba  65 // Bashkir
#define EGG_LANG_be  69 // Belarusian
#define EGG_LANG_bg  71 // Bulgarian
#define EGG_LANG_bh  72 // Bihari (deprecated)
#define EGG_LANG_bi  73 // Bislama
#define EGG_LANG_bm  77 // Bambara
#define EGG_LANG_bn  78 // Bengali
#define EGG_LANG_bo  79 // Tibetan
#define EGG_LANG_br  82 // Breton
#define EGG_LANG_bs  83 // Bosnian
#define EGG_LANG_ca  97 // Catalan
#define EGG_LANG_ce 101 // Chechen
#define EGG_LANG_ch 104 // Chamorro
#define EGG_LANG_co 111 // Corsican
#define EGG_LANG_cr 114 // Cree
#define EGG_LANG_cs 115 // Czech
#define EGG_LANG_cu 117 // Church Slavonic
#define EGG_LANG_cv 118 // Chuvash
#define EGG_LANG_cy 121 // Welsh
#define EGG_LANG_da 129 // Danish
#define EGG_LANG_de 133 // German
#define EGG_LANG_dv 150 // Divehi
#define EGG_LANG_dz 154 // Dzongkha
#define EGG_LANG_ee 165 // Ewe
#define EGG_LANG_el 172 // Greek
#define EGG_LANG_en 174 // English
#define EGG_LANG_eo 175 // Esperanto
#define EGG_LANG_es 179 // Spanish
#define EGG_LANG_et 180 // Estonian
#define EGG_LANG_eu 181 // Basque
#define EGG_LANG_fa 193 // Persian
#define EGG_LANG_ff 198 // Fulah
#define EGG_LANG_fi 201 // Finnish
#define EGG_LANG_fj 202 // Fijian
#define EGG_LANG_fo 207 // Faroese
#define EGG_LANG_fr 210 // French
#define EGG_LANG_fy 217 // Frisian
#define EGG_LANG_ga 225 // Irish
#define EGG_LANG_gd 228 // Gaelic
#define EGG_LANG_gl 236 // Galician
#define EGG_LANG_gn 238 // Guarani
#define EGG_LANG_gu 245 // Gujarati
#define EGG_LANG_gv 246 // Manx
#define EGG_LANG_ha 257 // Hausa
#define EGG_LANG_he 261 // Hebrew
#define EGG_LANG_hi 265 // Hindi
#define EGG_LANG_ho 271 // Hiri Motu
#define EGG_LANG_hr 274 // Croatian
#define EGG_LANG_ht 276 // Haitian
#define EGG_LANG_hu 277 // Hungarian
#define EGG_LANG_hy 281 // Armenian
#define EGG_LANG_hz 282 // Herero
#define EGG_LANG_ia 289 // Interlingua
#define EGG_LANG_id 292 // Indonesian
#define EGG_LANG_ie 293 // Occidental Interlingue
#define EGG_LANG_ig 295 // Igbo
#define EGG_LANG_ii 297 // Sichuan Yi
#define EGG_LANG_ik 299 // Inupiaq
#define EGG_LANG_in 302 // Indonesian (deprecated)
#define EGG_LANG_io 303 // Ido
#define EGG_LANG_is 307 // Icelandic
#define EGG_LANG_it 308 // Italian
#define EGG_LANG_iu 309 // Inuktitut
#define EGG_LANG_iw 311 // Hebrew (deprecated; use "he")
#define EGG_LANG_ja 321 // Japanese
#define EGG_LANG_ji 329 // Yiddish (deprecated; use "yi")
#define EGG_LANG_jv 342 // Javanese
#define EGG_LANG_jw 343 // Javanese (deprecated; use "jv")
#define EGG_LANG_ka 353 // Georgian
#define EGG_LANG_kg 359 // Kongo
#define EGG_LANG_ki 361 // Kikuyu
#define EGG_LANG_kj 362 // Kuanyama
#define EGG_LANG_kk 363 // Kazakh
#define EGG_LANG_kl 364 // Kalaalisut
#define EGG_LANG_km 365 // Central Khmer
#define EGG_LANG_kn 366 // Kannada
#define EGG_LANG_ko 367 // Korean
#define EGG_LANG_kr 370 // Kanuri
#define EGG_LANG_ks 371 // Kashmiri
#define EGG_LANG_ku 373 // Kurdish
#define EGG_LANG_kv 374 // Komi
#define EGG_LANG_kw 375 // Cornish
#define EGG_LANG_ky 377 // Kyrgyz
#define EGG_LANG_la 385 // Latin
#define EGG_LANG_lb 386 // Luxembourgish
#define EGG_LANG_lg 391 // Ganda
#define EGG_LANG_li 393 // Limburgan
#define EGG_LANG_ln 398 // Lingala
#define EGG_LANG_lo 399 // Lao
#define EGG_LANG_lt 404 // Lithuanian
#define EGG_LANG_lu 405 // Luba-Katanga
#define EGG_LANG_lv 406 // Latvian
#define EGG_LANG_mg 423 // Malagasy
#define EGG_LANG_mh 424 // Mashallese
#define EGG_LANG_mi 425 // Maori
#define EGG_LANG_mk 427 // Macedonian
#define EGG_LANG_ml 428 // Malayalam
#define EGG_LANG_mn 430 // Mongolian
#define EGG_LANG_mo 431 // Moldovan (deprecated; use "ro")
#define EGG_LANG_mr 434 // Marathi
#define EGG_LANG_ms 435 // Malay
#define EGG_LANG_mt 436 // Maltese
#define EGG_LANG_my 441 // Burmese
#define EGG_LANG_na 449 // Nauru
#define EGG_LANG_nb 450 // Nowegian Bokmal
#define EGG_LANG_nd 452 // North Ndebele
#define EGG_LANG_ne 453 // Nepali
#define EGG_LANG_ng 455 // Ndonga
#define EGG_LANG_nl 460 // Flemish
#define EGG_LANG_nn 462 // Norwegian Nynorsk
#define EGG_LANG_no 463 // Norwegian
#define EGG_LANG_nr 466 // South Ndebele
#define EGG_LANG_nv 470 // Navajo
#define EGG_LANG_ny 473 // Chichewa
#define EGG_LANG_oc 483 // Occitan
#define EGG_LANG_oj 490 // Ojibwa
#define EGG_LANG_om 493 // Oromo
#define EGG_LANG_or 498 // Oriya
#define EGG_LANG_os 499 // Ossetia
#define EGG_LANG_pa 513 // Punjabi
#define EGG_LANG_pi 521 // Pali
#define EGG_LANG_pl 524 // Polish
#define EGG_LANG_ps 531 // Pashto
#define EGG_LANG_pt 532 // Portuguese
#define EGG_LANG_qu 565 // Quechua
#define EGG_LANG_rm 589 // Romansh
#define EGG_LANG_rn 590 // Rundi
#define EGG_LANG_ro 591 // Romanian
#define EGG_LANG_ru 597 // Russian
#define EGG_LANG_rw 599 // Kinyarwanda
#define EGG_LANG_sa 609 // Sanskrit
#define EGG_LANG_sc 611 // Sardinian
#define EGG_LANG_sd 612 // Sindhi
#define EGG_LANG_se 613 // Northern Sami
#define EGG_LANG_sg 615 // Sango
#define EGG_LANG_sh 616 // Serbo-Croatian (deprecated)
#define EGG_LANG_si 617 // Sinhala
#define EGG_LANG_sk 619 // Slovak
#define EGG_LANG_sl 620 // Slovenian
#define EGG_LANG_sm 621 // Samoan
#define EGG_LANG_sn 622 // Shona
#define EGG_LANG_so 623 // Somali
#define EGG_LANG_sq 625 // Albanian
#define EGG_LANG_sr 626 // Serbian
#define EGG_LANG_ss 627 // Swati
#define EGG_LANG_st 628 // Southern Sotho
#define EGG_LANG_su 629 // Sudanese
#define EGG_LANG_sv 630 // Swedish
#define EGG_LANG_sw 631 // Swahili
#define EGG_LANG_ta 641 // Tamil
#define EGG_LANG_te 645 // Telugu
#define EGG_LANG_tg 647 // Tajik
#define EGG_LANG_th 648 // Thai
#define EGG_LANG_ti 649 // Tigrinya
#define EGG_LANG_tk 651 // Turkmen
#define EGG_LANG_tl 652 // Tagalog
#define EGG_LANG_tn 654 // Tswana
#define EGG_LANG_to 655 // Tonga
#define EGG_LANG_tr 658 // Turkish
#define EGG_LANG_ts 659 // Tsonga
#define EGG_LANG_tt 660 // Tatar
#define EGG_LANG_tw 663 // Twi
#define EGG_LANG_ty 665 // Tahitian
#define EGG_LANG_ug 679 // Uighur
#define EGG_LANG_uk 683 // Ukrainian
#define EGG_LANG_ur 690 // Urdu
#define EGG_LANG_uz 698 // Uzbek
#define EGG_LANG_ve 709 // Venda
#define EGG_LANG_vi 713 // Vietnamese
#define EGG_LANG_vo 719 // Volapuk
#define EGG_LANG_wa 737 // Walloon
#define EGG_LANG_wo 751 // Wolof
#define EGG_LANG_xh 776 // Xhosa
#define EGG_LANG_yi 809 // Yiddish
#define EGG_LANG_yo 815 // Yoruba
#define EGG_LANG_za 833 // Zhuang
#define EGG_LANG_zh 840 // Chinese
#define EGG_LANG_zu 853 // Zulu

#endif
