/**************************************************
 * This file is for backwards compatibility only,
 * please do not add more entries here but rather
 * use the new types in the Markup namespace.
 **************************************************/

#ifndef _HTML_H_
#define _HTML_H_

#include "modules/logdoc/markup.h"

#define HTML_ElementType Markup::Type

#define HE_DOC_ROOT Markup::HTE_DOC_ROOT
#define HE_TEXT Markup::HTE_TEXT
#define HE_TEXTGROUP Markup::HTE_TEXTGROUP
#define HE_COMMENT Markup::HTE_COMMENT
#define HE_PROCINST Markup::HTE_PROCINST
#define HE_DOCTYPE Markup::HTE_DOCTYPE
#define HE_ENTITY Markup::HTE_ENTITY
#define HE_ENTITYREF Markup::HTE_ENTITYREF
#define HE_CDATA Markup::HTE_CDATA
#define HE_UNKNOWNDECL Markup::HTE_UNKNOWNDECL
#define HE_ANY Markup::HTE_ANY
#define HE_UNKNOWN Markup::HTE_UNKNOWN
#define HE_A Markup::HTE_A
#define HE_P Markup::HTE_P
#define HE_I Markup::HTE_I
#define HE_B Markup::HTE_B
#define HE_U Markup::HTE_U
#define HE_S Markup::HTE_S
#define HE_Q Markup::HTE_Q
#define HE_LI Markup::HTE_LI
#define HE_UL Markup::HTE_UL
#define HE_OL Markup::HTE_OL
#define HE_DL Markup::HTE_DL
#define HE_DT Markup::HTE_DT
#define HE_DD Markup::HTE_DD
#define HE_H1 Markup::HTE_H1
#define HE_H2 Markup::HTE_H2
#define HE_H3 Markup::HTE_H3
#define HE_H4 Markup::HTE_H4
#define HE_H5 Markup::HTE_H5
#define HE_H6 Markup::HTE_H6
#define HE_HR Markup::HTE_HR
#define HE_BR Markup::HTE_BR
#define HE_EM Markup::HTE_EM
#define HE_TT Markup::HTE_TT
#define HE_TH Markup::HTE_TH
#define HE_TR Markup::HTE_TR
#define HE_TD Markup::HTE_TD
#define HE_BQ Markup::HTE_BQ
#define HE_IMG Markup::HTE_IMG
#define HE_PRE Markup::HTE_PRE
#define HE_DIR Markup::HTE_DIR
#define HE_DIV Markup::HTE_DIV
#define HE_KBD Markup::HTE_KBD
#define HE_VAR Markup::HTE_VAR
#define HE_XMP Markup::HTE_XMP
#define HE_MAP Markup::HTE_MAP
#define HE_WBR Markup::HTE_WBR
#define HE_BIG Markup::HTE_BIG
#define HE_DFN Markup::HTE_DFN
#define HE_SUB Markup::HTE_SUB
#define HE_SUP Markup::HTE_SUP
#define HE_BDO Markup::HTE_BDO
#define HE_COL Markup::HTE_COL
#define HE_DEL Markup::HTE_DEL
#define HE_INS Markup::HTE_INS
#define HE_XML Markup::HTE_XML
#define HE_NAV Markup::HTE_NAV
#define HE_MENU Markup::HTE_MENU
#define HE_FONT Markup::HTE_FONT
#define HE_AREA Markup::HTE_AREA
#define HE_META Markup::HTE_META
#define HE_NOBR Markup::HTE_NOBR
#define HE_FORM Markup::HTE_FORM
#define HE_HEAD Markup::HTE_HEAD
#define HE_BODY Markup::HTE_BODY
#define HE_BASE Markup::HTE_BASE
#define HE_HTML Markup::HTE_HTML
#define HE_CITE Markup::HTE_CITE
#define HE_CODE Markup::HTE_CODE
#define HE_SAMP Markup::HTE_SAMP
#define HE_SPAN Markup::HTE_SPAN
#define HE_LINK Markup::HTE_LINK
#define HE_ABBR Markup::HTE_ABBR
#define HE_MARK Markup::HTE_MARK
#define HE_TIME Markup::HTE_TIME
#define HE_TABLE Markup::HTE_TABLE
#define HE_INPUT Markup::HTE_INPUT
#define HE_TITLE Markup::HTE_TITLE
#define HE_FRAME Markup::HTE_FRAME
#define HE_SMALL Markup::HTE_SMALL
#define HE_STYLE Markup::HTE_STYLE
#define HE_IMAGE Markup::HTE_IMAGE
#define HE_EMBED Markup::HTE_EMBED
#define HE_PARAM Markup::HTE_PARAM
#define HE_LABEL Markup::HTE_LABEL
#define HE_TBODY Markup::HTE_TBODY
#define HE_TFOOT Markup::HTE_TFOOT
#define HE_THEAD Markup::HTE_THEAD
#define HE_BLINK Markup::HTE_BLINK
#define HE_AUDIO Markup::HTE_AUDIO
#define HE_VIDEO Markup::HTE_VIDEO
#define HE_ASIDE Markup::HTE_ASIDE
#define HE_METER Markup::HTE_METER
#define HE_OBJECT Markup::HTE_OBJECT
#define HE_CANVAS Markup::HTE_CANVAS
#define HE_APPLET Markup::HTE_APPLET
#define HE_STRIKE Markup::HTE_STRIKE
#define HE_STRONG Markup::HTE_STRONG
#define HE_SELECT Markup::HTE_SELECT
#define HE_OPTION Markup::HTE_OPTION
#define HE_CENTER Markup::HTE_CENTER
#define HE_SCRIPT Markup::HTE_SCRIPT
#define HE_KEYGEN Markup::HTE_KEYGEN
#define HE_BUTTON Markup::HTE_BUTTON
#define HE_IFRAME Markup::HTE_IFRAME
#define HE_LEGEND Markup::HTE_LEGEND
#define HE_OUTPUT Markup::HTE_OUTPUT
#define HE_SOURCE Markup::HTE_SOURCE
#define HE_HGROUP Markup::HTE_HGROUP
#define HE_HEADER Markup::HTE_HEADER
#define HE_FOOTER Markup::HTE_FOOTER
#define HE_FIGURE Markup::HTE_FIGURE
#define HE_CAPTION Markup::HTE_CAPTION
#define HE_ADDRESS Markup::HTE_ADDRESS
#define HE_ISINDEX Markup::HTE_ISINDEX
#define HE_LISTING Markup::HTE_LISTING
#define HE_NOEMBED Markup::HTE_NOEMBED
#define HE_MARQUEE Markup::HTE_MARQUEE
#define HE_ACRONYM Markup::HTE_ACRONYM
#define HE_SECTION Markup::HTE_SECTION
#define HE_ARTICLE Markup::HTE_ARTICLE
#define HE_TEXTAREA Markup::HTE_TEXTAREA
#define HE_BASEFONT Markup::HTE_BASEFONT
#define HE_FRAMESET Markup::HTE_FRAMESET
#define HE_NOFRAMES Markup::HTE_NOFRAMES
#define HE_NOSCRIPT Markup::HTE_NOSCRIPT
#define HE_COLGROUP Markup::HTE_COLGROUP
#define HE_OPTGROUP Markup::HTE_OPTGROUP
#define HE_FIELDSET Markup::HTE_FIELDSET
#define HE_DATALIST Markup::HTE_DATALIST
#define HE_PROGRESS Markup::HTE_PROGRESS
#define HE_PLAINTEXT Markup::HTE_PLAINTEXT
#define HE_BLOCKQUOTE Markup::HTE_BLOCKQUOTE
#define HE_FIGCAPTION Markup::HTE_FIGCAPTION
#define HE_EVENT_SOURCE Markup::HTE_EVENT_SOURCE
#define HE_LAST Markup::HTE_LAST

#endif /* _HTML_H_ */

