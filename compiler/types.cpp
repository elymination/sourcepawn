/* vim: set sts=4 ts=8 sw=4 tw=99 et: */
/*  Pawn compiler
 *
 *  Function and variable definition and declaration, statement parser.
 *
 *  Copyright (c) ITB CompuPhase, 1997-2006
 *
 *  This software is provided "as-is", without any express or implied warranty.
 *  In no event will the authors be held liable for any damages arising from
 *  the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1.  The origin of this software must not be misrepresented; you must not
 *      claim that you wrote the original software. If you use this software in
 *      a product, an acknowledgment in the product documentation would be
 *      appreciated but is not required.
 *  2.  Altered source versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software.
 *  3.  This notice may not be removed or altered from any source distribution.
 *
 *  Version: $Id$
 */
#include <ctype.h>

#include <utility>

#include "compile-context.h"
#include "parse-node.h"
#include "sc.h"
#include "sctracker.h"
#include "types.h"

namespace sp {

using namespace ke;

Type::Type(Atom* name, TypeKind kind)
 : name_(name),
   index_(-1),
   kind_(kind)
{
}

const char* Type::prettyName() const {
  if (kind_ == TypeKind::Function)
    return kindName();
  return name()->chars();
}

const char*
Type::kindName() const
{
  switch (kind_) {
    case TypeKind::EnumStruct:
      return "enum struct";
    case TypeKind::Pstruct:
      return "struct";
    case TypeKind::Methodmap:
      return "methodmap";
    case TypeKind::Enum:
      return "enum";
    case TypeKind::Object:
      return "object";
    case TypeKind::Function:
      if (funcenum_ptr_) {
        if (funcenum_ptr_->entries.size() > 1)
          return "typeset";
        if (ke::StartsWith(name_->chars(), "::"))
          return "function";
        return "typedef";
      }
      return "function";
    default:
      return "type";
  }
}

TypeManager::TypeManager(CompileContext& cc)
  : cc_(cc)
{}

Type* TypeManager::find(Atom* atom) {
    auto iter = types_.find(atom);
    if (iter == types_.end())
        return nullptr;
    return iter->second;
}

Type* TypeManager::Get(int index) {
    return by_index_[index];
}

Type* TypeManager::add(const char* name, TypeKind kind) {
    return add(cc_.atom(name), kind);
}

Type* TypeManager::add(Atom* name, TypeKind kind) {
    Type* type = new Type(name, kind);
    RegisterType(type);
    return type;
}

void TypeManager::RegisterType(Type* type) {
    assert(types_.find(type->name()) == types_.end());

    type->set_index((int)by_index_.size());
    by_index_.emplace_back(type);

    types_.emplace(type->name(), type);
}

Type* TypeManager::defineBuiltin(const char* name, BuiltinType type) {
    Type* ptr = add(name, TypeKind::Builtin);
    ptr->setBuiltinType(type);
    return ptr;
}

void
TypeManager::init()
{
    type_int_ = defineBuiltin("int", BuiltinType::Int);
    types_.emplace(cc_.atom("_"), type_int_);

    type_bool_ = defineBuiltin("bool", BuiltinType::Bool);
    type_any_ = defineBuiltin("any", BuiltinType::Any);

    type_float_ = defineBuiltin("float", BuiltinType::Float);
    types_.emplace(cc_.atom("Float"), type_float_);

    type_void_ = defineBuiltin("void", BuiltinType::Void);
    type_null_ = defineBuiltin("null_t", BuiltinType::Null);

    type_string_ = defineBuiltin("char", BuiltinType::Char);
    types_.emplace(cc_.atom("String"), type_string_);

    type_function_ = defineFunction(cc_.atom("Function"), nullptr);
    type_object_ = defineObject("object");
}

Type* TypeManager::defineFunction(Atom* name, funcenum_t* fe) {
    Type* type = add(name, TypeKind::Function);
    type->setFunction(fe);
    return type;
}

Type*
TypeManager::defineObject(const char* name)
{
    Type* type = add(name, TypeKind::Object);
    type->setObject();
    return type;
}

Type* TypeManager::defineMethodmap(Atom* name, MethodmapDecl* map) {
    Type* type = find(name);
    if (!type)
        type = add(name, TypeKind::Methodmap);
    type->setMethodmap(map);
    return type;
}

Type*
TypeManager::defineEnumTag(const char* name)
{
    auto atom = cc_.atom(name);
    if (auto type = find(atom)) {
        assert(type->kind() == TypeKind::Methodmap);
        return type;
    }

    Type* type = add(atom, TypeKind::Enum);
    return type;
}

Type* TypeManager::defineEnumStruct(Atom* name, EnumStructDecl* decl) {
    Type* type = add(name, TypeKind::EnumStruct);
    type->setEnumStruct(decl);
    return type;
}

Type*
TypeManager::defineTag(Atom* name) {
    Type* type = add(name, TypeKind::Enum);
    return type;
}

Type* TypeManager::definePstruct(PstructDecl* decl) {
    assert(find(decl->name()) == nullptr);

    Type* type = add(decl->name(), TypeKind::Pstruct);
    type->setPstruct(decl);
    return type;
}

bool typeinfo_t::isCharArray() const {
    return numdim() == 1 && type->isChar();
}

TypenameInfo typeinfo_t::ToTypenameInfo() const {
    if (type)
        return TypenameInfo(type);
    return TypenameInfo(type_atom, is_label);
}

Type* typeinfo_t::enum_struct_type() const {
    if (!type->isInt())
        return nullptr;
    return declared_type;
}

Type* typeinfo_t::semantic_type() const {
    if (!type->isInt() || !declared_type)
        return type;
    return declared_type;
}

bool typeinfo_t::is_implicit_dim(int i) const {
    return semantic_type() != type && i == numdim() - 1;
}

} // namespace sp
