//
//  sub_ext.c
//  QLMarkdown
//
//  Created by Sbarex on 04/08/24.
//

#include "sub_ext.h"
#include <parser.h>
#include <render.h>

cmark_node_type CMARK_NODE_SUB;

static cmark_node *match(cmark_syntax_extension *self, cmark_parser *parser,
                         cmark_node *parent, unsigned char character,
                         cmark_inline_parser *inline_parser) {
  if (character != '~')
    return NULL;
  
  char buffer[101];
  int left_flanking, right_flanking, punct_before, punct_after, delims;
  
  int pos = cmark_inline_parser_get_offset(inline_parser);

  delims = cmark_inline_parser_scan_delimiters(
      inline_parser, sizeof(buffer) - 1, '~',
      &left_flanking,
      &right_flanking, &punct_before, &punct_after);

  if (!((left_flanking || right_flanking) && delims == 1)) {
    // Restore the original pos, allow to other extensions to process the same character.
    cmark_inline_parser_set_offset(inline_parser, pos);
    return NULL;
  }

  memset(buffer, '~', delims);
  buffer[delims] = 0;

  cmark_node *res = NULL;
  res = cmark_node_new_with_mem(CMARK_NODE_TEXT, parser->mem);
  cmark_node_set_literal(res, buffer);
  res->start_line = res->end_line = cmark_inline_parser_get_line(inline_parser);
  res->start_column = cmark_inline_parser_get_column(inline_parser) - delims;

  if ((left_flanking || right_flanking) && delims == 1) {
    cmark_inline_parser_push_delimiter(inline_parser, '!', left_flanking, right_flanking, res); // Use a fake delimiter (`!`) to prevent conflict with strikethroungh extension.
  }

  return res;
}

static delimiter *insert(cmark_syntax_extension *self, cmark_parser *parser,
                         cmark_inline_parser *inline_parser, delimiter *opener,
                         delimiter *closer) {
  cmark_node *sub;
  cmark_node *tmp, *next;
  delimiter *delim, *tmp_delim;
  delimiter *res = closer->next;

  sub = opener->inl_text;

  if (opener->inl_text->as.literal.len != closer->inl_text->as.literal.len)
    goto done;

  if (!cmark_node_set_type(sub, CMARK_NODE_SUB))
    goto done;

  cmark_node_set_syntax_extension(sub, self);

  tmp = cmark_node_next(opener->inl_text);

  while (tmp) {
    if (tmp == closer->inl_text)
      break;
    next = cmark_node_next(tmp);
    cmark_node_append_child(sub, tmp);
    tmp = next;
  }

  sub->end_column = closer->inl_text->start_column + closer->inl_text->as.literal.len - 1;
  cmark_node_free(closer->inl_text);

done:
  delim = closer;
  while (delim != NULL && delim != opener) {
    tmp_delim = delim->previous;
    cmark_inline_parser_remove_delimiter(inline_parser, delim);
    delim = tmp_delim;
  }

  cmark_inline_parser_remove_delimiter(inline_parser, opener);

  return res;
}

static const char *get_type_string(cmark_syntax_extension *extension,
                                   cmark_node *node) {
  return node->type == CMARK_NODE_SUB ? "sub" : "<unknown>";
}

static int can_contain(cmark_syntax_extension *extension, cmark_node *node,
                       cmark_node_type child_type) {
  if (node->type != CMARK_NODE_SUB)
    return false;

  return CMARK_NODE_TYPE_INLINE_P(child_type);
}

static void commonmark_render(cmark_syntax_extension *extension,
                              cmark_renderer *renderer, cmark_node *node,
                              cmark_event_type ev_type, int options) {
  renderer->out(renderer, node, "~", false, LITERAL);
}

static void html_render(cmark_syntax_extension *extension,
                        cmark_html_renderer *renderer, cmark_node *node,
                        cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  if (entering) {
    cmark_strbuf_puts(renderer->html, "<sub>");
  } else {
    cmark_strbuf_puts(renderer->html, "</sub>");
  }
}

static void plaintext_render(cmark_syntax_extension *extension,
                             cmark_renderer *renderer, cmark_node *node,
                             cmark_event_type ev_type, int options) {
  renderer->out(renderer, node, "~", false, LITERAL);
}

cmark_syntax_extension *create_sub_extension(void) {
  cmark_syntax_extension *ext = cmark_syntax_extension_new("sub");
  
  cmark_syntax_extension_set_get_type_string_func(ext, get_type_string);
  cmark_syntax_extension_set_can_contain_func(ext, can_contain);
  cmark_syntax_extension_set_commonmark_render_func(ext, commonmark_render);
  cmark_syntax_extension_set_html_render_func(ext, html_render);
  cmark_syntax_extension_set_plaintext_render_func(ext, plaintext_render);
  CMARK_NODE_SUB = cmark_syntax_extension_add_node(1);

  cmark_syntax_extension_set_match_inline_func(ext, match);
  cmark_syntax_extension_set_inline_from_delim_func(ext, insert);

  cmark_mem *mem = cmark_get_default_mem_allocator();
  cmark_llist *special_chars = NULL;
  special_chars = cmark_llist_append(mem, special_chars, (void *)'~');
  special_chars = cmark_llist_append(mem, special_chars, (void *)'!'); // register a fake marker to prevent conflict with the strikethroungh extension.
  cmark_syntax_extension_set_special_inline_chars(ext, special_chars);

  return ext;
}

