/** @mainpage The documentedit Module
This module contains the implementation for the feature FEATURE_EDITABLE_DOCUMENT. Editing of documents in WYSIWYG. 

The documenteditor itself, handles caret movement, editing and inserting/removing of styles and formatting (italic, bold, colors, textalignment etc.). 

It has a undo-redo-stack that records changes. 

It performs Tidy-operations of the html-tree. 

It has no UI. The UI is made by the webpage author and sends commands to the editor using javascript (execCommand, queryCommandEnabled, queryCommandState, queryCommandSupported, queryCommandValue). More advanced webpages implements functions that is not supported through the execCommand-API. Instead, they use DOM to manipulate the html-tree directly.

<hr>

Most functionality is based on calling theese few functions.

<b>OpDocumentEditSelection::RemoveContent</b>
<i>Removes the selected elements</i>

<b>OpDocumentEdit::InsertStyle</b>
<b>OpDocumentEdit::RemoveStyle</b>
<b>OpDocumentEdit::InsertBreak</b>
<b>OpDocumentEdit::InsertBlockElementMultiple</b>
<b>OpDocumentEdit::RemoveBlockTypes</b>
<b>OpDocumentEdit::InsertTextHTML</b>
<b>OpDocumentEdit::Tidy</b>

Always, when a change is being made from inside the documenteditor, call BeginChange and EndChange to get it registered in the undostack and Tidyed-up etc.

<hr>

Basic demo can be found on: http://www.mozilla.org/editor/midasdemo/

Advanced demo: http://tinymce.moxiecode.com/opera/examples/example_full.htm

Any document can be made editable by entering this in the urlfield javascript:void(document.designMode="on");

*/
