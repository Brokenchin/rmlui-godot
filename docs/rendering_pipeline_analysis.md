# Rendering Pipeline Deep Analysis — Slot Invisibility & Architectural Gaps

**Date**: 2026-05-28  
**Status**: Active investigation  
**Symptom**: Overflow:hidden slot content (images, text overlays) intermittently invisible despite CPU cull logging them as DRAW. Background/section text renders correctly. Removing the CPU cull makes ALL slots disappear.

---

## 1. Architecture Overview: The Two-Stage Bridge

```
RmlUI Core                    GodotRenderInterface              RmlContext::_draw()
─────────────                 ────────────────────              ───────────────────
Context::Render()             Stores DrawCommands               Replays commands via
  → Element::Render()           in a vector                      Godot RenderingServer
    → SetClippingRegion()     (scissor state, transform,       Creates canvas items,
    → RenderGeometry()         texture, geometry handle)         parents them, adds meshes
    → PushLayer/PopLayer()
    → EnableClipMask()
    → RenderToClipMask()
```

RmlUI's API is designed for **immediate-mode GPU renderers** (OpenGL/Vulkan) with hardware scissor, stencil buffers, and framebuffer objects. Godot's RenderingServer is a **retained-mode** scene graph of canvas items. The bridge translates between these fundamentally different models.

---

## 2. Confirmed Bugs

### BUG-1: EnableClipMask mode overwrite (CLIP_AND_DRAW → TRANSPARENT)

**Severity**: HIGH (breaks border-radius overflow clipping)

RmlUI's clip mask lifecycle:
1. `EnableClipMask(true)` — start masking
2. `RenderToClipMask()` — draw mask geometry  
3. Content renders (should be clipped by mask)
4. `EnableClipMask(false)` — stop masking (called when next element has different clip scope)

Our implementation:
- `EnableClipMask(true)` → `canvas_item_set_canvas_group_mode(item, CLIP_AND_DRAW)`
- `EnableClipMask(false)` → `canvas_item_set_canvas_group_mode(item, TRANSPARENT)`

**The problem**: Godot evaluates canvas group mode at **frame render time**, not at command issue time. The last mode set wins. When `EnableClipMask(false)` fires (which it always does — RmlUI calls it for the next element), it switches the canvas group back to TRANSPARENT. The clip mask geometry becomes normal visible content instead of a mask. **All clip masking is effectively disabled.**

**Affected elements**: Any with `overflow:hidden` + `border-radius`, or with CSS transforms + overflow clipping. Simple overflow:hidden (no border-radius) uses only scissor, so it bypasses this bug.

**Fix direction**: Each clip mask scope needs its OWN canvas group item. Don't reuse the layer's canvas group — create a child group with CLIP_AND_DRAW that is NOT subsequently switched to TRANSPARENT.

### BUG-2: No pixel-level scissor clipping

**Severity**: HIGH (no actual clipping at sub-mesh granularity)

The scissor sub-items created in `_draw()` are plain canvas items:
```cpp
godot::RID sub = rs->canvas_item_create();
rs->canvas_item_set_parent(sub, layer_stack.back().canvas_item);
rs->canvas_item_set_material(sub, mat_rid);
```

**No clip rect is ever set.** The CPU-side AABB cull is the ONLY enforcement of scissor boundaries, and it operates at whole-mesh granularity. Meshes partially overlapping the scissor boundary render their full extent.

Godot has no direct `canvas_item_set_scissor_rect()` API. The available options:
- `set_clip_contents(true)` on a Control node (but we're using RIDs, not nodes)
- CLIP_AND_DRAW canvas group with a filled rect as the mask
- Custom shader with `discard` for pixels outside the rect

**Fix direction**: For each scissor region, create a CLIP_AND_DRAW canvas group where the first draw call is a filled rect of the scissor dimensions. All subsequent geometry draws to child items of this group, getting pixel-level clipping.

### BUG-3: CPU cull ignores rotation/scale from transforms

**Severity**: MODERATE (breaks CSS-transformed elements)

The AABB cull code:
```cpp
godot::Vector2 origin = xform.get_origin();
float mesh_left = origin.x + aabb3.position.x;  // Only uses translation!
```

When `cmd.has_transform` is true, the xform contains rotation/scale. The mesh's screen-space bounds should be the TRANSFORMED AABB, not just translated. A 45° rotated 100×100 mesh at (50,50) would have screen extent roughly (-20,50)→(120,120), but the cull computes (50,50)→(150,150).

**Fix direction**: Transform the AABB corners by the full affine transform, then compute the axis-aligned bounding box of the result.

---

## 3. Design Gaps

### GAP-1: Scissor sub-items serve no rendering purpose

The scissor sub-items are structurally meaningless — they're plain canvas items that provide no clipping. Their only function is as organizational containers for draw calls. The CPU cull decides what goes in; the sub-item adds nothing. This is architectural debt from trying to map RmlUI's scissor concept to Godot's canvas item tree without an actual clipping mechanism.

### GAP-2: The cull-and-scissor entanglement

The CPU cull's `continue` statement skips BOTH the draw AND the scissor tracking:
```cpp
if (culled) continue;  // Skips scissor sub-item creation below
```

This creates a coupling where the cull affects the canvas item hierarchy. Removing the cull changes WHICH canvas items are created and HOW meshes are grouped. The fact that removing the cull breaks rendering proves the canvas item structure depends on cull behavior — which is wrong. The cull should be a pure performance optimization with no correctness impact.

### GAP-3: Canvas items recreated every frame

Every `_draw()` call:
1. Frees ALL scissor items and layer items from the previous frame
2. Creates new ones from scratch

This means potentially hundreds of `canvas_item_create()` + `free_rid()` calls per frame. While functional, it's wasteful and could interact badly with Godot's canvas item culling system (Godot issue #73077: CanvasGroup culling causes random other groups to be culled).

### GAP-4: self_modulate for opacity (wrong API)

```cpp
rs->canvas_item_set_self_modulate(current_layer, Color(1,1,1, opacity));
```

`self_modulate` affects only the item's own drawing, NOT its children. For a TRANSPARENT canvas group, opacity should apply to the entire composited group. The correct call is `canvas_item_set_modulate()` (affects children) or setting alpha on the composite step.

---

## 4. Root Cause Hypotheses for Slot Invisibility

### Hypothesis A: Canvas group mode interference (MOST LIKELY)

Even though simple overflow:hidden doesn't use clip masks, the `EnableClipMask` commands ARE generated by RmlUI (with `false` to clear any previous mask). In `_draw()`:

```cpp
case CmdType::ENABLE_CLIP_MASK: {
    if (layer_stack.size() < 2) break;  // Skips if no layer pushed
    ...
}
```

This is safe IF there's never a PushLayer active. But if ANY ancestor element triggers PushLayer (CSS filter, opacity, mask-image, backdrop-filter), then `layer_stack.size() >= 2` and the EnableClipMask commands fire on the layer's canvas group.

**Scenario**: Parent with `opacity: 0.8` → PushLayer. Child with `overflow:hidden + border-radius` → EnableClipMask(true) → CLIP_AND_DRAW. Next child (simple overflow:hidden) → EnableClipMask(false) → TRANSPARENT. The layer renders in TRANSPARENT mode. But the mask geometry from the first child is now rendered as visible content, potentially covering the second child's slot content.

**Worse scenario**: If the mask geometry from RenderToClipMask is a filled rect at the border-radius element's position, and it renders on top of the slot content in the same layer. The slot content IS drawn but is BEHIND the mask rect. With premultiplied alpha, a fully opaque mask rect would completely cover what's behind it.

### Hypothesis B: Godot CanvasGroup culling bug (MODERATE LIKELIHOOD)

Godot issue #73077 documents that CanvasGroup items can cause OTHER CanvasGroups to be randomly culled. If we create canvas groups for layers, the Godot-level culling might incorrectly cull groups that contain our slot content.

This would explain the intermittent nature ("when it happens"). The culling depends on the scene state and viewport, so different window sizes or frame timings could trigger or avoid the bug.

### Hypothesis C: Draw target pointing to wrong canvas item (MODERATE LIKELIHOOD)

After a PushLayer/PopLayer cycle, `active_scissor` is reset to `false`. If the next geometry command has `scissor_enabled=true`, a new sub-item is created parented to the NEW top of layer_stack. But if the PopLayer happened DURING the processing of slot content (because effects on a child element popped a layer), the remaining slot content would draw to a different parent than the first part.

This could cause slot content to be split across canvas items in different layers, with some parts becoming invisible when the layer composites.

### Hypothesis D: RenderToClipMask draws to stale draw_target (LOW LIKELIHOOD)

The `RENDER_TO_CLIP_MASK` case draws to `draw_target`:
```cpp
rs->canvas_item_add_mesh(draw_target, mask_mesh->get_rid(), xform, ...);
```

If `draw_target` is a scissor sub-item (from a preceding GEOMETRY command) instead of the layer's canvas group item, the mask geometry goes to the wrong place. For CLIP_AND_DRAW to work, the mask must be drawn as the parent's own content, not as a child item's content.

This is mitigated because PushLayer resets draw_target, but if any GEOMETRY command fires between PushLayer and RENDER_TO_CLIP_MASK, draw_target moves to a sub-item.

### Hypothesis E: Premultiplied alpha double-application in canvas groups (LOW LIKELIHOOD)

When children draw premultiplied-alpha content into a TRANSPARENT canvas group's offscreen buffer, and the compositing step also uses premultiplied blending, the alpha SHOULD be correct. But if Godot's internal compositing uses standard blending on the group's output, the alpha gets squared, making semi-transparent content darker or invisible.

Our material (`BLEND_MODE_PREMULT_ALPHA`) is set on both children and the group item, which should be correct. But Godot's internal canvas group compositing path might not respect the child material's blend mode during the offscreen→main composite step.

---

## 5. Diagnostic Tests

To determine which hypothesis is correct:

### Test 1: Dump the full command stream
Instead of logging only textured meshes during cull, log EVERY command type (PUSH_LAYER, POP_LAYER, ENABLE_CLIP_MASK, RENDER_TO_CLIP_MASK, GEOMETRY) for a single frame. This reveals the exact sequence and whether layers/clip masks are involved for the failing slots.

### Test 2: Disable canvas group modes entirely
Replace all `CANVAS_GROUP_MODE_TRANSPARENT` and `CANVAS_GROUP_MODE_CLIP_AND_DRAW` calls with no-ops (just use plain canvas items for layers). If slots appear, the canvas group mode is the culprit.

### Test 3: Bypass scissor sub-items
Draw ALL geometry directly to the layer's canvas item (remove the scissor sub-item creation entirely). If slots appear, the sub-item hierarchy is the culprit.

### Test 4: Log canvas item RIDs and hierarchy
Print the RID of each canvas item created, its parent, and what meshes are added to it. This reveals the exact tree structure and whether meshes end up in the right places.

---

## 6. Should We Rewrite From Scratch?

### Arguments FOR rewrite:
1. The bridge has multiple confirmed bugs and design gaps
2. The scissor approximation is fundamentally lossy
3. The canvas group mode mapping is fragile (mode-switching semantics don't match RmlUI's expectations)
4. The cull-scissor entanglement makes the code hard to reason about
5. Debugging is difficult (symptoms far from root causes)

### Arguments AGAINST rewrite:
1. The basic rendering works for MOST cases (text, backgrounds, decorators)
2. The same Godot API limitations would apply to any rewrite
3. A rewrite risks introducing new bugs without fixing the core limitations

### Recommended approach: TARGETED REWRITE of the _draw() method

Keep the GodotRenderInterface (command recording) as-is. Rewrite `_draw()` with these design changes:

1. **Proper scissor clipping**: For each scissor region, create a CLIP_AND_DRAW canvas group with a filled-rect mask. All geometry within that scissor scope draws to children of this group.

2. **Separate clip mask canvas groups**: Don't switch the layer's mode between CLIP_AND_DRAW and TRANSPARENT. Create a DEDICATED child canvas group for each clip mask scope.

3. **Decouple cull from canvas structure**: Move the cull to AFTER scissor tracking, so the canvas item hierarchy is always correct regardless of cull decisions. The cull only skips the `canvas_item_add_mesh` call, not the structural bookkeeping.

4. **Transform-aware cull**: Compute the transformed AABB for the cull check.

5. **Fix opacity**: Use `canvas_item_set_modulate()` instead of `canvas_item_set_self_modulate()`.

This targeted approach fixes the confirmed bugs while preserving the working parts of the pipeline (command recording, texture management, geometry compilation).

---

## 7. Quick Reference: RmlUI Rendering Call Patterns

### Simple overflow:hidden (no border-radius)
```
EnableScissorRegion(true)
SetScissorRegion(element_bounds)
EnableClipMask(false)          ← clears any previous mask
RenderGeometry(background)
RenderGeometry(content)
[children render with their own clipping]
```

### overflow:hidden + border-radius
```
EnableScissorRegion(true)
SetScissorRegion(element_bounds)
EnableClipMask(true)
RenderToClipMask(Set, rounded_rect_geometry, offset)
RenderGeometry(background)
RenderGeometry(content)
[children render]
EnableClipMask(false)          ← next element clears mask
```

### Element with CSS filter (e.g., opacity < 1)
```
PushLayer()
[SetClippingRegion + EnableClipMask as above]
RenderGeometry(background)
RenderGeometry(content)
[children render]
CompositeLayers(src, dst, blend, [opacity_filter])
PopLayer()
```

### Stacking context (z-index, position:relative) without effects
```
[No PushLayer/PopLayer]
[Normal SetClippingRegion + RenderGeometry]
[Children rendered in stacking context order]
```
