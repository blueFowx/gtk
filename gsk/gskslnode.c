/* GTK - The GIMP Toolkit
 *   
 * Copyright © 2017 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gskslnodeprivate.h"

#include "gskslexpressionprivate.h"
#include "gskslfunctionprivate.h"
#include "gskslpointertypeprivate.h"
#include "gskslpreprocessorprivate.h"
#include "gskslprinterprivate.h"
#include "gskslscopeprivate.h"
#include "gsksltokenizerprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvalueprivate.h"
#include "gskslvariableprivate.h"
#include "gskspvwriterprivate.h"

#include <string.h>

static GskSlNode *
gsk_sl_node_alloc (const GskSlNodeClass *klass,
                   gsize                 size)
{
  GskSlNode *node;

  node = g_slice_alloc0 (size);

  node->class = klass;
  node->ref_count = 1;

  return node;
}
#define gsk_sl_node_new(_name, _klass) ((_name *) gsk_sl_node_alloc ((_klass), sizeof (_name)))

/* EMPTY */

/* FIXME: This exists only so we dont return NULL from empty statements (ie just a semicolon)
 */

typedef struct _GskSlNodeEmpty GskSlNodeEmpty;

struct _GskSlNodeEmpty {
  GskSlNode parent;
};

static void
gsk_sl_node_empty_free (GskSlNode *node)
{
  GskSlNodeEmpty *empty = (GskSlNodeEmpty *) node;

  g_slice_free (GskSlNodeEmpty, empty);
}

static void
gsk_sl_node_empty_print (const GskSlNode *node,
                         GskSlPrinter    *printer)
{
}

static guint32
gsk_sl_node_empty_write_spv (const GskSlNode *node,
                             GskSpvWriter    *writer)
{
  return 0;
}

static const GskSlNodeClass GSK_SL_NODE_EMPTY = {
  gsk_sl_node_empty_free,
  gsk_sl_node_empty_print,
  gsk_sl_node_empty_write_spv
};

/* DECLARATION */

typedef struct _GskSlNodeDeclaration GskSlNodeDeclaration;

struct _GskSlNodeDeclaration {
  GskSlNode parent;

  GskSlVariable *variable;
  GskSlExpression *initial;
};

static void
gsk_sl_node_declaration_free (GskSlNode *node)
{
  GskSlNodeDeclaration *declaration = (GskSlNodeDeclaration *) node;

  gsk_sl_variable_unref (declaration->variable);
  if (declaration->initial)
    gsk_sl_expression_unref (declaration->initial);

  g_slice_free (GskSlNodeDeclaration, declaration);
}

static void
gsk_sl_node_declaration_print (const GskSlNode *node,
                               GskSlPrinter    *printer)
{
  GskSlNodeDeclaration *declaration = (GskSlNodeDeclaration *) node;

  gsk_sl_variable_print (declaration->variable, printer);
  if (declaration->initial)
    {
      gsk_sl_printer_append (printer, " = ");
      gsk_sl_expression_print (declaration->initial, printer);
    }
}

static guint32
gsk_sl_node_declaration_write_spv (const GskSlNode *node,
                                   GskSpvWriter    *writer)
{
  GskSlNodeDeclaration *declaration = (GskSlNodeDeclaration *) node;
  guint32 variable_id;

  variable_id = gsk_spv_writer_get_id_for_variable (writer, declaration->variable);
  
  if (declaration->initial && ! gsk_sl_variable_get_initial_value (declaration->variable))
    {
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_CODE,
                          3, GSK_SPV_OP_STORE,
                          (guint32[2]) { variable_id,
                                         gsk_sl_expression_write_spv (declaration->initial, writer)});
    }

  return variable_id;
}

static const GskSlNodeClass GSK_SL_NODE_DECLARATION = {
  gsk_sl_node_declaration_free,
  gsk_sl_node_declaration_print,
  gsk_sl_node_declaration_write_spv
};

/* RETURN */

typedef struct _GskSlNodeReturn GskSlNodeReturn;

struct _GskSlNodeReturn {
  GskSlNode parent;

  GskSlExpression *value;
};

static void
gsk_sl_node_return_free (GskSlNode *node)
{
  GskSlNodeReturn *return_node = (GskSlNodeReturn *) node;

  if (return_node->value)
    gsk_sl_expression_unref (return_node->value);

  g_slice_free (GskSlNodeReturn, return_node);
}

static void
gsk_sl_node_return_print (const GskSlNode *node,
                          GskSlPrinter    *printer)
{
  GskSlNodeReturn *return_node = (GskSlNodeReturn *) node;

  gsk_sl_printer_append (printer, "return");
  if (return_node->value)
    {
      gsk_sl_printer_append (printer, " ");
      gsk_sl_expression_print (return_node->value, printer);
    }
}

static guint32
gsk_sl_node_return_write_spv (const GskSlNode *node,
                              GskSpvWriter    *writer)
{
  g_assert_not_reached ();

  return 0;
}

static const GskSlNodeClass GSK_SL_NODE_RETURN = {
  gsk_sl_node_return_free,
  gsk_sl_node_return_print,
  gsk_sl_node_return_write_spv
};

/* EXPRESSION */
 
typedef struct _GskSlNodeExpression GskSlNodeExpression;

struct _GskSlNodeExpression {
  GskSlNode parent;

  GskSlExpression *expression;
};

static void
gsk_sl_node_expression_free (GskSlNode *node)
{
  GskSlNodeExpression *expression_node = (GskSlNodeExpression *) node;
 
  gsk_sl_expression_unref (expression_node->expression);
 
  g_slice_free (GskSlNodeExpression, expression_node);
}
 
static void
gsk_sl_node_expression_print (const GskSlNode *node,
                              GskSlPrinter    *printer)
{
  GskSlNodeExpression *expression_node = (GskSlNodeExpression *) node;

  gsk_sl_expression_print (expression_node->expression, printer);
}
 
static guint32
gsk_sl_node_expression_write_spv (const GskSlNode *node,
                                  GskSpvWriter    *writer)
{
  GskSlNodeExpression *expression_node = (GskSlNodeExpression *) node;

  return gsk_sl_expression_write_spv (expression_node->expression, writer);
}
 
static const GskSlNodeClass GSK_SL_NODE_EXPRESSION = {
  gsk_sl_node_expression_free,
  gsk_sl_node_expression_print,
  gsk_sl_node_expression_write_spv
};

/* API */

static GskSlNode *
gsk_sl_node_parse_declaration (GskSlScope        *scope,
                               GskSlPreprocessor *stream,
                               GskSlDecorations  *decoration,
                               GskSlType         *type)
{
  GskSlNodeDeclaration *declaration;
  GskSlPointerType *pointer_type;
  GskSlValue *value = NULL;
  const GskSlToken *token;
  char *name;

  declaration = gsk_sl_node_new (GskSlNodeDeclaration, &GSK_SL_NODE_DECLARATION);
  
  token = gsk_sl_preprocessor_get (stream);
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
    {
      name = g_strdup (token->str);
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) declaration);

      token = gsk_sl_preprocessor_get (stream);
      if (gsk_sl_token_is (token, GSK_SL_TOKEN_EQUAL))
        {
          GskSlValue *unconverted;

          gsk_sl_preprocessor_consume (stream, (GskSlNode *) declaration);
          declaration->initial = gsk_sl_expression_parse_assignment (scope, stream);
          if (!gsk_sl_type_can_convert (type, gsk_sl_expression_get_return_type (declaration->initial)))
            {
              gsk_sl_preprocessor_error (stream, TYPE_MISMATCH,
                                         "Cannot convert from initializer type %s to variable type %s",
                                         gsk_sl_type_get_name (gsk_sl_expression_get_return_type (declaration->initial)),
                                         gsk_sl_type_get_name (type));
              gsk_sl_expression_unref (declaration->initial);
              declaration->initial = NULL;
            }
          else
            {
              unconverted = gsk_sl_expression_get_constant (declaration->initial);
              if (unconverted)
                {
                  value = gsk_sl_value_new_convert (unconverted, type);
                  gsk_sl_value_free (unconverted);
                }
            }
        }
    }
  else
    {
      name = NULL;
      value = NULL;
    }

  pointer_type = gsk_sl_pointer_type_new (type, TRUE, decoration->values[GSK_SL_DECORATION_CALLER_ACCESS].value);
  declaration->variable = gsk_sl_variable_new (pointer_type, name, value, decoration->values[GSK_SL_DECORATION_CONST].set);
  gsk_sl_pointer_type_unref (pointer_type);
  gsk_sl_scope_add_variable (scope, declaration->variable);

  return (GskSlNode *) declaration;
}

GskSlNode *
gsk_sl_node_parse_statement (GskSlScope        *scope,
                             GskSlPreprocessor *preproc)
{
  const GskSlToken *token;
  GskSlNode *node;

  token = gsk_sl_preprocessor_get (preproc);

  switch ((guint) token->type)
  {
    case GSK_SL_TOKEN_SEMICOLON:
      node = (GskSlNode *) gsk_sl_node_new (GskSlNodeEmpty, &GSK_SL_NODE_EMPTY);
      break;

    case GSK_SL_TOKEN_EOF:
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Unexpected end of document");
      return (GskSlNode *) gsk_sl_node_new (GskSlNodeEmpty, &GSK_SL_NODE_EMPTY);

    case GSK_SL_TOKEN_CONST:
    case GSK_SL_TOKEN_IN:
    case GSK_SL_TOKEN_OUT:
    case GSK_SL_TOKEN_INOUT:
    case GSK_SL_TOKEN_INVARIANT:
    case GSK_SL_TOKEN_COHERENT:
    case GSK_SL_TOKEN_VOLATILE:
    case GSK_SL_TOKEN_RESTRICT:
    case GSK_SL_TOKEN_READONLY:
    case GSK_SL_TOKEN_WRITEONLY:
    case GSK_SL_TOKEN_VOID:
    case GSK_SL_TOKEN_FLOAT:
    case GSK_SL_TOKEN_DOUBLE:
    case GSK_SL_TOKEN_INT:
    case GSK_SL_TOKEN_UINT:
    case GSK_SL_TOKEN_BOOL:
    case GSK_SL_TOKEN_BVEC2:
    case GSK_SL_TOKEN_BVEC3:
    case GSK_SL_TOKEN_BVEC4:
    case GSK_SL_TOKEN_IVEC2:
    case GSK_SL_TOKEN_IVEC3:
    case GSK_SL_TOKEN_IVEC4:
    case GSK_SL_TOKEN_UVEC2:
    case GSK_SL_TOKEN_UVEC3:
    case GSK_SL_TOKEN_UVEC4:
    case GSK_SL_TOKEN_VEC2:
    case GSK_SL_TOKEN_VEC3:
    case GSK_SL_TOKEN_VEC4:
    case GSK_SL_TOKEN_DVEC2:
    case GSK_SL_TOKEN_DVEC3:
    case GSK_SL_TOKEN_DVEC4:
    case GSK_SL_TOKEN_MAT2:
    case GSK_SL_TOKEN_MAT3:
    case GSK_SL_TOKEN_MAT4:
    case GSK_SL_TOKEN_DMAT2:
    case GSK_SL_TOKEN_DMAT3:
    case GSK_SL_TOKEN_DMAT4:
    case GSK_SL_TOKEN_MAT2X2:
    case GSK_SL_TOKEN_MAT2X3:
    case GSK_SL_TOKEN_MAT2X4:
    case GSK_SL_TOKEN_MAT3X2:
    case GSK_SL_TOKEN_MAT3X3:
    case GSK_SL_TOKEN_MAT3X4:
    case GSK_SL_TOKEN_MAT4X2:
    case GSK_SL_TOKEN_MAT4X3:
    case GSK_SL_TOKEN_MAT4X4:
    case GSK_SL_TOKEN_DMAT2X2:
    case GSK_SL_TOKEN_DMAT2X3:
    case GSK_SL_TOKEN_DMAT2X4:
    case GSK_SL_TOKEN_DMAT3X2:
    case GSK_SL_TOKEN_DMAT3X3:
    case GSK_SL_TOKEN_DMAT3X4:
    case GSK_SL_TOKEN_DMAT4X2:
    case GSK_SL_TOKEN_DMAT4X3:
    case GSK_SL_TOKEN_DMAT4X4:
    case GSK_SL_TOKEN_STRUCT:
      {
        GskSlType *type;
        GskSlDecorations decoration;

its_a_type:
        gsk_sl_decoration_list_parse (scope,
                                      preproc,
                                      &decoration);

        type = gsk_sl_type_new_parse (scope, preproc);

        token = gsk_sl_preprocessor_get (preproc);

        if (token->type == GSK_SL_TOKEN_LEFT_PAREN)
          {
            GskSlNodeExpression *node_expression;
            GskSlFunction *constructor;
                
            constructor = gsk_sl_function_new_constructor (type);
            node_expression = gsk_sl_node_new (GskSlNodeExpression, &GSK_SL_NODE_EXPRESSION);
            if (gsk_sl_function_is_builtin_constructor (constructor))
              {
                node_expression->expression = gsk_sl_expression_parse_function_call (scope, preproc, NULL, constructor);
              }
            else
              {
                GskSlFunctionMatcher matcher;
                gsk_sl_function_matcher_init (&matcher, g_list_prepend (NULL, constructor));
                node_expression->expression = gsk_sl_expression_parse_function_call (scope, preproc, &matcher, constructor);
                gsk_sl_function_matcher_finish (&matcher);
              }
            node = (GskSlNode *) node_expression;
            gsk_sl_function_unref (constructor);
          }
        else
          {
            node = gsk_sl_node_parse_declaration (scope, preproc, &decoration, type);
          }

        gsk_sl_type_unref (type);
      }
      break;

    case GSK_SL_TOKEN_RETURN:
      {
        GskSlNodeReturn *return_node;
        GskSlType *return_type;

        return_node = gsk_sl_node_new (GskSlNodeReturn, &GSK_SL_NODE_RETURN);
        gsk_sl_preprocessor_consume (preproc, (GskSlNode *) return_node);
        token = gsk_sl_preprocessor_get (preproc);
        if (!gsk_sl_token_is (token, GSK_SL_TOKEN_SEMICOLON))
          return_node->value = gsk_sl_expression_parse (scope, preproc);

        return_type = gsk_sl_scope_get_return_type (scope);
        node = (GskSlNode *) return_node;

        if (return_type == NULL)
          {
            gsk_sl_preprocessor_error (preproc, SCOPE, "Cannot return from here.");
          }
        else if (return_node->value == NULL)
          {
            if (!gsk_sl_type_equal (return_type, gsk_sl_type_get_scalar (GSK_SL_VOID)))
              {
                gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,"Functions expectes a return value of type %s", gsk_sl_type_get_name (return_type));
              }
          }
        else
          {
            if (gsk_sl_type_equal (return_type, gsk_sl_type_get_scalar (GSK_SL_VOID)))
              {
                gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Cannot return a value from a void function.");
              }
            else if (!gsk_sl_type_can_convert (return_type, gsk_sl_expression_get_return_type (return_node->value)))
              {
                gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                           "Cannot convert type %s to return type %s.",
                                           gsk_sl_type_get_name (gsk_sl_expression_get_return_type (return_node->value)),
                                           gsk_sl_type_get_name (return_type));
                break;
              }
            }
        }
      break;

    case GSK_SL_TOKEN_IDENTIFIER:
      if (gsk_sl_scope_lookup_type (scope, token->str))
        goto its_a_type;
      /* else fall through*/

    default:
      {
        GskSlNodeExpression *node_expression;

        node_expression = gsk_sl_node_new (GskSlNodeExpression, &GSK_SL_NODE_EXPRESSION);
        node_expression->expression = gsk_sl_expression_parse (scope, preproc);

        node = (GskSlNode *) node_expression;
      }
      break;
  }

  token = gsk_sl_preprocessor_get (preproc);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_SEMICOLON))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "No semicolon at end of statement.");
      gsk_sl_preprocessor_sync (preproc, GSK_SL_TOKEN_SEMICOLON);
    }
  gsk_sl_preprocessor_consume (preproc, (GskSlNode *) node);

  return node;
}

GskSlNode *
gsk_sl_node_ref (GskSlNode *node)
{
  g_return_val_if_fail (node != NULL, NULL);

  node->ref_count += 1;

  return node;
}

void
gsk_sl_node_unref (GskSlNode *node)
{
  if (node == NULL)
    return;

  node->ref_count -= 1;
  if (node->ref_count > 0)
    return;

  node->class->free (node);
}

void
gsk_sl_node_print (const GskSlNode *node,
                   GskSlPrinter    *printer)
{
  node->class->print (node, printer);
}

guint32
gsk_sl_node_write_spv (const GskSlNode *node,
                       GskSpvWriter    *writer)
{
  return node->class->write_spv (node, writer);
}
