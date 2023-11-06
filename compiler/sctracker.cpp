// vim: set ts=8 sts=4 sw=4 tw=99 et:
//
//  Copyright (c) 2023 AlliedModders LLC
//
//  This software is provided "as-is", without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  Permission is granted to anyone to use this software for any purpose,
//  including commercial applications, and to alter it and redistribute it
//  freely, subject to the following restrictions:
//
//  1.  The origin of this software must not be misrepresented; you must not
//      claim that you wrote the original software. If you use this software in
//      a product, an acknowledgment in the product documentation would be
//      appreciated but is not required.
//  2.  Altered source versions must be plainly marked as such, and must not be
//      misrepresented as being the original software.
//  3.  This notice may not be removed or altered from any source distribution.
#include "sctracker.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <utility>

#include <amtl/am-raii.h>
#include <amtl/am-vector.h>
#include "compile-context.h"
#include "lexer.h"
#include "sc.h"
#include "semantics.h"
#include "symbols.h"
#include "types.h"

namespace sp {

funcenum_t* funcenums_add(CompileContext& cc, Atom* name, bool anonymous) {
    if (anonymous) {
        if (auto type = cc.types()->find(name)) {
            assert(type->kind() == TypeKind::Function);
            assert(type->toFunction()->anonymous);
            return type->toFunction();
        }
    }

    auto e = new funcenum_t;
    e->name = name;
    e->anonymous = anonymous;

    auto type = cc.types()->defineFunction(name, e);
    e->tag = type->tagid();
    return e;
}

funcenum_t* funcenum_for_symbol(CompileContext& cc, symbol* sym) {
    FunctionDecl* fun = sym->decl->as<FunctionDecl>();

    functag_t* ft = new functag_t;
    ft->ret_tag = sym->tag;

    std::vector<funcarg_t> args;
    for (auto arg : fun->canonical()->args()) {
        funcarg_t dest;
        dest.type = arg->type();

        if (dest.type.ident != iARRAY && dest.type.ident != iREFARRAY)
          assert(dest.type.dim.empty());

        args.emplace_back(dest);
    }
    new (&ft->args) PoolArray<funcarg_t>(args);

    auto name = ke::StringPrintf("::ft:%s:%d:%d", sym->name(), sym->addr(), sym->codeaddr);
    funcenum_t* fe = funcenums_add(cc, cc.atom(name), true);
    new (&fe->entries) PoolArray<functag_t*>({ft});

    return fe;
}

// Finds a functag that was created intrinsically.
functag_t*
functag_from_tag(int tag)
{
    Type* type = CompileContext::get().types()->find(tag);
    funcenum_t* fe = type->asFunction();
    if (!fe)
        return nullptr;
    if (fe->entries.empty())
        return nullptr;
    return fe->entries.back();
}

methodmap_t::methodmap_t(methodmap_t* parent, Atom* name)
 : parent(parent),
   tag(0),
   nullable(false),
   keyword_nullable(false),
   name(name),
   dtor(nullptr),
   ctor(nullptr),
   is_bound(false)
{
}

int methodmap_method_t::property_tag() const {
    auto types = CompileContext::get().types();

    assert(getter || setter);
    if (getter)
        return getter->tag;
    if (setter->decl->as<FunctionDecl>()->canonical()->args().size() != 2)
        return types->tag_void();
    ArgDecl* valp = setter->decl->as<FunctionDecl>()->canonical()->args()[1];
    if (valp->type().ident != iVARIABLE)
        return types->tag_void();
    return valp->type().tag();
}

methodmap_t*
methodmap_add(CompileContext& cc, methodmap_t* parent, Atom* name)
{
    auto map = new methodmap_t(parent, name);

    if (parent) {
        if (parent->nullable)
            map->nullable = parent->nullable;
        if (parent->keyword_nullable)
            map->keyword_nullable = parent->keyword_nullable;
    }

    map->tag = cc.types()->defineMethodmap(name->chars(), map)->tagid();
    return map;
}

methodmap_t*
methodmap_find_by_tag(int tag)
{
    return CompileContext::get().types()->find(tag)->asMethodmap();
}

methodmap_t*
methodmap_find_by_name(Atom* name)
{
    auto type = CompileContext::get().types()->find(name);
    if (!type)
        return NULL;
    return methodmap_find_by_tag(type->tagid());
}

methodmap_method_t*
methodmap_find_method(methodmap_t* map, Atom* name)
{
    auto iter = map->methods.find(name);
    if (iter != map->methods.end())
        return iter->second;

    if (map->parent)
        return methodmap_find_method(map->parent, name);
    return nullptr;
}

} // namespace sp
