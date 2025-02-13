internal IR_Query*
ir_generate_from_ast(Arena* arena, SQL_Node* ast_root)
{
  if (!ast_root || ast_root->type != SQL_NodeType_Use) 
  {
    log_error("Invalid AST root node. Expected USE node.");
    return NULL;
  }
  
  IR_Query *ir_query = push_array(arena, IR_Query, 1);
  ir_query->select_columns = NULL;
  ir_query->from_table = NULL;
  ir_query->where_conditions = NULL;
  ir_query->select_column_count = 0;
  
  SQL_Node *use_node = ast_root;
  if (!use_node->first || use_node->type != SQL_NodeType_Use) 
  {
    log_error("Invalid USE clause. Expected a database name.");
    return NULL;
  }
  
  IR_Node *ir_use = push_array(arena, IR_Node, 1);
  ir_use->type = IR_NodeType_Database;
  ir_use->value = use_node->first->value;
  ir_use->left = NULL;
  ir_use->right = NULL;
  ir_use->next = NULL;
  
  ir_query->database = ir_use;
  
  SQL_Node *select_node = use_node->next;
  if (!select_node || select_node->type != SQL_NodeType_Select)
  {
    log_error("Expected SELECT clause after USE clause.");
    return NULL;
  }
  
  SQL_Node *current_child = select_node->first;
  IR_Node *ir_select_head = NULL;
  IR_Node *ir_select_tail = NULL;
  
  while (current_child && current_child->type == SQL_NodeType_Column) 
  {
    IR_Node *ir_column = push_array(arena, IR_Node, 1);
    ir_column->type = IR_NodeType_Column;
    ir_column->value = current_child->value;
    ir_column->left = NULL;
    ir_column->right = NULL;
    ir_column->next = NULL;
    
    if (!ir_select_head) 
    {
      ir_select_head = ir_select_tail = ir_column;
    }
    else 
    {
      ir_select_tail->next = ir_column;
      ir_select_tail = ir_column;
    }
    
    current_child = current_child->next;
    ir_query->select_column_count += 1;
  }
  
  ir_query->select_columns = ir_select_head;
  
  SQL_Node *current_sibling = select_node->next;
  
  while (current_sibling)
  {
    if (current_sibling->type == SQL_NodeType_Table)
    {
      IR_Node *ir_table = push_array(arena, IR_Node, 1);
      ir_table->type = IR_NodeType_Table;
      ir_table->value = current_sibling->value;
      ir_table->left = NULL;
      ir_table->right = NULL;
      ir_table->next = NULL;
      
      ir_query->from_table = ir_table;
    } 
    else if (current_sibling->type == SQL_NodeType_Operator)
    {
      IR_Node *ir_where = push_array(arena, IR_Node, 1);
      ir_where->type = IR_NodeType_Condition;
      ir_where->value = str8_lit("");
      ir_where->left = NULL;
      ir_where->right = NULL;
      ir_where->next = NULL;
      
      IR_Node *condition_root = ir_convert_expression(arena, current_sibling);
      if (!condition_root) 
      {
        log_error("Failed to convert WHERE clause to IR.");
        return NULL;
      }
      
      ir_where->left = condition_root;
      ir_query->where_conditions = ir_where;
    }
    
    current_sibling = current_sibling->next;
  }
  
  return ir_query;
}

internal IR_Node*
ir_convert_expression(Arena* arena, SQL_Node *ast_expr) 
{
  if (!ast_expr) return NULL;
  
  IR_Node *ir_node = push_array(arena, IR_Node, 1);
  ir_node->type = (ast_expr->type == SQL_NodeType_Column) ? IR_NodeType_Column :
  (ast_expr->type == SQL_NodeType_Literal) ? IR_NodeType_Literal :
  (ast_expr->type == SQL_NodeType_Operator) ? IR_NodeType_Operator : IR_NodeType_Condition;
  ir_node->value = ast_expr->value;
  ir_node->next = NULL;
  ir_node->left = NULL;
  ir_node->right = NULL;
  
  if (ast_expr->first)
  {
    ir_node->left = ir_convert_expression(arena, ast_expr->first);
    if (ast_expr->first->next)
    {
      ir_node->right = ir_convert_expression(arena, ast_expr->first->next);
    }
  }
  
  return ir_node;
}

internal void
ir_print_node(IR_Node *node, U64 depth)
{
  if (!node) return;
  
  for (U64 i = 0; i < depth; i++) printf("  ");
  
  printf("IR_Node: type=%d, value=%.*s\n",
         node->type,
         (int)node->value.size, node->value.str);
  
  if (node->left) 
  {
    for (U64 i = 0; i < depth; i++) printf("  ");
    printf("Left:\n");
    ir_print_node(node->left, depth + 1);
  }
  if (node->right)
  {
    for (U64 i = 0; i < depth; i++) printf("  ");
    printf("Right:\n");
    ir_print_node(node->right, depth + 1);
  }
  
  if (node->next) 
  {
    ir_print_node(node->next, depth);
  }
}

internal void
ir_print_query(IR_Query *query)
{
  printf("IR Query:\n");
  
  printf("Use database:\n");
  if (query->database) 
  {
    ir_print_node(query->database, 1);
  }
  else
  {
    printf("  None\n");
  }
  
  printf("Select Columns:\n");
  ir_print_node(query->select_columns, 1);
  
  printf("From Table:\n");
  if (query->from_table)
  {
    ir_print_node(query->from_table, 1);
  }
  else
  {
    printf("  None\n");
  }
  
  printf("Where Conditions:\n");
  if (query->where_conditions)
  {
    ir_print_node(query->where_conditions, 1);
  } 
  else
  {
    printf("  None\n");
  }
}
