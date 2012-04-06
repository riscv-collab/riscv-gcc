/* This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3, or (at your option) any later
   version.

   GCC is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "ansidecl.h"
#include "coretypes.h"
#include "tm.h"
#include "opts.h"
#include "tree.h"
#include "tree-iterator.h"
#include "tree-pass.h"
#include "gimple.h"
#include "toplev.h"
#include "debug.h"
#include "options.h"
#include "flags.h"
#include "convert.h"
#include "diagnostic-core.h"
#include "langhooks.h"
#include "langhooks-def.h"
#include "target.h"
#include "cgraph.h"

#include <gmp.h>
#include <mpfr.h>
#include <vec.h>
#include <hashtab.h>

#include "gpython.h"
#include "py-il-dot.h"
#include "py-il-tree.h"
#include "py-vec.h"
#include "py-runtime.h"

static tree gpy_dot_pass_genericify_toplevl_functor_decl (gpy_dot_tree_t *,
							  VEC(gpy_context_t, gc) *);
static VEC(tree,gc) * gpy_dot_pass_genericify_toplevl_class_decl (gpy_dot_tree_t *,
								  VEC(gpy_context_t, gc) *,
								  gpy_hash_tab_t * );
static tree gpy_dot_pass_genericify_class_method_attrib (gpy_dot_tree_t *, const char *,
							 VEC(gpy_context_t, gc) *);

static void gpy_dot_pass_genericify_setup_runtime_globls (gpy_hash_tab_t * const);
static void gpy_dot_pass_genericify_create_offsets_globl_context (tree, tree *,
								  VEC(gpy_context_t,gc) *);
static tree gpy_dot_pass_genericify_scalar (gpy_dot_tree_t *, tree *);
static tree gpy_dot_pass_lower_expr (gpy_dot_tree_t *, tree *, VEC(gpy_context_t,gc) *);

char * gpy_dot_pass_genericify_gen_concat (const char * s1,
					   const char * s2)
{
  size_t s1len = strlen (s1);
  size_t s2len = strlen (s2);
  size_t tlen = s1len + s2len;
  
  char buffer[tlen+3]; 
  char * p;
  for (p = buffer; *s1 != '\0'; ++s1)
    {
      *p = *s1;
      ++p;
    }
  *p = '.';
  p++;
  for (; *s2 != '\0'; ++s2)
    {
      *p = *s2;
      ++p;
    }
  *p = '\0';

  return xstrdup (buffer);
}

static
tree gpy_dot_pass_generificify_get_module_type (const char * s, 
						gpy_hash_tab_t * modules)
{
  tree retval = error_mark_node;

  gpy_hashval_t h = gpy_dd_hash_string (s);
  gpy_hash_entry_t * e = gpy_dd_hash_lookup_table (modules, h);
  if (e)
    {
      if (e->data)
        retval = (tree) e->data;
    }

  return retval;
}

static
void gpy_dot_pass_genericify_setup_runtime_globls (gpy_hash_tab_t * context)
{
  tree decl = build_decl (BUILTINS_LOCATION, VAR_DECL,
			  get_identifier (GPY_RR_globl_stack),
			  gpy_unsigned_char_ptr);
  TREE_STATIC (decl) = 0;
  TREE_PUBLIC (decl) = 1;
  TREE_USED (decl) = 1;
  DECL_ARTIFICIAL (decl) = 1;
  DECL_EXTERNAL (decl) = 1;

  gcc_assert (!gpy_dd_hash_insert (gpy_dd_hash_string (GPY_RR_globl_stack),
				   decl, context)
	      );

  decl = build_decl (BUILTINS_LOCATION, VAR_DECL,
		     get_identifier (GPY_RR_globl_stack_pointer),
		     gpy_object_type_ptr_ptr);
  TREE_STATIC (decl) = 0;
  TREE_PUBLIC (decl) = 1;
  TREE_USED (decl) = 1;
  DECL_ARTIFICIAL (decl) = 1;
  DECL_EXTERNAL (decl) = 1;

  gcc_assert (!gpy_dd_hash_insert (gpy_dd_hash_string (GPY_RR_globl_stack_pointer),
				   decl, context)
	      );
}

static
void gpy_dot_pass_genericify_create_offsets_globl_context (tree type, tree * cblock,
							   VEC(gpy_context_t, gc) * context)
{
  gpy_context_t globls = VEC_index (gpy_context_t, context, 0);
  gpy_context_t globls_symbols = VEC_index (gpy_context_t, context, 1);
  gpy_hash_entry_t * e = gpy_dd_hash_lookup_table (globls,
						   gpy_dd_hash_string (GPY_RR_globl_stack_pointer));
  gcc_assert (e);
  tree stack_pointer = (tree)e->data;

  int offset = 0, field_count = 0;
  tree field;
  for (field = TYPE_FIELDS (type); field != NULL_TREE;
       field = DECL_CHAIN (field)
       )
    {
      gcc_assert (TREE_CODE (field) == FIELD_DECL);
      const char * ident = IDENTIFIER_POINTER (DECL_NAME (field));
      debug ("calculating globl stack offset for <%s> from type <%s>!\n",
	     ident, IDENTIFIER_POINTER (TYPE_NAME (type)));

      tree element_size = TYPE_SIZE_UNIT (TREE_TYPE (field));
      tree offs = fold_build2_loc (UNKNOWN_LOCATION, MULT_EXPR, sizetype,
				   build_int_cst (sizetype, offset),
				   element_size);
      tree addr = fold_build2_loc (UNKNOWN_LOCATION, POINTER_PLUS_EXPR,
				   TREE_TYPE (stack_pointer),
				   stack_pointer, offs);
      gcc_assert (!gpy_dd_hash_insert (gpy_dd_hash_string (ident), addr, globls_symbols));
      offset--;
      field_count++;
    }
  append_to_statement_list (GPY_RR_extend_globl_stack (build_int_cst (integer_type_node,
								      field_count)),
			    cblock);
}

/* 
   Creates a DECL_CHAIN of stmts to fold the scalar 
   with the last tree being the address of the primitive 
*/
static
tree gpy_dot_pass_genericify_scalar (gpy_dot_tree_t * decl, tree * cblock)
{
  tree retval = error_mark_node;

  gcc_assert (DOT_TYPE (decl) == D_PRIMITIVE);
  gcc_assert (DOT_lhs_T (decl) == D_TD_COM);

  switch (DOT_lhs_TC (decl)->T)
    {
    case D_T_INTEGER:
      {
        retval = build_decl (UNKNOWN_LOCATION, VAR_DECL, create_tmp_var_name ("P"),
                             gpy_object_type_ptr);
        append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr, retval,
                                          GPY_RR_fold_integer (build_int_cst (integer_type_node,
									      DOT_lhs_TC (decl)->o.integer)
							       )),
                                  cblock);
      }
      break;

    default:
      error ("invalid scalar type!\n");
      break;
    }

  return retval;
}

static
void gpy_dot_pass_genericify_print_stmt (gpy_dot_tree_t * decl, tree * block,
					 VEC(gpy_context_t,gc) * context)
{
  gpy_dot_tree_t * arguments = decl->opa.t;
  
  VEC(tree,gc) * callvec_tmp = VEC_alloc (tree,gc,0);
  gpy_dot_tree_t * it = NULL;
  for (it = arguments; it != NULL; it = DOT_CHAIN (it))
    {
      VEC_safe_push (tree, gc, callvec_tmp,
		     gpy_dot_pass_lower_expr (it, block, context));
    }
  VEC(tree,gc) * callvec = VEC_alloc (tree,gc,0);
  VEC_safe_push (tree, gc, callvec, build_int_cst (integer_type_node, 1));
  VEC_safe_push (tree, gc, callvec, build_int_cst (integer_type_node, VEC_length (tree, callvec_tmp)));

  GPY_VEC_stmts_append (tree, callvec, callvec_tmp);

  append_to_statement_list (GPY_RR_eval_print (callvec), block);
}

static
int gpy_dot_pass_push_decl (tree decl, const char * ident,
			    gpy_context_t context)
{
  int retval = 0;
  gpy_hashval_t h = gpy_dd_hash_string (ident);
  void ** slot = gpy_dd_hash_insert (h, decl, context);
  if (!slot)
    {
      debug ("pushed decl <%s> into context!\n", ident);
      retval = 1;
    }
  else
    {
      debug ("error pushing decl <%s>!\n", ident);
    }
  return retval;
}

static
tree gpy_dot_pass_decl_lookup (VEC(gpy_context_t,gc) * context,
			       const char * identifier)
{
  tree retval = error_mark_node;
  gpy_hashval_t h = gpy_dd_hash_string (identifier);

  int idx, length = VEC_length (gpy_context_t, context);
  for (idx = length - 1; idx >= 0; --idx)
    {
      gpy_context_t ctx = VEC_index (gpy_context_t, context, idx);
      gpy_hash_entry_t * o = NULL;
      o = gpy_dd_hash_lookup_table (ctx, h);
      if (o)
	{
	  if (o->data)
	    {
	      debug ("found decl <%s>!\n", identifier);
	      retval = (tree) o->data;
	      break;
	    }
	}
    }
  return retval;
}

static
tree gpy_dot_pass_genericify_modify (gpy_dot_tree_t * decl, tree * block,
				     VEC(gpy_context_t,gc) * context)
{
  tree retval = error_mark_node;
  gpy_dot_tree_t * lhs = DOT_lhs_TT (decl);
  gpy_dot_tree_t * rhs = DOT_rhs_TT (decl);

  /*
    We dont handle full target lists yet
    all targets are in the lhs tree.
   
    To implment a target list such as:
    x,y,z = 1

    The lhs should be a DOT_CHAIN of identifiers!
    So we just iterate over them and deal with it as such!
   */

  if (DOT_TYPE (lhs) == D_IDENTIFIER)
    {
      tree addr = gpy_dot_pass_decl_lookup (context, DOT_IDENTIFIER_POINTER (lhs));
      if (!addr)
	{
	  /* since not previously declared we need to declare the variable! */
	  gpy_hash_tab_t * current_context = VEC_index (gpy_context_t, context,
							(VEC_length (gpy_context_t, context) - 1));
	  addr = build_decl (UNKNOWN_LOCATION, VAR_DECL,
			     get_identifier (DOT_IDENTIFIER_POINTER (lhs)),
			     gpy_object_type_ptr);
	  if (!gpy_dot_pass_push_decl (addr, DOT_IDENTIFIER_POINTER (lhs), 
				       current_context))
	    error ("error pushing decl <%s>!\n", IDENTIFIER_POINTER (DECL_NAME (addr)));
	}
      gcc_assert (addr != error_mark_node);
      tree addr_rhs_tree = gpy_dot_pass_lower_expr (rhs, block, context);
      
      switch (TREE_CODE (addr))
	{
	case VAR_DECL:
	  {
	    debug ("var_decl assign!\n");
	    append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr,
					      addr,
					      addr_rhs_tree),
				      block);
	    retval = addr;
	  }
	  break;

	default:
	  {
	    tree tmp = build_decl (UNKNOWN_LOCATION, VAR_DECL,
				   create_tmp_var_name ("A"),
				   gpy_object_type_ptr_ptr);
	    append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr_ptr,
					      tmp,
					      addr),
				      block);
	    tree refaddr = build_fold_indirect_ref (tmp);
	    append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr_ptr,
					      refaddr,
					      addr_rhs_tree),
				      block);
	    retval = refaddr;
	  }
	  break;
	}
      append_to_statement_list (GPY_RR_incr_ref_count (retval), block);
    }
  else
    fatal_error ("unhandled modify target type!\n");
  
  return retval;
}

static
tree gpy_dot_pass_genericify_binary_op (gpy_dot_tree_t * decl, tree * block,
					VEC(gpy_context_t,gc) * context)
{
  tree retval = error_mark_node;

  gcc_assert (DOT_T_FIELD (decl) == D_D_EXPR);

  gpy_dot_tree_t * lhs = DOT_lhs_TT (decl);
  gpy_dot_tree_t * rhs = DOT_rhs_TT (decl);

  tree lhs_eval = gpy_dot_pass_lower_expr (lhs, block, context);
  tree rhs_eval = gpy_dot_pass_lower_expr (rhs, block, context);

  tree op = error_mark_node;
  switch (DOT_TYPE (decl))
    {
    case D_ADD_EXPR:
      op = GPY_RR_eval_expression (lhs_eval, rhs_eval, build_int_cst (integer_type_node,
								      DOT_TYPE (decl))
				   );
      break;

      // .... THE REST OF THE BIN OPERATORS 

    default:
      error ("unhandled binary operation type!\n");
      break;
    }
  gcc_assert (op != error_mark_node);
  
  tree retaddr = build_decl (UNKNOWN_LOCATION, VAR_DECL, create_tmp_var_name ("T"),
                             gpy_object_type_ptr);
  append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr, retaddr, op),
			    block);
  retval = retaddr;

  return retval;
}

static
tree gpy_dot_pass_lower_expr (gpy_dot_tree_t * decl, tree * block,
			      VEC(gpy_context_t,gc) * context)
{
  tree retval = error_mark_node;

  switch (DOT_TYPE (decl))
    {
    case D_PRIMITIVE:
      retval = gpy_dot_pass_genericify_scalar (decl, block);
      break;

    case D_IDENTIFIER:
      retval = gpy_dot_pass_decl_lookup (context, DOT_IDENTIFIER_POINTER (decl));
      break;

    default:
      {
	switch (DOT_TYPE (decl))
          {
          case D_MODIFY_EXPR:
            retval = gpy_dot_pass_genericify_modify (decl, block, context);
            break;

          case D_ADD_EXPR:
            retval = gpy_dot_pass_genericify_binary_op (decl, block, context);
            break;

            // ... the rest of the binary operators!

          default:
            error ("unhandled operation type!\n");
            break;
          }
      }
      break;
    }

  return retval;
}

static
tree gpy_dot_pass_genericify_toplevl_functor_decl (gpy_dot_tree_t * decl,
						   VEC(gpy_context_t, gc) * context)
{
  tree fntype = build_function_type_list (void_type_node,
					  /* Arguments .. */
					  NULL_TREE);
  tree ident = GPY_dot_pass_genericify_gen_concat_identifier (GPY_current_module_name,
							      DOT_IDENTIFIER_POINTER (DOT_FIELD (decl))
							      );
  tree fndecl = build_decl (BUILTINS_LOCATION, FUNCTION_DECL, ident, fntype);
  debug ("lowering toplevel function <%s> to <%s>!\n", DOT_IDENTIFIER_POINTER (DOT_FIELD (decl)),
	 IDENTIFIER_POINTER (ident));

  TREE_STATIC (fndecl) = 1;
  TREE_PUBLIC (fndecl) = 1;
  
  /* Define the return type (represented by RESULT_DECL) for the main functin */
  tree resdecl = build_decl(BUILTINS_LOCATION, RESULT_DECL,
			    NULL_TREE, TREE_TYPE(fntype));
  DECL_CONTEXT (resdecl) = fndecl;
  DECL_ARTIFICIAL (resdecl) = true;
  DECL_IGNORED_P (resdecl) = true;

  DECL_RESULT (fndecl) = resdecl;

  /* This is usually used for debugging purpose. this is currently unused */
  tree block = build_block (NULL_TREE, NULL_TREE, NULL_TREE, NULL_TREE);
  DECL_INITIAL (fndecl) = block;

  tree stmts = alloc_stmt_list ();

  /*
    lower the function suite here and append all initilization
  */
  tree declare_vars = resdecl;

  tree bind = NULL_TREE;
  tree bl = build_block (declare_vars, NULL_TREE, fndecl, NULL_TREE);
  DECL_INITIAL (fndecl) = bl;
  TREE_USED (bl) = 1;

  bind = build3 (BIND_EXPR, void_type_node, BLOCK_VARS(bl),
		 NULL_TREE, bl);
  TREE_SIDE_EFFECTS (bind) = 1;

  /* Finalize the main function */
  BIND_EXPR_BODY(bind) = stmts;
  stmts = bind;
  DECL_SAVED_TREE (fndecl) = stmts;
  
  gimplify_function_tree (fndecl);
  cgraph_finalize_function (fndecl, false);

  return fndecl;
}

static
tree gpy_dot_pass_genericify_class_method_attrib (gpy_dot_tree_t * decl,
						  const char * parent_ident,
						  VEC(gpy_context_t,gc) * context)
{
  tree fntype = build_function_type_list (void_type_node,
					  /* Arguments .. */
					  NULL_TREE);
  tree ident = GPY_dot_pass_genericify_gen_concat_identifier (GPY_current_module_name,
							      parent_ident);
  ident = GPY_dot_pass_genericify_gen_concat_identifier (IDENTIFIER_POINTER (ident),
							 DOT_IDENTIFIER_POINTER (DOT_FIELD (decl)));

  tree fndecl = build_decl (BUILTINS_LOCATION, FUNCTION_DECL, ident, fntype);
  debug ("lowering class attribute <%s> to <%s>!\n", DOT_IDENTIFIER_POINTER (DOT_FIELD (decl)),
	 IDENTIFIER_POINTER (ident));

  TREE_STATIC (fndecl) = 1;
  TREE_PUBLIC (fndecl) = 1;
  
  /* Define the return type (represented by RESULT_DECL) for the main functin */
  tree resdecl = build_decl(BUILTINS_LOCATION, RESULT_DECL,
			    NULL_TREE, TREE_TYPE(fntype));
  DECL_CONTEXT (resdecl) = fndecl;
  DECL_ARTIFICIAL (resdecl) = true;
  DECL_IGNORED_P (resdecl) = true;

  DECL_RESULT (fndecl) = resdecl;

  /* This is usually used for debugging purpose. this is currently unused */
  tree block = build_block (NULL_TREE, NULL_TREE, NULL_TREE, NULL_TREE);
  DECL_INITIAL (fndecl) = block;

  tree stmts = alloc_stmt_list ();
  /*
    lower the function suite here and append all initilization
  */
  tree declare_vars = resdecl;

  tree bind = NULL_TREE;
  tree bl = build_block (declare_vars, NULL_TREE, fndecl, NULL_TREE);
  DECL_INITIAL (fndecl) = bl;
  TREE_USED (bl) = 1;

  bind = build3 (BIND_EXPR, void_type_node, BLOCK_VARS(bl),
		 NULL_TREE, bl);
  TREE_SIDE_EFFECTS (bind) = 1;

  /* Finalize the main function */
  BIND_EXPR_BODY(bind) = stmts;
  stmts = bind;
  DECL_SAVED_TREE (fndecl) = stmts;
  
  gimplify_function_tree (fndecl);
  cgraph_finalize_function (fndecl, false);

  return fndecl;
}

static
VEC(tree,gc) * gpy_dot_pass_genericify_toplevl_class_decl (gpy_dot_tree_t * decl,
							   VEC(gpy_context_t,gc) * context,
							   gpy_hash_tab_t * types)
{
  VEC(tree,gc) * lowered_decls = VEC_alloc (tree,gc,0);

  tree fntype = build_function_type_list (void_type_node,
					  /* Arguments .. */
					  NULL_TREE);
  tree ident = GPY_dot_pass_genericify_gen_concat_identifier (GPY_current_module_name,
								DOT_IDENTIFIER_POINTER (DOT_FIELD (decl))
								);
  ident = GPY_dot_pass_genericify_gen_concat_identifier (IDENTIFIER_POINTER (ident),
							   "__field_init__");
  tree fndecl = build_decl (BUILTINS_LOCATION, FUNCTION_DECL, ident, fntype);
  debug ("lowering toplevel class <%s> to <%s>!\n", DOT_IDENTIFIER_POINTER (DOT_FIELD (decl)),
	 IDENTIFIER_POINTER (ident));

  TREE_STATIC (fndecl) = 1;
  TREE_PUBLIC (fndecl) = 1;
  
  /* Define the return type (represented by RESULT_DECL) for the main functin */
  tree resdecl = build_decl(BUILTINS_LOCATION, RESULT_DECL,
			    NULL_TREE, TREE_TYPE(fntype));
  DECL_CONTEXT (resdecl) = fndecl;
  DECL_ARTIFICIAL (resdecl) = true;
  DECL_IGNORED_P (resdecl) = true;

  DECL_RESULT (fndecl) = resdecl;

  /* This is usually used for debugging purpose. this is currently unused */
  tree block = build_block (NULL_TREE, NULL_TREE, NULL_TREE, NULL_TREE);
  DECL_INITIAL (fndecl) = block;

  tree stmts = alloc_stmt_list ();

  /*
    lower the function suite here and append all initilization
  */
  gpy_dot_tree_t * class_suite = DOT_lhs_TT (decl);
  gpy_dot_tree_t * node = class_suite;
  do {
    if (DOT_T_FIELD (node) ==  D_D_EXPR)
      {
	// append to stmt list as this goes into the module initilizer...
	gpy_dot_pass_lower_expr (node, &block, context);
	continue;
      }
    
    switch (DOT_TYPE (node))
      {
      case D_PRINT_STMT:
	gpy_dot_pass_genericify_print_stmt (node, &block, context);
	break;
	
      case D_STRUCT_METHOD:
	{
	  tree a = gpy_dot_pass_genericify_class_method_attrib (node,
								DOT_IDENTIFIER_POINTER (DOT_FIELD (decl)),
								context);
	  VEC_safe_push (tree, gc, lowered_decls, a);
	}
	break;

      default:
	break;
      }
  } while ((node = DOT_CHAIN (node)));
  
  tree declare_vars = resdecl;

  tree bind = NULL_TREE;
  tree bl = build_block (declare_vars, NULL_TREE, fndecl, NULL_TREE);
  DECL_INITIAL (fndecl) = bl;
  TREE_USED (bl) = 1;

  bind = build3 (BIND_EXPR, void_type_node, BLOCK_VARS(bl),
		 NULL_TREE, bl);
  TREE_SIDE_EFFECTS (bind) = 1;

  /* Finalize the main function */
  BIND_EXPR_BODY(bind) = stmts;
  stmts = bind;
  DECL_SAVED_TREE (fndecl) = stmts;
  
  gimplify_function_tree (fndecl);
  cgraph_finalize_function (fndecl, false);

  VEC_safe_push (tree, gc, lowered_decls, fndecl);

  return lowered_decls;
}

static
VEC(tree,gc) * gpy_dot_pass_genericify_TU (gpy_hash_tab_t * modules,
					   VEC(gpydot,gc) * decls)
{
  VEC(tree,gc) * lowered_decls = VEC_alloc (tree, gc, 0);
  
  gpy_hash_tab_t toplvl, topnxt;
  gpy_dd_hash_init_table (&toplvl);
  gpy_dd_hash_init_table (&topnxt);

  tree main_init_module = gpy_dot_pass_generificify_get_module_type (GPY_current_module_name,
								     modules);
  tree pdecl_t = build_pointer_type (main_init_module);

  tree fntype = build_function_type_list (void_type_node,
					  NULL_TREE);
  tree ident = GPY_dot_pass_genericify_gen_concat_identifier (GPY_current_module_name,
								"__field_init__");
  tree fndecl = build_decl (BUILTINS_LOCATION, FUNCTION_DECL, ident, fntype);
  debug ("lowering toplevel module <%s> to <%s>!\n", GPY_current_module_name,
	 IDENTIFIER_POINTER (ident));

  TREE_STATIC (fndecl) = 1;
  TREE_PUBLIC (fndecl) = 1;
  
  tree argslist = NULL_TREE;
  DECL_ARGUMENTS (fndecl) = argslist;

  /* Define the return type (represented by RESULT_DECL) for the main functin */
  tree resdecl = build_decl (BUILTINS_LOCATION, RESULT_DECL,
			     NULL_TREE, TREE_TYPE(fntype));
  DECL_CONTEXT (resdecl) = fndecl;
  DECL_ARTIFICIAL (resdecl) = true;
  DECL_IGNORED_P (resdecl) = true;

  DECL_RESULT (fndecl) = resdecl;

  /* This is usually used for debugging purpose. this is currently unused */
  tree block = build_block (NULL_TREE, NULL_TREE, NULL_TREE, NULL_TREE);
  DECL_INITIAL (fndecl) = block;

  tree stmts = alloc_stmt_list ();

  VEC(gpy_context_t,gc) * context = VEC_alloc (gpy_context_t, gc, 0);
  VEC_safe_push (gpy_context_t, gc, context, &toplvl);
  VEC_safe_push (gpy_context_t, gc, context, &topnxt);

  gpy_dot_pass_genericify_setup_runtime_globls (&toplvl);
  gpy_dot_pass_genericify_create_offsets_globl_context (main_init_module,
							&stmts,
							context);
  int idx;
  gpy_dot_tree_t * idtx = NULL_DOT;
  /*
    Iterating over the DOT IL to lower/generate the GENERIC code
    required to compile the stmts and decls
  */
  for (idx = 0; VEC_iterate (gpydot, decls, idx, idtx); ++idx)
    {
      if (DOT_T_FIELD (idtx) ==  D_D_EXPR)
	{
	  // append to stmt list as this goes into the module initilizer...
          gpy_dot_pass_lower_expr (idtx, &stmts, context);
          continue;
	}

      switch (DOT_TYPE (idtx))
        {
	case D_PRINT_STMT:
	  gpy_dot_pass_genericify_print_stmt (idtx, &stmts, context);
	  break;

        case D_STRUCT_METHOD:
          {
	    tree t = gpy_dot_pass_genericify_toplevl_functor_decl (idtx, context);
	    VEC_safe_push (tree, gc, lowered_decls, t);
          }
          break;

	case D_STRUCT_CLASS:
	  {
	    VEC(tree,gc) * cdecls = gpy_dot_pass_genericify_toplevl_class_decl (idtx, context,
										modules);
	    GPY_VEC_stmts_append (tree, lowered_decls, cdecls);
	  }
	  break;

        default:
          fatal_error ("unhandled dot tree code <%i>!\n", DOT_TYPE (idtx));
          break;
        }
    }

  tree declare_vars = resdecl;

  tree bind = NULL_TREE;
  tree bl = build_block (declare_vars, NULL_TREE, fndecl, NULL_TREE);
  DECL_INITIAL (fndecl) = bl;
  TREE_USED (bl) = 1;

  bind = build3 (BIND_EXPR, void_type_node, BLOCK_VARS(bl),
		 NULL_TREE, bl);
  TREE_SIDE_EFFECTS (bind) = 1;

  /* Finalize the main function */
  BIND_EXPR_BODY(bind) = stmts;
  stmts = bind;
  DECL_SAVED_TREE (fndecl) = stmts;
  
  gimplify_function_tree (fndecl);
  cgraph_finalize_function (fndecl, false);

  VEC_safe_push (tree, gc, lowered_decls, fndecl);

  return lowered_decls;
}

VEC(tree,gc) * gpy_dot_pass_genericify (VEC(tree,gc) *modules,
					VEC(gpydot,gc) *decls)
{
  VEC(tree,gc) * retval = VEC_alloc (tree, gc, 0);

  gpy_hash_tab_t module_ctx;
  gpy_dd_hash_init_table (&module_ctx);

  int idx;
  tree itx = NULL_TREE;
  for (idx = 0; VEC_iterate (tree, modules, idx, itx); ++idx)
    {
      debug ("hashing module name <%s>!\n", IDENTIFIER_POINTER (TYPE_NAME(itx)));
      gpy_hashval_t h = gpy_dd_hash_string (IDENTIFIER_POINTER (TYPE_NAME(itx)));
      void ** e = gpy_dd_hash_insert (h, itx, &module_ctx);
      
      if (e)
        error ("module <%s> is already defined!\n",
	       IDENTIFIER_POINTER (DECL_NAME (itx))
	       );
    }

  debug ("genericification...!\n");
  retval = gpy_dot_pass_genericify_TU (&module_ctx, decls);
  debug ("finishing genericification!\n");

  if (module_ctx.array)
    free (module_ctx.array);

  return retval;
}
