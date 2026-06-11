@tool
class_name RmlUiSyntaxHighlighterBase
extends EditorSyntaxHighlighter
## Base for tokenizer-driven highlighters that need cross-line region state
## (multi-line comments, <style> blocks, ...).
##
## EditorSyntaxHighlighter is strictly line-based, so this class maintains a
## lazily-computed cache of each line's entry state, invalidated from the
## edited line down via TextEdit.lines_edited_from. Subclasses implement
## _tokenize_line(text, entry_state) -> {"tokens": Dictionary, "state": int}.

# _entry_states[i] is the tokenizer region state at the START of line i.
# Entries [0, _valid) are trustworthy; everything after is recomputed on demand.
var _entry_states := PackedInt32Array()
var _valid := 0
var _connected := false


func _get_line_syntax_highlighting(line_num: int) -> Dictionary:
	var te := get_text_edit()
	if te == null:
		return {}
	if not _connected:
		te.lines_edited_from.connect(_on_lines_edited)
		_connected = true

	var entry := _entry_state_for(line_num, te)
	var result := _tokenize_line(te.get_line(line_num), entry)
	_store_exit_state(line_num, result.get("state", 0))
	return result.get("tokens", {})


## Override in subclasses.
func _tokenize_line(_text: String, _entry_state: int) -> Dictionary:
	return {}


func _clear_highlighting_cache() -> void:
	_valid = 0


func _on_lines_edited(from_line: int, to_line: int) -> void:
	# The edit changes the exit state of the edited line at the earliest, so
	# entry states up to and including that line stay valid.
	_valid = clampi(mini(from_line, to_line) + 1, 0, _valid)


func _entry_state_for(line: int, te: TextEdit) -> int:
	if _entry_states.size() <= line + 1:
		_entry_states.resize(line + 2)
	if _valid == 0:
		_entry_states[0] = 0
		_valid = 1
	while _valid <= line:
		var prev := _valid - 1
		var r := _tokenize_line(te.get_line(prev), _entry_states[prev])
		_entry_states[_valid] = r.get("state", 0)
		_valid += 1
	return _entry_states[line]


func _store_exit_state(line: int, state: int) -> void:
	if _entry_states.size() <= line + 1:
		_entry_states.resize(line + 2)
	if _entry_states[line + 1] != state:
		_entry_states[line + 1] = state
		_valid = line + 2
	else:
		_valid = maxi(_valid, line + 2)
