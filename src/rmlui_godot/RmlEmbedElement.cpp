#include "RmlEmbedElement.hpp"

namespace RmlGodot {

RmlEmbedElement::RmlEmbedElement(const Rml::String& tag) : Rml::Element(tag) {}

RmlEmbedElement::~RmlEmbedElement() = default;

Rml::ElementPtr RmlEmbedElementInstancer::InstanceElement(Rml::Element* /*parent*/,
	const Rml::String& tag, const Rml::XMLAttributes& /*attributes*/) {
	return Rml::ElementPtr(new RmlEmbedElement(tag));
}

void RmlEmbedElementInstancer::ReleaseElement(Rml::Element* element) {
	delete element;
}

} // namespace RmlGodot
