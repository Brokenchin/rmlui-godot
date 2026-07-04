#pragma once

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementInstancer.h>

#include <string>

namespace RmlGodot {

/// Host element for an embedded sub-document — the `<embed-doc>` tag.
///
/// Issue #56: an embedded document (its own .rml + <link> RCSS + <script>) is
/// mounted as this element's child (see RmlContext::mount_embed), so it lays out
/// as an ordinary box inside the parent document's layout tree — participating in
/// the parent's flexbox / @media / anchoring / overflow — while keeping its own
/// GDScript instance and inline-handler resolution.
///
/// This works because RmlUi preserves a document's owner_document across
/// reparenting (Element::SetOwnerDocument is a no-op when owner_document == this),
/// so the embedded document's <script> blocks and inline gdscript: handlers keep
/// resolving to the embedded document even though it now lives under this element
/// rather than directly under the context root.
class RmlEmbedElement final : public Rml::Element {
public:
	RMLUI_RTTI_DefineWithParent(RmlEmbedElement, Rml::Element)

	explicit RmlEmbedElement(const Rml::String& tag);
	~RmlEmbedElement() override;

	bool is_mounted() const { return _mounted; }
	void set_mounted(bool v) { _mounted = v; }

	const std::string& embed_id() const { return _embed_id; }
	void set_embed_id(std::string id) { _embed_id = std::move(id); }

	/// Mark this host's owning (parent) document as needing re-layout. Used after
	/// an embed's internal layout changed its outer size, so the parent reflows
	/// and repositions sibling widgets around it. Public wrapper around the
	/// protected Element::DirtyLayout(), which routes to GetOwnerDocument().
	void dirty_parent_layout() { DirtyLayout(); }

	/// Layout boundary (the `layout-boundary` attribute / mount_embed option):
	/// the host reports the embedded document's formatted size as its intrinsic
	/// dimensions, so the PARENT document lays it out as a replaced element (a
	/// fixed box, like <img>) and — with the fork's ReplacedFormattingContext
	/// gate — never formats the embed's subtree. A parent reflow then costs
	/// O(parent shell) instead of O(parent + every embedded subtree), and an
	/// embed's internal layout stays isolated to its own document (formatted by
	/// RmlContext::_update_embed_layout). Measured: a populated panel's root
	/// reflow was ~22ms with recursion, ~1ms as a shell.
	bool is_layout_boundary() const { return _layout_boundary; }
	bool GetIntrinsicDimensions(Rml::Vector2f& dimensions, float& ratio) override;

protected:
	void OnAttributeChange(const Rml::ElementAttributes& changed_attributes) override;

private:
	bool _mounted = false;
	bool _layout_boundary = false;
	std::string _embed_id;
};

/// Instances <embed-doc> as RmlEmbedElement. Registered process-globally by
/// RmlManager after Rml::Initialise() (alongside the "body" document instancer).
class RmlEmbedElementInstancer final : public Rml::ElementInstancer {
public:
	Rml::ElementPtr InstanceElement(Rml::Element* parent, const Rml::String& tag,
		const Rml::XMLAttributes& attributes) override;
	void ReleaseElement(Rml::Element* element) override;
};

} // namespace RmlGodot
