#pragma once

#include <godot_cpp/core/class_db.hpp>

// RM_GD_CLASS — declares a Godot-registered class with inline _bind_methods.
// Self-contained: zero Kafet dependencies.  Follows the same pattern as
// KAFET_GD_CLASS so all projects share a consistent class declaration style.
//
// Usage:
//   class RM_GD_CLASS(MyNode, godot::Control, {
//       godot::ClassDB::bind_method(D_METHOD("foo"), &MyNode::foo);
//   });
//   public:
//       void foo();
//   };

#define RM_GD_CLASS(ClassName, BaseClass, BIND_BODY)                \
ClassName : public BaseClass {                                      \
GDCLASS(ClassName, BaseClass);                                      \
protected:                                                          \
static void _bind_methods() BIND_BODY

// RM_GD_CLASS_EXTENDED — splits binds between header (inline) and cpp.
// The header contains core binds; heavy-include binds go into
// a _bind_methods_extended() defined in the .cpp file.

#define RM_GD_CLASS_EXTENDED(ClassName, BaseClass, BIND_BODY)       \
ClassName : public BaseClass {                                      \
GDCLASS(ClassName, BaseClass);                                      \
protected:                                                          \
static void _bind_methods() { BIND_BODY _bind_methods_extended(); } \
static void _bind_methods_extended();
