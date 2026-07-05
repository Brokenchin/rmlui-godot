#!/usr/bin/env bash
# Runs the full automated test suite.
#
# Usage:
#   GODOT=/path/to/Godot_v4.5_console.exe tests/run_all.sh
# (run from the project/ directory, or from anywhere — it cds itself)
#
# Two tiers:
#   - headless: API/unit suites (test_runner.gd) + standalone regression scripts
#   - windowed: pixel-readback tests (viewport textures are empty headless)
set -u
GODOT="${GODOT:?Set GODOT to a Godot 4.5+ executable (console build recommended)}"
cd "$(dirname "$0")/.."

PASS=0
FAIL=0
FAILED=()

run() { # $1 = script path, $2 = headless|windowed
	local flags=""
	[ "$2" = "headless" ] && flags="--headless"
	printf -- "--- %-44s (%s)\n" "$1" "$2"
	if timeout 240 "$GODOT" $flags --path . -s "$1" >/tmp/rmlui_test_out.txt 2>&1; then
		PASS=$((PASS + 1))
	else
		FAIL=$((FAIL + 1))
		FAILED+=("$1")
		echo "    FAILED — last lines:"
		tail -6 /tmp/rmlui_test_out.txt | sed 's/^/    /'
	fi
}

# API/unit suites (assert-based, aggregated by test_runner.gd)
run tests/test_runner.gd headless

# Standalone regression scripts (each quits with its failure count)
run tests/test_rcss_tokenizer.gd headless
run tests/test_completion_helpers.gd headless
run tests/test_link_navigation.gd headless
run tests/test_splice_rcss.gd headless
run tests/test_diagnostics.gd headless
run tests/test_log_chain.gd headless
run tests/test_property_export.gd headless
run tests/test_inline_gdscript.gd headless
run tests/test_struct_array_binding.gd headless
run tests/test_external_script.gd headless
run tests/test_embed_documents.gd headless
run tests/test_embed_hardening.gd headless
run tests/test_embed_input.gd headless
run tests/test_embed_focus.gd headless
run tests/test_embed_drag.gd headless
run tests/test_embed_clip.gd headless
run tests/test_embed_scoped.gd headless
run tests/test_editor_script_gate.gd headless
run tests/test_inline_script_freeze.gd headless
run tests/test_decorator_shader_warn.gd windowed
run tests/test_shutdown_stress.gd headless
run tests/test_leak_churn.gd headless
run tests/test_error_fuzz.gd headless
run tests/test_input_actions.gd headless
run tests/test_gamepad_nav.gd headless
run tests/test_debugger_toggle.gd headless
run tests/test_script_completion.gd headless
run tests/test_gdscript_highlight.gd headless
run tests/test_script_line_mapping.gd headless

# Pixel-readback tests need a window
run tests/test_embed_render_clip.gd windowed
run tests/test_live_preview_parity.gd windowed
run tests/test_subviewport_recreate.gd windowed
run tests/test_preview_click.gd windowed
run tests/test_hover_bridge.gd windowed
run tests/test_input_prehandler.gd windowed
run tests/test_perf_stress.gd windowed
run tests/test_perf_hover.gd windowed
run tests/test_render_dirty.gd windowed
run tests/test_effects_reuse.gd windowed
run tests/test_text_filtering.gd windowed
run tests/test_gradient_render.gd windowed
run tests/test_transition_settle.gd windowed

echo
echo "==============================================="
echo "  RESULTS: $PASS passed, $FAIL failed"
echo "==============================================="
if [ "$FAIL" -gt 0 ]; then
	for f in "${FAILED[@]}"; do echo "  FAILED: $f"; done
	exit 1
fi
