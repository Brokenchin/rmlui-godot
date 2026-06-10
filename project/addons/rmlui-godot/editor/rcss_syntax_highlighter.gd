@tool
class_name RcssSyntaxHighlighter
extends RmlUiSyntaxHighlighterBase
## Syntax highlighting for .rcss files in the script editor.
## All scanning lives in RcssTokenizer (shared with the RML highlighter);
## the base class handles cross-line state (multi-line comments, nesting).

var _tokenizer := RcssTokenizer.new()


func _get_name() -> String:
	return "RCSS"


func _get_supported_languages() -> PackedStringArray:
	return PackedStringArray(["rcss"])


func _tokenize_line(text: String, entry_state: int) -> Dictionary:
	return _tokenizer.tokenize(text, 0, text.length(), entry_state)
