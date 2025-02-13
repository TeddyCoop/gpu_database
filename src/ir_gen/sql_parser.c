internal B32
sql_is_string_keyword(String8 string)
{
  for (U32 i = 0; i < ArrayCount(g_sql_keywords); i++)
  {
    if (str8_match(g_sql_keywords[i], string, StringMatchFlag_CaseInsensitive))
    {
      return 1;
    }
  }
  return 0;
}

internal B32
sql_is_string_operator(String8 string)
{
  for (U32 i = 0; i < ArrayCount(g_sql_operators); i++)
  {
    if (str8_match(g_sql_operators[i], string, StringMatchFlag_CaseInsensitive))
    {
      return 1;
    }
  }
  return 0;
}

internal B32
sql_is_string_symbol(String8 string)
{
  for (U32 i = 0; i < ArrayCount(g_sql_symbols); i++)
  {
    if (str8_match(g_sql_symbols[i], string, StringMatchFlag_CaseInsensitive))
    {
      return 1;
    }
  }
  return 0;
}


internal SQL_TokenizeResult
sql_tokenize_from_text(Arena* arena, String8 text)
{
  SQL_Token* tokens = push_array(arena, SQL_Token, 256);
  U64 token_count = 0;
  
  U64 pos = 0;
  while (pos < text.size)
  {
    while (pos < text.size && char_is_space(text.str[pos]))
    {
      pos++;
    }
    
    if (pos >= text.size) break;
    
    U64 start = pos;
    
    String8 atoken_value = str8_substr(text, r1u64(start, pos));
    // tec: keywords and identifiers
    if (char_is_alpha(text.str[pos]))
    {
      while (pos < text.size && ((char_is_digit(text.str[pos], 10) || char_is_alpha(text.str[pos])) || text.str[pos] == '_'))
      {
        pos++;
      }
      String8 token_value = str8_substr(text, r1u64(start, pos));
      tokens[token_count++] = (SQL_Token)
      {
        .type = sql_is_string_keyword(token_value) ? SQL_TokenType_Keyword : SQL_TokenType_Identifier,
        .value = token_value,
        .range = r1u64(start, pos),
      };
    }
    else if (char_is_digit(text.str[pos], 10)) 
    {
      while (pos < text.size && char_is_digit(text.str[pos], 10))
      {
        pos++;
      }
      String8 token_value = str8_substr(text, r1u64(start, pos));
      tokens[token_count++] = (SQL_Token)
      {
        .type = SQL_TokenType_Number,
        .value = token_value,
        .range = r1u64(start, pos)
      };
    }
    else if (sql_is_string_symbol(str8_substr(text, r1u64(pos, pos+1))))
    {
      tokens[token_count++] = (SQL_Token) 
      {
        .type = SQL_TokenType_Symbol,
        .value = str8_substr(text, r1u64(pos, pos + 1)),
        .range = r1u64(pos, pos + 1)
      };
      pos++;
    }
    else
    {
      while (pos < text.size &&
             !char_is_space(text.str[pos]) && 
             !char_is_alpha(text.str[pos]) && !char_is_digit(text.str[pos], 10) && !sql_is_string_symbol(str8_substr(text, r1u64(pos, pos+1)))) 
      {
        pos++;
      }
      String8 token_value = str8_substr(text, r1u64(start, pos));
      if (sql_is_string_operator(token_value))
      {
        tokens[token_count++] = (SQL_Token) 
        {
          .type = SQL_TokenType_Operator,
          .value = token_value,
          .range = r1u64(start, pos)
        };
      }
    }
  }
  
  SQL_TokenizeResult result = { 0 };
  result.tokens = tokens;
  result.count = token_count;
  
  return result;
}

internal void
sql_tokens_print(SQL_TokenizeResult tokens)
{
  for (U64 i = 0; i < tokens.count; i++)
  {
    SQL_Token* token = &tokens.tokens[i];
    printf("%.*s\n", (int)token->value.size, token->value.str);
  }
}

//~ tec: ast
internal SQL_Node*
sql_parse(Arena* arena, SQL_Token* tokens, U64 token_count)
{
  U64 token_index = 0;
  
  
  if (tokens[token_index].type != SQL_TokenType_Keyword ||
      !str8_match(tokens[token_index].value, str8_lit("use"), StringMatchFlag_CaseInsensitive))
  {
    log_error("expected 'use' keyword at beginning of the query");
    return NULL;
  }
  
  
  SQL_Node *use_node = sql_parse_use_clause(arena, &tokens, &token_index, token_count);
  if (!use_node)
  {
    log_error("Failed to parse use clause.");
    return NULL;
  }
  
  SQL_Node *select_node = sql_parse_select_clause(arena, &tokens, &token_index, token_count);
  if (!select_node)
  {
    log_error("Failed to parse SELECT clause.");
    return NULL;
  }
  
  // tec: attach select to use
  use_node->next = select_node;
  select_node->parent = use_node;
  
  // Parse the FROM clause
  SQL_Node *from_node = sql_parse_from_clause(arena, &tokens, &token_index, token_count);
  if (!from_node) 
  {
    log_error("Failed to parse FROM clause.");
    return NULL;
  }
  
  // Attach the FROM node to the SELECT node
  select_node->next = from_node;
  from_node->parent = select_node;
  
  // Parse the WHERE clause (if it exists)
  if (token_index < token_count &&
      tokens[token_index].type == SQL_TokenType_Keyword &&
      str8_match(tokens[token_index].value, str8_lit("where"), StringMatchFlag_CaseInsensitive))
  {
    token_index++;
    SQL_Node *where_node = sql_parse_where_clause(arena, &tokens, &token_index, token_count);
    if (!where_node) 
    {
      log_error("Failed to parse WHERE clause.");
      return NULL;
    }
    from_node->next = where_node;
    where_node->prev = from_node;
    where_node->parent = select_node;
  }
  
  return use_node;
}

internal SQL_Node*
sql_parse_use_clause(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count)
{
  (*token_index)++;
  
  SQL_Node* use_node = push_array(arena, SQL_Node, 1);
  use_node->type = SQL_NodeType_Use;
  
  SQL_Node* tail = NULL;
  
  while (*token_index < token_count)
  {
    SQL_Token *token = &(*tokens)[*token_index];
    
    if (token->type == SQL_TokenType_Identifier) 
    {
      SQL_Node *column_node = push_array(arena, SQL_Node, 1);
      column_node->type = SQL_NodeType_Database;
      column_node->value = token->value;
      column_node->parent = use_node;
      
      if (!use_node->first) 
      {
        use_node->first = use_node->last = column_node;
      } 
      else 
      {
        tail->next = column_node;
        column_node->prev = tail;
        use_node->last = column_node;
      }
      
      tail = column_node;
      (*token_index)++;
      
      // Handle comma between columns
      if (*token_index < token_count && (*tokens)[*token_index].type == SQL_TokenType_Symbol &&
          str8_match((*tokens)[*token_index].value, str8_lit(","), 0))
      {
        (*token_index)++;
      } 
      else 
      {
        break;
      }
    } 
    else 
    {
      log_error("Expected table name in use clause, but found '%.*s'.",
                token->value.size, token->value.str);
      return NULL;
    }
  }
  
  return use_node;
}

internal SQL_Node*
sql_parse_select_clause(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count)
{
  (*token_index)++; // Skip the SELECT keyword
  (*token_index)++; // Skip the SELECT keyword
  
  SQL_Node* select_node = push_array(arena, SQL_Node, 1);
  select_node->type = SQL_NodeType_Select;
  
  SQL_Node* tail = NULL;
  
  while (*token_index < token_count)
  {
    SQL_Token *token = &(*tokens)[*token_index];
    
    if (token->type == SQL_TokenType_Identifier) 
    {
      SQL_Node *column_node = push_array(arena, SQL_Node, 1);
      column_node->type = SQL_NodeType_Column;
      column_node->value = token->value;
      column_node->parent = select_node;
      
      if (!select_node->first) 
      {
        select_node->first = select_node->last = column_node;
      } 
      else 
      {
        tail->next = column_node;
        column_node->prev = tail;
        select_node->last = column_node;
      }
      
      tail = column_node;
      (*token_index)++;
      
      // Handle comma between columns
      if (*token_index < token_count && (*tokens)[*token_index].type == SQL_TokenType_Symbol &&
          str8_match((*tokens)[*token_index].value, str8_lit(","), 0))
      {
        (*token_index)++;
      } 
      else 
      {
        break;
      }
    } 
    else 
    {
      log_error("Expected column name in SELECT clause, but found '%.*s'.",
                token->value.size, token->value.str);
      return NULL;
    }
  }
  
  return select_node;
}

internal SQL_Node*
sql_parse_from_clause(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count)
{
  if (*token_index >= token_count ||
      (*tokens)[*token_index].type != SQL_TokenType_Keyword ||
      !str8_match((*tokens)[*token_index].value, str8_lit("from"), StringMatchFlag_CaseInsensitive))
  {
    log_error("Expected 'FROM' keyword after SELECT clause.");
    return NULL;
  }
  
  (*token_index)++; // Skip the FROM keyword
  
  if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Identifier)
  {
    log_error("Expected table name after 'FROM'.");
    return NULL;
  }
  
  SQL_Node *table_node = push_array(arena, SQL_Node, 1);
  table_node->type = SQL_NodeType_Table;
  table_node->value = (*tokens)[*token_index].value;
  
  (*token_index)++;
  
  return table_node;
}

internal SQL_Node*
sql_parse_where_clause(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count)
{
  SQL_Node *left = sql_parse_expression(arena, tokens, token_index, token_count);
  if (!left) 
  {
    log_error("Failed to parse left-hand side of WHERE clause.");
    return NULL;
  }
  
  if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Operator)
  {
    log_error("Expected operator in WHERE clause.");
    return NULL;
  }
  
  SQL_Node *operator_node = push_array(arena, SQL_Node, 1);
  operator_node->type = SQL_NodeType_Operator;
  operator_node->value = (*tokens)[*token_index].value;
  operator_node->first = left; // Assign left node to the operator's first child
  (*token_index)++;
  
  SQL_Node *right = sql_parse_expression(arena, tokens, token_index, token_count);
  if (!right) 
  {
    log_error("Failed to parse right-hand side of WHERE clause.");
    return NULL;
  }
  
  left->next = right;
  right->prev = left;
  
  operator_node->last = right; // Assign right node to the operator's last child
  right->parent = operator_node;
  left->parent = operator_node; // Ensure left node also has the operator as its parent
  
  return operator_node;
}

internal SQL_Node*
sql_parse_expression(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count) 
{
  if (*token_index >= token_count) 
  {
    log_error("Unexpected end of input in expression.");
    return NULL;
  }
  
  SQL_Token *token = &(*tokens)[*token_index];
  
  if (token->type == SQL_TokenType_Identifier || token->type == SQL_TokenType_Number)
  {
    SQL_Node *node = push_array(arena, SQL_Node, 1);
    node->type = (token->type == SQL_TokenType_Identifier) ? SQL_NodeType_Column : SQL_NodeType_Literal;
    node->value = token->value;
    (*token_index)++;
    return node;
  }
  
  log_error("Unexpected token '%.*s' in expression.", token->value.size, token->value.str);
  return NULL;
}

internal void
sql_print_node(SQL_Node *node, U64 depth)
{
  if (!node) return;
  
  for (U64 i = 0; i < depth; i++) printf("  ");
  
  char* type = "no type";
  switch (node->type)
  {
    case SQL_NodeType_Select: type = "SQL_NodeType_Select"; break;
    case SQL_NodeType_Column: type = "SQL_NodeType_Column"; break;
    case SQL_NodeType_Table: type = "SQL_NodeType_Table"; break;
    case SQL_NodeType_Where: type = "SQL_NodeType_Where"; break;
    case SQL_NodeType_Operator: type = "SQL_NodeType_Operator"; break;
    case SQL_NodeType_Literal: type = "SQL_NodeType_Literal"; break;
  }
  
  printf("SQL_Node: type=%s, value=%.*s\n", type, (int)node->value.size, node->value.str);
  
  if (node->first)
  {
    for (U64 i = 0; i < depth; i++)
      printf("  ");
    printf("Children:\n");
    sql_print_node(node->first, depth + 1);
  }
  
  if (node->next)
  {
    sql_print_node(node->next, depth);
  }
}

internal void
sql_print_ast(SQL_Node *root)
{
  printf("SQL AST:\n");
  sql_print_node(root, 0);
}
