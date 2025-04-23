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
  ProfBeginFunction();
  
  SQL_Token* tokens = push_array(arena, SQL_Token, 2056);
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
    
    if (text.str[pos] == '\'')
    {
      pos++;
      start = pos;
      
      while (pos < text.size && text.str[pos] != '\'')
      {
        pos++;
      }
      
      if (pos >= text.size)
      {
        log_error("Unterminated string literal.");
        break;
      }
      
      String8 token_value = str8_substr(text, r1u64(start, pos));
      tokens[token_count++] = (SQL_Token)
      {
        .type = SQL_TokenType_String,
        .value = token_value,
        .range = r1u64(start - 1, pos + 1)
      };
      
      pos++;
    }
    else if (char_is_digit(text.str[pos], 10)) 
    {
      B32 has_dot = 0;
      
      while (pos < text.size && (char_is_digit(text.str[pos], 10) || (!has_dot && text.str[pos] == '.')))
      {
        if (text.str[pos] == '.')
        {
          has_dot = 1;
          if (pos + 1 >= text.size || !char_is_digit(text.str[pos + 1], 10))
          {
            break;
          }
        }
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
    // Keywords and identifiers
    else if (char_is_alpha(text.str[pos]))
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
    // Symbols
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
    // Operators and other tokens
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
  
  ProfEnd();
  return result;
}

internal void
sql_tokens_print(SQL_TokenizeResult tokens)
{
  for (U64 i = 0; i < tokens.count; i++)
  {
    SQL_Token* token = &tokens.tokens[i];
    
    String8 token_type = str8_lit("no type");
    switch (token->type)
    {
      case SQL_TokenType_Keyword: token_type = str8_lit("SQL_TokenType_Keyword"); break;
      case SQL_TokenType_Identifier: token_type = str8_lit("SQL_TokenType_Identifier"); break;
      case SQL_TokenType_Operator: token_type = str8_lit("SQL_TokenType_Operator"); break;
      case SQL_TokenType_Symbol: token_type = str8_lit("SQL_TokenType_Symbol"); break;
      case SQL_TokenType_Number: token_type = str8_lit("SQL_TokenType_Number"); break;
      case SQL_TokenType_String: token_type = str8_lit("SQL_TokenType_String"); break;
    }
    
    printf("\'%.*s\' : index=%llu : type=%.*s\n", (int)token->value.size, token->value.str,
           i, (int)token_type.size, token_type.str);
  }
}

//~ tec: ast
internal SQL_Node*
sql_parse(Arena* arena, SQL_Token* tokens, U64 token_count)
{
  ProfBeginFunction();
  
  U64 token_index = 0;
  SQL_Node *root = NULL;
  SQL_Node *current_node = NULL;
  SQL_Node *last_select_node = NULL;
  
  while (token_index < token_count)
  {
    SQL_Token *token = &tokens[token_index];
    B32 attach_to_select = 0;
    
    if (token->type == SQL_TokenType_Symbol && str8_match(token->value, str8_lit(";"), 0))
    {
      token_index++;
      last_select_node = NULL;
      continue;
    }
    
    if (token->type == SQL_TokenType_Keyword)
    {
      SQL_Node *new_node = NULL;
      
      if (str8_match(token->value, str8_lit("use"), StringMatchFlag_CaseInsensitive))
      {
        new_node = sql_parse_use_clause(arena, &tokens, &token_index, token_count);
      }
      else if (str8_match(token->value, str8_lit("select"), StringMatchFlag_CaseInsensitive))
      {
        new_node = sql_parse_select_clause(arena, &tokens, &token_index, token_count);
        last_select_node = new_node;
      }
      else if (str8_match(token->value, str8_lit("from"), StringMatchFlag_CaseInsensitive))
      {
        new_node = sql_parse_from_clause(arena, &tokens, &token_index, token_count);
        if (last_select_node)
        {
          new_node->parent = last_select_node;
          DLLPushBack(last_select_node->first, last_select_node->last, new_node);
        }
        attach_to_select = 1;
      }
      else if (str8_match(token->value, str8_lit("where"), StringMatchFlag_CaseInsensitive))
      {
        new_node = sql_parse_where_clause(arena, &tokens, &token_index, token_count);
        if (last_select_node)
        {
          new_node->parent = last_select_node;
          DLLPushBack(last_select_node->first, last_select_node->last, new_node);
        }
        attach_to_select = 1;
      }
      else if (str8_match(token->value, str8_lit("insert"), StringMatchFlag_CaseInsensitive))
      {
        new_node = sql_parse_insert_clause(arena, &tokens, &token_index, token_count);
      }
      else if (str8_match(token->value, str8_lit("import"), StringMatchFlag_CaseInsensitive))
      {
        new_node = sql_parse_import_clause(arena, &tokens, &token_index, token_count);
      }
      else if (str8_match(token->value, str8_lit("create"), StringMatchFlag_CaseInsensitive))
      {
        new_node = sql_parse_create_clause(arena, &tokens, &token_index, token_count);
      }
      else if (str8_match(token->value, str8_lit("alter"), StringMatchFlag_CaseInsensitive))
      {
        new_node = sql_parse_alter_clause(arena, &tokens, &token_index, token_count);
      }
      else if (str8_match(token->value, str8_lit("delete"), StringMatchFlag_CaseInsensitive))
      {
        new_node = sql_parse_delete_clause(arena, &tokens, &token_index, token_count);
        last_select_node = new_node;
      }
      else if (str8_match(token->value, str8_lit("order"), StringMatchFlag_CaseInsensitive))
      {
        new_node = sql_parse_order_by_clause(arena, &tokens, &token_index, token_count);
        if (last_select_node)
        {
          new_node->parent = last_select_node;
          DLLPushBack(last_select_node->first, last_select_node->last, new_node);
        }
        attach_to_select = 1;
      }
      else
      {
        log_error("unexpected keyword '%.*s' at token index %llu.", 
                  token->value.size, token->value.str, token_index);
        ProfEnd();
        return NULL;
      }
      
      if (new_node)
      {
        if (! attach_to_select)
        {
          if (!root)
          {
            root = new_node;
            current_node = new_node;
          }
          else
          {
            current_node->next = new_node;
            new_node->prev = current_node;
            current_node = new_node;
          }
        }
      }
    }
    else
    {
      log_error("unexpected token '%.*s' at token index %llu.", 
                token->value.size, token->value.str, token_index);
      ProfEnd();
      return NULL;
    }
  }
  
  ProfEnd();
  return root;
}

internal SQL_Node*
sql_parse_use_clause(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count)
{
  (*token_index)++; // Move past 'use'
  
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
      log_error("expected table name in 'use' clause, but found '%.*s'.",
                token->value.size, token->value.str);
      return NULL;
    }
  }
  
  return use_node;
}

internal SQL_Node*
sql_parse_select_clause(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count)
{
  (*token_index)++; // Move past 'select'
  
  SQL_Node* select_node = push_array(arena, SQL_Node, 1);
  select_node->type = SQL_NodeType_Select;
  
  SQL_Node* column_list = push_array(arena, SQL_Node, 1);
  column_list->type = SQL_NodeType_ColumnList;
  column_list->parent = select_node;
  select_node->first = select_node->last = column_list;
  
  SQL_Node* first = NULL;
  SQL_Node* last = NULL;
  
  while (*token_index < token_count)
  {
    SQL_Token *token = &(*tokens)[*token_index];
    
    if (token->type == SQL_TokenType_Identifier)
    {
      SQL_Node *column_node = push_array(arena, SQL_Node, 1);
      column_node->type = SQL_NodeType_Column;
      column_node->value = token->value;
      column_node->parent = column_list;
      
      DLLPushBack(first, last, column_node);
      
      (*token_index)++;
      
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
    else if (token->type == SQL_TokenType_Symbol)
    {
      SQL_Node *column_node = push_array(arena, SQL_Node, 1);
      column_node->type = SQL_NodeType_Column;
      column_node->value = token->value;
      column_node->parent = column_list;
      
      DLLPushBack(first, last, column_node);
      
      (*token_index)++;
      break;
    }
    else 
    {
      log_error("expected column name in 'select' clause, but found '%.*s'.",
                token->value.size, token->value.str);
      return NULL;
    }
  }
  
  column_list->first = first;
  column_list->last = last;
  
  return select_node;
}

internal SQL_Node*
sql_parse_from_clause(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count)
{
  if (*token_index >= token_count ||
      (*tokens)[*token_index].type != SQL_TokenType_Keyword ||
      !str8_match((*tokens)[*token_index].value, str8_lit("from"), StringMatchFlag_CaseInsensitive))
  {
    log_error("expected 'from' keyword after 'select' clause.");
    return NULL;
  }
  
  (*token_index)++; // Move past 'from'
  
  if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Identifier)
  {
    log_error("expected table name after 'from'");
    return NULL;
  }
  
  SQL_Node *table_node = push_array(arena, SQL_Node, 1);
  table_node->type = SQL_NodeType_Table;
  table_node->value = (*tokens)[*token_index].value;
  
  (*token_index)++;
  
  return table_node;
}

internal SQL_Node*
sql_parse_comparison_expression(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count)
{
  // tec: expressions inside parentheses
  if (*token_index < token_count &&
      (*tokens)[*token_index].type == SQL_TokenType_Symbol &&
      str8_match((*tokens)[*token_index].value, str8_lit("("), 0))
  {
    (*token_index)++; // move past '('
    
    SQL_Node *expr = sql_parse_logical_expression(arena, tokens, token_index, token_count);
    if (!expr) 
    {
      log_error("failed to parse expression inside parentheses");
      return NULL;
    }
    
    // tec: expect closing ')'
    if (*token_index >= token_count ||
        (*tokens)[*token_index].type != SQL_TokenType_Symbol ||
        !str8_match((*tokens)[*token_index].value, str8_lit(")"), 0))
    {
      log_error("expected ')' after expression");
      return NULL;
    }
    
    (*token_index)++; // move past ')'
    return expr;
  }
  
  // tec: left hand side column/literal
  SQL_Node *left = sql_parse_expression(arena, tokens, token_index, token_count);
  if (!left) 
  {
    log_error("failed to parse left-hand side of comparison");
    return NULL;
  }
  
  // Expect comparison operator
  if (*token_index >= token_count || !((*tokens)[*token_index].type == SQL_TokenType_Operator ||
                                       (*tokens)[*token_index].type == SQL_TokenType_Keyword))
  {
    log_error("expected comparison operator in expression");
    return NULL;
  }
  
  // Create comparison operator node
  SQL_Node *operator_node = push_array(arena, SQL_Node, 1);
  operator_node->type = SQL_NodeType_Operator;
  operator_node->value = (*tokens)[*token_index].value;
  (*token_index)++; // Move past operator
  
  // tec: right hand side column/literal
  SQL_Node *right = sql_parse_expression(arena, tokens, token_index, token_count);
  if (!right) 
  {
    log_error("failed to parse right hand side of comparison");
    return NULL;
  }
  
  // tec: link
  operator_node->first = left;
  operator_node->last = right;
  left->next = right;
  right->prev = left;
  left->parent = operator_node;
  right->parent = operator_node;
  
  return operator_node;
}

internal SQL_Node*
sql_parse_logical_expression(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count)
{
  SQL_Node *left = sql_parse_comparison_expression(arena, tokens, token_index, token_count);
  if (!left) 
  {
    log_error("Failed to parse left-hand side of logical expression.");
    return NULL;
  }
  
  while (*token_index < token_count &&
         (*tokens)[*token_index].type == SQL_TokenType_Keyword &&
         (str8_match((*tokens)[*token_index].value, str8_lit("and"), StringMatchFlag_CaseInsensitive) ||
          str8_match((*tokens)[*token_index].value, str8_lit("or"), StringMatchFlag_CaseInsensitive)))
  {
    // tec: create logical operator node
    SQL_Node *operator_node = push_array(arena, SQL_Node, 1);
    operator_node->type = SQL_NodeType_Operator;
    operator_node->value = (*tokens)[*token_index].value;
    (*token_index)++; // tec: move past operator
    
    SQL_Node *right = sql_parse_comparison_expression(arena, tokens, token_index, token_count);
    if (!right) 
    {
      log_error("failed to parse right hand side of logical expression");
      return NULL;
    }
    
    // tec: link
    operator_node->first = left;
    operator_node->last = right;
    left->next = right;
    right->prev = left;
    left->parent = operator_node;
    right->parent = operator_node;
    
    // tec: update left to be the new root
    left = operator_node;
  }
  
  return left;
}

internal SQL_Node*
sql_parse_where_clause(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count)
{
  if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Keyword ||
      !str8_match((*tokens)[*token_index].value, str8_lit("where"), StringMatchFlag_CaseInsensitive))
  {
    log_error("expected 'where' keyword.");
    return NULL;
  }
  
  SQL_Node* where_node = push_array(arena, SQL_Node, 1);
  where_node->type = SQL_NodeType_Where;
  
  (*token_index)++; // tec: move past 'where'
  
  SQL_Node* logic_node = sql_parse_logical_expression(arena, tokens, token_index, token_count);
  where_node->first = logic_node;
  where_node->last = logic_node;
  
  return where_node;
  
}

internal SQL_Node*
sql_parse_insert_clause(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count)
{
  SQL_Node* insert_node = push_array(arena, SQL_Node, 1);
  insert_node->type = SQL_NodeType_Insert;
  
  (*token_index)++; // tec: move past 'insert'
  
  // tec: expect 'into'
  if (*token_index >= token_count || 
      (*tokens)[*token_index].type != SQL_TokenType_Keyword || 
      !str8_match((*tokens)[*token_index].value, str8_lit("into"), StringMatchFlag_CaseInsensitive))
  {
    log_error("expected 'into' keyword in 'insert' statement");
    return NULL;
  }
  (*token_index)++; // tec: move past "into"
  
  // tec: expect table name
  if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Identifier)
  {
    log_error("expected table name in 'insert' statement");
    return NULL;
  }
  
  SQL_Node* table_node = push_array(arena, SQL_Node, 1);
  table_node->type = SQL_NodeType_Table;
  table_node->value = (*tokens)[*token_index].value;
  (*token_index)++;
  
  insert_node->first = table_node;
  table_node->parent = insert_node;
  
  // tec: optional column list
  SQL_Node* column_list_node = NULL;
  SQL_Node* prev_column = NULL;
  if (*token_index < token_count && (*tokens)[*token_index].type == SQL_TokenType_Symbol &&
      str8_match((*tokens)[*token_index].value, str8_lit("("), 0))
  {
    (*token_index)++; // tec: move past '('
    
    column_list_node = push_array(arena, SQL_Node, 1);
    column_list_node->type = SQL_NodeType_ColumnList;
    column_list_node->parent = insert_node;
    
    SQL_Node* prev_column = NULL;
    while (*token_index < token_count && 
           (*tokens)[*token_index].type != SQL_TokenType_Symbol &&
           !str8_match((*tokens)[*token_index].value, str8_lit(")"), 0))
    {
      if ((*tokens)[*token_index].type != SQL_TokenType_Identifier)
      {
        log_error("expected column name in 'insert' statement");
        return NULL;
      }
      
      SQL_Node* column_node = push_array(arena, SQL_Node, 1);
      column_node->type = SQL_NodeType_Column;
      column_node->value = (*tokens)[*token_index].value;
      column_node->parent = column_list_node;
      (*token_index)++;
      
      if (!column_list_node->first)
      {
        column_list_node->first = column_node;
      }
      else
      {
        column_list_node->last->next = column_node;
        column_node->prev = column_list_node->last;
      }
      column_list_node->last = column_node;
      prev_column = column_node;
      
      // tec: skip comma
      if (*token_index < token_count && (*tokens)[*token_index].type == SQL_TokenType_Symbol &&
          str8_match((*tokens)[*token_index].value, str8_lit(","), 0))
      {
        (*token_index)++;
      }
    }
    
    // tec: expect closing ')'
    if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Symbol ||
        !str8_match((*tokens)[*token_index].value, str8_lit(")"), 0))
    {
      log_error("Expected closing ')' in column list.");
      return NULL;
    }
    (*token_index)++; // tec: move past ')'
    
    // tec: attach column list node to insert_node
    table_node->next = column_list_node;
    insert_node->last = column_list_node;
  }
  
  // tec: expect 'values' keyword
  if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Keyword ||
      !str8_match((*tokens)[*token_index].value, str8_lit("values"), StringMatchFlag_CaseInsensitive))
  {
    log_error("Expected 'values' keyword in 'insert' statement");
    return NULL;
  }
  (*token_index)++; // tec: move past 'values'
  
  // tec: parse 'values' clause
  SQL_Node* values_node = sql_parse_values_clause(arena, tokens, token_index, token_count);
  if (!values_node)
  {
    return NULL;
  }
  
  // tec: link values to the insert node
  SQL_Node* last_child = insert_node->last ? insert_node->last : table_node;
  last_child->next = values_node;
  values_node->prev = last_child;
  values_node->parent = insert_node;
  insert_node->last = values_node;
  
  return insert_node;
}

internal SQL_Node*
sql_parse_import_clause(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count)
{
  SQL_Node* import_node = push_array(arena, SQL_Node, 1);
  import_node->type = SQL_NodeType_Import;
  
  (*token_index)++; // tec: move past 'IMPORT'
  
  // tec: expect 'INTO'
  if (*token_index >= token_count || 
      (*tokens)[*token_index].type != SQL_TokenType_Keyword || 
      !str8_match((*tokens)[*token_index].value, str8_lit("into"), StringMatchFlag_CaseInsensitive))
  {
    log_error("Expected 'INTO' keyword in 'IMPORT' statement");
    return NULL;
  }
  (*token_index)++; // tec: move past 'INTO'
  
  // tec: expect table name
  if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Identifier)
  {
    log_error("Expected table name after 'INTO' in 'IMPORT' statement");
    return NULL;
  }
  
  SQL_Node* table_node = push_array(arena, SQL_Node, 1);
  table_node->type = SQL_NodeType_Table;
  table_node->value = (*tokens)[*token_index].value;
  (*token_index)++;
  
  import_node->first = table_node;
  table_node->parent = import_node;
  
  // tec: expect 'FROM'
  if (*token_index >= token_count || 
      (*tokens)[*token_index].type != SQL_TokenType_Keyword || 
      !str8_match((*tokens)[*token_index].value, str8_lit("from"), StringMatchFlag_CaseInsensitive))
  {
    log_error("Expected 'FROM' keyword in 'IMPORT' statement");
    return NULL;
  }
  (*token_index)++; // tec: move past 'FROM'
  
  // tec: expect file path as string literal
  if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_String)
  {
    log_error("Expected file path after 'FROM' in 'IMPORT' statement");
    return NULL;
  }
  
  SQL_Node* path_node = push_array(arena, SQL_Node, 1);
  path_node->type = SQL_NodeType_Literal;
  path_node->value = (*tokens)[*token_index].value;
  (*token_index)++;
  
  table_node->next = path_node;
  path_node->prev = table_node;
  path_node->parent = import_node;
  import_node->last = path_node;
  
  return import_node;
}

internal SQL_Node*
sql_parse_create_clause(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count)
{
  SQL_Node* create_node = push_array(arena, SQL_Node, 1);
  create_node->type = SQL_NodeType_Create;
  
  (*token_index)++; // tec: move past 'create'
  
  // tec: expect 'table'
  if (*token_index >= token_count || 
      (*tokens)[*token_index].type != SQL_TokenType_Keyword)
  {
    log_error("expected 'table' or 'database' keyword in 'create' statement");
    return NULL;
  }
  String8 keyword = (*tokens)[*token_index].value;
  (*token_index)++; // tec: move past keyword
  
  if (str8_match(keyword, str8_lit("database"), StringMatchFlag_CaseInsensitive))
  {
    // tec: expect database name
    if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Identifier)
    {
      log_error("expected database name in 'create' statement");
      return NULL;
    }
    
    SQL_Node* database_node = push_array(arena, SQL_Node, 1);
    database_node->type = SQL_NodeType_Database;
    database_node->value = (*tokens)[*token_index].value;
    (*token_index)++;
    
    create_node->first = database_node;
    create_node->last = database_node;
    database_node->parent = create_node;
    
  }
  else if (str8_match(keyword, str8_lit("table"), StringMatchFlag_CaseInsensitive))
  {
    // tec: expect table name
    if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Identifier)
    {
      log_error("expected table name in 'create' statement");
      return NULL;
    }
    
    SQL_Node* table_node = push_array(arena, SQL_Node, 1);
    table_node->type = SQL_NodeType_Table;
    table_node->value = (*tokens)[*token_index].value;
    (*token_index)++;
    
    create_node->first = table_node;
    create_node->last = table_node;
    table_node->parent = create_node;
    
    // tec: expect column definitions
    if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Symbol ||
        !str8_match((*tokens)[*token_index].value, str8_lit("("), 0))
    {
      log_error("expected '(' in 'create table' statement.");
      return NULL;
    }
    (*token_index)++; // tec: move past '('
    
    SQL_Node* prev_column = NULL;
    while (*token_index < token_count && (*tokens)[*token_index].type != SQL_TokenType_Symbol &&
           !str8_match((*tokens)[*token_index].value, str8_lit(")"), 0))
    {
      // tec: expect column name
      if ((*tokens)[*token_index].type != SQL_TokenType_Identifier)
      {
        log_error("expected column name in 'create table' statement");
        return NULL;
      }
      
      SQL_Node* column_node = push_array(arena, SQL_Node, 1);
      column_node->type = SQL_NodeType_Column;
      column_node->value = (*tokens)[*token_index].value;
      (*token_index)++;
      
      // tec: expect column type
      if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Keyword)
      {
        log_error("expected column type in 'create table' statement.");
        return NULL;
      }
      
      SQL_Node* type_node = push_array(arena, SQL_Node, 1);
      type_node->type = SQL_NodeType_Type;
      type_node->value = (*tokens)[*token_index].value;
      (*token_index)++;
      
      column_node->first = type_node;
      column_node->last = type_node;
      type_node->parent = column_node;
      
      column_node->parent = table_node;
      
      if (prev_column)
      {
        prev_column->next = column_node;
        column_node->prev = prev_column;
      }
      else
      {
        table_node->first = column_node;
      }
      table_node->last = column_node;
      
      prev_column = column_node;
      
      // tec: skip comma
      if (*token_index < token_count && (*tokens)[*token_index].type == SQL_TokenType_Symbol &&
          str8_match((*tokens)[*token_index].value, str8_lit(","), StringMatchFlag_CaseInsensitive))
      {
        (*token_index)++;
      }
    }
    
    // tec: expect closing ')'
    if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Symbol ||
        !str8_match((*tokens)[*token_index].value, str8_lit(")"), StringMatchFlag_CaseInsensitive))
    {
      log_error("expected closing ')' in 'create table' statement");
      return NULL;
    }
    (*token_index)++; // tec: move past ')'
    
  }
  else
  {
    log_error("unexpected keyword in 'create' statemnet, exepected 'table' or 'database'");
  }
  
  return create_node;
}

internal SQL_Node* 
sql_parse_alter_clause(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count)
{
  if (*token_index >= token_count || 
      (*tokens)[*token_index].type != SQL_TokenType_Keyword ||
      !str8_match((*tokens)[*token_index].value, str8_lit("alter"), StringMatchFlag_CaseInsensitive))
  {
    log_error("expected 'alter' keyword");
    return NULL;
  }
  (*token_index)++; // tec: move past 'alter'
  
  if (*token_index >= token_count || 
      (*tokens)[*token_index].type != SQL_TokenType_Keyword ||
      !str8_match((*tokens)[*token_index].value, str8_lit("table"), StringMatchFlag_CaseInsensitive))
  {
    log_error("expected 'table' keyword after 'alter'");
    return NULL;
  }
  (*token_index)++; // tec: move past 'table'
  
  // tec: expect table name
  if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Identifier)
  {
    log_error("expected table name after 'alter table'");
    return NULL;
  }
  
  SQL_Node* alter_node = push_array(arena, SQL_Node, 1);
  alter_node->type = SQL_NodeType_Alter;
  
  SQL_Node* table_node = push_array(arena, SQL_Node, 1);
  table_node->type = SQL_NodeType_Table;
  table_node->value = (*tokens)[*token_index].value;
  (*token_index)++; // tec: move past table name
  
  table_node->parent = alter_node;
  alter_node->first = table_node;
  
  // tec: expect alter operation
  if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Keyword)
  {
    log_error("expected operation after 'alter table <table>'");
    return NULL;
  }
  
  SQL_Node* operation_node = push_array(arena, SQL_Node, 1);
  operation_node->parent = alter_node;
  alter_node->first->next = operation_node;
  
  // tec: alter operations
  if (str8_match((*tokens)[*token_index].value, str8_lit("add"), StringMatchFlag_CaseInsensitive))
  {
    (*token_index)++; // tec: move past 'ADadd'
    if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Keyword ||
        !str8_match((*tokens)[*token_index].value, str8_lit("column"), StringMatchFlag_CaseInsensitive))
    {
      log_error("expected 'column' after 'add' in 'alter table'");
      return NULL;
    }
    (*token_index)++; // tec: move past 'column'
    
    // tec: expect column name
    if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Identifier)
    {
      log_error("expected column name after 'add column'");
      return NULL;
    }
    operation_node->type = SQL_NodeType_Alter_AddColumn;
    
    SQL_Node* column_node = push_array(arena, SQL_Node, 1);
    column_node->type = SQL_NodeType_Column;
    column_node->parent = alter_node;
    column_node->value = (*tokens)[*token_index].value;
    (*token_index)++; // Move past column name
    
    operation_node->first = column_node;
    
    // tec: optional column type
    if (*token_index < token_count && (*tokens)[*token_index].type == SQL_TokenType_Keyword)
    {
      SQL_Node* type_node = push_array(arena, SQL_Node, 1);
      type_node->type = SQL_NodeType_Type;
      type_node->value = (*tokens)[*token_index].value;
      type_node->parent = operation_node;
      operation_node->first->next = type_node;
      (*token_index)++; // tec: move past column type
    }
  }
  else if (str8_match((*tokens)[*token_index].value, str8_lit("drop"), StringMatchFlag_CaseInsensitive))
  {
    (*token_index)++; // Move past 'DROP'
    if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Keyword ||
        !str8_match((*tokens)[*token_index].value, str8_lit("column"), StringMatchFlag_CaseInsensitive))
    {
      log_error("expected 'column' after 'drop' in 'alter table'");
      return NULL;
    }
    (*token_index)++; // tec: move past 'column'
    
    if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Identifier)
    {
      log_error("expected column name after 'drop column'");
      return NULL;
    }
    operation_node->type = SQL_NodeType_Alter_DropColumn;
    operation_node->value = (*tokens)[*token_index].value;
    (*token_index)++; // tec: move past column name
  }
  else if (str8_match((*tokens)[*token_index].value, str8_lit("rename"), StringMatchFlag_CaseInsensitive))
  {
    (*token_index)++; // tec: move past 'rename'
    if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Keyword ||
        !str8_match((*tokens)[*token_index].value, str8_lit("to"), StringMatchFlag_CaseInsensitive))
    {
      log_error("expected 'to' after 'rename' in 'alter table'");
      return NULL;
    }
    (*token_index)++; // tec: move past 'to'
    
    if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Identifier)
    {
      log_error("expected new table name after 'rename to'");
      return NULL;
    }
    operation_node->type = SQL_NodeType_Alter_Rename;
    operation_node->value = (*tokens)[*token_index].value;
    (*token_index)++; // tec: move past new table name
  }
  else
  {
    log_error("Unknown ALTER TABLE operation.");
    return NULL;
  }
  
  return alter_node;
}

internal SQL_Node*
sql_parse_values_clause(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count)
{
  SQL_Node* values_root = push_array(arena, SQL_Node, 1);
  values_root->type = SQL_NodeType_Value;
  
  SQL_Node* prev_value_group = NULL;
  
  while (*token_index < token_count)
  {
    // tec: expect opening '('
    if ((*tokens)[*token_index].type != SQL_TokenType_Symbol ||
        !str8_match((*tokens)[*token_index].value, str8_lit("("), 0))
    {
      log_error("expected '(' before values in 'values' clause");
      return NULL;
    }
    (*token_index)++; // tec: move past '('
    
    SQL_Node* value_group = push_array(arena, SQL_Node, 1);
    value_group->type = SQL_NodeType_ValueGroup;
    
    SQL_Node* prev_value = NULL;
    
    while (*token_index < token_count &&
           (*tokens)[*token_index].type != SQL_TokenType_Symbol &&
           !str8_match((*tokens)[*token_index].value, str8_lit(")"), 0))
    {
      if ((*tokens)[*token_index].type != SQL_TokenType_Number &&
          (*tokens)[*token_index].type != SQL_TokenType_String)
      {
        log_error("expected a literal value in 'values' clause.");
        return NULL;
      }
      
      SQL_Node* value_node = push_array(arena, SQL_Node, 1);
      value_node->type = (*tokens)[*token_index].type == SQL_TokenType_Number ? SQL_NodeType_Numeric : SQL_NodeType_Literal;
      value_node->value = (*tokens)[*token_index].value;
      (*token_index)++;
      
      if (!value_group->first)
      {
        value_group->first = value_node;
      }
      if (prev_value)
      {
        prev_value->next = value_node;
        value_node->prev = prev_value;
      }
      prev_value = value_node;
      
      if (*token_index < token_count && (*tokens)[*token_index].type == SQL_TokenType_Symbol)
      {
        if (str8_match((*tokens)[*token_index].value, str8_lit(","), 0))
        {
          (*token_index)++; // tec: skip ','
          continue;
        }
      }
    }
    
    // tec: expect closing ')'
    if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Symbol ||
        !str8_match((*tokens)[*token_index].value, str8_lit(")"), 0))
    {
      log_error("expected closing ')' after values in 'values' clause.");
      return NULL;
    }
    (*token_index)++; // tec: move past ')'
    
    if (prev_value_group)
    {
      prev_value_group->next = value_group;
      value_group->prev = prev_value_group;
    }
    else
    {
      values_root->first = value_group;
    }
    
    prev_value_group = value_group;
    
    if (*token_index < token_count && (*tokens)[*token_index].type == SQL_TokenType_Symbol &&
        str8_match((*tokens)[*token_index].value, str8_lit(","), 0))
    {
      (*token_index)++; // tec: move past ',' to next value group
    }
    else
    {
      break;
    }
  }
  
  values_root->last = prev_value_group;
  return values_root;
}

internal SQL_Node*
sql_parse_order_by_clause(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count)
{
  if (*token_index >= token_count || 
      (*tokens)[*token_index].type != SQL_TokenType_Keyword ||
      !str8_match((*tokens)[*token_index].value, str8_lit("order"), StringMatchFlag_CaseInsensitive))
  {
    log_error("expected 'order' keyword");
    return NULL;
  }
  (*token_index)++;
  
  if (*token_index >= token_count || 
      (*tokens)[*token_index].type != SQL_TokenType_Keyword ||
      !str8_match((*tokens)[*token_index].value, str8_lit("by"), StringMatchFlag_CaseInsensitive))
  {
    log_error("expected 'by' keyword after 'order'");
    return NULL;
  }
  (*token_index)++;
  
  SQL_Node* order_by_root = push_array(arena, SQL_Node, 1);
  order_by_root->type = SQL_NodeType_OrderBy;
  
  SQL_Node* prev_order = NULL;
  
  while (*token_index < token_count)
  {
    if ((*tokens)[*token_index].type != SQL_TokenType_Identifier)
    {
      log_error("expected column name in 'order by' clause");
      return NULL;
    }
    
    SQL_Node* column_node = push_array(arena, SQL_Node, 1);
    column_node->type = SQL_NodeType_Column;
    column_node->value = (*tokens)[*token_index].value;
    (*token_index)++;
    
    if (*token_index < token_count && (*tokens)[*token_index].type == SQL_TokenType_Keyword)
    {
      SQL_Node* sort_node = push_array(arena, SQL_Node, 1);
      sort_node->value = (*tokens)[*token_index].value;
      
      if (str8_match((*tokens)[*token_index].value, str8_lit("asc"), StringMatchFlag_CaseInsensitive))
      {
        sort_node->type = SQL_NodeType_Ascending;
        (*token_index)++;
      }
      else if (str8_match((*tokens)[*token_index].value, str8_lit("desc"), StringMatchFlag_CaseInsensitive))
      {
        sort_node->type = SQL_NodeType_Descending;
        (*token_index)++;
      }
      
      column_node->first = sort_node;
      sort_node->parent = column_node;
    }
    
    if (!order_by_root->first)
    {
      order_by_root->first = column_node;
    }
    else
    {
      order_by_root->last->next = column_node;
      column_node->prev = order_by_root->last;
    }
    order_by_root->last = column_node;
    
    if (*token_index < token_count && (*tokens)[*token_index].type == SQL_TokenType_Symbol &&
        str8_match((*tokens)[*token_index].value, str8_lit(","), 0))
    {
      (*token_index)++;
      continue;
    }
    else
    {
      break;
    }
  }
  
  return order_by_root;
}

internal SQL_Node*
sql_parse_delete_clause(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count)
{
  if (*token_index >= token_count || 
      (*tokens)[*token_index].type != SQL_TokenType_Keyword ||
      !str8_match((*tokens)[*token_index].value, str8_lit("DELETE"), 0))
  {
    log_error("Expected 'DELETE' keyword.");
    return NULL;
  }
  (*token_index)++; // Move past 'DELETE'
  
  if (*token_index >= token_count || 
      (*tokens)[*token_index].type != SQL_TokenType_Keyword ||
      !str8_match((*tokens)[*token_index].value, str8_lit("FROM"), 0))
  {
    log_error("Expected 'FROM' keyword after 'DELETE'.");
    return NULL;
  }
  (*token_index)++; // Move past 'FROM'
  
  // Expect table name
  if (*token_index >= token_count || (*tokens)[*token_index].type != SQL_TokenType_Identifier)
  {
    log_error("Expected table name after 'DELETE FROM'.");
    return NULL;
  }
  
  SQL_Node* delete_node = push_array(arena, SQL_Node, 1);
  delete_node->type = SQL_NodeType_Delete;
  delete_node->value = (*tokens)[*token_index].value;
  
  SQL_Node* table_node = push_array(arena, SQL_Node, 1);
  table_node->type = SQL_NodeType_Table;
  table_node->value = (*tokens)[*token_index].value;
  
  delete_node->first = table_node;
  
  (*token_index)++; // Move past table name
  
  // Check for optional WHERE clause
  if (*token_index < token_count && 
      (*tokens)[*token_index].type == SQL_TokenType_Keyword &&
      str8_match((*tokens)[*token_index].value, str8_lit("WHERE"), 0))
  {
    SQL_Node* where_clause = sql_parse_where_clause(arena, tokens, token_index, token_count);
    if (!where_clause)
    {
      log_error("Failed to parse WHERE clause in DELETE statement.");
      return NULL;
    }
    
    delete_node->first->next = where_clause;
    where_clause->parent = delete_node;
  }
  
  return delete_node;
}


internal SQL_Node*
sql_parse_expression(Arena* arena, SQL_Token **tokens, U64 *token_index, U64 token_count) 
{
  if (*token_index >= token_count) 
  {
    log_error("unexpected end of input in expression");
    return NULL;
  }
  
  SQL_Token *token = &(*tokens)[*token_index];
  
  if (token->type == SQL_TokenType_Identifier || 
      token->type == SQL_TokenType_Number ||
      token->type == SQL_TokenType_String)
  {
    SQL_Node *node = push_array(arena, SQL_Node, 1);
    
    switch (token->type)
    {
      case SQL_TokenType_Identifier: node->type = SQL_NodeType_Column; break;
      case SQL_TokenType_Number: node->type = SQL_NodeType_Numeric; break;
      case SQL_TokenType_String: node->type = SQL_NodeType_Literal; break;
    }
    node->value = token->value;
    (*token_index)++;
    return node;
  }
  
  log_error("unexpected token '%.*s' in expression", token->value.size, token->value.str);
  return NULL;
}


//~ tec: debug printing

internal String8
sql_node_type_to_string(SQL_NodeType type)
{
  String8 result = str8_lit("no type");
  
  switch (type)
  {
    case SQL_NodeType_Use: result = str8_lit("SQL_NodeType_Use"); break;
    case SQL_NodeType_Select: result = str8_lit("SQL_NodeType_Select"); break;
    case SQL_NodeType_Column: result = str8_lit("SQL_NodeType_Column"); break;
    case SQL_NodeType_ColumnList: result = str8_lit("SQL_NodeType_ColumnList"); break;
    case SQL_NodeType_Table: result = str8_lit("SQL_NodeType_Table"); break;
    case SQL_NodeType_Database: result = str8_lit("SQL_NodeType_Database"); break;
    case SQL_NodeType_Where: result = str8_lit("SQL_NodeType_Where"); break;
    case SQL_NodeType_Operator: result = str8_lit("SQL_NodeType_Operator"); break;
    case SQL_NodeType_Numeric: result = str8_lit("SQL_NodeType_Numeric"); break;
    case SQL_NodeType_Identifier: result = str8_lit("SQL_NodeType_Identifier"); break;
    case SQL_NodeType_Literal: result = str8_lit("SQL_NodeType_Literal"); break;
    case SQL_NodeType_Insert: result = str8_lit("SQL_NodeType_Insert"); break;
    case SQL_NodeType_Import: result = str8_lit("SQL_NodeType_Import"); break;
    case SQL_NodeType_Delete: result = str8_lit("SQL_NodeType_Delete"); break;
    case SQL_NodeType_Create: result = str8_lit("SQL_NodeType_Create"); break;
    case SQL_NodeType_Drop: result = str8_lit("SQL_NodeType_Drop"); break;
    case SQL_NodeType_Alter: result = str8_lit("SQL_NodeType_Alter"); break;
    case SQL_NodeType_Row: result = str8_lit("SQL_NodeType_Row"); break;
    case SQL_NodeType_Value: result = str8_lit("SQL_NodeType_Value"); break;
    case SQL_NodeType_ValueGroup: result = str8_lit("SQL_NodeType_ValueGroup"); break;
    case SQL_NodeType_Type: result = str8_lit("SQL_NodeType_Type"); break;
    case SQL_NodeType_OrderBy: result = str8_lit("SQL_NodeType_OrderBy"); break;
    case SQL_NodeType_Alter_AddColumn: result = str8_lit("SQL_NodeType_Alter_AddColumn"); break;
    case SQL_NodeType_Alter_ColumnType: result = str8_lit("SQL_NodeType_Alter_ColumnType"); break;
    case SQL_NodeType_Alter_DropColumn: result = str8_lit("SQL_NodeType_Alter_DropColumn"); break;
    case SQL_NodeType_Alter_Rename: result = str8_lit("SQL_NodeType_Alter_Rename"); break;
  }
  
  return result;
}

internal void
sql_print_node(SQL_Node *node, U64 depth)
{
  if (!node) return;
  
  for (U64 i = 0; i < depth; i++) printf("  ");
  
  String8 type = sql_node_type_to_string(node->type);
  printf("SQL_Node: type=%.*s, value=%.*s\n", (int)type.size, type.str, (int)node->value.size, node->value.str);
  
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
