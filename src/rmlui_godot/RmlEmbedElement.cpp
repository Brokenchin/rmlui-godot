#include "RmlEmbedElement.hpp"

#include <RmlUi/Core/ElementDocument.h>

namespace RmlGodot {

RmlEmbedElement::RmlEmbedElement(const Rml::String& tag) : Rml::Element(tag) {}

RmlEmbedElement::~RmlEmbedElement() = default;

bool RmlEmbedElement::GetIntrinsicDimensions(Rml::Vector2f& dimensions, float& /*ratio*/) {
	if (!_layout_boundary) return false;
	// Intrinsic size = the embedded document's current margin box. The document formats
	// itself against this host's box (ElementDocument::UpdateLayout takes the parent's
	// box as its containing block) in RmlContext::_update_embed_layout; the parent
	// document only ever sees the resulting fixed box. Explicit CSS width/height on the
	// host still win (replaced-element sizing rules). Not mounted yet -> no intrinsic
	// size, so the empty host lays out as a normal element.
	for (int i = 0; i < GetNumChildren(); ++i) {
		if (auto* doc = rmlui_dynamic_cast<Rml::ElementDocument*>(GetChild(i))) {
			dimensions = doc->GetBox().GetSize(Rml::BoxArea::Margin);
			return true;
		}
	}
	return false;
}

void RmlEmbedElement::OnAttributeChange(const Rml::ElementAttributes& changed_attributes) {
	Rml::Element::OnAttributeChange(changed_attributes);
	if (changed_attributes.count("layout-boundary") != 0) {
		_layout_boundary = HasAttribute("layout-boundary") &&
			GetAttribute<Rml::String>("layout-boundary", "true") != "false";
		DirtyLayout();   // the parent must re-lay this host out under the new mode
	}
}

Rml::ElementPtr RmlEmbedElementInstancer::InstanceElement(Rml::Element* /*parent*/,
	const Rml::String& tag, const Rml::XMLAttributes& /*attributes*/) {
	return Rml::ElementPtr(new RmlEmbedElement(tag));
}

void RmlEmbedElementInstancer::ReleaseElement(Rml::Element* element) {
	delete element;
}

} // namespace RmlGodot
